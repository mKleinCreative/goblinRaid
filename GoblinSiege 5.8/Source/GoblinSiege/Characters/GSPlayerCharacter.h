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
class UGSAimComponent;
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

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Aim")
	UGSAimComponent* GetAimComponent() const { return AimComponent; }

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

	/** True when the attack button should draw the bow instead of swinging - ranged mode with a bow
	 *  ability actually assigned. The null check is deliberate: without it, swapping to ranged on a
	 *  character whose BowShotAbilityClass was never filled in would silently disable the attack
	 *  button entirely, which looks exactly like a broken input binding. */
	bool IsRangedAttackMode() const;

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

	/** Bound to UGSAimComponent::OnAimStateChanged. Aiming changes both what the camera does and
	 *  which way the body faces, and before this existed the torch aim changed neither. */
	UFUNCTION()
	void HandleAimStateChanged(bool bIsAimingNow);

	/** "The body should point where the camera points" - the Aim key, an active ranged aim, or simply
	 *  having the bow equipped (tech doc §16). Consumed by UpdateRotationMode. */
	bool WantsAimFacing() const;

	/** "The camera should be over the left shoulder and tight" - only while a shot is actually being
	 *  aimed. Narrower than WantsAimFacing on purpose; see the definition. */
	bool WantsAimCamera() const;

	/** Advances CameraAimAlpha and writes the boom/FOV. Early-outs entirely once the blend has
	 *  arrived, so a goblin standing still costs nothing per frame. */
	void UpdateAimCamera(float DeltaSeconds);

	/** Adds MaxWalkSpeedCrouched to the base class's derivation - the crouch speed has to scale with
	 *  the same multiplier, or crouch-walking silently ignores whatever is slowing you. */
	virtual void ApplyMoveSpeed() override;

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

	/** Owns the aim state, the predicted trajectory and the arc ribbon for every ranged verb. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Aim")
	TObjectPtr<UGSAimComponent> AimComponent;

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

	/** The Scout's ranged half. Routed from the ATTACK button while the weapon component reports
	 *  ranged mode - "sword <-> bow, live-swap" means one attack key whose meaning follows the mode,
	 *  not a second key the player has to remember. Not C++-defaulted, matching SwordHeavyAbilityClass:
	 *  point it at UGSGA_BowShot or a Blueprint child of it. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Abilities")
	TSubclassOf<UGameplayAbility> BowShotAbilityClass;

	// ---- torch aiming (2026-08-03, moved to UGSAimComponent 2026-08-04) --------------------
	// TorchAimMaxSimSeconds and TorchAimArcColour now live on UGSAimComponent (as MaxSimSeconds and
	// TorchArcColour), and bAimingTorch is UGSAimComponent::IsAiming(). Only the input-policy switch
	// stayed here, because it decides what the BUTTON does, not what the arc looks like.

	/** Hold the throw button to aim, release to throw. Off makes the torch an instant press-to-throw
	 *  again, which is the pre-2026-08-03 behaviour. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Torch")
	bool bTorchAimEnabled = true;

	// ---- aim camera (2026-08-04) -----------------------------------------------------------
	// Over the LEFT shoulder while aiming, back to the right at rest. The hip values are captured
	// from the components in BeginPlay rather than duplicated here as constants, so retuning the
	// boom on BP_GSPlayerCharacter is not silently undone by C++ on the first aim.

	/** Boom length while aiming. Pulling in from 450 is most of what makes an aim read as an aim. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Camera|Aim", meta = (ClampMin = "0.0"))
	float AimArmLength = 250.f;

	/** Negative Y is the goblin's LEFT. The hip rig sits at +55 (right shoulder), so this is a swap
	 *  across the body rather than a nudge, and the swing itself is a large part of the feedback. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Camera|Aim")
	FVector AimSocketOffset = FVector(0.f, -55.f, 45.f);

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Camera|Aim", meta = (ClampMin = "20.0", ClampMax = "170.0"))
	float AimFOV = 70.f;

	/**
	 * Seconds for the full blend, in BOTH directions.
	 *
	 * Driven as an explicit 0..1 alpha rather than an FInterpTo, deliberately: FInterpTo is
	 * asymptotic, so it never actually arrives, and "never actually arrives" on a camera means the
	 * boom keeps being written every frame forever and the Tick early-out below can never fire.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Camera|Aim", meta = (ClampMin = "0.01"))
	float AimBlendSeconds = 0.2f;

private:
	FTimerHandle HeavyChargeTimer;
	float AttackPressedTime = -1.f;
	bool bAttackHeld = false;
	bool bHeavyFiredThisHold = false;

	// ---- aim camera runtime state ----------------------------------------------------------
	/** 0 = hip, 1 = aiming. Advanced linearly by AimBlendSeconds and eased on read. */
	float CameraAimAlpha = 0.f;

	/** Captured from the spring arm and camera in BeginPlay - see AimArmLength's comment for why
	 *  these are read from the components rather than written as constants. */
	float HipArmLength = 450.f;
	FVector HipSocketOffset = FVector(0.f, 55.f, 65.f);
	float HipFOV = 90.f;

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


	/** Last non-zero 2D move input, used by GetDodgeDirection() so a dodge reads as "where I was
	 *  heading" rather than always forward. */
	FVector2D LastMoveInput = FVector2D::ZeroVector;
};
