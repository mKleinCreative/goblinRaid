#include "Raid/GSScoreSubsystem.h"
#include "Raid/GSRaidDirector.h"
#include "Destruction/GSBurnObjectiveBase.h"
#include "EngineUtils.h"

namespace
{
	/**
	 * Balance lives here for now, deliberately as named constants rather than a data asset.
	 *
	 * A DataAsset or DeveloperSettings would be the tunable answer and is premature: nothing has
	 * played a scored raid yet, so there is no evidence about what these should be, and the cost of
	 * moving them later is one find-and-replace. What would NOT be cheap to undo is scattering
	 * literals through the scoring functions, which is why they are gathered here.
	 *
	 * Duplicates score less, not zero: the Q-37 demotion pass marks siblings Optional once one of a
	 * type completes (AGSRaidDirector), so burning a second field is real work that simply does not
	 * advance the win condition. Zero would tell the player it was pointless.
	 */
	constexpr int32 DeedsPerRequiredObjective = 100;
	constexpr int32 DeedsPerOptionalObjective = 40;

	/** Paid once, on Extracted only. Getting out is the verb the whole raid is built around, and a
	 *  raid you won should never total less than the same objectives burned and then lost. */
	constexpr int32 DeedsForExtraction = 150;
}

void UGSScoreSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// World sweep, same shape as UGSRaidDirector's carrier roster. No self-announce counterpart here:
	// unlike the director, a late-spawning objective that never gets scored costs points, not a
	// broken win condition - and there is no runtime objective spawning today. Worth revisiting when
	// the settlement generator lands.
	for (TActorIterator<AGSBurnObjectiveBase> It(&InWorld); It; ++It)
	{
		if (AGSBurnObjectiveBase* Objective = *It)
		{
			Objective->OnBurnObjectiveCompleted.AddDynamic(this, &UGSScoreSubsystem::HandleObjectiveCompleted);
			BoundObjectives.Add(Objective);
		}
	}

	if (UGSRaidDirector* Director = InWorld.GetSubsystem<UGSRaidDirector>())
	{
		Director->OnRaidEnded.AddDynamic(this, &UGSScoreSubsystem::HandleRaidEnded);
	}

	UE_LOG(LogTemp, Log, TEXT("[GoblinSiege] Score subsystem bound to %d burn objective(s)."),
		BoundObjectives.Num());
}

void UGSScoreSubsystem::Deinitialize()
{
	// Unbind exactly what was bound. A stale dynamic delegate onto a destroyed subsystem is a crash,
	// and the objectives can outlive us on a seamless travel.
	for (const TWeakObjectPtr<AGSBurnObjectiveBase>& Weak : BoundObjectives)
	{
		if (AGSBurnObjectiveBase* Objective = Weak.Get())
		{
			Objective->OnBurnObjectiveCompleted.RemoveDynamic(this, &UGSScoreSubsystem::HandleObjectiveCompleted);
		}
	}
	BoundObjectives.Reset();

	if (const UWorld* World = GetWorld())
	{
		if (UGSRaidDirector* Director = World->GetSubsystem<UGSRaidDirector>())
		{
			Director->OnRaidEnded.RemoveDynamic(this, &UGSScoreSubsystem::HandleRaidEnded);
		}
	}

	Super::Deinitialize();
}

void UGSScoreSubsystem::HandleObjectiveCompleted(AGSBurnObjectiveBase* Objective)
{
	if (!Objective)
	{
		return;
	}

	// Completion is meant to be terminal and first-wins, but a re-broadcast would silently double the
	// score and nothing downstream would notice a wrong number. Cheap to guard, impossible to debug
	// after the fact.
	if (ScoredObjectives.Contains(Objective))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] Score: '%s' completed twice - ignoring the second award."),
			*Objective->GetName());
		return;
	}
	ScoredObjectives.Add(Objective);

	// FIRST of its type scores full; later ones of the same type score the duplicate rate.
	//
	// Not read from GetListState(): AGSBurnObjectiveBase::HandleCompleted sets the state to Complete
	// BEFORE this delegate fires, so by the time we see it every objective is Complete and the
	// Required/Optional distinction has already been erased. Asking UGSRaidDirector instead would
	// work but would depend on whether the director's handler ran before ours - both bind the same
	// delegate, and that ordering is not contractual.
	//
	// Tracking the types we have scored ourselves is self-contained and deterministic. It also
	// matches what the demotion pass MEANS: the second field burned is a duplicate of a type already
	// satisfied, whether or not the director got there first.
	const FGameplayTag TypeTag = Objective->GetObjectiveTypeTag();
	const bool bFirstOfType = TypeTag.IsValid() && !DeedsByType.Contains(TypeTag);
	const int32 Points = bFirstOfType ? DeedsPerRequiredObjective : DeedsPerOptionalObjective;

	++ObjectivesCompleted;
	AddDeeds(TypeTag, Points);

	UE_LOG(LogTemp, Log, TEXT("[GoblinSiege] Score: '%s' (%s %s) +%d deeds -> %d total."),
		*Objective->GetName(), bFirstOfType ? TEXT("first") : TEXT("duplicate"),
		TypeTag.IsValid() ? *TypeTag.ToString() : TEXT("untagged"), Points, GetTotal());
}

void UGSScoreSubsystem::HandleRaidEnded(EGSRaidResult Result)
{
	// Extraction is the only result that pays. LeftBehind and OutOfLives keep every deed already
	// earned - the burning happened - they simply do not get the bonus for getting out.
	if (Result == EGSRaidResult::Extracted)
	{
		AddDeeds(FGameplayTag(), DeedsForExtraction);
	}

	UE_LOG(LogTemp, Log, TEXT("[GoblinSiege] Score final: %d deeds, %d loot, %d total (%d objectives)."),
		Deeds, Loot, GetTotal(), ObjectivesCompleted);
}

void UGSScoreSubsystem::AddDeeds(FGameplayTag TypeTag, int32 Points)
{
	if (Points == 0)
	{
		return;
	}
	Deeds += Points;

	// An invalid tag is the extraction bonus, which belongs to no objective type. Kept out of the
	// breakdown rather than filed under an empty key, so a per-type readout stays honest.
	if (TypeTag.IsValid())
	{
		DeedsByType.FindOrAdd(TypeTag) += Points;
	}

	OnScoreChanged.Broadcast();
}

void UGSScoreSubsystem::AddLoot(int32 Points)
{
	if (Points == 0)
	{
		return;
	}
	Loot += Points;
	bLootWasScored = true;
	OnScoreChanged.Broadcast();
}

void UGSScoreSubsystem::ResetLoot()
{
	if (Loot == 0)
	{
		return;
	}
	Loot = 0;
	OnScoreChanged.Broadcast();
}

int32 UGSScoreSubsystem::GetDeedsForType(FGameplayTag TypeTag) const
{
	const int32* Found = DeedsByType.Find(TypeTag);
	return Found ? *Found : 0;
}

FString UGSScoreSubsystem::BuildSummaryLine() const
{
	FString Line = FString::Printf(TEXT("%d points"), GetTotal());

	if (ObjectivesCompleted > 0)
	{
		Line += FString::Printf(TEXT("   %d objective%s burned"),
			ObjectivesCompleted, ObjectivesCompleted == 1 ? TEXT("") : TEXT("s"));
	}

	// Only mentioned once something has actually scored loot. Printing "0 loot" every raid would
	// read as a system that is broken rather than one that is not wired up yet.
	if (bLootWasScored)
	{
		Line += FString::Printf(TEXT("   %d loot"), Loot);
	}

	return Line;
}
