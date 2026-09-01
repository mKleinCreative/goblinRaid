#include "Combat/GSGE_Burning.h"
#include "Combat/GSDamageExecCalculation.h"
#include "Combat/GSGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGSGE_Burning::UGSGE_Burning()
{
	// Infinite, not HasDuration - see the header. The roll is the only way out.
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// The per-tick DAMAGE is SetByCaller, set by whoever ignites the pawn (currently only
	// AGSFireVolume::ApplyBurningStatus) - same reasoning UGSGE_FireDamage gives for leaving its own
	// magnitude to the caller: the number is per-hazard tuning, not something this class should own.
	// Period itself is a plain literal here rather than SetByCaller - GAS reads it once, at apply
	// time, to schedule the tick timer, and every ignition source in the game wants the same
	// cadence ("a slow burn"), so there is nothing for a caller to tune.
	Period = 1.f;
	bExecutePeriodicEffectOnApplication = false; // the contact tick that ignited you already hurt you this frame

	// Same shared calc every damage source in the game runs through - see UGSGE_FireDamage's
	// comment. The caller supplies both GSTags::Damage_Fire (as a dynamic asset tag, so the calc
	// knows which SetByCaller key to read) and its magnitude.
	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UGSDamageExecCalculation::StaticClass();
	Executions.Add(Execution);
}

void UGSGE_Burning::PostInitProperties()
{
	Super::PostInitProperties();

	// Grants State.Burning to the target for exactly as long as this effect is active - the tag
	// UGSGA_DodgeRoll removes by (RemoveActiveEffectsWithGrantedTags) to put the fire out, and the
	// tag AGSFireVolume::ApplyBurningStatus checks before igniting so standing in the fire doesn't
	// stack a second infinite instance on top of the first. UTargetTagsGameplayEffectComponent, not
	// the legacy InheritableOwnedTagsContainer field - that field is UE_DEPRECATED(5.3) and this
	// project has already been bitten once by carrying a deprecated GAS member past an engine
	// upgrade (UGameplayAbility::AbilityTags, see CLAUDE.md).
	//
	// Here, not the constructor - see the header. Only ever runs once in practice: GAS reads a
	// GameplayEffect through its CLASS DEFAULT OBJECT (MakeOutgoingSpec resolves
	// TSubclassOf<UGameplayEffect>::GetDefaultObject()), and this is a plain C++ class with no
	// Blueprint subclass expected, so there is exactly one UGSGE_Burning instance for
	// PostInitProperties to ever run on.
	UTargetTagsGameplayEffectComponent& TagsComponent = AddComponent<UTargetTagsGameplayEffectComponent>();
	FInheritedTagContainer TagChanges;
	TagChanges.Added.AddTag(GSTags::State_Burning);
	TagsComponent.SetAndApplyTargetTagChanges(TagChanges);
}
