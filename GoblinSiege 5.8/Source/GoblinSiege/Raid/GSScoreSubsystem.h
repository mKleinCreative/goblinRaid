// The raid's tally. Added 2026-08-06 - the u=4.0 NEXT item, and the other half of "what happens when
// a raid ends" that #050's end panel opened.
//
// TWO KINDS, per the design doc: DEEDS (what you destroyed) and LOOT (what you carried out). Be aware
// that only one of them has a source today - see the Loot accessors. That is stated in the API rather
// than hidden, because a loot counter that silently always reads 0 is worse than no loot counter.
//
// A UWorldSubsystem, matching UGSRaidDirector, and for the same reason: it needs no level authoring,
// so a generated map gets scoring for free.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Raid/GSRaidTypes.h"
#include "GSScoreSubsystem.generated.h"

class AGSBurnObjectiveBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnScoreChanged);

UCLASS()
class GOBLINSIEGE_API UGSScoreSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** What you burned. Fed by AGSBurnObjectiveBase::OnBurnObjectiveCompleted. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Score")
	int32 GetDeeds() const { return Deeds; }

	/**
	 * What you carried out. **Always 0 today.**
	 *
	 * Nothing in the project has a loot value - `UGSCarryComponent` carries objects, and no object
	 * declares what it is worth. The counter exists because the two-kind split is a design decision
	 * and the score screen should not have to be rebuilt when loot arrives; `AddLoot` is the seam.
	 * `HasLootSource()` is how the UI knows to hide the line rather than print a permanent zero.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Score")
	int32 GetLoot() const { return Loot; }

	/** False until something calls AddLoot. Lets the end screen omit a line that would only ever
	 *  read 0, without hardcoding "loot is not implemented" into the UI. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Score")
	bool HasLootSource() const { return bLootWasScored; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Score")
	int32 GetTotal() const { return Deeds + Loot; }

	/** Per-type deed breakdown, keyed by the objective's ObjectiveTypeTag. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Score")
	int32 GetDeedsForType(FGameplayTag TypeTag) const;

	/** How many objectives were completed, regardless of type. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Score")
	int32 GetObjectivesCompleted() const { return ObjectivesCompleted; }

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Score")
	void AddDeeds(FGameplayTag TypeTag, int32 Points);

	/** The seam loot hangs off when something finally has a value. Sets HasLootSource. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Score")
	void AddLoot(int32 Points);

	/** One line for the end panel: "142 points   3 objectives". Built here rather than in the widget
	 *  so the score's own wording lives with the score. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Score")
	FString BuildSummaryLine() const;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Score")
	FGSOnScoreChanged OnScoreChanged;

protected:
	UFUNCTION()
	void HandleObjectiveCompleted(AGSBurnObjectiveBase* Objective);

	UFUNCTION()
	void HandleRaidEnded(EGSRaidResult Result);

private:
	int32 Deeds = 0;
	int32 Loot = 0;
	int32 ObjectivesCompleted = 0;
	bool bLootWasScored = false;

	/** Guards against a second award for the same objective. Completion is meant to be terminal, but
	 *  a re-broadcast would silently double a player's score and nothing else would notice. */
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<AGSBurnObjectiveBase>> ScoredObjectives;

	TMap<FGameplayTag, int32> DeedsByType;

	/** Everything this subsystem bound to, so Deinitialize can unbind exactly what it bound. */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AGSBurnObjectiveBase>> BoundObjectives;
};
