#include "Combat/GSGE_FireDamage.h"
#include "Combat/GSDamageExecCalculation.h"

UGSGE_FireDamage::UGSGE_FireDamage()
{
	// Instant: each fire tick is its own discrete application, so a pawn walking out of the flames
	// simply stops receiving them. A Duration/Periodic effect would keep burning them after they
	// left, which is a different (and later) design - the "on fire" status, not standing in fire.
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// The shared calc reads the Damage.* dynamic asset tag as its SetByCaller key, applies the race
	// matchup multiplier, then flat armor. The caller supplies both the tag and the magnitude.
	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UGSDamageExecCalculation::StaticClass();
	Executions.Add(Execution);
}
