// Shared base for every goblin, human, elf, and dwarf pawn. Owns the AbilitySystemComponent and
// AttributeSet so damage, armor, and status effects (root/stun/burn) work identically across
// player-controlled goblins and AI-controlled defenders - only the granted abilities and data
// assets differ per subclass. See design doc §10 "Road to Unreal" table.
#pragma once

#include "CoreMinimal.h"
// ACF migration Phase 2a (2026-08-21, #223). Was "GameFramework/Character.h" / ACharacter.
//
// AACFCharacter builds FOURTEEN components of its own, including ActionsComp (a
// UACFAbilitySystemComponent) and StatisticsComp (UACFGASStatisticsComponent), and its
// GetAbilitySystemComponent() returns ActionsComp. We therefore no longer create an ASC: doing so
// would put TWO ability system components on every character, which is the failure ruling 25 exists
// to prevent. See the AbilitySystemComponent member below - it is now a cached pointer, not a
// subobject.
#include "Actors/ACFCharacter.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GSCharacterBase.generated.h"

class UAbilitySystemComponent;
class UGSAttributeSetBase;
class UGSEngagementComponent;
class UGameplayEffect;
class UGameplayAbility;
class UAnimMontage;
struct FOnAttributeChangeData;

/** New, Max, Delta. Delta is negative for damage. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGSOnHealthChanged, float, NewHealth, float, MaxHealth, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnDied);

/** Fired on EVERY damaging hit, with the attacker. Distinct from OnHealthChanged, which also fires
 *  for heals and cannot name who did it. Exists for the horde's Frenzy rule (GDD §2.5, "anything
 *  that attacks you ... gets swarmed automatically") - UGSHordeSubsystem is the intended listener. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSOnDamaged, AActor*, Attacker, float, Damage);

/** Fired on the ATTACKER when it lands a damaging hit. The other half of the Frenzy rule ("anything
 *  you attack"). Broadcast by the abilities that deal damage, not by the attribute path - the victim
 *  is what the swing already knows and the attacker is what it already is. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnDealtDamage, AActor*, Victim);

UCLASS(Abstract)
// IAbilitySystemInterface is NOT listed here any more: AACFCharacter already declares it (along with
// IGenericTeamAgentInterface, IACFEntityInterface and IALSSavableInterface), and UHT treats a
// re-declaration in a derived class as an error.
class GOBLINSIEGE_API AGSCharacterBase : public AACFCharacter
{
	GENERATED_BODY()

public:
	// AACFCharacter has NO default constructor - it takes an FObjectInitializer (ACFCharacter.h:54),
	// so the whole chain must pass one down (#223).
	AGSCharacterBase(const FObjectInitializer& ObjectInitializer);

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	/**
	 * Re-exposed as PUBLIC (#223). AACFCharacter declares GetAbilitySystemComponent as **protected**,
	 * which broke four external callers the moment we reparented - GSBuffAuraComponent,
	 * GSWeaponComponent and GSObjective_KillLandlord (twice). This does not change WHAT is returned:
	 * it forwards to Super, so ACF's ActionsComp remains the one and only ASC. No UFUNCTION specifier:
	 * UHT rejects one above an override of a parent UFUNCTION.
	 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UGSAttributeSetBase* GetAttributeSetBase() const { return AttributeSetBase; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Combat")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Combat")
	float GetMaxHealth() const;

	/**
	 * NOT a UFUNCTION any more (#223). AACFCharacter declares its own
	 * `UFUNCTION(BlueprintPure) bool IsAlive() const`, and UHT rejects a same-named UFUNCTION in a
	 * derived class outright. Dropping the specifier keeps this as plain C++ and costs nothing:
	 * a search of every .uasset in Content found ZERO Blueprint callers of IsAlive, while 21 C++
	 * call sites depend on it.
	 *
	 * THIS ONE IS THE TRUTH IN PHASE 2A, and ACF's is not. ACF's reads
	 * `GetDamageHandlerComponent()->GetIsAlive()`, whose `bIsAlive` defaults to true and is only
	 * ever cleared by ACF's OWN damage path - which nothing in this project drives yet. So ACF's
	 * IsAlive answers "alive" for a corpse. That matters the moment ACF's targeting or
	 * IACFEntityInterface::IsEntityAlive starts being consulted, which is Phase 2b's job: route
	 * death through UACFDamageHandlerComponent and then DELETE this function rather than keep two.
	 */
	bool IsAlive() const { return !bIsDead; }

	/** Applied on respawn: sets Health to RespawnHealthFraction * MaxHealth and grants brief i-frames. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Combat")
	virtual void ApplyRespawnState(float HealthFraction, float InvulnerabilitySeconds);

	/**
	 * DEBUG ONLY. Kill outright by driving the Health attribute to zero.
	 *
	 * Deliberately NOT a call to HandleDeath. Setting the attribute is what a real killing blow
	 * does, so this runs the whole genuine chain - HandleHealthChanged, the death tags, the ragdoll,
	 * the GameMode's life accounting - rather than testing a shortcut that skips the parts most
	 * likely to be broken. UGameplayStatics::ApplyDamage cannot be used here: damage in this project
	 * is a GameplayEffect, so ApplyDamage leaves Health untouched (confirmed 2026-08-06 - six calls,
	 * HP stayed 100).
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Debug")
	void DebugKill();

	/**
	 * Kill this character now, with no instigator and no damage type.
	 *
	 * The shared body of DebugKill and of drowning. Zeroes Health directly rather than applying a
	 * damage effect: there is nothing for armour, blocking or the frontal-arc rule to act on, and
	 * routing a drowning through damage would let a raised shield survive it.
	 */
	void KillOutright();

	/** Per-archetype/per-weapon turn-rate identity (Brute turns like a barge, Slasher/Scout turns
	 *  sharp - design doc "Turn rate"). Pushes the value into CharacterMovementComponent::RotationRate
	 *  so bOrientRotationToMovement-driven turning actually uses it. Called by UGSWeaponComponent on
	 *  equip so a weapon's identity applies itself to its wearer (tech doc §16). */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Movement")
	void SetTurnRateRadPerSec(float NewTurnRateRadPerSec);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Movement")
	float GetTurnRateRadPerSec() const { return TurnRateRadPerSec; }

	/** Which race this character fights for. Melee refuses to damage a target sharing it.
	 *
	 *  Unset = hits everything, which is the pre-2026-08-04 behaviour and the safe default for
	 *  anything that has not opted in. AGSEnemyCharacter adopts its race data's RaceTag, so the six
	 *  human defenders inherit Race.Human from DA_Race_Human without touching a single Blueprint. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Combat")
	FGameplayTag GetRaceTag() const { return RaceTag; }

	/** False when both sides share a race (or either is unset - see GetRaceTag). Checked by the
	 *  melee sweep; NOT by fire, which burns everyone on purpose. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Combat")
	bool IsHostileTo(const AActor* Other) const;

	/** Sets the unmodified walk speed and re-derives the effective one. For anything that owns a
	 *  character's baseline rather than a temporary slow - archetype init, a weapon's identity.
	 *  Temporary slows must NOT come through here; they are GameplayEffects on MoveSpeedMultiplier. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Movement")
	void SetBaseWalkSpeed(float NewBaseWalkSpeed);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Movement")
	float GetBaseWalkSpeed() const { return BaseWalkSpeed; }

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	/** Grants the base GameplayEffect (Health/MaxHealth/Armor/MoveSpeed init values) from data. Called
	 *  by subclasses after their race/weapon data asset is known. */
	void InitializeAttributesFromEffect(TSubclassOf<UGameplayEffect> InitEffectClass, float Level = 1.f);

	/** Bound to the Health attribute's OnAttributeChanged delegate in BeginPlay. */
	virtual void HandleHealthChanged(const FOnAttributeChangeData& Data);

	/** Bound to MoveSpeedMultiplier in BeginPlay. Lives on the BASE, not on the player: every slow in
	 *  the game is a GameplayEffect on that attribute now (UGSGE_MoveSpeedScalar), and a defender that
	 *  did not listen would raise its guard with no mobility cost at all. */
	void HandleMoveSpeedMultiplierChanged(const FOnAttributeChangeData& Data);

	/** Turns BaseWalkSpeed and the MoveSpeedMultiplier attribute into the movement component's
	 *  effective speed. The one place walk speed is written. Subclasses override to derive their own
	 *  extra speeds from the same multiplier (the player adds MaxWalkSpeedCrouched). */
	virtual void ApplyMoveSpeed();

	/** Walk speed before any multiplier - captured from the movement component in BeginPlay, and
	 *  replaced by SetBaseWalkSpeed when something authoritative (an archetype row) supplies one. */
	float BaseWalkSpeed = 600.f;

	/** See GetRaceTag. EditAnywhere so a one-off placed actor can override what its race data says. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Combat")
	FGameplayTag RaceTag;

public:
	/** Broadcast on every Health change, damage or heal. Exists because the attribute delegate GAS
	 *  gives us is non-dynamic and therefore invisible to Blueprint and UMG - this is the version a
	 *  health bar can actually bind to. Delta is negative for damage. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Combat")
	FGSOnHealthChanged OnHealthChanged;

	/** Broadcast once, when this character dies. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Combat")
	FGSOnDied OnDied;

	/** Broadcast on every damaging hit, with whoever landed it (may be null). Unlike
	 *  OnHealthChanged this does not fire for heals and does name the attacker, which is what the
	 *  horde's Frenzy rule needs. Damage is reported positive. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Combat")
	FGSOnDamaged OnDamaged;

	/** Broadcast on the attacker when one of its abilities damages someone. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Combat")
	FGSOnDealtDamage OnDealtDamage;

	/** Called by the damage-dealing abilities on the INSTIGATOR after a hit lands. Keeps the
	 *  "anything you attack" half of Frenzy out of the attribute path, which never sees the swing. */
	void NotifyDealtDamage(AActor* Victim);

	/** Play a flinch. Direction is the world-space vector from this character to whatever hit them;
	 *  pass zero if unknown and the front reaction is used. Safe to call every frame - it self-gates
	 *  on the cooldown and on State.HitReact. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Combat")
	/**
	 * @param Causer  WHO caused this flinch. Named Causer, not Instigator: UHT refuses the
	 *        latter because AActor already declares an Instigator in scope and shadowing is an error. Optional, and null is honest rather than lazy - a
	 *        fall or a fire volume genuinely has no attacker. Forwarded to the engagement
	 *        component so CanBeAttackedBy can refuse the causer alone rather than the whole gang
	 *        (#221); an unnamed instigator refuses nobody.
	 */
	void PlayHitReact(const FVector& FromDirection, AActor* Causer = nullptr);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Combat")
	bool IsBlocking() const;

	/**
	 * Refuses a montage authored for a different skeleton, and returns 0 instead of playing it.
	 *
	 * UE does NOT reject this by itself. Every montage in the project lives on GOB_Scout_v2_Skeleton
	 * (the goblin rig) - there are no human ones - and the six human defenders run SK_Human_Skeleton.
	 * Playing AM_GS_Atk_Light on a guard returns a cheerful 1.150s and drives the slot, but the tracks
	 * map to nothing, so the slot evaluates to the REFERENCE POSE: every attack, block and guard break
	 * snapped a guard into a T-pose for the length of the montage, taking his sword arm out sideways
	 * with it (measured: hand 104uu from the body centre in idle, 149uu while the montage plays).
	 *
	 * This is a stopgap, not the fix. It trades a T-posing guard for an unanimated one; the real
	 * answer is human combat animations, retargeted or authored. Delete this the day they exist.
	 */
	virtual float PlayAnimMontage(UAnimMontage* AnimMontage, float InPlayRate = 1.f,
		FName StartSectionName = NAME_None) override;

	/**
	 * Called on the ATTACKER when one of its hits was turned aside by a guard.
	 *
	 * Three things happen, and they are one mechanic: the swing in flight is cancelled so it cannot
	 * follow through or chain, State.Recoil is applied for RecoilSeconds so the attacker can neither
	 * swing again nor raise its own guard, and a flinch sells it. That window is the defender's
	 * reward for reading the attack - without it, blocking is only "take 20% damage instead of 100%"
	 * and a fight between two competent guards is a stalemate neither side can break.
	 *
	 * Deliberately does NOT block dodging: a player who realises mid-recoil that they are about to
	 * be punished should still have one way out.
	 *
	 * Symmetric on purpose. This fires for the player's blocked swings exactly as it does for an
	 * AI's, and it is the reason an allied goblin - which carries no guard break - still has an
	 * answer to a defender who turtles.
	 */
	void NotifyAttackWasBlocked(AActor* Blocker, float RecoilSeconds);

	/** True while this character is open from a blocked swing. Read by UBTTask_MeleeAttack to
	 *  bypass its own attack cooldown - an opening nobody exploits is not an opening. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Combat")
	bool IsRecoiling() const;

	/** Who is allowed to swing at THIS character, and where they may stand while doing it. On the
	 *  base so player, defender and horde goblin are all rationed by the same rules - a crowd
	 *  control system the player is exempt from would be immediately visible. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Combat")
	UGSEngagementComponent* GetEngagement() const { return EngagementComponent; }

	// ---- combat verbs (hoisted from AGSEnemyCharacter 2026-08-07, ticket #069) ------------
	// Moved DOWN to the base rather than reparenting AGSHordeGoblin to AGSEnemyCharacter. The
	// reparent was one line, but it silently flips five class-identity checks that all read
	// "is this a defender": GSFireVolume.cpp:394 would stop applying FriendlyFireScalar to the
	// horde, GSTargetingComponent.cpp:50 would snap the player's soft-lock onto his own goblins,
	// and GSBuffAuraComponent.cpp:48 would let defender auras buff them. None fail loudly.
	// Michael's ruling, 2026-08-07.
	//
	// Deliberately the SAME abilities the player runs, not an AI-only reimplementation - if a
	// defender's swing were its own code path it would drift from the player's within a week.
	// Which verbs a character HAS stays data, not code: allied goblins get light and heavy,
	// humans additionally get the guard break. Leave a class unset and it cannot do that thing.

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Combat")
	bool TryLightAttack();

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Combat")
	bool TryHeavyAttack();

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Combat")
	bool TryGuardBreak();

	/** Raises the guard and leaves it up until StopBlocking. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Combat")
	bool StartBlocking();

	/** Drops the guard. Cancels by tag, never by class, so a swing in flight survives. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Combat")
	void StopBlocking();

	/** True when this character has a guard break available - the one verb allied goblins lack. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Combat")
	bool CanGuardBreak() const { return GuardBreakAbilityClass != nullptr; }

	/** Looses an arrow. The SAME ability the player's bow runs (UGSGA_BowShot), not an AI-only
	 *  reimplementation - it was written AI-ready on purpose: it fires on a release TIMER rather
	 *  than on an input release, and its muzzle falls back to the pawn's control rotation when
	 *  there is no aim component. An archer's shot and the player's therefore share a rate limit,
	 *  a projectile and a damage path. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Combat")
	bool TryRangedAttack();

	/** True when this character has a bow at all. The archer's equivalent of CanGuardBreak: which
	 *  verbs a character HAS stays data, so a militiaman simply leaves this unset. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Combat")
	bool CanRangedAttack() const { return RangedAttackAbilityClass != nullptr; }

protected:
	/**
	 * Grants whichever of the four combat ability classes are set. Server only.
	 *
	 * NOT called from this class's BeginPlay on purpose. AGSPlayerCharacter carries its own
	 * ability-class UPROPERTYs and grants them itself, so an automatic grant here would hand the
	 * player a second spec of every ability. Subclasses that want the AI verbs call this.
	 */
	void GrantCombatAbilities();

	/** UGSGA_SwordLight Blueprint child - the multi-stage combo. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Combat|Abilities")
	TSubclassOf<UGameplayAbility> LightAttackAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Combat|Abilities")
	TSubclassOf<UGameplayAbility> HeavyAttackAbilityClass;

	/** Left unset on allied goblins on purpose - the guard break is a human answer to turtling,
	 *  and giving it to everyone would make blocking worthless for both sides. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Combat|Abilities")
	TSubclassOf<UGameplayAbility> GuardBreakAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Combat|Abilities")
	TSubclassOf<UGameplayAbility> BlockAbilityClass;

	/** Set to UGSGA_BowShot on archers, left unset on everyone else. The player carries his own bow
	 *  slot on AGSPlayerCharacter (BowShotAbilityClass) because his is driven by input and a weapon
	 *  slot rather than by a behaviour tree; this is the AI-side grant of the same ability. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Combat|Abilities")
	TSubclassOf<UGameplayAbility> RangedAttackAbilityClass;

	/** Grants a class if set, so a grant list reads as a list rather than four null checks. */
	void GrantIfSet(TSubclassOf<UGameplayAbility> AbilityClass);

	bool TryActivate(TSubclassOf<UGameplayAbility> AbilityClass);


	/**
	 * The GAME consequences of dying: the dead latch, State.Dead, OnDied, and the game mode's pool
	 * accounting. NOT the presentation - ragdoll, movement lock, capsule collision and corpse
	 * lifespan are AACFCharacter::HandleCharacterDeath's since #228.
	 *
	 * UFUNCTION because it is bound to UACFDamageHandlerComponent::OnOwnerDeath, a dynamic
	 * multicast delegate. It is no longer called from the health delegate: ARS health reaching zero
	 * is what starts the chain now.
	 */
	UFUNCTION()
	virtual void HandleDeath();

	/**
	 * CACHED, NOT OWNED (#223). Points at AACFCharacter's ActionsComp, assigned in
	 * PostInitializeComponents. Kept under the old name deliberately: ~20 call sites in this class
	 * and its subclasses read it directly, and renaming them would have made a reparent look like a
	 * refactor. There is exactly one ASC on the actor and this is it.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Abilities")
	TObjectPtr<UGSAttributeSetBase> AttributeSetBase;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Combat")
	TObjectPtr<UGSEngagementComponent> EngagementComponent;

	/** Turn rate in rad/s - per-archetype tuning knob called out repeatedly in the design doc
	 *  (Brute 5, Slasher 12, Shaman 10, Militia/Knight slower still). Drives
	 *  CharacterMovementComponent::RotationRate via SetTurnRateRadPerSec rather than a snap-to-facing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GoblinSiege|Movement")
	float TurnRateRadPerSec = 8.f;

	UPROPERTY(BlueprintReadOnly, Category = "GoblinSiege|Combat")
	bool bIsDead = false;

	bool bAttributesInitialized = false;

	// ---- Death presentation (2026-08-02) -------------------------------------------------
	// Ragdoll moved from "skip" to "adopt" - the physics comedy is on-brand for a comedy game
	// (Michael's ruling, see claude/goblin-siege-acf-integration-plan.md). This does NOT replace
	// UGSGibComponent when that lands: gib is for lethal overkill, ragdoll for ordinary death.
	// Exposed as EditDefaultsOnly rather than hard-coded because death feel is a tuning pass, and
	// with Live Coding unable to add UPROPERTYs, a knob you forgot costs a full rebuild.

	/** Simulate physics on the mesh when Health hits 0. Off for anything that should stay standing. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Combat|Death")
	bool bRagdollOnDeath = true;

	/** Collision profile applied to the mesh before simulating. "Ragdoll" is the engine default. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Combat|Death")
	FName RagdollCollisionProfile = TEXT("Ragdoll");

	/** Extra shove along the killing blow's direction, so a corpse sells the hit. 0 = limp drop. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Combat|Death")
	float DeathImpulse = 0.f;

	/** Seconds before the corpse is destroyed. 0 = never (correct for a playtest - you want to see
	 *  what you killed). Set non-zero once a raid has enough bodies to matter. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Combat|Death", meta = (ClampMin = "0.0"))
	float CorpseLifespan = 0.f;

	// ---- Hit reactions (2026-08-03) ------------------------------------------------------
	// Combat read as stiff because nothing acknowledged a hit: health dropped and the victim
	// carried on as though nothing had happened. A flinch is the cheapest possible feedback and
	// it does most of the work.

	/** Flinch played when the hit lands in front. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Combat|HitReact")
	TObjectPtr<UAnimMontage> HitReactFront;

	/** Optional. Used when the hit comes from the character's left; falls back to HitReactFront. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Combat|HitReact")
	TObjectPtr<UAnimMontage> HitReactLeft;

	/** Optional. Used when the hit comes from the character's right; falls back to HitReactFront. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Combat|HitReact")
	TObjectPtr<UAnimMontage> HitReactRight;

	/** Played instead of a flinch when the hit was blocked - the guard absorbs it. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Combat|HitReact")
	TObjectPtr<UAnimMontage> BlockReact;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Combat|HitReact", meta = (ClampMin = "0.1"))
	float HitReactPlayRate = 1.4f;

	/** Minimum gap between flinches. Without this a three-hit combo restarts the montage on every
	 *  contact and the victim vibrates instead of staggering. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Combat|HitReact", meta = (ClampMin = "0.0"))
	float HitReactCooldownSeconds = 0.45f;

	/** Fraction of MaxHealth a single hit must exceed to flinch. Stops a burning field or a
	 *  damage-over-time tick from making a character flinch continuously. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Combat|HitReact", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HitReactMinDamageFraction = 0.04f;

	/** Whether a flinch is even attempted on this character. Off for anything that should look
	 *  unshakeable - a Brute mid-charge, say. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Combat|HitReact")
	bool bEnableHitReact = true;

public:
	/** Called by UGSAttributeSetBase immediately before it drains IncomingDamage into Health.
	 *  The attribute-change delegate that drives HandleHealthChanged cannot see the effect
	 *  context - on the base-value write path GAS passes GEModData as null - so the attacker has
	 *  to be handed over from the one place that still has it. Without this, FromDirection is
	 *  always zero and the left/right flinch variants are unreachable dead code. */
	void SetPendingDamageInstigator(AActor* InInstigator) { PendingDamageInstigator = InInstigator; }

private:
	float LastHitReactTime = -1000.f;

	/** Attacker for the damage event currently being applied. Consumed and cleared by
	 *  HandleHealthChanged; weak so a killed attacker cannot keep itself alive here. */
	TWeakObjectPtr<AActor> PendingDamageInstigator;

	/** Previous Health, tracked here because FOnAttributeChangeData::OldValue is unusable on the
	 *  base-value write path damage actually takes. Negative means "no sample yet". */
	float LastKnownHealth = -1.f;
};
