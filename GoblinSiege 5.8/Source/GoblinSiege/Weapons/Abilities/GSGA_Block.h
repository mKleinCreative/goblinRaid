// Hold-to-block.
//
// The whole mechanic is one gameplay tag: State.Blocking, applied for as long as the button is
// held. UGSDamageExecCalculation reads it and mitigates FRONTAL hits only (GS.Combat.BlockArc),
// which is what makes blocking a decision - you choose what to face, not whether to hold a key.
//
// Deliberately no shield mesh, no stamina, no guard break. Those are the next three arguments to
// have; none of them can be had usefully until it is known whether holding a guard feels good at
// all, and that question is answerable today with a tag and an idle pose.
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GSGA_Block.generated.h"

class UAnimMontage;

UCLASS()
class GOBLINSIEGE_API UGSGA_Block : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGSGA_Block();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Guard pose. Looped for as long as the block is held. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Block")
	TObjectPtr<UAnimMontage> BlockIdleMontage;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Block", meta = (ClampMin = "0.1"))
	float MontagePlayRate = 1.f;

	/** Movement speed while guarding, as a fraction of normal. Blocking should cost mobility -
	 *  otherwise there is never a reason to lower the guard. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Block", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlockMoveSpeedScale = 0.45f;

private:
	float CachedMaxWalkSpeed = 0.f;
};
