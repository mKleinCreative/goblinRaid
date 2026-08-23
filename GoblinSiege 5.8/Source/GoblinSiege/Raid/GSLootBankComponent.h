// One banking implementation, given to both places that take loot.
//
// GDD §9 carried this as a live defect for months: "UGSScoreSubsystem::AddLoot has ZERO callers
// project-wide - its own header says 'What you carried out. Always 0 today.' ... The Warren needs
// the same seam, so build it once and give it both consumers."
//
// AGSWarren grew its own copy first and it works (watched banking a pig for 40 loot, 2026-08-21).
// This is that logic lifted into a component so the runic site can have it without a second copy.
// AGSWarren is deliberately NOT migrated in the same pass - its banking is verified behaviour and
// re-plumbing it while adding a new consumer would put both at risk in one change. That migration
// is a follow-up with its own observation.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSLootBankComponent.generated.h"

class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSOnLootBankedHere, AActor*, Source, int32, Points);

/**
 * Takes cargo off a pawn, or off the ground, and turns it into loot on the score subsystem.
 *
 * DOES NOT OWN AN OVERLAP VOLUME. The owner decides what "arriving" means and calls in - the Warren
 * has a banking sphere, the runic site already has an extraction sphere, and giving this component
 * a third would mean two volumes fighting over the same circle on the portal.
 *
 * LOOT, NEVER DEEDS, and there is deliberately no flag to change that. GDD §9: deeds bank only
 * through the real portal at the END of a raid, because a second place to bank them would erase the
 * reason to risk the run home. Loot is the forgiving half; deeds are the half you carry.
 */
UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSLootBankComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSLootBankComponent();

	/**
	 * Something came into the owner's circle. Sorts out what it is and banks accordingly.
	 *
	 * A PAWN BANKS WHAT IT CARRIES, NOT ITSELF - checked first, because a courier goblin carrying a
	 * pig is both a pawn and, in principle, a loose actor, and swallowing the courier would be a
	 * spectacular way to lose the horde.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Loot Bank")
	int32 BankFromOverlap(AActor* Arriving);

	/** A pawn walked in carrying something. Returns the points banked, 0 if it carried nothing
	 *  worth anything. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Loot Bank")
	int32 BankCarriedLoot(APawn* Pawn);

	/** Something is lying in the circle under its own steam - thrown in, or put down by a courier. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Loot Bank")
	int32 BankLooseActor(AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Loot Bank")
	int32 GetPointsBankedHere() const { return PointsBankedHere; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Loot Bank")
	int32 GetItemsBankedHere() const { return ItemsBankedHere; }

	/** What this bank is called in logs and status readouts. Set it on the owner so
	 *  "portal" and "warren" can be told apart when both are banking. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Loot Bank")
	FString BankLabel = TEXT("bank");

	/** Fires BEFORE the cargo is destroyed, so a listener can still read its location, class or
	 *  mesh. Handing Blueprint a pending-kill actor is a crash waiting for the first missing
	 *  IsValid. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Loot Bank")
	FGSOnLootBankedHere OnLootBanked;

	/** What an actor is worth, or 0. Static so a caller can ask before deciding to bank. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Loot Bank")
	static int32 GetLootValueOf(const AActor* Actor);

private:
	/** Scores it, counts it, tells anyone listening, then destroys the cargo - in that order. */
	int32 CommitBank(AActor* Cargo, int32 Value);

	int32 PointsBankedHere = 0;
	int32 ItemsBankedHere = 0;
};
