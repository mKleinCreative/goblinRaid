#include "Combat/GSGE_WeaponDamage.h"
#include "Combat/GSDamageExecCalculation.h"

UGSGE_WeaponDamage::UGSGE_WeaponDamage()
{
	// Instant: a swing is a discrete event. Bleed/burn statuses are Duration effects and are a
	// separate class when they arrive.
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UGSDamageExecCalculation::StaticClass();
	Executions.Add(Execution);
}
