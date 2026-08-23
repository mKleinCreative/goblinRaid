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
class UAudioComponent;
class USceneComponent;
class USoundBase;

UCLASS()
class GOBLINSIEGE_API UGSGA_Horn : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGSGA_Horn();

	/**
	 * The button came up. Called by AGSPlayerCharacter::Input_HornReleased.
	 *
	 * NOT UAbilityTask_WaitInputRelease, and that is the whole reason this function exists. That
	 * task only fires for an ability activated THROUGH a bound input id; the horn is activated by
	 * TryActivateAbilityByClass, which binds nothing, so the task would wait forever and every tap
	 * would stream the entire pool. The release has to be delivered by hand.
	 */
	void NotifyHornReleased();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** Clears the streaming timer. Every exit runs through here - release, cap reached, dry pool,
	 *  the avatar dying mid-blast - so the timer cannot outlive the ability. */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;


	/** How long the blast lasts before the ability ends. State.Horn is held for this whole window,
	 *  and the ability blocks on its own tag, so this doubles as the re-blow cooldown. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horn", meta = (ClampMin = "0.1"))
	float BlastDurationSeconds = 1.6f;

	/**
	 * The blast animation, in three montages: raise the horn, hold it at the lips for as long as
	 * the button is down, lower it.
	 *
	 * THREE ASSETS RATHER THAN THREE SECTIONS OF ONE, and that is not a style choice. Montage
	 * sections cannot be authored from the Python asset API - the section array is not exposed
	 * under any name and FCompositeSection has no settable StartTime, verified 2026-08-21. Only
	 * runtime section CHAINING is reachable, which is what UGSGA_Block uses. So the split lives in
	 * the assets, and each montage carries a single Default section that this class self-chains to
	 * loop, exactly as Block does.
	 *
	 * The shape deliberately matches the horn's VOICE below - attack, sustain, release - because
	 * they are the same event and drift between them would be visible.
	 *
	 * All optional and all soft: a missing montage costs the animation, not the feature. The summon
	 * and the alarm do not depend on any of this.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horn")
	TSoftObjectPtr<UAnimMontage> HornMontageIntro;

	/** The hold. Self-chained to loop while the button is down. Authored as a CONSTANT pose, so the
	 *  wrap is the same frame twice and cannot hitch - measured at 0.0000 deg. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horn")
	TSoftObjectPtr<UAnimMontage> HornMontageLoop;

	/** Lowering the horn. Plays once on release and is NOT interrupted by the ability ending. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horn")
	TSoftObjectPtr<UAnimMontage> HornMontageOutro;

	/**
	 * The horn's voice, in three parts: the breath and the note coming up, a sustain that loops
	 * seamlessly for as long as the button is down, and the fall-off when the lips leave it.
	 *
	 * ONE CONTINUOUS NOTE, not a blast re-struck per animation cycle. Michael's ruling,
	 * 2026-08-21: "I'd love to have it play a continuous note for as long as they hold it down."
	 * A horn held for six seconds is one breath, and re-articulating it made the hold sound like a
	 * stutter rather than an effort.
	 *
	 * Three assets rather than one looping file because a single loop cannot have an attack: the
	 * attack would come back round every cycle. The sustain is authored to be sample-accurate
	 * seamless (integer periods, integer samples, converged filter state) and carries no attack of
	 * its own, so it can run indefinitely without a seam.
	 *
	 * HornSoundLoop MUST have Looping ticked on the imported asset. Nothing here re-triggers it -
	 * if that flag is off, the horn speaks once and then goes quiet under a still-held button.
	 *
	 * This is the first sound in the GoblinSiege module - there was no audio anywhere in Source/
	 * before it - so it deliberately spawns bare attached components and does not reach for a
	 * SoundClass, submix or attenuation asset that the project does not yet own. When audio does
	 * get a proper spine, this is one call site to revisit, not a system to unpick.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horn")
	TSoftObjectPtr<USoundBase> HornSoundStart;

	/** The sustain. Must be marked Looping on the asset - see HornSoundStart. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horn")
	TSoftObjectPtr<USoundBase> HornSoundLoop;

	/** The fall-off, one-shot, when the note ends. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horn")
	TSoftObjectPtr<USoundBase> HornSoundEnd;

	/** Blend-out on the montage when the blast ends, so the goblin lowers the horn rather than
	 *  snapping back to idle on the frame the button comes up. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horn", meta = (ClampMin = "0.0"))
	float MontageBlendOutSeconds = 0.35f;


	/**
	 * Seconds between goblins while the horn is held.
	 *
	 * This is the feel dial for the whole feature. Too fast and the squad is a spray that makes the
	 * hold pointless; too slow and filling a squad of ten is a chore performed with a held mouse
	 * button. 0.55 fills ten in about five and a half seconds, counting the first blast.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horn", meta = (ClampMin = "0.05"))
	float SummonIntervalSeconds = 0.55f;

	/**
	 * Hard ceiling on one held blast. 0 means "until the active cap", which is the ruling: Michael,
	 * 2026-08-20, "a full squad of 10 eventually, but it summons them as you blast the horn".
	 *
	 * SUPERSEDES decision 9 ("SummonsPerBlast is a fixed 4 - a player counting his pool should not
	 * have to guess"). The guessing objection is answered better by the new shape than by the old
	 * number: you get exactly one per press, and holding shows you each one arriving.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horn", meta = (ClampMin = "0"))
	int32 MaxSummonsPerHold = 0;

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

	/** One goblin out of the hole, then decide whether to keep going. Drives itself on a timer. */
	void SummonNext();

	/** Raises the horn: prop visible, intro montage, first note. */
	void StartBlast();

	/** Hands the animation from the intro to the looping hold. Timed off the intro montage's own
	 *  length, so re-authoring the intro cannot desynchronise it - same rule as the audio handover. */
	void BeginHornHold();

	/** Plays a montage and points its first section at itself so it loops for as long as we leave
	 *  it alone. The section trick is UGSGA_Block's, for the reason its comment gives: a montage
	 *  marked looping in the ASSET would loop everywhere it is ever used. */
	void PlayLooping(UAnimMontage* Montage);

	/** Hides the horn prop once the outro has played out. Deferred rather than done at EndAbility,
	 *  because the outro IS the goblin lowering the horn and hiding it on the first frame of that
	 *  would make the prop vanish out of a moving hand. */
	void LowerHornProp();

	/** Strikes the attack and schedules the sustain to take over from it. */
	void StartHornVoice();

	/** Hands the note from the attack to the looping sustain. Timed off the attack's own duration
	 *  rather than a hand-tuned constant, so re-authoring the attack cannot desynchronise them. */
	void BeginHornSustain();

	/** Cuts the sustain and lets the fall-off finish the note. */
	void StopHornVoice();

	/** The goblin's mesh, or null if there is no avatar to hang a sound on. All three voice parts
	 *  attach to the same place, and all three have to survive the avatar dying mid-blast. */
	USceneComponent* GetVoiceAttachPoint() const;

	/** Lowers the horn: unhooks the section loop, blends the montage out and fades the voice.
	 *  Called from EndAbility only, so every exit path lowers it exactly once. */
	void StopBlast();

	/** True from activation until the button comes up. A tap is simply a hold that ended before
	 *  the first goblin was delivered - which is why a tap still yields exactly one. */
	bool bHornHeld = false;

	int32 SummonedThisBlast = 0;

	/** A plain world timer, not an AbilityTask. The ability is ServerOnly and InstancedPerActor, so
	 *  there is nothing to predict and nothing to replicate; an AbilityTask would buy lifetime
	 *  management this class already has to do in EndAbility anyway. */
	FTimerHandle SummonTimerHandle;

	/** Fires once, at the end of the attack, to start the sustain. Separate from SummonTimerHandle
	 *  because the two cadences are deliberately unrelated: one goblin every 0.55s, one handover at
	 *  whatever the attack sample happens to be. Sharing a timer would tie the summon rate to the
	 *  length of an audio file. */
	FTimerHandle VoiceHandoffTimerHandle;

	/** Intro -> hold handover. Separate from the voice's handover because the two assets are
	 *  authored independently and their lengths are not required to match. */
	FTimerHandle MontageHandoffTimerHandle;

	/** Fires at the end of the outro to hide the horn prop. Cleared by StartBlast, so blowing again
	 *  before the outro finishes does not hide the horn out from under the new blast. */
	FTimerHandle HornLowerTimerHandle;

	/** Whichever montage is currently playing - intro or loop. Kept so the stop path halts the same
	 *  object it started; the soft pointer could resolve to nothing by then if the package was
	 *  unloaded mid-blast. The outro is deliberately NOT tracked here: nothing stops it. */
	UPROPERTY()
	TObjectPtr<UAnimMontage> ActiveHornMontage;

	/** The attack and the sustain, held separately: on release the sustain has to be cut while the
	 *  attack - if it is somehow still running on a very short tap - is left to finish rather than
	 *  clipped mid-breath.
	 *
	 *  Weak: these components are owned by the audio engine and can be torn down under us when a
	 *  sound finishes or the avatar dies. Every use re-checks them. */
	TWeakObjectPtr<UAudioComponent> AttackAudio;
	TWeakObjectPtr<UAudioComponent> LoopAudio;
};
