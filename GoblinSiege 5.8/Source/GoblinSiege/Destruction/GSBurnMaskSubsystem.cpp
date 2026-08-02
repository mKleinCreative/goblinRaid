#include "Destruction/GSBurnMaskSubsystem.h"

#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInstanceDynamic.h"

// IsRunningDedicatedServer() lives here. It compiled without this today only because something
// else in the include list drags CoreMisc in transitively, which is a dependency on the engine's
// header layout rather than on anything we control - one header reshuffle upstream and this file
// stops compiling for a reason that has nothing to do with it (2026-07-31).
#include "Misc/CoreMisc.h"
#include "TimerManager.h"

namespace GSBurnMask
{
	/**
	 * The material parameter names a crop (or ground) material must declare to receive the mask.
	 * Kept together so the header's documented contract and the code that fulfils it cannot drift.
	 *
	 * RENAMED 2026-07-31 from GS_FieldBurnMask / GS_FieldOrigin / GS_FieldSize. The old names were
	 * accurate when the rectangle was one field's grid box; they are actively misleading now that
	 * it is the whole world, and a material author reading "Field" would reasonably assume the
	 * rectangle moves with a field actor. Any material already wired to the old names must be
	 * re-pointed - there are none in Content as of today, which is why this is a rename rather
	 * than an alias.
	 */
	static const FName ParamBurnMask(TEXT("GS_BurnMask"));
	static const FName ParamBurnMaskOrigin(TEXT("GS_BurnMaskOrigin"));
	static const FName ParamBurnMaskSize(TEXT("GS_BurnMaskSize"));
}

UGSBurnMaskSubsystem::UGSBurnMaskSubsystem()
{
	// Tutorial_Island's crop, by mesh name: SM_VillageWheat_01, SM_VillageWheat_02,
	// SM_VillageWheat_Grass. One substring catches all three. Set in the constructor rather than
	// inline because a TArray UPROPERTY default has to be built somewhere, and Config = Game means
	// a project can replace this list entirely from DefaultGame.ini without touching code.
	CropMeshNameFilters.Add(FName("Wheat"));
}

// ====================================================================== lifecycle

bool UGSBurnMaskSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	// A dedicated server has no renderer. Creating the subsystem there would mean a 4MB render
	// target and a canvas pass every 0.15 s on the one machine that can never see either. Every
	// caller is already null-safe (see Get), so simply not existing is the cheapest possible
	// "dedicated server does no canvas work".
	if (IsRunningDedicatedServer())
	{
		return false;
	}

	const UWorld* World = Cast<UWorld>(Outer);
	if (!World)
	{
		return false;
	}

	// Game and PIE only. UE spins up editor, preview, thumbnail and inactive worlds constantly -
	// an asset thumbnail should not allocate a level-sized render target.
	return World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE;
}

void UGSBurnMaskSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Deliberately empty of work. The render target is created lazily on first use and the
	// landscape sweep waits for OnWorldBeginPlay, because level actors are not reliably spawned
	// during world initialisation and a bounds derivation that runs too early silently falls back
	// to the default rectangle - which then looks like the mask is misaligned rather than early.
}

void UGSBurnMaskSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!IsMaskEnabled())
	{
		return;
	}

	EnsureBounds();
	EnsureMask();

	// Settle the brush question NOW rather than on the first flush (2026-07-31). The effective
	// splat radius it decides is read by GetOverlapCompensation, and the field asks for that on
	// its first cosmetic tick - which can land before the first canvas flush. Resolving here means
	// the answer is never "not asked yet" during play, and the missing-brush warning arrives at
	// begin-play, next to the rest of the mask's start-up log, instead of a second into a raid.
	ResolveBrush();

	RegisterCropMaterials();

	// The timer is NOT started here. It starts on the first splat and stops again when the queue
	// drains (see EnsureFlushTimer / FlushSplats), so a level where nothing has been set alight
	// costs literally nothing per frame.
}

void UGSBurnMaskSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlushTimerHandle);
	}

	PendingSplats.Reset();
	TransientSettleFlushes = 0;

	// MIDs first, RT second - the MIDs point AT the render target, and the crop components they
	// live on (InstancedFoliageActor's foliage components, most of all) outlive this subsystem.
	ReleaseCropMaterials();

	// RELEASE, then null. Nulling alone only drops our reference and leaves the GPU resource to be
	// reclaimed whenever the RT object is eventually collected; on PIE stop/start that is a 4MB
	// target per session sitting around until the next GC.
	if (MaskRT)
	{
		UKismetRenderingLibrary::ReleaseRenderTarget2D(MaskRT.Get());
	}
	MaskRT = nullptr;
	ResolvedSplatBrush = nullptr;

	Super::Deinitialize();
}

UGSBurnMaskSubsystem* UGSBurnMaskSubsystem::Get(const UObject* WorldContextObject)
{
	if (!GEngine || !WorldContextObject)
	{
		return nullptr;
	}

	// ReturnNull rather than LogAndReturnNull: a CDO or a component mid-teardown asking for the
	// mask is normal, and it is not worth a warning per call.
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UGSBurnMaskSubsystem>() : nullptr;
}

bool UGSBurnMaskSubsystem::IsMaskEnabled() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Belt-and-braces against ShouldCreateSubsystem: a listen server is fine, a dedicated one is
	// not, and net mode is only reliably known once the world is up.
	return !IsRunningDedicatedServer() && World->GetNetMode() != NM_DedicatedServer;
}

// ====================================================================== bounds

void UGSBurnMaskSubsystem::EnsureBounds()
{
	if (bBoundsResolved)
	{
		return;
	}
	bBoundsResolved = true;

	UWorld* World = GetWorld();

	// ALandscapeProxy rather than ALandscape, deliberately: ALandscape is the base actor, but a
	// World Partition or streaming landscape splits its geometry across ALandscapeStreamingProxy
	// actors that are NOT children of it. Iterating the proxy base class and unioning covers both
	// layouts; iterating ALandscape alone would return the correct bounds on a simple map and a
	// fraction of the terrain on a streamed one, which is the kind of bug that only appears on the
	// level you ship.
	FBox LandscapeBounds(ForceInit);
	if (World)
	{
		for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
		{
			if (const ALandscapeProxy* Proxy = *It)
			{
				LandscapeBounds += Proxy->GetComponentsBoundingBox(/*bNonColliding=*/true);
			}
		}
	}

	if (LandscapeBounds.IsValid != 0 && LandscapeBounds.GetSize().X > 1.f && LandscapeBounds.GetSize().Y > 1.f)
	{
		const FVector Min = LandscapeBounds.Min - FVector(BoundsMarginWorld, BoundsMarginWorld, 0.f);
		const FVector Max = LandscapeBounds.Max + FVector(BoundsMarginWorld, BoundsMarginWorld, 0.f);

		MaskOrigin = FVector2D(Min.X, Min.Y);
		MaskSize = FVector2D(Max.X - Min.X, Max.Y - Min.Y);

		UE_LOG(LogTemp, Log,
			TEXT("[GoblinSiege] Burn mask bounds from landscape: origin (%.0f, %.0f) size (%.0f, %.0f) uu "
				 "- %.2f uu per texel at %d."),
			MaskOrigin.X, MaskOrigin.Y, MaskSize.X, MaskSize.Y,
			MaskSize.X / FMath::Max(1, BurnMaskResolution), BurnMaskResolution);
	}
	else
	{
		// No landscape: a blockout, a test map, a modular interior. Fall back to a square centred
		// on the world origin and SAY SO ONCE, because "my burn marks are in the wrong place" is
		// otherwise a debugger session rather than a log line.
		const float Half = FMath::Max(1000.f, FallbackWorldSize) * 0.5f;
		MaskOrigin = FVector2D(-Half, -Half);
		MaskSize = FVector2D(Half * 2.f, Half * 2.f);

		if (!bWarnedNoLandscape)
		{
			bWarnedNoLandscape = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[GoblinSiege] Burn mask found no ALandscapeProxy in this world - falling back to a "
					 "%.0f uu square centred on the world origin. Anything burning outside that square "
					 "will mark the wrong texel or none at all. Call SetBounds to fix it."),
				FallbackWorldSize);
		}
	}

	// A non-square world rectangle against a square render target means anisotropic texels. That
	// is fine and is handled at draw time: FlushSplats converts the splat radius per axis, so a
	// splat still comes out as a CIRCLE in world space even when the texels are not square.
}

void UGSBurnMaskSubsystem::SetBounds(FVector2D InOrigin, FVector2D InSize)
{
	if (InSize.X <= 0.f || InSize.Y <= 0.f)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] Burn mask SetBounds ignored - size (%.1f, %.1f) must be positive on both axes."),
			InSize.X, InSize.Y);
		return;
	}

	MaskOrigin = InOrigin;
	MaskSize = InSize;
	bBoundsResolved = true;

	// Every existing texel describes the OLD rectangle. Reinterpreting it under the new one would
	// smear the burn so far across the level, so the honest move is to start clean and re-publish.
	ClearMask();
	for (const TObjectPtr<UMaterialInstanceDynamic>& MID : CropMaterials)
	{
		PublishParamsToMID(MID.Get());
	}
}

// ====================================================================== the render target

void UGSBurnMaskSubsystem::EnsureMask()
{
	if (MaskRT || !IsMaskEnabled())
	{
		return;
	}

	EnsureBounds();

	const int32 Resolution = FMath::Clamp(BurnMaskResolution, 16, 8192);
	MaskRT = UKismetRenderingLibrary::CreateRenderTarget2D(
		this, Resolution, Resolution, ETextureRenderTargetFormat::RTF_RGBA8);

	if (MaskRT)
	{
		UKismetRenderingLibrary::ClearRenderTarget2D(this, MaskRT.Get(), FLinearColor::Black);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] Burn mask could not create its render target - crop and ground will burn "
				 "without marking. Nothing else about the fire is affected."));
	}
}

UTextureRenderTarget2D* UGSBurnMaskSubsystem::GetMaskRT()
{
	EnsureMask();
	return MaskRT.Get();
}

void UGSBurnMaskSubsystem::ClearMask()
{
	PendingSplats.Reset();
	TransientSettleFlushes = 0;

	if (MaskRT)
	{
		UKismetRenderingLibrary::ClearRenderTarget2D(this, MaskRT.Get(), FLinearColor::Black);
	}
}

UTexture2D* UGSBurnMaskSubsystem::ResolveBrush()
{
	// RESOLVE ONCE, INCLUDING THE FAILURE. Same pattern as AGSFieldFireObjective's
	// bSmokeSystemResolveFailed: the warn latch below suppresses our log line, but only this latch
	// suppresses the WORK. LoadSynchronous does not cache a failed lookup, so a soft pointer at an
	// unauthored asset is a full failed package load on every flush - 6-7 times a second, for the
	// length of a raid - unless the answer "there is nothing there" is remembered too.
	if (!ResolvedSplatBrush && !bSplatBrushResolveFailed)
	{
		ResolvedSplatBrush = SplatBrush.IsNull() ? nullptr : SplatBrush.LoadSynchronous();
		bSplatBrushResolveFailed = (ResolvedSplatBrush == nullptr);

		// THE ONE PLACE THE EFFECTIVE RADIUS IS DECIDED (2026-07-31). Both the quad FlushSplats
		// draws and the divisor GetOverlapCompensation applies read this single value, so they
		// cannot disagree about how wide a splat is. They did briefly: the no-brush clamp shrank
		// the stamp to half a cell while the compensation still divided by the authored radius,
		// which quietly cost ~30% of the char on a project with no brush authored - which is
		// every project today.
		//
		// Half a cell without a brush: a flat white square has no falloff at all, so anything
		// wider chars the neighbouring cells and the doused firebreaks at full strength.
		EffectiveSplatRadiusCells = ResolvedSplatBrush
			? FMath::Max(0.5f, SplatRadiusCells)
			: 0.5f;
	}

	if (!ResolvedSplatBrush && !bWarnedMissingSplatBrush)
	{
		bWarnedMissingSplatBrush = true;

		// One line, once, and it now names the SECOND half of the degradation as well (2026-07-31):
		// with no brush the stamp has no falloff at all, so FlushSplats also clamps its radius to
		// half a cell to keep a full-strength square off the neighbouring ground. Blocky AND
		// tighter than tuned - both are consequences of the missing asset, and someone reading
		// "why is my burn a small hard square" deserves to find the answer in one place.
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] Burn mask has no SplatBrush (%s) - falling back to hard squares, and "
				 "clamping the splat radius to half a cell (from %.2f cells) because a flat white "
				 "stamp has no falloff to spare and would char the neighbouring cells and the "
				 "firebreaks at full strength. The world still marks, it just reads as blocks "
				 "instead of as a burn. Wants a greyscale radial gradient, white centre to black "
				 "edge."),
			*SplatBrush.ToString(), SplatRadiusCells);
	}

	return ResolvedSplatBrush;
}

// ====================================================================== deposit arithmetic

float UGSBurnMaskSubsystem::GetEffectiveSplatRadiusCells() const
{
	// 0 = ResolveBrush has not run yet. Fall back to the AUTHORED radius rather than the degraded
	// one: a caller that beats begin-play should get the tuned number, not the missing-art number.
	// OnWorldBeginPlay resolves the brush precisely so this branch is dead during play.
	return EffectiveSplatRadiusCells > 0.f
		? EffectiveSplatRadiusCells
		: FMath::Max(0.5f, SplatRadiusCells);
}

float UGSBurnMaskSubsystem::GetOverlapCompensation(float CellSizeWorld) const
{
	// ONE DEFINITION OF THE OVERLAP, and it has to agree EXACTLY with the quad FlushSplats draws
	// or the correction is worse than the bug it fixes (2026-07-31). The header carries the full
	// derivation and the worked numbers; what follows is the same arithmetic in code, in the same
	// order, deliberately un-collapsed so the two can be read side by side.
	//
	// Both over-deposit terms are here because both are real and they multiply:
	//   (1) the quad is 2 * radius CELLS wide, so that many cells cover any given point;
	//   (2) a texel is wider than a cell at this resolution, and FlushSplats floors every splat at
	//       half a texel, so the smallest quad the mask can draw is one whole texel - ~1.44 cells
	//       on Tutorial_Island - however small the cell is.
	// A cell-space-only derivation catches (1) and misses (2), which left R integrating to ~2
	// instead of 1 and the char ramping in half the burn.
	const float RadiusCells = GetEffectiveSplatRadiusCells();

	// The quad, in cells per axis, before the mask's own resolution has any say.
	const float QuadCells = 2.f * RadiusCells;

	// The pure cell-space answer. It is also the fallback: with no cell size, no bounds or no
	// resolution there is nothing to convert to texels with, and term (1) alone is strictly better
	// than no correction at all.
	const float GeometricOverlap = FMath::Square(QuadCells);

	const int32 Resolution = FMath::Clamp(BurnMaskResolution, 16, 8192); // as EnsureMask clamps it
	if (CellSizeWorld <= 0.f || MaskSize.X <= 0.f || MaskSize.Y <= 0.f)
	{
		return 1.f / FMath::Max(1.f, GeometricOverlap);
	}

	// uu per texel, PER AXIS: the world rectangle is not necessarily square while the render
	// target always is, so texels can be anisotropic - the same reason FlushSplats converts the
	// splat radius separately on each axis.
	const float WorldPerTexelX = static_cast<float>(MaskSize.X) / static_cast<float>(Resolution);
	const float WorldPerTexelY = static_cast<float>(MaskSize.Y) / static_cast<float>(Resolution);

	const float QuadWorld = QuadCells * CellSizeWorld;

	// max(1, ...) IS the half-texel floor from FlushSplats, expressed as a whole quad: that floor
	// is on the half-extent, so the smallest quad it can produce is one texel across. Skip this
	// and the compensation would keep shrinking with the cell while the drawn quad could not.
	// The 1e-4 floors are paranoia rather than a real case - MaskSize is positive by the guard
	// above and the resolution is clamped to >= 16 - but a divide by zero here would poison every
	// deposit in the level with a NaN, and the mask has no way to notice it had.
	const float QuadTexelsX = FMath::Max(1.f, QuadWorld / FMath::Max(1e-4f, WorldPerTexelX));
	const float QuadTexelsY = FMath::Max(1.f, QuadWorld / FMath::Max(1e-4f, WorldPerTexelY));

	// Back from texels to cells: how many cells' worth of ground the quad REALLY covers, which is
	// how many cells really deposit into any texel it touches.
	const float FootprintCellsX = QuadTexelsX * WorldPerTexelX / CellSizeWorld;
	const float FootprintCellsY = QuadTexelsY * WorldPerTexelY / CellSizeWorld;

	const float Overlap = FootprintCellsX * FootprintCellsY;

	// Floored at 1 so this can only ever THIN a deposit. A compensation that amplified one would
	// be a bug wearing this function's name, and it is the sort of thing a hand-edited config
	// could otherwise produce.
	return 1.f / FMath::Max(1.f, Overlap);
}

int32 UGSBurnMaskSubsystem::GetTransientSettleFlushCount() const
{
	// Same clamp FlushSplats applies, so the count is derived from the decay that will actually be
	// used rather than from the raw config value.
	const float Decay = FMath::Clamp(TransientChannelDecay, 0.f, 0.99f);

	// Decay 0 wipes the transient channels to black in a single modulate pass - one flush is the
	// whole settle. (Also the guard against Loge(0) = -inf.)
	if (Decay <= 0.f)
	{
		return 1;
	}

	// Unreachable through the clamp above, and kept anyway: a decay of 1 never fades, so no finite
	// count settles it. Returning 1 keeps the timer from spinning forever over a mask that will
	// not change; the ember staying put is then honestly the decay's doing, not a leaked timer.
	if (Decay >= 1.f)
	{
		return 1;
	}

	// Solve Decay^n <= 1/255 for n - one 8-bit step is the point below which the channel IS black.
	//     n = log(1/255) / log(Decay)
	// Both logs are negative so the quotient is positive; ceil takes it to the first whole flush
	// that reaches black, and +1 is the margin for the rounding that put 255 there in the first
	// place. At the default 0.55 this is 11 flushes (~1.7 s); at 0.9 it is 54 (~8 s), which is the
	// case the old hard-coded 16 froze an ember on.
	const float Flushes = FMath::Loge(1.f / 255.f) / FMath::Loge(Decay);

	return FMath::CeilToInt(Flushes) + 1;
}

// ====================================================================== splatting

void UGSBurnMaskSubsystem::SplatBurn(const FVector& WorldLocation, float RadiusWorld,
	float Burnt01, float Ember01, float Doused01)
{
	if (!IsMaskEnabled() || RadiusWorld <= 0.f)
	{
		return;
	}

	const float R = FMath::Clamp(Burnt01, 0.f, 1.f);
	const float G = FMath::Clamp(Ember01, 0.f, 1.f);
	const float B = FMath::Clamp(Doused01, 0.f, 1.f);

	// A splat that deposits nothing is not worth a canvas draw. Callers splat per cell per tick,
	// so most of what arrives here while a field is half burnt is exactly this.
	if (R <= 0.f && G <= 0.f && B <= 0.f)
	{
		return;
	}

	EnsureBounds();

	FGSBurnSplat& Splat = PendingSplats.AddDefaulted_GetRef();
	Splat.WorldXY = FVector2D(WorldLocation.X, WorldLocation.Y);
	Splat.RadiusWorld = RadiusWorld;
	Splat.Burnt01 = R;
	Splat.Ember01 = G;
	Splat.Doused01 = B;

	// The transient channels are wiped a little further toward black on every flush, so anything
	// that just published a G or B has to keep the decay running long enough for it to reach
	// black - otherwise a fire that stops dead freezes its last ember frame on the mask forever.
	//
	// DERIVED from the decay rather than assumed (2026-07-31): this used to be a literal 16, which
	// is only long enough for the default 0.55 and silently too short for any slower fade the
	// project sets in DefaultGame.ini. See GetTransientSettleFlushCount.
	if (G > 0.f || B > 0.f)
	{
		TransientSettleFlushes = GetTransientSettleFlushCount();
	}

	EnsureFlushTimer();

	// Safety valve for a debug command that razes a village in one frame. Normal play never gets
	// near this - three fields re-splatting ~40 cells each is ~120 entries per interval.
	if (PendingSplats.Num() >= FMath::Max(16, MaxQueuedSplats))
	{
		FlushSplats();
	}
}

void UGSBurnMaskSubsystem::EnsureFlushTimer()
{
	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(FlushTimerHandle))
	{
		return;
	}

	// Timer, not tick - house style, and here it is also the whole batching mechanism.
	World->GetTimerManager().SetTimer(FlushTimerHandle, this,
		&UGSBurnMaskSubsystem::FlushSplats,
		FMath::Max(0.02f, MaskRedrawInterval), true);
}

void UGSBurnMaskSubsystem::FlushSplats()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// IDLE = OFF. Once the queue has drained and the transient channels have settled to black,
	// stop the timer entirely rather than opening an empty canvas 6-7 times a second for the rest
	// of the raid. SplatBurn restarts it.
	if (PendingSplats.Num() == 0 && TransientSettleFlushes <= 0)
	{
		World->GetTimerManager().ClearTimer(FlushTimerHandle);
		return;
	}

	EnsureMask();
	if (!MaskRT || MaskSize.X <= 0.f || MaskSize.Y <= 0.f)
	{
		// STOP THE TIMER, do not just drop the queue (2026-07-31). This early-out returns above
		// both the settle decrement and the idle check, so a world where CreateRenderTarget2D
		// fails - out of video memory, a headless-ish client, a hostile RHI - used to sit here
		// re-attempting a 4 MB allocation 6-7 times a second for the rest of the raid, forever,
		// with nothing to show for it. There is no mask and no queue, so there is nothing for this
		// timer to do; SplatBurn restarts it if a caller ever gives it work again, which also
		// re-attempts the allocation at a sane cadence rather than as a spin.
		PendingSplats.Reset();
		TransientSettleFlushes = 0;
		World->GetTimerManager().ClearTimer(FlushTimerHandle);
		return;
	}

	UTexture2D* Brush = ResolveBrush();

	// NO BRUSH = NO FALLOFF, so shrink the stamp to fit inside its own cell (2026-07-31).
	//
	// The null path resolves to GWhiteTexture, which is a flat (1,1,1,1) square - not a radial
	// gradient with hard edges, no gradient at all. SplatRadiusCells is tuned on the assumption
	// that the outer part of the stamp is faint; with a flat source the outer part is as strong as
	// the centre, so a burning cell paints its neighbours' ground fully charred and the doused
	// firebreak beside it renders as burnt as the fire. That is the defenders' verb visibly not
	// working, caused entirely by missing art.
	//
	// THE SHRINK IS EXPRESSED AS A RATIO AGAINST THE SAME EFFECTIVE RADIUS GetOverlapCompensation
	// DIVIDES BY, and that is the point of routing it through the getter rather than repeating the
	// 0.5: the quad drawn here and the divisor applied there are then two readings of one number
	// and cannot drift. When they did drift - the stamp clamped, the divisor not - char landed
	// ~30% short of the ramp on every project with no brush authored.
	//
	// A prop splat passes its own bounds rather than a cell size, so it is scaled by the same
	// ratio and simply stamps a slightly tighter square - the right side of the trade for a stamp
	// that has no soft edge to spare.
	const float SplatRadiusScale =
		GetEffectiveSplatRadiusCells() / FMath::Max(0.5f, SplatRadiusCells);

	UCanvas* Canvas = nullptr;
	FVector2D CanvasSize = FVector2D::ZeroVector;
	FDrawToRenderTargetContext Context;

	// ONE CANVAS PAIR FOR THE WHOLE BATCH. This is the entire performance argument for the queue:
	// BeginDrawCanvasToRenderTarget flushes rendering commands and transitions the target, and
	// doing that per splat with three fields alight would be dozens of transitions a second.
	// Drawing INTO an open canvas is nearly free by comparison. Never open a canvas per splat.
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, MaskRT.Get(), Canvas, CanvasSize, Context);

	if (Canvas && CanvasSize.X > 0.f && CanvasSize.Y > 0.f)
	{
		// ---- pass 1: decay the TRANSIENT channels ------------------------------------------
		//
		// THE G/B DECISION, and it is the one architectural choice in this file that deserves the
		// space (2026-07-31). The brief offered two options: rebuild the whole mask from the
		// registered sources every flush, or accept that G and B cannot be erased on an additive
		// canvas and document that they are not decayed. NEITHER was taken, because a third
		// option is strictly better and costs one quad:
		//
		//     A full-target draw with BLEND_Modulate and colour (1, decay, decay, 1).
		//     Modulate is dest * src, so:  R *= 1   (bit-identical, monotonic guarantee intact)
		//                                  G *= decay, B *= decay  (exponential fade to black)
		//
		// Why not the full rebuild: there is no source registry to rebuild FROM, and building one
		// would be worse than the problem. The mask is world-scale now - its sources are every
		// cell of every field (three fields x 40 cells on Tutorial_Island), plus every prop that
		// has ever burned down, and a burned-down prop is a ONE-SHOT event with no live object
		// holding "I am still scorched here". A rebuild would therefore need a permanently growing
		// list of every mark ever made, re-drawn 6-7 times a second, forever. The per-field mask
		// could afford a wholesale rebuild because it was 40 splats into a 128x128 target; this is
		// not that system.
		//
		// Why not "document that G/B never decay": because ember glow that never fades is not a
		// degraded feature, it is a broken one - a razed field would glow at full ember over ash
		// for the rest of the raid, which is precisely the "the raid has no memory, but the wrong
		// way round" failure this whole change exists to avoid. And doused would be permanent,
		// making the brigade's water an indelible blue stain.
		//
		// The cost of the chosen approach: sources must RE-SPLAT their live G and B, because the
		// mask no longer remembers them. AGSFieldFireObjective::CosmeticTick does exactly that for
		// every burning and doused cell, which is also what animates the ember arc - the same work
		// the old RedrawBurnMask did for the same reason, just expressed as deposits rather than
		// as a full redraw. R needs no re-splat: it accumulates and stays.
		const float Decay = FMath::Clamp(TransientChannelDecay, 0.f, 0.99f);
		Canvas->K2_DrawTexture(
			nullptr, // GWhiteTexture: a flat (1,1,1,1) source, so the modulate is purely our colour
			FVector2D::ZeroVector,
			CanvasSize,
			FVector2D::ZeroVector,
			FVector2D::UnitVector,
			FLinearColor(1.f, Decay, Decay, 1.f),
			BLEND_Modulate);

		// ---- pass 2: the queued splats ------------------------------------------------------
		const float TexelsPerUnitX = static_cast<float>(CanvasSize.X) / static_cast<float>(MaskSize.X);
		const float TexelsPerUnitY = static_cast<float>(CanvasSize.Y) / static_cast<float>(MaskSize.Y);

		for (const FGSBurnSplat& Splat : PendingSplats)
		{
			// World -> UV -> texels. X maps to canvas X and Y to canvas Y, with no swizzle, so a
			// material samples with a plain (AbsoluteWorldPosition.xy - Origin.xy) / Size.xy.
			const float CentreX = static_cast<float>((Splat.WorldXY.X - MaskOrigin.X) * TexelsPerUnitX);
			const float CentreY = static_cast<float>((Splat.WorldXY.Y - MaskOrigin.Y) * TexelsPerUnitY);

			// Converted per axis so a splat is a CIRCLE in world space even when the world
			// rectangle is not square and the texels therefore are not either.
			//
			// Floored at half a texel: at 1024 over a 4 km landscape a texel is ~3.9 m, so a
			// single 270 uu field cell is smaller than one texel. Without the floor those splats
			// would round to a zero-size quad and vanish entirely - the field would burn and
			// nothing would mark, which is the exact failure this resolution tradeoff has to be
			// watched for (see BurnMaskResolution).
			//
			// SplatRadiusScale is 1 with a brush and shrinks the stamp to half a cell without one -
			// see where it is computed. Applied BEFORE the half-texel floor, so the degraded stamp
			// is still never allowed to round away to nothing.
			const float ScaledRadius = Splat.RadiusWorld * SplatRadiusScale;
			const float SplatX = FMath::Max(0.5f, ScaledRadius * TexelsPerUnitX);
			const float SplatY = FMath::Max(0.5f, ScaledRadius * TexelsPerUnitY);

			// ADDITIVE, and the brush is read for its RGB falloff rather than its alpha, because
			// additive blending ignores source alpha - the feathering has to live in the colour.
			//
			// R IS AN ACCUMULATOR AND IT SATURATES; it is never recomputed from a source. Deposits
			// sum and clamp at 1 in RGBA8, which is what makes "monotonic, never decreases" true by
			// construction rather than by bookkeeping: there is no code path anywhere that can
			// write a smaller R than the texel already holds. The consequence callers must design
			// around is that a splat is a DEPOSIT, not a set - re-splatting 1.0 every tick drives
			// the texel black in one flush, which is right for a prop burning down and wrong for a
			// cell part-way through its burn. See SplatBurn's parameter docs.
			//
			// A null brush resolves to GWhiteTexture in the engine's canvas path, giving the
			// documented hard-square fallback rather than nothing at all.
			Canvas->K2_DrawTexture(
				Brush,
				FVector2D(CentreX - SplatX, CentreY - SplatY),
				FVector2D(SplatX * 2.f, SplatY * 2.f),
				FVector2D::ZeroVector,
				FVector2D::UnitVector,
				FLinearColor(Splat.Burnt01, Splat.Ember01, Splat.Doused01, 1.f),
				BLEND_Additive);
		}
	}

	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);

	PendingSplats.Reset();

	// Decremented OUTSIDE the canvas block on purpose: if the canvas ever fails to open, the
	// settle counter must still run down or the timer never goes idle again and this function
	// re-tries a broken canvas 6-7 times a second for the rest of the raid.
	if (TransientSettleFlushes > 0)
	{
		--TransientSettleFlushes;
	}
}

// ====================================================================== crop material binding

void UGSBurnMaskSubsystem::PublishParamsToMID(UMaterialInstanceDynamic* MID) const
{
	if (!MID)
	{
		return;
	}

	// SETTING A PARAMETER THE MATERIAL DOES NOT DECLARE IS A SAFE NO-OP IN UNREAL - the MID records
	// the override and nothing ever reads it. That is precisely why binding broadly is safe, and it
	// is the property the whole auto-bind rests on: a mesh whose material knows about GS_BurnMask
	// reacts, a mesh that has never heard of it is left looking exactly as it did.
	MID->SetTextureParameterValue(GSBurnMask::ParamBurnMask, MaskRT.Get());

	MID->SetVectorParameterValue(GSBurnMask::ParamBurnMaskOrigin,
		FLinearColor(static_cast<float>(MaskOrigin.X), static_cast<float>(MaskOrigin.Y), 0.f, 0.f));

	// Z is published as 1, not 0. Nothing should divide by it, but a material author who writes
	// WorldPos / GS_BurnMaskSize without masking to .xy gets a finite result instead of a NaN that
	// blows out the whole crop material.
	MID->SetVectorParameterValue(GSBurnMask::ParamBurnMaskSize,
		FLinearColor(static_cast<float>(MaskSize.X), static_cast<float>(MaskSize.Y), 1.f, 0.f));
}

bool UGSBurnMaskSubsystem::ShouldBindComponent(const UMeshComponent* Mesh, const AActor* Owner) const
{
	if (!Mesh)
	{
		return false;
	}

	// Escape hatch first: a tagged actor binds everything it has, whatever the meshes are called.
	// For crop the art side named off-pattern, and for ground meshes that want scorch but are not
	// crop at all.
	if (Owner && !CropActorTag.IsNone() && Owner->ActorHasTag(CropActorTag))
	{
		return true;
	}

	// No filter configured = bind everything. Legal, honest and expensive - see CropMeshNameFilters.
	if (CropMeshNameFilters.Num() == 0)
	{
		return true;
	}

	// The filter matches the STATIC MESH's name, not the component's or the actor's. Foliage
	// components are named by the foliage type and the owning InstancedFoliageActor is named for
	// the level, so neither of those identifies wheat; the mesh asset does.
	const UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Mesh);
	const UStaticMesh* StaticMesh = StaticMeshComp ? StaticMeshComp->GetStaticMesh() : nullptr;
	if (!StaticMesh)
	{
		return false;
	}

	const FString MeshName = StaticMesh->GetName();
	for (const FName& Filter : CropMeshNameFilters)
	{
		if (!Filter.IsNone() && MeshName.Contains(Filter.ToString(), ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

void UGSBurnMaskSubsystem::RegisterCropMaterials()
{
	if (!IsMaskEnabled())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	EnsureBounds();
	EnsureMask();
	if (!MaskRT)
	{
		return;
	}

	// Drop the cache first so a level that streams crop out does not keep MIDs for meshes that are
	// gone. Anything still present is re-found below, and CreateAndSetMaterialInstanceDynamic
	// returns the EXISTING MID for a component already bound rather than making a second one - so
	// re-running this is cheap and cannot double-bind. That is also what makes PIE stop/start safe
	// on the other side: a new PIE world gets a new subsystem, and this one's Deinitialize has
	// already nulled the texture out of every MID it made.
	ReleaseCropMaterials();

	int32 BoundSlots = 0;
	int32 BoundComponents = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		// UMeshComponent, and this is the line that makes the whole re-architecture work.
		//
		// UFoliageInstancedStaticMeshComponent derives from UHierarchicalInstancedStaticMeshComponent
		// -> UInstancedStaticMeshComponent -> UStaticMeshComponent -> UMeshComponent, so painted
		// foliage is picked up by this sweep with NO dependency on the Foliage module and no
		// special case. That matters here more than anywhere: Tutorial_Island's wheat is ~275,000
		// foliage instances across three InstancedFoliageActors and ZERO plain static mesh actors,
		// so a sweep that missed instanced components would find precisely nothing to bind.
		//
		// A MID on one of those components covers every instance it holds at once. Under the old
		// per-field masks that was the fatal defect (one component, three competing render
		// targets); with one world mask it is a feature - 91,500 wheat instances bound in one call.
		TArray<UMeshComponent*> Meshes;
		Actor->GetComponents<UMeshComponent>(Meshes);

		for (UMeshComponent* Mesh : Meshes)
		{
			if (!ShouldBindComponent(Mesh, Actor))
			{
				continue;
			}

			bool bBoundAnySlot = false;

			const int32 NumSlots = Mesh->GetNumMaterials();
			for (int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex)
			{
				// An empty slot yields a null MID and an engine warning; skip it.
				if (!Mesh->GetMaterial(SlotIndex))
				{
					continue;
				}

				UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(SlotIndex);
				if (!MID)
				{
					continue;
				}

				PublishParamsToMID(MID);

				CropMaterials.Add(MID);
				++BoundSlots;
				bBoundAnySlot = true;
			}

			if (bBoundAnySlot)
			{
				++BoundComponents;
			}
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("[GoblinSiege] World burn mask bound %d material slot(s) across %d mesh component(s). "
			 "Materials sample it with UV = (AbsoluteWorldPosition.xy - GS_BurnMaskOrigin.xy) / "
			 "GS_BurnMaskSize.xy; channels are R=burnt (monotonic), G=burning, B=doused."),
		BoundSlots, BoundComponents);
}

void UGSBurnMaskSubsystem::RefreshCropBindings()
{
	// A "re-publish to what we already hold first" loop stood here and was DELETED on 2026-07-31,
	// because it could not do the thing its comment claimed. RegisterCropMaterials below opens with
	// ReleaseCropMaterials, which nulls the mask out of every MID this loop had just written to and
	// empties the array - so the loop's entire output was discarded one line later. The re-sweep
	// then re-finds the same components, and CreateAndSetMaterialInstanceDynamic hands back the
	// SAME MIDs (it returns an existing one rather than making a second), which are published to
	// properly. Nothing was ever missing; the loop was pure work.
	//
	// Re-sweeping is the point of the call: crop that the hamlet generator SPAWNS did not
	// exist at world begin-play and so was never bound. A mesh that does not exist yet cannot be
	// found, and a field that silently does not mark is indistinguishable from a broken one.
	RegisterCropMaterials();
}

void UGSBurnMaskSubsystem::ReleaseCropMaterials()
{
	// Null the render target out of every MID before dropping it. These MIDs live on components
	// owned by OTHER actors - an InstancedFoliageActor holding 91,500 wheat instances, most of all -
	// which outlive this subsystem comfortably. Leaving them pointing at a render target we are
	// about to release is a genuine dangling reference, not a housekeeping nicety.
	for (const TObjectPtr<UMaterialInstanceDynamic>& MID : CropMaterials)
	{
		if (MID)
		{
			MID->SetTextureParameterValue(GSBurnMask::ParamBurnMask, nullptr);
		}
	}
	CropMaterials.Reset();
}
