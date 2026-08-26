#include "World/GSCorruptionSubsystem.h"

#include "World/GSCorruptionDirector.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogGSCorruption);

// ---- feel numbers, still being found -------------------------------------------------------
// These are cvars rather than data-asset rows on purpose: they are the two numbers that decide
// whether the world "lurches" or "creeps", and they want to be draggable during a playtest without
// a rebuild or an asset edit. They graduate onto DA_Corruption_Default in stage 4, once the feel is
// settled. Same reasoning, and the same wording, as the block tuning in GSDamageExecCalculation.cpp.

static float GSCorruptionRiseRate = 0.06f;
static FAutoConsoleVariableRef CVarGSCorruptionRiseRate(
	TEXT("GS.Corruption.RiseRate"),
	GSCorruptionRiseRate,
	TEXT("Corruption units per second while climbing. Constant rate, so a big step reads as a lurch ")
	TEXT("and a small one as a creep. Graduates to DA_Corruption_Default in stage 4."),
	ECVF_Cheat);

static float GSCorruptionFallRate = 0.02f;
static FAutoConsoleVariableRef CVarGSCorruptionFallRate(
	TEXT("GS.Corruption.FallRate"),
	GSCorruptionFallRate,
	TEXT("Corruption units per second while falling. Deliberately slower than the rise - the land ")
	TEXT("gives ground grudgingly. Only reachable via the console override; the ratchet (ruling 41) ")
	TEXT("stops the drivers ever pulling downward."),
	ECVF_Cheat);

static float GSCorruptionRatchetFraction = 1.0f;
static FAutoConsoleVariableRef CVarGSCorruptionRatchet(
	TEXT("GS.Corruption.RatchetFraction"),
	GSCorruptionRatchetFraction,
	TEXT("Fraction of the high-water mark that floors the target. 1.0 = fully monotonic (ruling 41, ")
	TEXT("the shipping value). Lower it only to explore recession during a playtest."),
	ECVF_Cheat);

namespace
{
	const TCHAR* StageNames[] = { TEXT("Quiet"), TEXT("Scarred"), TEXT("Burning"), TEXT("Mordor") };
}

bool UGSCorruptionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UGSCorruptionSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (TickIntervalSeconds <= 0.f)
	{
		UE_LOG(LogGSCorruption, Warning,
			TEXT("Corruption tick DISABLED (TickIntervalSeconds = %.3f). The scalar will never move ")
			TEXT("and the world will stay exactly as the level author left it."), TickIntervalSeconds);
		return;
	}

	EnsureDirector();

	InWorld.GetTimerManager().SetTimer(
		CorruptionTickTimer, this, &UGSCorruptionSubsystem::TickCorruption,
		TickIntervalSeconds, /*bLoop*/ true);

	UE_LOG(LogGSCorruption, Log,
		TEXT("Corruption online at %.0f Hz. No drivers are wired in stage 1 - use GS.Corruption.Set."),
		1.f / TickIntervalSeconds);
}

void UGSCorruptionSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CorruptionTickTimer);
	}

	// The director owns the only state that outlives us badly - it edits actors that belong to the
	// level. Hand it back to its captured baselines so a PIE stop does not leave the editor world
	// sitting at whatever corruption the session ended on.
	if (AGSCorruptionDirector* D = Director.Get())
	{
		D->RestoreBaselines();
	}

	Super::Deinitialize();
}

UGSCorruptionSubsystem* UGSCorruptionSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	return World ? World->GetSubsystem<UGSCorruptionSubsystem>() : nullptr;
}

void UGSCorruptionSubsystem::EnsureDirector()
{
	UWorld* World = GetWorld();
	if (!World || Director.IsValid())
	{
		return;
	}

	// Find one a level artist placed, first.
	for (TActorIterator<AGSCorruptionDirector> It(World); It; ++It)
	{
		Director = *It;
		UE_LOG(LogGSCorruption, Log, TEXT("Using placed director '%s'."), *It->GetName());
		return;
	}

	// None placed - spawn one. This is the whole reason the feature works on a map nobody wired by
	// hand. The horde's arrival markers are on record as "the single most likely reason a correctly
	// built horn appears to do nothing"; a placement requirement here would fail the same way, on
	// L_CombatArena and every PCG hamlet.
	UClass* DirectorClass = AGSCorruptionDirector::StaticClass();
	if (CorruptionDirectorClassPath.IsValid())
	{
		if (UClass* Loaded = CorruptionDirectorClassPath.TryLoadClass<AGSCorruptionDirector>())
		{
			DirectorClass = Loaded;
		}
		else
		{
			UE_LOG(LogGSCorruption, Warning,
				TEXT("CorruptionDirectorClassPath '%s' did not load. Falling back to the C++ class - ")
				TEXT("any per-map look overrides on that Blueprint are NOT in effect."),
				*CorruptionDirectorClassPath.ToString());
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	Director = World->SpawnActor<AGSCorruptionDirector>(DirectorClass, FTransform::Identity, Params);

	if (!Director.IsValid())
	{
		UE_LOG(LogGSCorruption, Error,
			TEXT("Failed to spawn a corruption director. The scalar will still move and NOTHING ")
			TEXT("will appear on screen."));
	}
}

AGSCorruptionDirector* UGSCorruptionSubsystem::GetDirector() const
{
	return Director.Get();
}

void UGSCorruptionSubsystem::RefreshOutputs()
{
	EnsureDirector();
	if (AGSCorruptionDirector* D = Director.Get())
	{
		D->RefreshDiscovery();
		D->ApplyCorruption(GetCorruption01());
	}
}

float UGSCorruptionSubsystem::ComputeTarget01() const
{
	// Stage 1 has no drivers. Every term arrives in stage 2 and 3; until then the target is whatever
	// the console last asked for. Returning the override here (rather than short-circuiting the
	// follower) is what makes GS.Corruption.Set animate instead of snap.
	return bOverridden ? ForcedOverride01 : CorruptionTarget01;
}

void UGSCorruptionSubsystem::TickCorruption()
{
	const float Dt = TickIntervalSeconds;

	float Target = ComputeTarget01();

	// The ratchet (ruling 41). Applied to the TARGET, not the follower, so the follower still eases
	// rather than being clamped mid-interpolation.
	HighWaterMark01 = FMath::Max(HighWaterMark01, Target);
	if (!bOverridden)
	{
		Target = FMath::Max(Target, HighWaterMark01 * GSCorruptionRatchetFraction);
	}
	Target = FMath::Clamp(Target, 0.f, 1.f);
	CorruptionTarget01 = Target;

	const float Rate = (Target > Corruption01) ? GSCorruptionRiseRate : GSCorruptionFallRate;
	Corruption01 = FMath::FInterpConstantTo(Corruption01, Target, Dt, Rate);

	const float Display = FMath::Clamp(Corruption01, 0.f, 1.f);

	if (AGSCorruptionDirector* D = Director.Get())
	{
		D->ApplyCorruption(Display);
	}

	if (!FMath::IsNearlyEqual(Display, LastBroadcast01, KINDA_SMALL_NUMBER))
	{
		LastBroadcast01 = Display;
		OnCorruptionChanged.Broadcast(Display);
	}

	const int32 NewStage = BandFor(Display);
	if (NewStage != CurrentStage)
	{
		const int32 OldStage = CurrentStage;
		CurrentStage = NewStage;
		UE_LOG(LogGSCorruption, Log, TEXT("Stage %d (%s) -> %d (%s) at %.2f."),
			OldStage, StageNames[OldStage], NewStage, StageNames[NewStage], Display);
		OnCorruptionStageChanged.Broadcast(NewStage, OldStage);
	}
}

int32 UGSCorruptionSubsystem::BandFor(float InCorruption01)
{
	if (InCorruption01 >= 0.75f) { return 3; }
	if (InCorruption01 >= 0.50f) { return 2; }
	if (InCorruption01 >= 0.25f) { return 1; }
	return 0;
}

float UGSCorruptionSubsystem::GetCorruption01() const
{
	return FMath::Clamp(Corruption01, 0.f, 1.f);
}

float UGSCorruptionSubsystem::GetCorruptionTarget01() const
{
	return CorruptionTarget01;
}

int32 UGSCorruptionSubsystem::GetCorruptionStage() const
{
	return CurrentStage;
}

FString UGSCorruptionSubsystem::GetStageName() const
{
	return StageNames[FMath::Clamp(CurrentStage, 0, 3)];
}

bool UGSCorruptionSubsystem::IsOverridden() const
{
	return bOverridden;
}

void UGSCorruptionSubsystem::SetCorruptionOverride(float Value01)
{
	ForcedOverride01 = FMath::Clamp(Value01, 0.f, 1.f);
	bOverridden = true;
}

void UGSCorruptionSubsystem::ReleaseCorruptionOverride()
{
	bOverridden = false;
}

void UGSCorruptionSubsystem::StepCorruptionStage()
{
	const float Next = FMath::Clamp(FMath::FloorToFloat(GetCorruption01() * 4.f + 1.f) * 0.25f, 0.f, 1.f);
	SetCorruptionOverride(Next);
}

FString UGSCorruptionSubsystem::DescribeState() const
{
	FString Out;

	Out += FString::Printf(
		TEXT("[GS.Corruption] display %.2f  target %.2f  high-water %.2f  stage %d (%s)  (OVERRIDDEN: %s)\n"),
		GetCorruption01(), CorruptionTarget01, HighWaterMark01,
		CurrentStage, *GetStageName(), bOverridden ? TEXT("yes") : TEXT("no"));

	// Stage 1 has no driver terms. Saying so explicitly is the point: a term reading 0.00 and a term
	// that does not exist yet look identical on screen, and confusing them is how a wired-but-broken
	// driver survives a playtest.
	Out += TEXT("[GS.Corruption]   drivers: NONE WIRED YET (stage 1 of 6) - console only\n");

	Out += TEXT("[GS.Corruption] OUTPUTS AS THE ENGINE HAS THEM:\n");
	if (const AGSCorruptionDirector* D = Director.Get())
	{
		Out += D->DescribeOutputs();
	}
	else
	{
		Out += TEXT("[GS.Corruption]   director            NONE - nothing is being driven\n");
	}

	return Out;
}
