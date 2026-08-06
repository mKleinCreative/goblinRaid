# Code Architect — run live-002

*Every decision below was logged before any file was written anywhere.*


## WHAT IT RECEIVED

- GDD: `D:\goblinRaid\GoblinSiege 5.8\Tools\CodeArchitect\docs\goblin-siege-gdd.md` — parsed **21** systems from §12.1, 8 blocks from §12.2

## WHAT IT PERCEIVED

- Source files scanned: **94**; folders: AI, Alarm, Attributes, Characters, Combat, Core, Destruction, Horde, Missions, Progression, UI, Weapons
- UCLASS/USTRUCT declarations: **57** (38 wired, 19 stubbed/header-only)
- Content data-layer sweep: {"BP_GS": 4, "DA_": 1, "GA_": 4, "IA_": 13, "IMC_": 1, "L_": 8}

## WHAT IT SCORED

| # | feature | block | utility | eligible | rationale |
|---|---------|-------|---------|----------|-----------|
| 1 | Interact framework â€” hold-E channels + carry | A | 10.0 | yes | impact 5; urgency 5; unblocks 5 open feature(s) [stealth_five, runic_site, loot_couriers, score_system, civilians_livestock]; effort 3; block A; completeness 0% |
| 2 | Death & hit-reaction clips retargeted ('nothing can die on screen') | B | 6.0 | EDITOR | impact 4; urgency 4; unblocks 0 open feature(s) [-]; effort 2; block B; completeness 0%; REQUIRES_EDITOR -> never headless-generatable; route to supervised session |
| 3 | Someone to fight â€” human race data, BT_Militia, enemy attack path | B | 5.75 | yes | impact 5; urgency 4; unblocks 3 open feature(s) [horde, stealth_five, patrol_director]; effort 4; block B; completeness 25% |
| 4 | Lives / respawn on PlayerState | E | 4.0 | yes | impact 3; urgency 2; unblocks 0 open feature(s) [-]; effort 2; block E; completeness 0% |
| 5 | Runic site â€” spawn/respawn, objective-gated portal, staging, 90s collapse | E | 4.0 | blocked | impact 5; urgency 2; unblocks 0 open feature(s) [-]; effort 3; block E; completeness 0%; BLOCKED by ['interact_framework'] |
| 6 | Score system â€” deeds/loot two-kind tally + end screen | G | 4.0 | blocked | impact 5; urgency 2; unblocks 0 open feature(s) [-]; effort 3; block G; completeness 0%; BLOCKED by ['interact_framework'] |
| 7 | Horn & horde (subsystem, pool, BT, point command) | D | 3.2 | blocked | impact 5; urgency 3; unblocks 1 open feature(s) [loot_couriers]; effort 5; block D; completeness 0%; BLOCKED by ['militia_enemy'] |
| 8 | The stealth five (noise, crouch-detect, takedown, corpse-suspicion, coin toss) | F | 3.0 | blocked | impact 5; urgency 2; unblocks 0 open feature(s) [-]; effort 4; block F; completeness 0%; BLOCKED by ['interact_framework', 'militia_enemy'] |
| 9 | Gore/gib system (intensity scalar, feather-poof) | G | 2.5 | blocked | impact 2; urgency 1; unblocks 0 open feature(s) [-]; effort 2; block G; completeness 0%; BLOCKED by ['militia_enemy'] |
| 10 | Barks + Overlord whispers (runtime side) | H | 2.5 | yes | impact 2; urgency 1; unblocks 0 open feature(s) [-]; effort 2; block H; completeness 0% |
| 11 | Patrol director â€” 5-7 min cadence + castle reinforcements | F | 2.33 | blocked | impact 3; urgency 1; unblocks 0 open feature(s) [-]; effort 3; block F; completeness 0%; BLOCKED by ['militia_enemy'] |
| 12 | Loot couriers â€” sacks + livestock cargo, point-to-courier | G | 2.25 | blocked | impact 4; urgency 1; unblocks 0 open feature(s) [-]; effort 4; block G; completeness 50%; BLOCKED by ['interact_framework', 'horde'] |
| 13 | Civilians + livestock (routines, disbelief, brigade, flee) | G | 1.75 | blocked | impact 3; urgency 1; unblocks 0 open feature(s) [-]; effort 4; block G; completeness 33%; BLOCKED by ['militia_enemy', 'interact_framework'] |

## WHAT IT PICKED

**Interact framework â€” hold-E channels + carry** — utility 10.0. impact 5; urgency 5; unblocks 5 open feature(s) [stealth_five, runic_site, loot_couriers, score_system, civilians_livestock]; effort 3; block A; completeness 0%

> NOTE: provider 'auto' resolved to 'claude-cli' — claude-cli bills the Max subscription, not API credits.

## WHAT IT ISSUED — generate:interact_framework

*model:* `fixture:interact_framework`

<details><summary>system prompt</summary>

```
You are the Code Architect for Goblin Siege, an Unreal Engine 5.8 C++ project
(module GoblinSiege, API macro GOBLINSIEGE_API). You write production C++ for review by the
project owner. House rules, non-negotiable:
- Class prefix GS (AGSFoo actors, UGSFoo components/objects). Canonical player class name is SCOUT.
- GAS is the combat backbone: abilities derive from UGameplayAbility, attributes live on
  UGSAttributeSetBase, native gameplay tags are declared in GSGameplayTags.h/.cpp.
- Never rewrite existing files wholesale. If an existing file needs an addition (a tag, an include,
  a component on the character), emit it in "notes" as an exact, minimal patch instruction instead.
- Multiplayer posture: single-player slice, co-op-ready — replicate the cheap root state
  (bIsX flags) but do not build prediction.
- Comments explain design intent tersely; no boilerplate comment noise.
Return ONLY a JSON object: {"files": {"Source/GoblinSiege/<path>": "<content>", ...},
"notes": "<markdown notes incl. exact patch instructions for existing files>",
"decisions": ["<why choices were made>", ...]}
```
</details>

<details><summary>user prompt</summary>

```
## Feature to build
Interact framework â€” hold-E channels + carry (feature key `interact_framework`, GDD system 8, block A)

Missing evidence to satisfy: UGSInteractableComponent, UGSInteractionComponent, UGSGA_Interact, UGSCarryComponent

Stubbed evidence to flesh out: none

## Design constraints (from the GDD)

- Hold-E channel framework; the slice verbs are loot / takedown / foul-well / extract, plus a carry state (GDD §4). Verbs are data (gameplay tags), not subclasses.
- Channel: configurable duration; progress 0..1 exposed for the HUD channel bar.
- Abort rules (stealth spec): taking damage aborts; releasing the key aborts; leaving range or breaking facing aborts. Interruptible always.
- Eligibility: range + facing cone from the interactor; interactable advertises verb tag, channel seconds, and whether it is currently available.
- On complete: interactable fires its effect (delegate + BlueprintNativeEvent), e.g. loot grants pouch, extract banks — those systems hook in later; the framework only owns the channel lifecycle.
- GAS integration: UGSGA_Interact activates on the Interact input, applies a State.Interacting tag while channeling, and is cancelled by damage (listen for the existing damage flow via attribute change or a gameplay event).
- Carry: UGSCarryComponent owns carried-object state (slows movement, blocks attack abilities via State.Carrying tag); pick-up/put-down route through the same channel.

## Existing code the new files must sit beside (verbatim headers)

### Source/GoblinSiege/Characters/GSCharacterBase.h
```cpp
// Shared base for every goblin, human, elf, and dwarf pawn. Owns the AbilitySystemComponent and
// AttributeSet so damage, armor, and status effects (root/stun/burn) work identically across
// player-controlled goblins and AI-controlled defenders - only the granted abilities and data
// assets differ per subclass. See design doc Â§10 "Road to Unreal" table.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GSCharacterBase.generated.h"

class UAbilitySystemComponent;
class UGSAttributeSetBase;
class UGameplayEffect;
class UAnimMontage;
struct FOnAttributeChangeData;

/** New, Max, Delta. Delta is negative for damage. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGSOnHealthChanged, float, NewHealth, float, MaxHealth, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnDied);

UCLASS(Abstract)
class GOBLINSIEGE_API AGSCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AGSCharacterBase();

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystemComponent; }
	UGSAttributeSetBase* GetAttributeSetBase() const { return AttributeSetBase; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Combat")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Combat")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Combat")
	bool IsAlive() const { return !bIsDead; }

	/** Applied on respawn: sets Health to RespawnHealthFraction * MaxHealth and grants brief i-frames. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Combat")
	virtual void ApplyRespawnState(float HealthFraction, float InvulnerabilitySeconds);

	/** Per-archetype/per-weapon turn-rate identity (Brute turns like a barge, Slasher/Scout turns
	 *  sharp - design doc "Turn rate"). Pushes the value into CharacterMovementComponent::RotationRate
	 *  so bOrientRotationToMovement-driven turning actually uses it. Called by UGSWeaponComponent on
	 *  equip so a weapon's identity applies itself to its wearer (tech doc Â§16). */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Movement")
	void SetTurnRateRadPerSec(float NewTurnRateRadPerSec);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Movement")
	float GetTurnRateRadPerSec() const { return TurnRateRadPerSec; }

protected:
	virtual void BeginPlay() override;

	/** Grants the base GameplayEffect (Health/MaxHealth/Armor/MoveSpeed init values) from data. Called
	 *  by subclasses after their race/weapon data asset is known. */
	void InitializeAttributesFromEffect(TSubclassOf<UGameplayEffect> InitEffectClass, float Level = 1.f);

	/** Bound to the Health attribute's OnAttributeChanged delegate in BeginPlay. */
	virtual void HandleHealthChanged(const FOnAttributeChangeData& Data);

public:
	/** Broadcast on every Health change, damage or heal. Exists because the attribute delegate GAS
	 *  gives us is non-dynamic and therefore invisible to Blueprint and UMG - this is the version a
	 *  health bar can actually bind to. Delta is negative for damage. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Combat")
	FGSOnHealthChanged OnHealthChanged;

	/** Broadcast once, when this character dies. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Combat")
	FGSOnDied OnDied;

	/** Play a flinch. Direction is the world-space vector from this character to whatever hit them;
	 *  pass zero if unknown and the front reaction is used. Safe to call every frame - it self-gates
	 *  on the cooldown and on State.HitReact. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Combat")
	void PlayHitReact(const FVector& FromDirection);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Combat")
	bool IsBlocking() const;

protected:

	/** Called once when Health first reaches 0. Notifies GSGameMode::HandleGoblinDeath. */
	virtual void HandleDeath();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Abilities")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Abilities")
	TObjectPtr<UGSAttributeSetBase> AttributeSetBase;

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

```

### Source/GoblinSiege/Characters/GSPlayerCharacter.h
```cpp
// The player goblin - this slice's Scout kit (daggers <-> bow, live-swap). Class/weapon identity
// lives on UGSWeaponComponent (equip/swap is a data operation); torch toss and dodge roll are
// granted at the character level - universal racial verbs (design doc Â§4). Camera/rotation/crouch
// here implement tech doc Â§16 (third-person camera & control) and the design doc Â§7 stealth
// foundation's crouch toggle.
#pragma once

#include "CoreMinimal.h"
#include "Characters/GSCharacterBase.h"
#include "InputActionValue.h"
#include "GSPlayerCharacter.generated.h"

class UGSWeaponComponent;
class UGSTargetingComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UGameplayAbility;

/** 0..1, reaching 1 at HeavyHoldSeconds. Broadcast every frame while the attack button is held. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnHeavyChargeChanged, float, ChargeAlpha);

UCLASS()
class GOBLINSIEGE_API AGSPlayerCharacter : public AGSCharacterBase
{
	GENERATED_BODY()

public:
	AGSPlayerCharacter();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Weapon")
	UGSWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Targeting")
	UGSTargetingComponent* GetTargetingComponent() const { return TargetingComponent; }

	/** World-space direction the dodge roll should launch toward: the last held movement input
	 *  resolved against camera yaw, or forward if the player dodges from a standstill. Used by
	 *  UGSGA_DodgeRoll so the roll reads as "where I was going," not always forward. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Movement")
	FVector GetDodgeDirection() const;

protected:
	virtual void BeginPlay() override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	virtual void Tick(float DeltaSeconds) override;

	void Input_Dodge(const FInputActionValue& Value);

	/** Light and heavy share one button (2026-08-03 ruling: tap for light, hold 1.5s for heavy).
	 *  Pressed starts the charge timer; Released fires the light UNLESS the charge already
	 *  matured into a heavy. The light therefore resolves on RELEASE, which is the unavoidable
	 *  cost of putting two attacks on one key - a press cannot know yet whether it is a tap. */
	void Input_AttackPressed(const FInputActionValue& Value);
	void Input_AttackReleased(const FInputActionValue& Value);

	/** Fired by the charge timer at HeavyHoldSeconds, while the button is still down. */
	void TriggerHeavyAttack();

	void Input_Attack(const FInputActionValue& Value);
	void Input_HeavyAttack(const FInputActionValue& Value);
	void Input_BlockStart(const FInputActionValue& Value);
	void Input_BlockStop(const FInputActionValue& Value);

	/** Guard break (X). Low damage on its own - the payoff is the opening it makes. */
	void Input_GuardBreak(const FInputActionValue& Value);

	/** Torch is now press-to-aim, release-to-throw. Started begins the aim arc; Completed and
	 *  Canceled both throw, so letting go anywhere - including alt-tabbing - resolves the throw
	 *  rather than leaving the goblin permanently aiming. */
	void Input_ThrowTorchStart(const FInputActionValue& Value);
	void Input_ThrowTorchRelease(const FInputActionValue& Value);

	/** Draws the predicted torch arc. Reads speed and gravity off the projectile CDO rather than
	 *  duplicating them, so the line the player aims with is the line the torch actually flies. */
	void DrawTorchAimArc();

	void Input_ThrowTorch(const FInputActionValue& Value);
	void Input_SwapWeaponMode(const FInputActionValue& Value);
	void Input_AimStart(const FInputActionValue& Value);
	void Input_AimStop(const FInputActionValue& Value);
	void Input_ToggleCrouch(const FInputActionValue& Value);

	UFUNCTION()
	void HandleWeaponModeChanged(bool bRangedMode);

	/** Switches between movement-facing (default) and aim-facing (holding Aim, or a ranged weapon
	 *  mode equipped) rotation, per tech doc Â§16: "the character faces movement, and faces the aim
	 *  while attacking or aiming." Also mirrors the result onto the State.Aiming gameplay tag so
	 *  animation/combat systems can query it without reaching into player-only state. */
	void UpdateRotationMode();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Weapon")
	TObjectPtr<UGSWeaponComponent> WeaponComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Targeting")
	TObjectPtr<UGSTargetingComponent> TargetingComponent;

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> DodgeAction;

	/** Light attack. Bound to Started, not Triggered - a held button should not machine-gun the
	 *  swing; the ability's own recovery window is what gates the rate. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> AttackAction;

	/** Optional dedicated heavy key. The primary route is holding the light-attack button; this
	 *  stays so a controller face button or a rebind can drive the heavy directly. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> HeavyAttackAction;

	/** How long the attack button must be held before it becomes a heavy. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input", meta = (ClampMin = "0.2"))
	float HeavyHoldSeconds = 1.5f;

	/** Hold to guard. Bound to Started and Completed/Canceled - the guard is up exactly as long as
	 *  the button is down. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> BlockAction;

	/** Guard break - X. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> GuardBreakAction;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> ThrowTorchAction;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> SwapWeaponModeAction;

	/** Hold to aim - forces aim-facing rotation while held (tech doc Â§16). */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> AimAction;

	/** The stealth stance toggle - universal kit (design doc Â§7). */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> CrouchAction;

	/** Universal torch toss (racial trait) - granted in BeginPlay regardless of weapon kit. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Abilities")
	TSubclassOf<UGameplayAbility> TorchTossAbilityClass;

	/** Light melee swing. Defaults to UGSGA_SwordLight in the constructor so a fresh character
	 *  Blueprint can swing without anyone remembering to fill this in - the same reasoning that
	 *  put C++ defaults on GSTorchProjectile's FireVolumeClass. Per-weapon ability grants replace
	 *  this once DA_Weapon_* assets carry their own GrantedAbilities. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Abilities")
	TSubclassOf<UGameplayAbility> SwordLightAbilityClass;

	/** Heavy swing. Shares UGSGA_SwordLight's staged-swing machinery - a heavy is just a chain of
	 *  one slower, harder stage - so it is a second Blueprint child rather than a second class. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Abilities")
	TSubclassOf<UGameplayAbility> SwordHeavyAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Abilities")
	TSubclassOf<UGameplayAbility> BlockAbilityClass;

	/** Guard break. Another UGSGA_SwordLight Blueprint child - one stage, low damage, with
	 *  bBreaksGuard set. Not C++-defaulted, for the same reason as the heavy: a default would
	 *  silently hand it the light's stage array and it would behave like a second light attack. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Abilities")
	TSubclassOf<UGameplayAbility> GuardBreakAbilityClass;

	// ---- torch aiming (2026-08-03) --------------------------------------------------------
	/** Draw the predicted arc while the throw button is held. Off makes the torch an instant
	 *  press-to-throw again, which is the pre-aiming behaviour. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Torch")
	bool bTorchAimEnabled = true;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Torch", meta = (ClampMin = "0.2"))
	float TorchAimMaxSimSeconds = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Torch")
	FLinearColor TorchAimArcColour = FLinearColor(1.f, 0.45f, 0.1f, 1.f);

	/** True between the throw button going down and coming back up. */
	UPROPERTY(BlueprintReadOnly, Category = "GoblinSiege|Torch")
	bool bAimingTorch = false;

private:
	FTimerHandle HeavyChargeTimer;
	float AttackPressedTime = -1.f;
	bool bAttackHeld = false;
	bool bHeavyFiredThisHold = false;

public:
	/** 0..1 while the attack button is held, reaching 1 at HeavyHoldSeconds. Broadcast so a HUD
	 *  can draw a charge ring - without some feedback, a 1.5s threshold is pure guesswork for the
	 *  player, and "I held it and got a light attack" is the complaint that follows. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Combat")
	FGSOnHeavyChargeChanged OnHeavyChargeChanged;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Combat")
	float GetHeavyChargeAlpha() const;

protected:

	/** Universal dodge roll (racial trait, design doc Â§7) - set to UGSGA_DodgeRoll in the character
	 *  Blueprint defaults. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Abilities")
	TSubclassOf<UGameplayAbility> DodgeAbilityClass;

	/** Crouch-walk speed as a fraction of the character's normal walk speed (design doc Â§7: crouch
	 *  "slows you to a creep"). Applied to MaxWalkSpeedCrouched on OnStartCrouch against
	 *  BaseWalkSpeed, which is cached in BeginPlay. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Movement")
	float CrouchSpeedMultiplier = 0.5f;

	/** True while the Aim input is held; combines with ranged-weapon-mode in UpdateRotationMode to
	 *  decide movement-facing vs. aim-facing. */
	bool bIsAiming = false;

	/** MaxWalkSpeed captured in BeginPlay, before any crouch adjustment - the baseline
	 *  CrouchSpeedMultiplier scales from. */
	float BaseWalkSpeed = 600.f;

	/** Last non-zero 2D move input, used by GetDodgeDirection() so a dodge reads as "where I was
	 *  heading" rather than always forward. */
	FVector2D LastMoveInput = FVector2D::ZeroVector;
};

```

### Source/GoblinSiege/Combat/GSGameplayTags.h
```cpp
// Native gameplay tags for the damage pipeline (design doc Â§5, race matchup matrix columns).
// One Damage.* tag per weapon-kit damage type; the tag doubles as the SetByCaller key on the
// attacking GameplayEffectSpec, so GSDamageExecCalculation needs no separate lookup table.
// Reconstructed 2026-07-19 to match GSDamageExecCalculation.cpp's GSTags:: usage exactly.
// Extended 2026-07-24 (player character controller pass): State.Crouching/Dodging/Aiming for the
// universal kit's stealth stance, dodge roll, and aim-facing rotation mode (tech doc Â§16, design
// doc Â§7).
#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace GSTags
{
	// Damage types - one per weapon kit / hazard, matching the race-design-*.md matrix rows.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Dagger);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Bow);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Greatclub);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_ShadowMagic);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_BloodMagic);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Blast);

	/** Modifier tag: added alongside a Damage.* tag when the hit should skip flat armor
	 *  (Shadow/Blood magic per race-design-dwarves.md "armor beats the physical, not the arcane";
	 *  Blast skips armor unconditionally without needing this tag). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_IgnoresArmor);

	// Character state tags consumed by abilities/effects.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invulnerable);

	/** Stealth stance active - crouch toggle, universal kit (design doc Â§7). Queried by the future
	 *  Stealth & Interaction Agent's detection-radius math; set/cleared by AGSPlayerCharacter's
	 *  OnStartCrouch/OnEndCrouch. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Crouching);

	/** Dodge roll in flight - i-frames + committed movement lock (design doc Â§7: 0.22s i-frames,
	 *  committed recovery). Owned by UGSGA_DodgeRoll's ActivationOwnedTags; GAS adds/removes it
	 *  automatically for the ability's lifetime. AGSPlayerCharacter::Input_Move checks it to block
	 *  movement input during the roll; future attack abilities should add it to their own
	 *  ActivationBlockedTags. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dodging);

	/** Facing the camera/aim direction instead of the movement direction (tech doc Â§16 - "the
	 *  character faces movement, and faces the aim while attacking or aiming"). Set/cleared by
	 *  AGSPlayerCharacter::UpdateRotationMode so animation Blueprints and future combat systems can
	 *  query it without reaching into player-only state. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Aiming);

	/** Guard up. Read by UGSDamageExecCalculation, which mitigates frontal hits only - a block
	 *  that protects your back is not a block. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Blocking);

	/** Present only while a hit-react montage is playing. Exists so a flurry cannot restart the
	 *  flinch every frame, which reads as a seizure rather than a stagger. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_HitReact);

	/** Guard kicked open. Held for the stagger window, during which UGSGA_Block refuses to
	 *  activate - that refusal is the whole point: an interrupt that lets you immediately re-block
	 *  punishes nothing, so the opening has to persist for long enough to be exploited. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_GuardBroken);

	// ------------------------------------------------------------------ burn objective types
	/**
	 * The three burn-objective TYPE tags the burn-types spec Â§5 has owed since it was written,
	 * added 2026-07-31 for Q-37 (the state half of Q-32's Required -> Optional -> Complete ruling).
	 *
	 * Every placed burn carrier declares one of these through
	 * AGSBurnObjectiveBase::ObjectiveTypeTag, and it is the LOOKUP KEY for the win condition:
	 * ruled 2026-07-31, the raid is won by burning one of each TYPE, so the moment the first
	 * carrier of a type completes, the remaining carriers of that same type demote to Optional.
	 * "Same type" is exactly "same tag" - which is why this is a tag and not the
	 * EGSBurnObjectiveType enum it parallels. The enum is a C++ switch value that needs a recompile
	 * to extend; a fourth burn type (a granary, a tannery) should join the win condition by being
	 * placed and tagged in a level.
	 *
	 * Objective.Burn.* rather than the existing Damage.* / State.* roots because these describe a
	 * MISSION-LAYER object, not a hit or a character state, and grouping them under a shared
	 * Objective.Burn parent is what lets a future query match "any burn objective" with one
	 * MatchesTag call.
	 *
	 * The market is never demoted: only one exists, so it has no siblings to be demoted by. That
	 * falls out of the rule and needs no special case in the tag set.
	 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Objective_Burn_Mill);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Objective_Burn_Field);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Objective_Burn_Market);
}

```

### Source/GoblinSiege/Weapons/Abilities/GSGA_DodgeRoll.h
```cpp
// Universal kit: every goblin can dodge-roll regardless of weapon kit (design doc Â§4/Â§7 - 0.22s
// i-frames, committed recovery). Granted at the character level alongside torch toss, not the
// weapon level - dodge is a racial verb, not a class one (mirrors GSGA_TorchToss's pattern).
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GSGA_DodgeRoll.generated.h"

UCLASS()
class GOBLINSIEGE_API UGSGA_DodgeRoll : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGSGA_DodgeRoll();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UFUNCTION()
	void OnDodgeFinished();

	/** Roll speed in cm/s, applied as a single launch impulse toward the dodge direction (design
	 *  doc: "committed recovery" - no steering once you commit). A root-motion dodge montage can
	 *  replace this launch once the Scout's animation set exists (character-design-log open item);
	 *  this is a functional placeholder, not a final-feel implementation. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Dodge")
	float DodgeSpeed = 900.f;

	/** i-frame + roll-motion window (design doc: 0.22s i-frames). ActivationOwnedTags (constructor)
	 *  keeps State.Dodging active for exactly this long, which is what blocks movement input
	 *  (AGSPlayerCharacter::Input_Move) and, once other abilities declare ActivationBlockedTags
	 *  against it, blocks attack cancels during the roll. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Dodge")
	float DodgeDurationSeconds = 0.22f;

	/** A short duration GameplayEffect granting State.Invulnerable for DodgeDurationSeconds - author
	 *  as a data asset (same pattern as GSCharacterBase::ApplyRespawnState's invulnerability note).
	 *  Left null-safe: the roll still moves and locks input without it, it just won't grant i-frames
	 *  until Michael authors the effect in-editor. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Dodge")
	TSubclassOf<UGameplayEffect> IFrameEffectClass;
};

```

### Source/GoblinSiege/Attributes/GSAttributeSetBase.h
```cpp
// Shared attribute set for every character - goblins and the three defender races alike, so the
// damage pipeline (GSDamageExecCalculation, Combat module) only has to know one attribute layout.
// Per-race/per-archetype differences (HP pools, armor, speed) come from data (GSRaceDataAsset /
// GSWeaponDataAsset) applied as GameplayEffects at spawn, not from separate attribute set classes.
#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GSAttributeSetBase.generated.h"

// Note: GAMEPLAYATTRIBUTE_PROPERTY_GETTER / _VALUE_GETTER / _VALUE_SETTER / _VALUE_INITTER are
// already defined by the engine in AttributeSet.h (included above) - do not redefine them here
// (was causing C4005 macro-redefinition warnings).

/**
 * Base attribute set. Armor is a flat mitigation value consumed by GSDamageExecCalculation,
 * which also applies the race/weapon matchup multiplier matrix (race-design-*.md) before armor.
 * IncomingDamage is a "meta" attribute: it is never read directly, only written to by a
 * GameplayEffect execution, then immediately drained from Health in PostGameplayEffectExecute.
 */
UCLASS()
class GOBLINSIEGE_API UGSAttributeSetBase : public UAttributeSet
{
	GENERATED_BODY()

public:
	UGSAttributeSetBase();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes")
	FGameplayAttributeData Health;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UGSAttributeSetBase, Health)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(Health)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(Health)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes")
	FGameplayAttributeData MaxHealth;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UGSAttributeSetBase, MaxHealth)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(MaxHealth)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(MaxHealth)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(MaxHealth)

	/** Flat mitigation, e.g. Knight = 6 (design doc Â§5). Explosive/blast damage types skip this entirely. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Armor, Category = "Attributes")
	FGameplayAttributeData Armor;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UGSAttributeSetBase, Armor)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(Armor)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(Armor)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(Armor)

	/** Multiplier applied to base movement speed - lets abilities/effects (roots, slows, buffs) stack cleanly. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeedMultiplier, Category = "Attributes")
	FGameplayAttributeData MoveSpeedMultiplier;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UGSAttributeSetBase, MoveSpeedMultiplier)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(MoveSpeedMultiplier)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(MoveSpeedMultiplier)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(MoveSpeedMultiplier)

	/** Meta attribute: written by damage GameplayEffects, drained into Health, never replicated directly. */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData IncomingDamage;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UGSAttributeSetBase, IncomingDamage)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(IncomingDamage)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(IncomingDamage)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(IncomingDamage)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Armor(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue);
};

```

## Task
Write the missing classes as complete .h/.cpp pairs under Source/GoblinSiege/. Follow the existing include style (module-relative includes as seen in the headers above). List every edit needed to EXISTING files (tags, character wiring, Build.cs) in notes as exact patch instructions — do not emit modified copies of existing files.
```
</details>

**GENERATION FAILED:** no JSON object in model output
# Code Architect — run live-002

*Every decision below was logged before any file was written anywhere.*


## WHAT IT RECEIVED

- GDD: `D:\goblinRaid\GoblinSiege 5.8\Tools\CodeArchitect\docs\goblin-siege-gdd.md` — parsed **21** systems from §12.1, 8 blocks from §12.2

## WHAT IT PERCEIVED

- Source files scanned: **94**; folders: AI, Alarm, Attributes, Characters, Combat, Core, Destruction, Horde, Missions, Progression, UI, Weapons
- UCLASS/USTRUCT declarations: **57** (38 wired, 19 stubbed/header-only)
- Content data-layer sweep: {"BP_GS": 4, "DA_": 1, "GA_": 4, "IA_": 13, "IMC_": 1, "L_": 8}

## WHAT IT SCORED

| # | feature | block | utility | eligible | rationale |
|---|---------|-------|---------|----------|-----------|
| 1 | Interact framework â€” hold-E channels + carry | A | 10.0 | yes | impact 5; urgency 5; unblocks 5 open feature(s) [stealth_five, runic_site, loot_couriers, score_system, civilians_livestock]; effort 3; block A; completeness 0% |
| 2 | Death & hit-reaction clips retargeted ('nothing can die on screen') | B | 6.0 | EDITOR | impact 4; urgency 4; unblocks 0 open feature(s) [-]; effort 2; block B; completeness 0%; REQUIRES_EDITOR -> never headless-generatable; route to supervised session |
| 3 | Someone to fight â€” human race data, BT_Militia, enemy attack path | B | 5.75 | yes | impact 5; urgency 4; unblocks 3 open feature(s) [horde, stealth_five, patrol_director]; effort 4; block B; completeness 25% |
| 4 | Lives / respawn on PlayerState | E | 4.0 | yes | impact 3; urgency 2; unblocks 0 open feature(s) [-]; effort 2; block E; completeness 0% |
| 5 | Runic site â€” spawn/respawn, objective-gated portal, staging, 90s collapse | E | 4.0 | blocked | impact 5; urgency 2; unblocks 0 open feature(s) [-]; effort 3; block E; completeness 0%; BLOCKED by ['interact_framework'] |
| 6 | Score system â€” deeds/loot two-kind tally + end screen | G | 4.0 | blocked | impact 5; urgency 2; unblocks 0 open feature(s) [-]; effort 3; block G; completeness 0%; BLOCKED by ['interact_framework'] |
| 7 | Horn & horde (subsystem, pool, BT, point command) | D | 3.2 | blocked | impact 5; urgency 3; unblocks 1 open feature(s) [loot_couriers]; effort 5; block D; completeness 0%; BLOCKED by ['militia_enemy'] |
| 8 | The stealth five (noise, crouch-detect, takedown, corpse-suspicion, coin toss) | F | 3.0 | blocked | impact 5; urgency 2; unblocks 0 open feature(s) [-]; effort 4; block F; completeness 0%; BLOCKED by ['interact_framework', 'militia_enemy'] |
| 9 | Gore/gib system (intensity scalar, feather-poof) | G | 2.5 | blocked | impact 2; urgency 1; unblocks 0 open feature(s) [-]; effort 2; block G; completeness 0%; BLOCKED by ['militia_enemy'] |
| 10 | Barks + Overlord whispers (runtime side) | H | 2.5 | yes | impact 2; urgency 1; unblocks 0 open feature(s) [-]; effort 2; block H; completeness 0% |
| 11 | Patrol director â€” 5-7 min cadence + castle reinforcements | F | 2.33 | blocked | impact 3; urgency 1; unblocks 0 open feature(s) [-]; effort 3; block F; completeness 0%; BLOCKED by ['militia_enemy'] |
| 12 | Loot couriers â€” sacks + livestock cargo, point-to-courier | G | 2.25 | blocked | impact 4; urgency 1; unblocks 0 open feature(s) [-]; effort 4; block G; completeness 50%; BLOCKED by ['interact_framework', 'horde'] |
| 13 | Civilians + livestock (routines, disbelief, brigade, flee) | G | 1.75 | blocked | impact 3; urgency 1; unblocks 0 open feature(s) [-]; effort 4; block G; completeness 33%; BLOCKED by ['militia_enemy', 'interact_framework'] |

## WHAT IT PICKED

**Interact framework â€” hold-E channels + carry** — utility 10.0. impact 5; urgency 5; unblocks 5 open feature(s) [stealth_five, runic_site, loot_couriers, score_system, civilians_livestock]; effort 3; block A; completeness 0%

> NOTE: provider 'auto' resolved to 'claude-cli' — claude-cli bills the Max subscription, not API credits.

## WHAT IT ISSUED — generate:interact_framework

*model:* `fixture:interact_framework`

<details><summary>system prompt</summary>

```
You are the Code Architect for Goblin Siege, an Unreal Engine 5.8 C++ project
(module GoblinSiege, API macro GOBLINSIEGE_API). You write production C++ for review by the
project owner. House rules, non-negotiable:
- Class prefix GS (AGSFoo actors, UGSFoo components/objects). Canonical player class name is SCOUT.
- GAS is the combat backbone: abilities derive from UGameplayAbility, attributes live on
  UGSAttributeSetBase, native gameplay tags are declared in GSGameplayTags.h/.cpp.
- Never rewrite existing files wholesale. If an existing file needs an addition (a tag, an include,
  a component on the character), emit it in "notes" as an exact, minimal patch instruction instead.
- Multiplayer posture: single-player slice, co-op-ready — replicate the cheap root state
  (bIsX flags) but do not build prediction.
- Comments explain design intent tersely; no boilerplate comment noise.
Return ONLY a JSON object: {"files": {"Source/GoblinSiege/<path>": "<content>", ...},
"notes": "<markdown notes incl. exact patch instructions for existing files>",
"decisions": ["<why choices were made>", ...]}
```
</details>

<details><summary>user prompt</summary>

```
## Feature to build
Interact framework â€” hold-E channels + carry (feature key `interact_framework`, GDD system 8, block A)

Missing evidence to satisfy: UGSInteractableComponent, UGSInteractionComponent, UGSGA_Interact, UGSCarryComponent

Stubbed evidence to flesh out: none

## Design constraints (from the GDD)

- Hold-E channel framework; the slice verbs are loot / takedown / foul-well / extract, plus a carry state (GDD §4). Verbs are data (gameplay tags), not subclasses.
- Channel: configurable duration; progress 0..1 exposed for the HUD channel bar.
- Abort rules (stealth spec): taking damage aborts; releasing the key aborts; leaving range or breaking facing aborts. Interruptible always.
- Eligibility: range + facing cone from the interactor; interactable advertises verb tag, channel seconds, and whether it is currently available.
- On complete: interactable fires its effect (delegate + BlueprintNativeEvent), e.g. loot grants pouch, extract banks — those systems hook in later; the framework only owns the channel lifecycle.
- GAS integration: UGSGA_Interact activates on the Interact input, applies a State.Interacting tag while channeling, and is cancelled by damage (listen for the existing damage flow via attribute change or a gameplay event).
- Carry: UGSCarryComponent owns carried-object state (slows movement, blocks attack abilities via State.Carrying tag); pick-up/put-down route through the same channel.

## Existing code the new files must sit beside (verbatim headers)

### Source/GoblinSiege/Characters/GSCharacterBase.h
```cpp
// Shared base for every goblin, human, elf, and dwarf pawn. Owns the AbilitySystemComponent and
// AttributeSet so damage, armor, and status effects (root/stun/burn) work identically across
// player-controlled goblins and AI-controlled defenders - only the granted abilities and data
// assets differ per subclass. See design doc Â§10 "Road to Unreal" table.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GSCharacterBase.generated.h"

class UAbilitySystemComponent;
class UGSAttributeSetBase;
class UGameplayEffect;
class UAnimMontage;
struct FOnAttributeChangeData;

/** New, Max, Delta. Delta is negative for damage. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGSOnHealthChanged, float, NewHealth, float, MaxHealth, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnDied);

UCLASS(Abstract)
class GOBLINSIEGE_API AGSCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AGSCharacterBase();

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystemComponent; }
	UGSAttributeSetBase* GetAttributeSetBase() const { return AttributeSetBase; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Combat")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Combat")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Combat")
	bool IsAlive() const { return !bIsDead; }

	/** Applied on respawn: sets Health to RespawnHealthFraction * MaxHealth and grants brief i-frames. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Combat")
	virtual void ApplyRespawnState(float HealthFraction, float InvulnerabilitySeconds);

	/** Per-archetype/per-weapon turn-rate identity (Brute turns like a barge, Slasher/Scout turns
	 *  sharp - design doc "Turn rate"). Pushes the value into CharacterMovementComponent::RotationRate
	 *  so bOrientRotationToMovement-driven turning actually uses it. Called by UGSWeaponComponent on
	 *  equip so a weapon's identity applies itself to its wearer (tech doc Â§16). */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Movement")
	void SetTurnRateRadPerSec(float NewTurnRateRadPerSec);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Movement")
	float GetTurnRateRadPerSec() const { return TurnRateRadPerSec; }

protected:
	virtual void BeginPlay() override;

	/** Grants the base GameplayEffect (Health/MaxHealth/Armor/MoveSpeed init values) from data. Called
	 *  by subclasses after their race/weapon data asset is known. */
	void InitializeAttributesFromEffect(TSubclassOf<UGameplayEffect> InitEffectClass, float Level = 1.f);

	/** Bound to the Health attribute's OnAttributeChanged delegate in BeginPlay. */
	virtual void HandleHealthChanged(const FOnAttributeChangeData& Data);

public:
	/** Broadcast on every Health change, damage or heal. Exists because the attribute delegate GAS
	 *  gives us is non-dynamic and therefore invisible to Blueprint and UMG - this is the version a
	 *  health bar can actually bind to. Delta is negative for damage. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Combat")
	FGSOnHealthChanged OnHealthChanged;

	/** Broadcast once, when this character dies. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Combat")
	FGSOnDied OnDied;

	/** Play a flinch. Direction is the world-space vector from this character to whatever hit them;
	 *  pass zero if unknown and the front reaction is used. Safe to call every frame - it self-gates
	 *  on the cooldown and on State.HitReact. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Combat")
	void PlayHitReact(const FVector& FromDirection);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Combat")
	bool IsBlocking() const;

protected:

	/** Called once when Health first reaches 0. Notifies GSGameMode::HandleGoblinDeath. */
	virtual void HandleDeath();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Abilities")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Abilities")
	TObjectPtr<UGSAttributeSetBase> AttributeSetBase;

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

```

### Source/GoblinSiege/Characters/GSPlayerCharacter.h
```cpp
// The player goblin - this slice's Scout kit (daggers <-> bow, live-swap). Class/weapon identity
// lives on UGSWeaponComponent (equip/swap is a data operation); torch toss and dodge roll are
// granted at the character level - universal racial verbs (design doc Â§4). Camera/rotation/crouch
// here implement tech doc Â§16 (third-person camera & control) and the design doc Â§7 stealth
// foundation's crouch toggle.
#pragma once

#include "CoreMinimal.h"
#include "Characters/GSCharacterBase.h"
#include "InputActionValue.h"
#include "GSPlayerCharacter.generated.h"

class UGSWeaponComponent;
class UGSTargetingComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UGameplayAbility;

/** 0..1, reaching 1 at HeavyHoldSeconds. Broadcast every frame while the attack button is held. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnHeavyChargeChanged, float, ChargeAlpha);

UCLASS()
class GOBLINSIEGE_API AGSPlayerCharacter : public AGSCharacterBase
{
	GENERATED_BODY()

public:
	AGSPlayerCharacter();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Weapon")
	UGSWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Targeting")
	UGSTargetingComponent* GetTargetingComponent() const { return TargetingComponent; }

	/** World-space direction the dodge roll should launch toward: the last held movement input
	 *  resolved against camera yaw, or forward if the player dodges from a standstill. Used by
	 *  UGSGA_DodgeRoll so the roll reads as "where I was going," not always forward. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Movement")
	FVector GetDodgeDirection() const;

protected:
	virtual void BeginPlay() override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	virtual void Tick(float DeltaSeconds) override;

	void Input_Dodge(const FInputActionValue& Value);

	/** Light and heavy share one button (2026-08-03 ruling: tap for light, hold 1.5s for heavy).
	 *  Pressed starts the charge timer; Released fires the light UNLESS the charge already
	 *  matured into a heavy. The light therefore resolves on RELEASE, which is the unavoidable
	 *  cost of putting two attacks on one key - a press cannot know yet whether it is a tap. */
	void Input_AttackPressed(const FInputActionValue& Value);
	void Input_AttackReleased(const FInputActionValue& Value);

	/** Fired by the charge timer at HeavyHoldSeconds, while the button is still down. */
	void TriggerHeavyAttack();

	void Input_Attack(const FInputActionValue& Value);
	void Input_HeavyAttack(const FInputActionValue& Value);
	void Input_BlockStart(const FInputActionValue& Value);
	void Input_BlockStop(const FInputActionValue& Value);

	/** Guard break (X). Low damage on its own - the payoff is the opening it makes. */
	void Input_GuardBreak(const FInputActionValue& Value);

	/** Torch is now press-to-aim, release-to-throw. Started begins the aim arc; Completed and
	 *  Canceled both throw, so letting go anywhere - including alt-tabbing - resolves the throw
	 *  rather than leaving the goblin permanently aiming. */
	void Input_ThrowTorchStart(const FInputActionValue& Value);
	void Input_ThrowTorchRelease(const FInputActionValue& Value);

	/** Draws the predicted torch arc. Reads speed and gravity off the projectile CDO rather than
	 *  duplicating them, so the line the player aims with is the line the torch actually flies. */
	void DrawTorchAimArc();

	void Input_ThrowTorch(const FInputActionValue& Value);
	void Input_SwapWeaponMode(const FInputActionValue& Value);
	void Input_AimStart(const FInputActionValue& Value);
	void Input_AimStop(const FInputActionValue& Value);
	void Input_ToggleCrouch(const FInputActionValue& Value);

	UFUNCTION()
	void HandleWeaponModeChanged(bool bRangedMode);

	/** Switches between movement-facing (default) and aim-facing (holding Aim, or a ranged weapon
	 *  mode equipped) rotation, per tech doc Â§16: "the character faces movement, and faces the aim
	 *  while attacking or aiming." Also mirrors the result onto the State.Aiming gameplay tag so
	 *  animation/combat systems can query it without reaching into player-only state. */
	void UpdateRotationMode();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Weapon")
	TObjectPtr<UGSWeaponComponent> WeaponComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Targeting")
	TObjectPtr<UGSTargetingComponent> TargetingComponent;

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> DodgeAction;

	/** Light attack. Bound to Started, not Triggered - a held button should not machine-gun the
	 *  swing; the ability's own recovery window is what gates the rate. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> AttackAction;

	/** Optional dedicated heavy key. The primary route is holding the light-attack button; this
	 *  stays so a controller face button or a rebind can drive the heavy directly. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> HeavyAttackAction;

	/** How long the attack button must be held before it becomes a heavy. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input", meta = (ClampMin = "0.2"))
	float HeavyHoldSeconds = 1.5f;

	/** Hold to guard. Bound to Started and Completed/Canceled - the guard is up exactly as long as
	 *  the button is down. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> BlockAction;

	/** Guard break - X. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> GuardBreakAction;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> ThrowTorchAction;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> SwapWeaponModeAction;

	/** Hold to aim - forces aim-facing rotation while held (tech doc Â§16). */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> AimAction;

	/** The stealth stance toggle - universal kit (design doc Â§7). */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> CrouchAction;

	/** Universal torch toss (racial trait) - granted in BeginPlay regardless of weapon kit. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Abilities")
	TSubclassOf<UGameplayAbility> TorchTossAbilityClass;

	/** Light melee swing. Defaults to UGSGA_SwordLight in the constructor so a fresh character
	 *  Blueprint can swing without anyone remembering to fill this in - the same reasoning that
	 *  put C++ defaults on GSTorchProjectile's FireVolumeClass. Per-weapon ability grants replace
	 *  this once DA_Weapon_* assets carry their own GrantedAbilities. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Abilities")
	TSubclassOf<UGameplayAbility> SwordLightAbilityClass;

	/** Heavy swing. Shares UGSGA_SwordLight's staged-swing machinery - a heavy is just a chain of
	 *  one slower, harder stage - so it is a second Blueprint child rather than a second class. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Abilities")
	TSubclassOf<UGameplayAbility> SwordHeavyAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Abilities")
	TSubclassOf<UGameplayAbility> BlockAbilityClass;

	/** Guard break. Another UGSGA_SwordLight Blueprint child - one stage, low damage, with
	 *  bBreaksGuard set. Not C++-defaulted, for the same reason as the heavy: a default would
	 *  silently hand it the light's stage array and it would behave like a second light attack. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Abilities")
	TSubclassOf<UGameplayAbility> GuardBreakAbilityClass;

	// ---- torch aiming (2026-08-03) --------------------------------------------------------
	/** Draw the predicted arc while the throw button is held. Off makes the torch an instant
	 *  press-to-throw again, which is the pre-aiming behaviour. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Torch")
	bool bTorchAimEnabled = true;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Torch", meta = (ClampMin = "0.2"))
	float TorchAimMaxSimSeconds = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Torch")
	FLinearColor TorchAimArcColour = FLinearColor(1.f, 0.45f, 0.1f, 1.f);

	/** True between the throw button going down and coming back up. */
	UPROPERTY(BlueprintReadOnly, Category = "GoblinSiege|Torch")
	bool bAimingTorch = false;

private:
	FTimerHandle HeavyChargeTimer;
	float AttackPressedTime = -1.f;
	bool bAttackHeld = false;
	bool bHeavyFiredThisHold = false;

public:
	/** 0..1 while the attack button is held, reaching 1 at HeavyHoldSeconds. Broadcast so a HUD
	 *  can draw a charge ring - without some feedback, a 1.5s threshold is pure guesswork for the
	 *  player, and "I held it and got a light attack" is the complaint that follows. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Combat")
	FGSOnHeavyChargeChanged OnHeavyChargeChanged;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Combat")
	float GetHeavyChargeAlpha() const;

protected:

	/** Universal dodge roll (racial trait, design doc Â§7) - set to UGSGA_DodgeRoll in the character
	 *  Blueprint defaults. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Abilities")
	TSubclassOf<UGameplayAbility> DodgeAbilityClass;

	/** Crouch-walk speed as a fraction of the character's normal walk speed (design doc Â§7: crouch
	 *  "slows you to a creep"). Applied to MaxWalkSpeedCrouched on OnStartCrouch against
	 *  BaseWalkSpeed, which is cached in BeginPlay. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Movement")
	float CrouchSpeedMultiplier = 0.5f;

	/** True while the Aim input is held; combines with ranged-weapon-mode in UpdateRotationMode to
	 *  decide movement-facing vs. aim-facing. */
	bool bIsAiming = false;

	/** MaxWalkSpeed captured in BeginPlay, before any crouch adjustment - the baseline
	 *  CrouchSpeedMultiplier scales from. */
	float BaseWalkSpeed = 600.f;

	/** Last non-zero 2D move input, used by GetDodgeDirection() so a dodge reads as "where I was
	 *  heading" rather than always forward. */
	FVector2D LastMoveInput = FVector2D::ZeroVector;
};

```

### Source/GoblinSiege/Combat/GSGameplayTags.h
```cpp
// Native gameplay tags for the damage pipeline (design doc Â§5, race matchup matrix columns).
// One Damage.* tag per weapon-kit damage type; the tag doubles as the SetByCaller key on the
// attacking GameplayEffectSpec, so GSDamageExecCalculation needs no separate lookup table.
// Reconstructed 2026-07-19 to match GSDamageExecCalculation.cpp's GSTags:: usage exactly.
// Extended 2026-07-24 (player character controller pass): State.Crouching/Dodging/Aiming for the
// universal kit's stealth stance, dodge roll, and aim-facing rotation mode (tech doc Â§16, design
// doc Â§7).
#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace GSTags
{
	// Damage types - one per weapon kit / hazard, matching the race-design-*.md matrix rows.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Dagger);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Bow);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Greatclub);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_ShadowMagic);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_BloodMagic);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Blast);

	/** Modifier tag: added alongside a Damage.* tag when the hit should skip flat armor
	 *  (Shadow/Blood magic per race-design-dwarves.md "armor beats the physical, not the arcane";
	 *  Blast skips armor unconditionally without needing this tag). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_IgnoresArmor);

	// Character state tags consumed by abilities/effects.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invulnerable);

	/** Stealth stance active - crouch toggle, universal kit (design doc Â§7). Queried by the future
	 *  Stealth & Interaction Agent's detection-radius math; set/cleared by AGSPlayerCharacter's
	 *  OnStartCrouch/OnEndCrouch. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Crouching);

	/** Dodge roll in flight - i-frames + committed movement lock (design doc Â§7: 0.22s i-frames,
	 *  committed recovery). Owned by UGSGA_DodgeRoll's ActivationOwnedTags; GAS adds/removes it
	 *  automatically for the ability's lifetime. AGSPlayerCharacter::Input_Move checks it to block
	 *  movement input during the roll; future attack abilities should add it to their own
	 *  ActivationBlockedTags. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dodging);

	/** Facing the camera/aim direction instead of the movement direction (tech doc Â§16 - "the
	 *  character faces movement, and faces the aim while attacking or aiming"). Set/cleared by
	 *  AGSPlayerCharacter::UpdateRotationMode so animation Blueprints and future combat systems can
	 *  query it without reaching into player-only state. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Aiming);

	/** Guard up. Read by UGSDamageExecCalculation, which mitigates frontal hits only - a block
	 *  that protects your back is not a block. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Blocking);

	/** Present only while a hit-react montage is playing. Exists so a flurry cannot restart the
	 *  flinch every frame, which reads as a seizure rather than a stagger. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_HitReact);

	/** Guard kicked open. Held for the stagger window, during which UGSGA_Block refuses to
	 *  activate - that refusal is the whole point: an interrupt that lets you immediately re-block
	 *  punishes nothing, so the opening has to persist for long enough to be exploited. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_GuardBroken);

	// ------------------------------------------------------------------ burn objective types
	/**
	 * The three burn-objective TYPE tags the burn-types spec Â§5 has owed since it was written,
	 * added 2026-07-31 for Q-37 (the state half of Q-32's Required -> Optional -> Complete ruling).
	 *
	 * Every placed burn carrier declares one of these through
	 * AGSBurnObjectiveBase::ObjectiveTypeTag, and it is the LOOKUP KEY for the win condition:
	 * ruled 2026-07-31, the raid is won by burning one of each TYPE, so the moment the first
	 * carrier of a type completes, the remaining carriers of that same type demote to Optional.
	 * "Same type" is exactly "same tag" - which is why this is a tag and not the
	 * EGSBurnObjectiveType enum it parallels. The enum is a C++ switch value that needs a recompile
	 * to extend; a fourth burn type (a granary, a tannery) should join the win condition by being
	 * placed and tagged in a level.
	 *
	 * Objective.Burn.* rather than the existing Damage.* / State.* roots because these describe a
	 * MISSION-LAYER object, not a hit or a character state, and grouping them under a shared
	 * Objective.Burn parent is what lets a future query match "any burn objective" with one
	 * MatchesTag call.
	 *
	 * The market is never demoted: only one exists, so it has no siblings to be demoted by. That
	 * falls out of the rule and needs no special case in the tag set.
	 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Objective_Burn_Mill);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Objective_Burn_Field);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Objective_Burn_Market);
}

```

### Source/GoblinSiege/Weapons/Abilities/GSGA_DodgeRoll.h
```cpp
// Universal kit: every goblin can dodge-roll regardless of weapon kit (design doc Â§4/Â§7 - 0.22s
// i-frames, committed recovery). Granted at the character level alongside torch toss, not the
// weapon level - dodge is a racial verb, not a class one (mirrors GSGA_TorchToss's pattern).
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GSGA_DodgeRoll.generated.h"

UCLASS()
class GOBLINSIEGE_API UGSGA_DodgeRoll : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGSGA_DodgeRoll();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UFUNCTION()
	void OnDodgeFinished();

	/** Roll speed in cm/s, applied as a single launch impulse toward the dodge direction (design
	 *  doc: "committed recovery" - no steering once you commit). A root-motion dodge montage can
	 *  replace this launch once the Scout's animation set exists (character-design-log open item);
	 *  this is a functional placeholder, not a final-feel implementation. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Dodge")
	float DodgeSpeed = 900.f;

	/** i-frame + roll-motion window (design doc: 0.22s i-frames). ActivationOwnedTags (constructor)
	 *  keeps State.Dodging active for exactly this long, which is what blocks movement input
	 *  (AGSPlayerCharacter::Input_Move) and, once other abilities declare ActivationBlockedTags
	 *  against it, blocks attack cancels during the roll. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Dodge")
	float DodgeDurationSeconds = 0.22f;

	/** A short duration GameplayEffect granting State.Invulnerable for DodgeDurationSeconds - author
	 *  as a data asset (same pattern as GSCharacterBase::ApplyRespawnState's invulnerability note).
	 *  Left null-safe: the roll still moves and locks input without it, it just won't grant i-frames
	 *  until Michael authors the effect in-editor. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Dodge")
	TSubclassOf<UGameplayEffect> IFrameEffectClass;
};

```

### Source/GoblinSiege/Attributes/GSAttributeSetBase.h
```cpp
// Shared attribute set for every character - goblins and the three defender races alike, so the
// damage pipeline (GSDamageExecCalculation, Combat module) only has to know one attribute layout.
// Per-race/per-archetype differences (HP pools, armor, speed) come from data (GSRaceDataAsset /
// GSWeaponDataAsset) applied as GameplayEffects at spawn, not from separate attribute set classes.
#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GSAttributeSetBase.generated.h"

// Note: GAMEPLAYATTRIBUTE_PROPERTY_GETTER / _VALUE_GETTER / _VALUE_SETTER / _VALUE_INITTER are
// already defined by the engine in AttributeSet.h (included above) - do not redefine them here
// (was causing C4005 macro-redefinition warnings).

/**
 * Base attribute set. Armor is a flat mitigation value consumed by GSDamageExecCalculation,
 * which also applies the race/weapon matchup multiplier matrix (race-design-*.md) before armor.
 * IncomingDamage is a "meta" attribute: it is never read directly, only written to by a
 * GameplayEffect execution, then immediately drained from Health in PostGameplayEffectExecute.
 */
UCLASS()
class GOBLINSIEGE_API UGSAttributeSetBase : public UAttributeSet
{
	GENERATED_BODY()

public:
	UGSAttributeSetBase();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes")
	FGameplayAttributeData Health;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UGSAttributeSetBase, Health)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(Health)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(Health)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes")
	FGameplayAttributeData MaxHealth;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UGSAttributeSetBase, MaxHealth)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(MaxHealth)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(MaxHealth)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(MaxHealth)

	/** Flat mitigation, e.g. Knight = 6 (design doc Â§5). Explosive/blast damage types skip this entirely. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Armor, Category = "Attributes")
	FGameplayAttributeData Armor;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UGSAttributeSetBase, Armor)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(Armor)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(Armor)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(Armor)

	/** Multiplier applied to base movement speed - lets abilities/effects (roots, slows, buffs) stack cleanly. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeedMultiplier, Category = "Attributes")
	FGameplayAttributeData MoveSpeedMultiplier;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UGSAttributeSetBase, MoveSpeedMultiplier)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(MoveSpeedMultiplier)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(MoveSpeedMultiplier)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(MoveSpeedMultiplier)

	/** Meta attribute: written by damage GameplayEffects, drained into Health, never replicated directly. */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData IncomingDamage;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UGSAttributeSetBase, IncomingDamage)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(IncomingDamage)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(IncomingDamage)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(IncomingDamage)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Armor(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue);
};

```

## Task
Write the missing classes as complete .h/.cpp pairs under Source/GoblinSiege/. Follow the existing include style (module-relative includes as seen in the headers above). List every edit needed to EXISTING files (tags, character wiring, Build.cs) in notes as exact patch instructions — do not emit modified copies of existing files.
```
</details>

**GENERATION FAILED:** Unterminated string starting at: line 20 column 12 (char 44113)
