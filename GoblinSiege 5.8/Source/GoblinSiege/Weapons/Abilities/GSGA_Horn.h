// The war-horn (GDD §2.3 "universal kit, every class", §2.5 "the horn and the horde").
//
// Two things happen, and they are deliberately not separable: goblins answer, and the town finds
// out. "The horn is *loud* - it is the formal end of the quiet half, and blowing it early is a
// choice you get to regret." An implementation that summoned without raising the alarm would delete
// the only real cost the horn has.
//
// INPUT: middle mouse, NOT G. Both design docs say G, but G is IA_Block as of the 2026-08-06
// movement remap (#058), which was Michael's own explicit ask; IMC_Default has 17 rows and 17
// distinct keys with no duplicates. Michael's ruling 2026-08-07: the horn takes the mouse.
// The GDD's "G" is a documentation erratum, and §2.3's "torch toss (Q)" is stale in the same way -
// IA_ThrowTorch has had no IMC row since the torch became a held weapon.
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GSGA_Horn.generated.h"

class UAnimMontage;

UCLASS()
class GOBLINSIEGE_API UGSGA_Horn : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGSGA_Horn();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** How long the blast lasts before the ability ends. State.Horn is held for this whole window,
	 *  and the ability blocks on its own tag, so this doubles as the re-blow cooldown. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horn", meta = (ClampMin = "0.1"))
	float BlastDurationSeconds = 1.6f;

	/** Optional. The blast is audible and summons regardless - a missing montage costs the wind-up
	 *  animation, not the feature. Soft so it costs no package load at module time. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horn")
	TSoftObjectPtr<UAnimMontage> HornMontage;

private:
	/** Does the summon, then ends. Bound to the blast-duration delay so the goblins answer the horn
	 *  rather than pre-empting it.
	 *
	 *  Takes no parameters and caches nothing: the handle, actor info and activation info are read
	 *  back through GetCurrentAbilitySpecHandle() / GetCurrentActorInfo() / GetCurrentActivationInfo()
	 *  at the point of use. Stashing the raw FGameplayAbilityActorInfo* across an async delay is the
	 *  obvious version and a dangling pointer if the avatar dies mid-blast - which, for an ability
	 *  whose entire purpose is to be blown in the middle of a fight, is not a rare case. */
	UFUNCTION()
	void OnBlastFinished();
};
