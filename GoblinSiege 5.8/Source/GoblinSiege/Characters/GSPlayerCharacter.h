// The player goblin - this slice's Scout kit (daggers <-> bow, live-swap). Class/weapon identity
// lives on UGSWeaponComponent (equip/swap is a data operation); torch toss and dodge roll are
// granted at the character level - universal racial verbs (design doc §4). Camera/rotation/crouch
// here implement tech doc §16 (third-person camera & control) and the design doc §7 stealth
// foundation's crouch toggle.
#pragma once

#include "CoreMinimal.h"
#include "Characters/GSCharacterBase.h"
#include "InputActionValue.h"
#include "GSPlayerCharacter.generated.h"

class UGSWeaponComponent;
class UGSTargetingComponent;
class UGSInteractionComponent;
class UGSCarryComponent;
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

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Interaction")
	UGSInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Interaction")
	UGSCarryComponent* GetCarryComponent() const { return CarryComponent; }

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

	/** Hold E to channel, release to abort. Release is routed straight at the interaction component
	 *  because the ability is activated by class rather than through an ASC input ID, so GAS's own
	 *  InputReleased never fires for it. */
	void Input_InteractStart(const FInputActionValue& Value);
	void Input_InteractStop(const FInputActionValue& Value);

	UFUNCTION()
	void HandleWeaponModeChanged(bool bRangedMode);

	/** MoveSpeedMultiplier is the single source of truth for walk speed: the carry slow, the block
	 *  slow, and every root/slow after them are GameplayEffects on that attribute, aggregated by GAS.
	 *  Nothing writes MaxWalkSpeed directly any more - that pattern cannot stack, and OnStartCrouch
	 *  reassigned MaxWalkSpeedCrouched out from under whoever had cached it. */
	void HandleMoveSpeedMultiplierChanged(const FOnAttributeChangeData& Data);

	/** Re-derives both walk speeds from BaseWalkSpeed and the current multiplier. */
	void ApplyMoveSpeed();

	/** Switches between movement-facing (default) and aim-facing (holding Aim, or a ranged weapon
	 *  mode equipped) rotation, per tech doc §16: "the character faces movement, and faces the aim
	 *  while attacking or aiming." Also mirrors the result onto the State.Aiming gameplay tag so
	 *  animation/combat systems can query it without reaching into player-only state. */
	void UpdateRotationMode();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Weapon")
	TObjectPtr<UGSWeaponComponent> WeaponComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Targeting")
	TObjectPtr<UGSTargetingComponent> TargetingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Interaction")
	TObjectPtr<UGSInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Interaction")
	TObjectPtr<UGSCarryComponent> CarryComponent;

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

	/** Hold to aim - forces aim-facing rotation while held (tech doc §16). */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> AimAction;

	/** The stealth stance toggle - universal kit (design doc §7). */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> CrouchAction;

	/** Hold-E interact (GDD §8). Bound to Started and Completed/Canceled - the channel runs exactly
	 *  as long as the key is down. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> InteractAction;

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

	/** Universal interact channel - C++-defaulted to UGSGA_Interact, same reasoning as
	 *  SwordLightAbilityClass: a framework verb nobody remembers to fill in is a framework nobody
	 *  can test. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Abilities")
	TSubclassOf<UGameplayAbility> InteractAbilityClass;

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

	/** Universal dodge roll (racial trait, design doc §7) - set to UGSGA_DodgeRoll in the character
	 *  Blueprint defaults. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Abilities")
	TSubclassOf<UGameplayAbility> DodgeAbilityClass;

	/** Crouch-walk speed as a fraction of the character's normal walk speed (design doc §7: crouch
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
