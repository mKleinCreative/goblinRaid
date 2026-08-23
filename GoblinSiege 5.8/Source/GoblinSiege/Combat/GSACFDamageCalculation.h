// Copyright Goblin Siege.

#pragma once

#include "CoreMinimal.h"
#include "Game/ACFDamageCalculation.h"
#include "GSACFDamageCalculation.generated.h"

/**
 * Goblin Siege's damage rules, expressed as ACF's calculator (#234, ruling 26).
 *
 * WRITTEN BUT NOT WIRED UP. Nothing assigns this to DamageCalculatorClass yet: the damage TRANSPORT
 * is a separate step (2b-2b), and until every source routes through
 * UACFDamageHandlerComponent::TakeDamage this class computes nothing. Existing combat is untouched.
 *
 * WHY NOT SUBCLASS UACFGASDamageCalculator, which is ACF's own: its `Internal_CalculateDamage` is
 * declared NON-VIRTUAL (`ACFGASDamageCalculator.h:30`) and shadows rather than overrides. Deriving
 * from it and overriding `CalculateFinalDamage_Implementation` gives a class whose base helper can
 * never be reached by our code and whose own helper is invisible to ACF - the worst of both. Ruling
 * 26 recorded this trap before any of it was written.
 *
 * THE FOUR RULES THIS CARRIES, all previously in UGSDamageExecCalculation:
 *   1. Race matchup - melee never damages a character of the attacker's own race (#091). Fire is
 *      deliberately exempt: the torch is the goblin equalizer and burns everyone.
 *   2. NPC-vs-NPC scalar 0.55 - defender-on-defender and goblin-on-goblin lethality is tuned to a
 *      time-to-kill budget, not to the player's weapon numbers.
 *   3. Directional plate (#091, GDD 217) - a 150 degree frontal arc scaled to 0.3 and THEN reduced
 *      by flat armour; a flank, back or takedown hit skips the armour entirely. A bow ignores the
 *      plate at any angle: it finds the gaps.
 *   4. Minimum damage floor of 1 (#093, Michael's ruling) - applied LAST so it catches every
 *      mitigation path rather than needing a guard in each. A knight once took exactly 0.00 from a
 *      light swing: 25 x 0.55 x 0.3 - 6.
 */
UCLASS()
class GOBLINSIEGE_API UGSACFDamageCalculation : public UACFDamageCalculation
{
	GENERATED_BODY()

public:
	virtual float CalculateFinalDamage_Implementation(const FACFDamageEvent& InDamageEvent,
		TArray<FAttributeData>& OtherAffectedAttributes) override;

protected:
	/** True when the hit lands inside the target's frontal plate arc. Flat-dot on the horizontal
	 *  plane, the same test the block arc uses - one idea about what "in front of me" means. */
	bool IsStraightOn(const AActor* Target, const AActor* Attacker) const;
};
