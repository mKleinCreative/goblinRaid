// THE WORLD BURN MASK. One render target for the whole level, owned by a world subsystem.
// Written 2026-07-31, replacing the per-field render target AGSFieldFireObjective used to own.
//
// WHY THIS EXISTS AT ALL: "fire must leave a mark" (Michael, 2026-07-31). Props mark themselves
// with a MID on the actor (UGSBurnFXComponent, GS_BurnAmount). Ground and crop cannot: a wheat
// field is not an actor per cell, so the burn has to be published as a TEXTURE that materials
// sample by world position.
//
// WHY IT MOVED OFF THE FIELD, which is the important half of this file's history:
//
//   The per-field mask was written on 2026-07-31 assuming crop is made of static mesh ACTORS
//   standing inside the field's grid box, and that assumption is simply false. Verified live in
//   the editor on 2026-07-31: Tutorial_Island's wheat is PAINTED FOLIAGE - roughly 91,500 +
//   91,400 + 91,900 instances of SM_VillageWheat_01 / _02 / _Grass living inside
//   InstancedFoliageActors - and there is not one plain static mesh actor in the level using a
//   wheat mesh. The thing the old code went looking for does not exist.
//
//   Foliage components are per-ISLAND, not per-field. All ~275,000 wheat instances hang off a
//   handful of components, so a material instance created on one of them affects every instance
//   on the island at once. With three fields each owning its own render target (T-09 confirms
//   three on Tutorial_Island), one foliage material could only ever sample ONE of the three
//   masks - whichever field happened to bind last. Two thirds of the arson in the level would
//   silently leave no mark, and no amount of tuning inside AGSFieldFireObjective could fix it,
//   because the defect is in the ownership model rather than in the drawing.
//
//   A WORLD-SPACE mask does not care whether the geometry it marks is instanced, batched,
//   Nanite, or hand-placed - it only cares where the geometry is. That is why this is the
//   architecture the content actually demands rather than a tidier version of the old one. It
//   also makes prop scorch fall out for free: UGSBurnFXComponent splats the ground under a
//   granary as it burns down, which the per-field mask could never have done because a granary
//   is not in any field's grid box.
//
// CHANNEL CONTRACT - carried over UNCHANGED from the per-field mask, because crop materials were
// already written against it. Do not repurpose channels:
//
//     R = burnt-ness.  0 = untouched -> 1 = fully charred. MONOTONIC: it only ever rises. Blend
//                      charred albedo and flatten the crop normal with this.
//     G = actively burning. Ember glow under the crop; rises and falls with the flame. Transient.
//     B = doused / wet. The bucket brigade's water. Darken and gloss with this. Transient.
//
// See FlushSplats in the .cpp for HOW those three guarantees are met on an additive canvas, and
// for why the transient channels are decayed rather than rebuilt from a source registry.
#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSBurnMaskSubsystem.generated.h"

class UMaterialInstanceDynamic;
class UMeshComponent;
class UTexture2D;
class UTextureRenderTarget2D;

/**
 * One queued mark. Splats are QUEUED rather than drawn, because opening a canvas is the
 * expensive part of this system and drawing into it is nearly free - see SplatBurn.
 *
 * Stored in world XY only. The mask is a top-down projection; Z is discarded deliberately, which
 * is also why a burning cellar and the field above it would mark the same texel. Nothing in the
 * hamlet stacks burnables vertically, and the day something does, this is the comment to read.
 */
USTRUCT()
struct FGSBurnSplat
{
	GENERATED_BODY()

	FVector2D WorldXY = FVector2D::ZeroVector;

	/** World-space radius in unreal units, converted to texels at flush time. */
	float RadiusWorld = 0.f;

	/** Char DEPOSITED by this splat, not char to set. See FlushSplats - R is an accumulator. */
	float Burnt01 = 0.f;

	float Ember01 = 0.f;
	float Doused01 = 0.f;
};

/**
 * The world burn mask. Purely cosmetic, so it does not exist on a dedicated server and does not
 * exist in the editor world - see ShouldCreateSubsystem.
 *
 * UWorldSubsystem rather than an actor placed in the level: there is nothing to place, nothing to
 * replicate and nothing for a designer to forget. A world has exactly one of these, it is created
 * with the world and destroyed with it, and PIE stop/start gets a fresh one because PIE gets a
 * fresh world - which is the whole of the "must not leak or double-bind across PIE" requirement,
 * solved by the choice of container rather than by bookkeeping.
 *
 * Config = Game: the tuning below is settable in DefaultGame.ini under
 * [/Script/GoblinSiege.GSBurnMaskSubsystem]. A subsystem has no editor-visible CDO, so
 * EditDefaultsOnly alone would give a designer no way in; Config is the "EditDefaultsOnly" of a
 * subsystem.
 */
UCLASS(Config = Game)
class GOBLINSIEGE_API UGSBurnMaskSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UGSBurnMaskSubsystem();

	// ------------------------------------------------------------------ lifecycle

	/**
	 * Cosmetic only, so: never on a dedicated server (no renderer - allocating a 4MB render target
	 * and running canvas passes there is pure waste), and never on a world that is not Game or PIE
	 * (an editor preview world, a thumbnail world, a data-only world - all of which UE spins up
	 * freely and none of which should be paying for this).
	 */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Where the crop auto-bind happens and where the flush timer starts. BeginPlay rather than
	 *  Initialize because level actors (and the landscape we derive bounds from) are not reliably
	 *  present yet during world initialisation. */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Null on a dedicated server and on any non-game world, by construction. Every caller must be
	 *  null-safe - that is the contract, not an oversight. */
	static UGSBurnMaskSubsystem* Get(const UObject* WorldContextObject);

	// ------------------------------------------------------------------ public API

	/**
	 * Queue a mark on the world. THE entry point - everything that burns calls this and nothing
	 * else needs to know a render target exists.
	 *
	 * @param WorldLocation  where; Z is ignored (top-down projection).
	 * @param RadiusWorld    footprint in unreal units. A field cell passes CellSize * ~1.35, a
	 *                       prop passes its own bounds - see UGSBurnFXComponent.
	 * @param Burnt01        char DEPOSITED by this call, 0..1. NOT a target value: R accumulates
	 *                       and saturates. A caller that wants a gradual char deposits a little
	 *                       each tick (AGSFieldFireObjective::CosmeticTick deposits
	 *                       Step / CellBurnSeconds, so a full burn integrates to exactly 1);
	 *                       a caller that wants an instant black mark deposits 1 once.
	 * @param Ember01        glow now, 0..1. TRANSIENT - it decays unless re-splatted, so a source
	 *                       that wants a steady glow must keep splatting.
	 * @param Doused01       wet now, 0..1. Transient on the same terms as Ember01.
	 *
	 * Cheap and safe to call at high frequency: it appends to an array. Nothing is drawn until the
	 * flush timer fires. Silently does nothing on a dedicated server.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|BurnMask")
	void SplatBurn(const FVector& WorldLocation, float RadiusWorld, float Burnt01, float Ember01, float Doused01);

	/** Wipes the mask back to untouched. Debug and level-reset use; also called by SetBounds,
	 *  because every texel means something different once the rectangle moves. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|BurnMask")
	void ClearMask();

	/** The mask itself, created lazily on first use. Null on a dedicated server. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|BurnMask")
	UTextureRenderTarget2D* GetMaskRT();

	/**
	 * Sweep the world for crop meshes and push the mask into their materials. Runs once at world
	 * begin-play; re-callable for crop that is SPAWNED afterwards (the hamlet generator), which is
	 * what RefreshCropBindings is for.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|BurnMask")
	void RegisterCropMaterials();

	/** Drop the cached MIDs, re-sweep, and re-publish the mask parameters. Call after spawning
	 *  crop, or after SetBounds. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|BurnMask")
	void RefreshCropBindings();

	/**
	 * Override the world rectangle the mask covers. Anything may call this - a level that is not
	 * landscape-based, a test map, the hamlet generator once it knows its own footprint.
	 *
	 * Clears the mask and re-publishes to every bound material, because the existing texels
	 * describe the OLD rectangle and reinterpreting them under a new one would smear the burn
	 * across the level.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|BurnMask")
	void SetBounds(FVector2D InOrigin, FVector2D InSize);

	/** World-space min corner (XY) of the masked rectangle - the point mask UV (0,0) sits on. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|BurnMask")
	FVector2D GetOrigin() const { return MaskOrigin; }

	/** World-space extent (XY) of the masked rectangle.
	 *      UV = (AbsoluteWorldPosition.xy - GS_BurnMaskOrigin.xy) / GS_BurnMaskSize.xy */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|BurnMask")
	FVector2D GetSize() const { return MaskSize; }

	/**
	 * Splat radius in CELLS, moved here from AGSFieldFireObjective on 2026-07-31 along with the
	 * rest of the mask. It is a property of how the mask is drawn, not of any one field.
	 *
	 * RETUNED 1.35 -> 0.6 on 2026-07-31, and the old rationale is retracted rather than tweaked.
	 * It read: "a grid drawn one-cell-per-splat reads as a grid; splats WIDER than a cell dissolve
	 * the lattice; below ~1.2 the grid shows through". Every word of that was written for the
	 * PER-FIELD mask, where 128 texels covered a 13x21 m plot and one 270 uu cell was ~9 texels
	 * across - at that resolution a cell really was a visible square and really did need feathering
	 * over its neighbours to stop reading as a lattice.
	 *
	 * The world mask is not that system. 1024 texels over a ~4 km landscape is ~390 uu per texel,
	 * so a 270 uu cell is 0.69 TEXEL - smaller than the smallest thing the mask can represent.
	 * There is no lattice left to dissolve; the resolution dissolved it. What a >1 radius buys now
	 * is only bleed: at 1.35 the splat quad spans ~1.9 texels, so a burning cell chars its
	 * neighbours' texels at full strength and a doused firebreak standing beside a burning cell
	 * renders as charred as the fire did - which reads as the defenders' one verb not working.
	 *
	 * 0.6 keeps a hair of overlap for the day the resolution goes up (or a smaller world makes
	 * cells multi-texel again) while keeping the quad inside its own neighbourhood today.
	 *
	 * NOTE it feeds GetOverlapCompensation - raising this thins every deposit quadratically, on
	 * purpose. The two are one tuning knob, not two. Below the point where a quad is smaller than
	 * a texel the compensation stops falling with it, because the drawn quad stops shrinking too
	 * (FlushSplats floors it at one texel) - so at today's 1024^2 anything under ~0.72 costs
	 * nothing and buys nothing.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|BurnMask")
	float GetSplatRadiusCells() const { return SplatRadiusCells; }

	/**
	 * The factor a PER-CELL deposit must be multiplied by so that a whole cell's worth of burning
	 * lands as exactly a whole cell's worth of char on the texels underneath it (2026-07-31).
	 *
	 * @param CellSizeWorld  the caller's cell pitch in unreal units (AGSFieldFireObjective::CellSize,
	 *                       270 on the hamlet grid). Passed IN rather than stored, because a cell
	 *                       belongs to the field and the mask has no business owning one - but the
	 *                       arithmetic belongs HERE, because only the mask knows how wide it draws
	 *                       the quad and how many unreal units a texel is worth. One definition,
	 *                       one file. Pass <= 0 and the texel term is skipped (see below).
	 *
	 * THE ARITHMETIC IN FULL, because resolution, landscape size and cell size will all change and
	 * the next person should not have to re-derive this from a screenshot. Two independent things
	 * make several cells deposit into the same texel, and BOTH have to be divided out:
	 *
	 *   (1) THE QUAD IS WIDER THAN A CELL.
	 *         quad side = 2 * EffectiveSplatRadiusCells cells
	 *       so the quad covers that many cells' worth of ground along each axis, and any given
	 *       point of ground is inside the quad of that many different cells.
	 *
	 *   (2) A TEXEL IS WIDER THAN A CELL, at this resolution and this landscape.
	 *         uu per texel = MaskSize / BurnMaskResolution      (per axis; texels need not be square)
	 *       On Tutorial_Island that is ~4 km / 1024 = ~390 uu against a 270 uu cell, so a cell is
	 *       0.69 TEXEL. FlushSplats floors every splat at half a texel - it has to, or a sub-texel
	 *       cell rounds to a zero-size quad and marks nothing - so the SMALLEST quad the mask can
	 *       actually draw is one whole texel, i.e. 1.44 cells wide, however small the cell is.
	 *       Roughly two cells therefore stamp the same texel even with a point-sized splat, and
	 *       that half of the over-deposit is invisible to a purely cell-space derivation.
	 *
	 *   Putting them together, per axis:
	 *         QuadWorld      = 2 * EffectiveSplatRadiusCells * CellSizeWorld      (uu)
	 *         QuadTexels     = max(1, QuadWorld / uuPerTexel)                     (the FlushSplats floor)
	 *         FootprintCells = QuadTexels * uuPerTexel / CellSizeWorld            (cells actually hit)
	 *         Overlap        = FootprintCellsX * FootprintCellsY
	 *         Compensation   = 1 / max(1, Overlap)
	 *
	 *   Worked, at 1024^2 over ~4 km with 270 uu cells and the 0.6 radius:
	 *         QuadWorld 324 uu -> 0.83 texel -> floored to 1 texel -> 1.44 cells/axis
	 *         Overlap 2.08  ->  Compensation 0.48
	 *   At the old 1.35 radius the same arithmetic gives 2.70 cells/axis, Overlap 7.29,
	 *   Compensation 0.137 - which is the 7.3x that made R hit full char in ~3 s of a 22 s burn,
	 *   erasing the ramp entirely and pinning the ember channel to a slab.
	 *   At 2048^2 the texel term falls away (a cell is 1.4 texels, the floor stops binding) and
	 *   this collapses to the pure quad term, 1 / (2 * radius)^2 = 0.69. That is the point of
	 *   deriving it: raise the resolution and the correction corrects itself.
	 *
	 * USES THE EFFECTIVE RADIUS, not the authored one - see GetEffectiveSplatRadiusCells. With no
	 * brush the stamp is clamped to half a cell, and the compensation has to be computed against
	 * the quad that is really drawn or the two disagree and char lands ~30% low.
	 *
	 * DERIVED, NOT MEASURED: exact for a flat (hard-square) stamp, and an over-estimate of the
	 * overlap for a radial brush, where the outer cells contribute less than the geometric count
	 * suggests. That errs toward under-deposit, which is the right side to err on - char that
	 * arrives a little late still arrives, char that saturated in the first second cannot be
	 * recovered.
	 *
	 * Applies to R and to G. NOT to B: doused is a "1 while the water is on it" flag rather than
	 * an accumulator, and it is re-splatted from scratch every tick, so saturation is the wanted
	 * behaviour there.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|BurnMask")
	float GetOverlapCompensation(float CellSizeWorld) const;

	/**
	 * The splat radius the mask WILL ACTUALLY DRAW WITH, in cells - which is not always the
	 * authored SplatRadiusCells (2026-07-31).
	 *
	 * ONE VALUE, TWO CONSUMERS, and that is the whole reason it exists: FlushSplats sizes the quad
	 * from it, GetOverlapCompensation divides by it. When the SplatBrush is missing the stamp is
	 * clamped to half a cell (a flat white square has no falloff and would char the neighbours and
	 * the firebreaks at full strength), and a compensation still computed from the authored radius
	 * would then divide by an overlap that is not happening - char landing ~30% short of where the
	 * ramp says it should be. Reading both off this getter makes that class of drift impossible
	 * rather than merely unlikely.
	 *
	 * Resolved by ResolveBrush, which OnWorldBeginPlay calls once so the answer is settled before
	 * the first cell is ever lit. Falls back to the authored radius if asked before then.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|BurnMask")
	float GetEffectiveSplatRadiusCells() const;

protected:
	// ------------------------------------------------------------------ internals

	/** True only where a mask is worth having: a game/PIE world with a renderer. */
	bool IsMaskEnabled() const;

	/** Creates the render target if it does not exist and clears it to black. */
	void EnsureMask();

	/** Derives the world rectangle from the landscape on first use. See the .cpp. */
	void EnsureBounds();

	/** Starts the flush timer if it is not already running. Idempotent. */
	void EnsureFlushTimer();

	/** Draws every queued splat, plus the transient-channel decay, in ONE canvas pair. */
	void FlushSplats();

	/** Resolve-once soft brush load, including caching the answer "there is nothing there". */
	UTexture2D* ResolveBrush();

	/** Pushes mask + origin + size at one MID. Missing parameters are a safe no-op in UE. */
	void PublishParamsToMID(UMaterialInstanceDynamic* MID) const;

	/** Null the mask texture out of every cached MID and drop them. The MIDs live on components
	 *  owned by OTHER actors, which outlive this subsystem - leaving them pointing at a released
	 *  render target is a real dangling reference, not a tidy-up nicety. */
	void ReleaseCropMaterials();

	/** Filter for the auto-bind. See CropMeshNameFilters. */
	bool ShouldBindComponent(const UMeshComponent* Mesh, const AActor* Owner) const;

	// ------------------------------------------------------------------ config

	/**
	 * Mask resolution, square, RGBA8.
	 *
	 * 1024 rather than the per-field mask's 128, because this one covers the whole level instead
	 * of a 13x21 m plot. At Tutorial_Island's ~3-4 km landscape that is roughly 3 m per texel,
	 * against 270 uu (2.7 m) field cells - so a cell is about one texel. That is coarse, and it is
	 * accepted on purpose: this is a soft char mask read through the crop material's own noise,
	 * not a decal sheet. THE TRADEOFF TO REVISIT IF EDGES READ MUSHY is exactly this number;
	 * 2048 quadruples the memory (4MB -> 16MB) for half the texel size, and is the first thing to
	 * try before anyone proposes going back to per-field targets.
	 *
	 * RGBA8, not float: every channel is a 0..1 mask and 8 bits is more precision than a charring
	 * blend can show. 1024^2 RGBA8 is 4MB, once, for the entire level.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "GoblinSiege|BurnMask")
	int32 BurnMaskResolution = 1024;

	/** Flush cadence. Every splat queued inside one interval is drawn in ONE canvas pair - see
	 *  FlushSplats for why that is the whole performance story of this system. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "GoblinSiege|BurnMask", meta = (ClampMin = "0.02"))
	float MaskRedrawInterval = 0.15f;

	/** See GetSplatRadiusCells (and GetOverlapCompensation, which is derived from it). */
	UPROPERTY(EditDefaultsOnly, Config, Category = "GoblinSiege|BurnMask", meta = (ClampMin = "0.5", ClampMax = "4.0"))
	float SplatRadiusCells = 0.6f;

	/**
	 * Soft radial brush stamped per splat. Wants a greyscale radial gradient, white centre falling
	 * to black at the edge - the splats are drawn ADDITIVELY, so it is the RGB falloff and not the
	 * alpha that does the feathering.
	 *
	 * Soft and null-safe. With nothing set, K2_DrawTexture with a null texture resolves to
	 * GWhiteTexture in the engine's canvas path (verified against engine source), so the mask
	 * still renders - as hard squares rather than soft blobs. "The art isn't authored yet"
	 * therefore degrades to BLOCKY, never to invisible and never to a load failure. Warned once.
	 *
	 * AND THE SQUARE IS SHRUNK TO FIT (2026-07-31). GWhiteTexture is flat (1,1,1,1) with no
	 * falloff whatsoever, so an unbrushed splat is not "a soft blob with hard edges" - it is a
	 * uniform slab at full strength right out to SplatRadiusCells. Every word of the radius tuning
	 * above assumes a falloff, and with no brush there is none, so a burning cell would paint its
	 * neighbours as black as itself and a doused firebreak would render fully charred. FlushSplats
	 * therefore clamps the effective radius to half a cell while the brush is missing: a hard
	 * square that stays inside its own cell is an honest degradation, a hard square that spills
	 * over the firebreak is a wrong picture. Restore a brush and the full radius returns.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "GoblinSiege|BurnMask")
	TSoftObjectPtr<UTexture2D> SplatBrush;

	/** Margin added around the landscape bounds, so crop or props sitting just off the terrain
	 *  edge still land inside the mask instead of clamping to its border. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "GoblinSiege|BurnMask")
	float BoundsMarginWorld = 5000.f;

	/** Fallback square edge length used when the level has no landscape at all - a blockout, a
	 *  test map, a fully modular interior. Centred on the world origin. Logged once when used, so
	 *  "my burn marks are in the wrong place" has an explanation in the log rather than needing a
	 *  debugger. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "GoblinSiege|BurnMask", meta = (ClampMin = "1000.0"))
	float FallbackWorldSize = 400000.f;

	/**
	 * Per-flush multiplier applied to the TRANSIENT channels (G, B). R is multiplied by exactly
	 * 1.0 and is therefore bit-identical across the pass - that is what keeps the monotonic
	 * guarantee true by construction. See FlushSplats.
	 *
	 * 0.55 at a 0.15 s flush is a ~0.35 s half-life: a cell that stops burning fades its ember out
	 * over about a second, and a source re-splatting every 0.2 s holds a steady glow. Push it
	 * toward 1 and ember smears behind the front; push it to 0 and anything whose re-splat cadence
	 * is slower than the flush cadence strobes.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "GoblinSiege|BurnMask", meta = (ClampMin = "0.0", ClampMax = "0.99"))
	float TransientChannelDecay = 0.55f;

	/**
	 * Safety valve: flush immediately rather than let the queue grow without bound. A field that
	 * re-splats ~40 cells per cosmetic tick across three fields is ~120 entries per 0.15 s, so
	 * this is nowhere near it in normal play - it exists for a debug command that razes everything
	 * at once.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "GoblinSiege|BurnMask", meta = (ClampMin = "16"))
	int32 MaxQueuedSplats = 4096;

	/**
	 * Mesh-name substrings that mark a mesh as crop, matched case-insensitively against the static
	 * mesh's name. Defaults to {"Wheat"}, which catches SM_VillageWheat_01 / _02 / _Grass.
	 *
	 * WITHOUT A FILTER THIS WOULD MID EVERY MESH IN A 9,000-ACTOR LEVEL. That is the entire reason
	 * this property exists: the old per-field bind was bounded by the field's grid box, and a
	 * world-space sweep has no such bound. Empty means "bind everything", which is legal, honest
	 * and expensive - use it only on a small test map.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "GoblinSiege|BurnMask")
	TArray<FName> CropMeshNameFilters;

	/**
	 * Mesh-name substrings that are NEVER bound, even when CropMeshNameFilters matches them.
	 * Checked before the include list, so the exclusion always wins.
	 *
	 * Defaults to {"Grass"}: the include filter is the single substring "Wheat", and the field's
	 * ground cover is named SM_VillageWheat_Grass, so without this the grass chars whenever the
	 * crop does - and that mesh is painted well beyond the fields, so the burn read as spreading
	 * across ground that was never alight. The crop itself (SM_VillageWheat_01 / _02) is unaffected.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "GoblinSiege|BurnMask")
	TArray<FName> CropMeshNameExclusions;

	/** Escape hatch: an actor carrying this tag has ALL its mesh components bound regardless of
	 *  mesh name. For crop the art side named off-pattern, and for ground meshes that want scorch
	 *  but are not crop at all. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "GoblinSiege|BurnMask")
	FName CropActorTag = FName("GS_BurnMaskReceiver");

	// ------------------------------------------------------------------ state

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> MaskRT;

	/** MIDs created by the auto-bind, kept alive here. Without a hard reference the GC takes them
	 *  the moment the component's material array is the only thing pointing at them, and a
	 *  collected MID silently reverts the mesh to its base material mid-raid. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> CropMaterials;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ResolvedSplatBrush;

	TArray<FGSBurnSplat> PendingSplats;

	FVector2D MaskOrigin = FVector2D::ZeroVector;
	FVector2D MaskSize = FVector2D::ZeroVector;

	bool bBoundsResolved = false;

	/** One-shot warn latches, matching AGSFieldFireObjective's bSmokeSystemResolveFailed pattern:
	 *  a missing soft asset says so ONCE, and the failed resolve is never retried. LoadSynchronous
	 *  does not cache a failure, so retrying it is a full failed package lookup every flush. */
	bool bSplatBrushResolveFailed = false;
	bool bWarnedMissingSplatBrush = false;
	bool bWarnedNoLandscape = false;

	/** Backing store for GetEffectiveSplatRadiusCells, written by ResolveBrush. 0 means "the brush
	 *  question has not been asked yet", which is distinct from any legal radius and is why the
	 *  getter can tell "not resolved" from "resolved to the degraded 0.5". */
	float EffectiveSplatRadiusCells = 0.f;

	/**
	 * Flushes still owed to the transient channels after the last G/B splat, so a fire that stops
	 * dead still fades its ember out instead of freezing it on the mask forever. When this hits 0
	 * and the queue is empty the timer stops entirely - an idle level does no canvas work at all.
	 *
	 * DERIVED from TransientChannelDecay, never hard-coded - see GetTransientSettleFlushCount.
	 */
	int32 TransientSettleFlushes = 0;

	/**
	 * How many flushes it takes for TransientChannelDecay to drive an 8-bit channel from 255 to 0,
	 * i.e. how long TransientSettleFlushes must run for after the last G/B splat.
	 *
	 * DERIVED 2026-07-31, replacing a hard-coded 16. Sixteen is the right answer for the DEFAULT
	 * decay of 0.55 and for no other value, and the decay is Config with ClampMax 0.99 - so a
	 * project that slows the ember fade to 0.9 in DefaultGame.ini got 0.9^16 = 0.185, meaning the
	 * timer stopped with an ember still glowing at 47/255 and FROZE IT THERE for the rest of the
	 * raid. That is precisely the failure the settle counter exists to prevent, reintroduced by
	 * the counter itself.
	 */
	int32 GetTransientSettleFlushCount() const;

	FTimerHandle FlushTimerHandle;
};
