#include "World/GSCorruptionSubsystem.h"

#include "World/GSCorruptionDirector.h"

#include "Combat/GSGameplayTags.h"
#include "Characters/GSEnemyCharacter.h"
#include "Core/GSGameMode.h"
#include "Core/GSGameState.h"
#include "Destruction/GSBurnObjectiveBase.h"
#include "Horde/GSHordeSubsystem.h"
#include "Raid/GSRaidDirector.h"

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

static float GSCorruptionCivilianWeight = 2.5f;
static FAutoConsoleVariableRef CVarGSCorruptionCivilianWeight(
	TEXT("GS.Corruption.CivilianWeight"),
	GSCorruptionCivilianWeight,
	TEXT("What one civilian death is worth against one armed defender's, for the kill term. ")
	TEXT("Ruling 62 decided civilians count MORE; the multiplier itself is a feel number and was ")
	TEXT("explicitly left out of the ruling, so it lives here until it settles and then graduates ")
	TEXT("onto DA_Corruption_Default in stage 4."),
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

	// Complain, never renormalise. A silent correction would hide the typo that caused it, and the
	// symptom of a bad sum is "the world never finishes turning", which nobody attributes to an ini.
	const float WeightSum = WeightObjectives + WeightKills + WeightStructures + WeightClock + WeightHorde;
	if (!FMath::IsNearlyEqual(WeightSum, 1.f, 0.001f))
	{
		UE_LOG(LogGSCorruption, Warning,
			TEXT("Corruption term weights sum to %.3f, not 1.0. Max reachable corruption is %.2f, so ")
			TEXT("the world will NEVER finish turning. Check ")
			TEXT("[/Script/GoblinSiege.GSCorruptionSubsystem] in DefaultGame.ini."),
			WeightSum, WeightSum);
	}

	// The kill bus (#325). GetAuthGameMode returns null on a client by construction - AGSGameMode
	// does not exist there - which is exactly why the kill driver is server-only with no explicit
	// authority check. If this ever needs one, something else has gone wrong first.
	if (AGSGameMode* GM = InWorld.GetAuthGameMode<AGSGameMode>())
	{
		GM->OnCharacterKilled.AddDynamic(this, &UGSCorruptionSubsystem::HandleCharacterKilled);
	}
	else
	{
		UE_LOG(LogGSCorruption, Warning,
			TEXT("No AGSGameMode - the KILL term will stay at 0.00 for this whole session. Expected ")
			TEXT("on a client; on a server it means the game mode is not AGSGameMode."));
	}

	EnsureDirector();

	InWorld.GetTimerManager().SetTimer(
		CorruptionTickTimer, this, &UGSCorruptionSubsystem::TickCorruption,
		TickIntervalSeconds, /*bLoop*/ true);

	UE_LOG(LogGSCorruption, Log,
		TEXT("Corruption online at %.0f Hz. All five drivers wired: objectives, kills, structures, ")
		TEXT("clock, horde presence. A civilian death is worth %.2f of a soldier's (ruling 62)."),
		1.f / TickIntervalSeconds, GSCorruptionCivilianWeight);
}

void UGSCorruptionSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CorruptionTickTimer);
	}

	// Unbind explicitly. Deinitialize being a clean unbind point is one of the reasons this lives on
	// a subsystem rather than the GameState (see the header), so it would be careless not to use it.
	if (UWorld* World = GetWorld())
	{
		if (AGSGameMode* GM = World->GetAuthGameMode<AGSGameMode>())
		{
			GM->OnCharacterKilled.RemoveDynamic(this, &UGSCorruptionSubsystem::HandleCharacterKilled);
		}
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
	// Force the objectives term too, not just actor discovery. Refresh exists to be typed after a
	// stream-in or a PCG spawn, and waiting out the 2 Hz cadence to see whether it worked would
	// make the command look like it had done nothing.
	ObjectiveRecomputeAccumulator = UE_BIG_NUMBER;

	EnsureDirector();
	if (AGSCorruptionDirector* D = Director.Get())
	{
		D->RefreshDiscovery();
		D->ApplyCorruption(GetCorruption01());
	}
}

float UGSCorruptionSubsystem::SoftKnee(float Count, float Knee)
{
	// N / (N + Knee). Saturating rather than linear so the term cannot be farmed to 1.0: at the
	// default knee of 20, twenty props is 0.50, fifty is 0.71, and two hundred is still only 0.91.
	const float N = FMath::Max(0.f, Count);
	return (Knee > KINDA_SMALL_NUMBER) ? (N / (N + Knee)) : 0.f;
}

float UGSCorruptionSubsystem::ObjectiveTypeWeight(const FGameplayTag& TypeTag)
{
	// THE FIX FOR RULING 42's SECOND HALF. UGSScoreSubsystem::GetDeeds() cannot drive this because
	// any burn objective pays deeds and a hamlet has ~30 houses (GDD 12.1 row 12) - burning houses
	// would darken the sky faster than detonating the mill. Weighting by TYPE is what stops that.
	//
	// Hardcoded here, deliberately, for stage 2 only: a TMap<FGameplayTag,float> in an ini is fiddly
	// to author and easy to get silently wrong. These move onto DA_Corruption_Default in stage 4,
	// where a designer owns them.
	if (TypeTag == GSTags::Objective_Burn_Market) { return 1.00f; }
	if (TypeTag == GSTags::Objective_Burn_Mill)   { return 1.00f; }
	if (TypeTag == GSTags::Objective_Burn_Field)  { return 0.35f; }
	if (TypeTag == GSTags::Objective_Burn_House)  { return 0.15f; }

	// An untagged or unknown carrier still counts, at the house rate. Returning 0 would make a
	// mis-tagged objective invisible rather than merely cheap, which is the harder bug to find.
	return 0.15f;
}

void UGSCorruptionSubsystem::RecomputeObjectiveTerm()
{
	ObjectiveBreakdown.Reset();
	CachedObjectives01 = 0.f;
	CachedObjectiveCount = 0;

	// Polled from the director rather than cached-and-invalidated on OnObjectiveRosterChanged. The
	// plan called for the binding; polling cannot go stale, so a PCG-spawned or streamed-in
	// objective contributes on the next recompute with nothing to remember to bind.
	UGSRaidDirector* Raid = UGSRaidDirector::Get(this);
	if (!Raid)
	{
		return;
	}

	TArray<AGSBurnObjectiveBase*> Carriers;
	Raid->GetTrackedCarriers(Carriers);

	// Group by TYPE first. This is the whole fix: weighting per instance let 67 houses take 75% of
	// the term on L_Tutorial_Island purely by outnumbering everything else. Averaging within a type
	// and weighting the type makes one house worth a 67th of "the houses", and "the houses" worth
	// 0.15 against the mill's 1.00 - which is what ruling 42 actually asked for.
	for (const AGSBurnObjectiveBase* Carrier : Carriers)
	{
		if (!Carrier)
		{
			continue;
		}

		const FGameplayTag TypeTag = Carrier->GetObjectiveTypeTag();
		const float C = FMath::Clamp(Carrier->GetCompletion01(), 0.f, 1.f);
		++CachedObjectiveCount;

		FGSObjectiveTypeRow* Row = ObjectiveBreakdown.FindByPredicate(
			[&TypeTag](const FGSObjectiveTypeRow& R) { return R.TypeTag == TypeTag; });

		if (!Row)
		{
			// Linear scan rather than a TMap: there are four objective types in the game and this
			// runs at 2 Hz. A map would allocate for no measurable gain.
			Row = &ObjectiveBreakdown.AddDefaulted_GetRef();
			Row->TypeTag = TypeTag;
			Row->Weight = ObjectiveTypeWeight(TypeTag);
		}

		// AverageCompletion accumulates a SUM here and is divided below - keeping a running mean
		// per row would need the count anyway and rounds more.
		Row->AverageCompletion += C;
		++Row->Count;
	}

	float WeightedSum = 0.f;
	float WeightTotal = 0.f;
	for (FGSObjectiveTypeRow& Row : ObjectiveBreakdown)
	{
		Row.AverageCompletion = (Row.Count > 0) ? (Row.AverageCompletion / Row.Count) : 0.f;
		WeightedSum += Row.Weight * Row.AverageCompletion;
		WeightTotal += Row.Weight;
	}

	CachedObjectives01 = (WeightTotal > KINDA_SMALL_NUMBER) ? (WeightedSum / WeightTotal) : 0.f;
}

void UGSCorruptionSubsystem::RecomputeTarget()
{
	FGSCorruptionTerms T;

	const UWorld* World = GetWorld();

	// ---- A: objectives (weight 0.45) ---------------------------------------------------------
	// On its own slower cadence - see RecomputeObjectiveTerm. Between recomputes the cached value
	// stands, which is harmless: the follower is easing toward it far more slowly than 2 Hz anyway.
	ObjectiveRecomputeAccumulator += TickIntervalSeconds;
	if (ObjectiveRecomputeAccumulator >= ObjectiveRecomputeIntervalSeconds)
	{
		ObjectiveRecomputeAccumulator = 0.f;
		RecomputeObjectiveTerm();
	}
	T.Objectives = CachedObjectives01;
	T.ObjectiveCount = CachedObjectiveCount;

	// ---- B: humans killed (weight 0.20) ------------------------------------------------------
	// Weighted, not counted: ruling 62 makes a civilian worth more than a soldier, so this is a
	// weighted body count fed through the same saturating knee as the structures term. Farming
	// cannot max it - the knee flattens long before the garrison runs out.
	const float Soldiers = static_cast<float>(FMath::Max(0, HumansKilled - CiviliansKilled));
	const float WeightedKills = Soldiers + static_cast<float>(CiviliansKilled) * GSCorruptionCivilianWeight;
	T.Kills = SoftKnee(WeightedKills, KillSoftKnee);

	// ---- C: structures smashed, toppled, burned down (weight 0.15) ----------------------------
	T.Structures = SoftKnee(StructuresDestroyed, StructureSoftKnee);

	// ---- D and F: the clock, and the razed floor ----------------------------------------------
	const AGSGameState* GS = World ? World->GetGameState<AGSGameState>() : nullptr;
	if (GS)
	{
		const float Remaining = GS->GetRaidSecondsRemaining();
		RaidDurationObserved = FMath::Max(RaidDurationObserved, Remaining);

		if (RaidDurationObserved > KINDA_SMALL_NUMBER)
		{
			T.Clock = FMath::Clamp(1.f - (Remaining / RaidDurationObserved), 0.f, 1.f);
		}

		// Collapsing or expired is the end of the raid whatever the clock arithmetic says.
		const EGSRaidClockPhase Phase = GS->GetRaidClockPhase();
		if (Phase == EGSRaidClockPhase::Collapsing || Phase == EGSRaidClockPhase::Expired)
		{
			T.Clock = 1.f;
		}
	}

	// ---- E: horde presence (weight 0.10) ------------------------------------------------------
	if (const UGSHordeSubsystem* Horde = UGSHordeSubsystem::Get(this))
	{
		const int32 Cap = Horde->GetActiveCap();
		if (Cap > 0)
		{
			T.Horde = FMath::Clamp(static_cast<float>(Horde->GetActiveCount()) / static_cast<float>(Cap), 0.f, 1.f);
		}
	}

	float Sum =
		  WeightObjectives * T.Objectives
		+ WeightKills      * T.Kills
		+ WeightStructures * T.Structures
		+ WeightClock      * T.Clock
		+ WeightHorde      * T.Horde;

	// ---- F: the razed floor -------------------------------------------------------------------
	// Reuses existing canon rather than re-deriving "the raid is over" from the terms.
	if (GS && (GS->IsDistrictRazed() || GS->GetAlarmPhase() == EGSAlarmPhase::Razed))
	{
		if (Sum < RazedFloor01)
		{
			Sum = RazedFloor01;
			T.bRazedFloorApplied = true;
		}
	}

	T.bDirectorSeen = Director.IsValid();
	Terms = T;

	CorruptionTarget01 = FMath::Clamp(Sum, 0.f, 1.f);
}

void UGSCorruptionSubsystem::TickCorruption()
{
	const float Dt = TickIntervalSeconds;

	RecomputeTarget();
	float Target = bOverridden ? ForcedOverride01 : CorruptionTarget01;

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

void UGSCorruptionSubsystem::HandleCharacterKilled(AGSCharacterBase* Victim, FGameplayTag VictimRaceTag, FVector Location)
{
	// Goblins are ours. An invalid race tag counts as human, matching the rule already in the tree
	// at GSCharacterBase.cpp:258 ("anything that is not a goblin is a human here") - one rule, not
	// two, because two rules that mean the same thing drift apart.
	if (VictimRaceTag == GSTags::Race_Goblin)
	{
		return;
	}

	++HumansKilled;

	// Ruling 62: a civilian death corrupts the land MORE than an armed defender's. The discriminator
	// is the archetype row key, which is how this project identifies a defender - one pawn class,
	// many data rows. DA_Race_Human carries Militia / Archer / Knight / Civilian.
	// CiviliansKilled is a SUBSET of HumansKilled, never a parallel total - HumansKilled was already
	// incremented above and must not be incremented again here. Stated because the two-counter shape
	// invites exactly that mistake, and the weighting below reads (Humans - Civilians) as "soldiers".
	bool bCivilian = false;
	if (const AGSEnemyCharacter* Enemy = Cast<AGSEnemyCharacter>(Victim))
	{
		bCivilian = (Enemy->GetArchetypeRowName() == CivilianArchetypeRowName);
		if (bCivilian)
		{
			++CiviliansKilled;
		}
	}

	UE_LOG(LogGSCorruption, Verbose,
		TEXT("Killed '%s' (%s%s) at %s -> %d human, %d civilian."),
		Victim ? *Victim->GetName() : TEXT("unnamed"),
		VictimRaceTag.IsValid() ? *VictimRaceTag.ToString() : TEXT("no race tag"),
		bCivilian ? TEXT(", CIVILIAN") : TEXT(""),
		*Location.ToCompactString(), HumansKilled, CiviliansKilled);

	// Location is unused by the GLOBAL model. Carried and logged anyway: it is free at the
	// broadcast, it is the one fact that cannot be recovered afterwards, and a per-corpse ash decal
	// or any future zoned model would otherwise need a new delegate to get it.
}

void UGSCorruptionSubsystem::ReportStructureDestroyed(const AActor* Structure)
{
	++StructuresDestroyed;

	UE_LOG(LogGSCorruption, Verbose, TEXT("Structure destroyed ('%s') -> %d total, term %.2f."),
		Structure ? *Structure->GetName() : TEXT("unnamed"),
		StructuresDestroyed, SoftKnee(StructuresDestroyed, StructureSoftKnee));
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

	// Every term shows weight x value = contribution, plus the raw count behind it. A term reading
	// 0.00 because nothing has happened and a term reading 0.00 because it is not wired look
	// identical otherwise, and that confusion is how a broken driver survives a playtest.
	Out += FString::Printf(TEXT("[GS.Corruption]   objectives %.2f x %.2f = %.2f   (%d carriers, %d types)\n"),
		WeightObjectives, Terms.Objectives, WeightObjectives * Terms.Objectives,
		Terms.ObjectiveCount, ObjectiveBreakdown.Num());

	// One line PER TYPE, not per carrier. The per-carrier version printed 71 entries on a hamlet,
	// which is not a readout anybody can read. Each row shows the type's SHARE of the term, which
	// is the number that actually matters and the one that was wrong: houses used to be 75% of it.
	float WeightTotal = 0.f;
	for (const FGSObjectiveTypeRow& Row : ObjectiveBreakdown)
	{
		WeightTotal += Row.Weight;
	}

	for (const FGSObjectiveTypeRow& Row : ObjectiveBreakdown)
	{
		FString TypeName = TEXT("<none>");
		if (Row.TypeTag.IsValid())
		{
			const FString Full = Row.TypeTag.ToString();
			if (!Full.Split(TEXT("."), nullptr, &TypeName, ESearchCase::CaseSensitive, ESearchDir::FromEnd)
				|| TypeName.IsEmpty())
			{
				TypeName = Full;
			}
		}

		const float Share = (WeightTotal > KINDA_SMALL_NUMBER) ? (Row.Weight / WeightTotal) : 0.f;
		Out += FString::Printf(
			TEXT("[GS.Corruption]     %-8s x%-3d  w%.2f  avg %.2f  -> %.0f%% of the term\n"),
			*TypeName, Row.Count, Row.Weight, Row.AverageCompletion, Share * 100.f);
	}

	const int32 SoldiersKilled = FMath::Max(0, HumansKilled - CiviliansKilled);
	Out += FString::Printf(
		TEXT("[GS.Corruption]   kills      %.2f x %.2f = %.2f   (%d soldiers + %d civilians x%.2f = %.1f weighted, knee %.0f)\n"),
		WeightKills, Terms.Kills, WeightKills * Terms.Kills,
		SoldiersKilled, CiviliansKilled, GSCorruptionCivilianWeight,
		static_cast<float>(SoldiersKilled) + static_cast<float>(CiviliansKilled) * GSCorruptionCivilianWeight,
		KillSoftKnee);

	Out += FString::Printf(TEXT("[GS.Corruption]   structures %.2f x %.2f = %.2f   (%d destroyed, knee %.0f)\n"),
		WeightStructures, Terms.Structures, WeightStructures * Terms.Structures,
		StructuresDestroyed, StructureSoftKnee);

	Out += FString::Printf(TEXT("[GS.Corruption]   clock      %.2f x %.2f = %.2f   (duration observed %.0fs)\n"),
		WeightClock, Terms.Clock, WeightClock * Terms.Clock, RaidDurationObserved);

	Out += FString::Printf(TEXT("[GS.Corruption]   horde      %.2f x %.2f = %.2f\n"),
		WeightHorde, Terms.Horde, WeightHorde * Terms.Horde);

	if (Terms.bRazedFloorApplied)
	{
		Out += FString::Printf(TEXT("[GS.Corruption]   RAZED FLOOR applied - target forced up to %.2f\n"), RazedFloor01);
	}

	const float WeightSum = WeightObjectives + WeightKills + WeightStructures + WeightClock + WeightHorde;
	if (!FMath::IsNearlyEqual(WeightSum, 1.f, 0.001f))
	{
		Out += FString::Printf(TEXT("[GS.Corruption]   ** WEIGHTS SUM TO %.3f, NOT 1.0 - max corruption is capped **\n"), WeightSum);
	}

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
