// "Kill the Landlord (designed): a named human authority figure... holds up somewhere in the
// city, guarded. Assassination objective... He should react to the raid: flee toward a safehouse
// or the far gate under escort once alarmed" (design doc §6). Reuses the same alarm/spawner/
// extraction backbone as Burn the Granaries with a target-and-hunt flavor.
//
// Open question per design doc §11: "how aggressively should he flee - a slow retreat you can cut
// off, or a real footrace with an escort?" - bFleeOnAlarmThreshold below is the tuning knob for
// that once it's settled; left conservative (slow retreat) for now.
#pragma once

#include "CoreMinimal.h"
#include "Missions/GSMissionObjective.h"
#include "GSObjective_KillLandlord.generated.h"

class AGSCharacterBase;
class AGSGameState;
struct FOnAttributeChangeData;

UCLASS()
class GOBLINSIEGE_API AGSObjective_KillLandlord : public AGSMissionObjective
{
	GENERATED_BODY()

public:
	virtual void BeginObjective() override;

protected:
	UFUNCTION()
	void HandleAlarmChanged(float NewAlarm01);

	/** The specific landlord pawn placed in the level - assigned by the level (via a tagged actor
	 *  lookup, matching AGSObjective_BurnGranaries' pattern) rather than a hard reference here. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Missions")
	FName LandlordActorTag = TEXT("Objective.Landlord");

	UPROPERTY(BlueprintReadOnly, Category = "GoblinSiege|Missions")
	TObjectPtr<AGSCharacterBase> LandlordPawn;

	/** Alarm01 threshold above which the landlord's Behavior Tree should switch to fleeing (read
	 *  via a Blackboard bool this objective sets - "IsFleeing", see GSAIControllerBase notes). */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Missions|Tuning")
	float FleeOnAlarmThreshold01 = 0.5f;

	bool bHasTriggeredFlee = false;

	/** Bound to the landlord's Health attribute-changed delegate; fires on every change, guarded
	 *  against non-fatal hits inside the implementation. Not a UFUNCTION() - AddUObject (not
	 *  AddDynamic) is used here since FOnGameplayAttributeValueChange is a non-dynamic multicast. */
	void HandleLandlordHealthChanged(const FOnAttributeChangeData& Data);
};
