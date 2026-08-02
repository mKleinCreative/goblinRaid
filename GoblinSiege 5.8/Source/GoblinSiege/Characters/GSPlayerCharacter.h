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
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UGameplayAbility;

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
	void Input_Dodge(const FInputActionValue& Value);
	void Input_Attack(const FInputActionValue& Value);
	void Input_ThrowTorch(const FInputActionValue& Value);
	void Input_SwapWeaponMode(const FInputActionValue& Value);
	void Input_AimStart(const FInputActionValue& Value);
	void Input_AimStop(const FInputActionValue& Value);
	void Input_ToggleCrouch(const FInputActionValue& Value);

	UFUNCTION()
	void HandleWeaponModeChanged(bool bRangedMode);

	/** Switches between movement-facing (default) and aim-facing (holding Aim, or a ranged weapon
	 *  mode equipped) rotation, per tech doc §16: "the character faces movement, and faces the aim
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

	/** Universal torch toss (racial trait) - granted in BeginPlay regardless of weapon kit. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Abilities")
	TSubclassOf<UGameplayAbility> TorchTossAbilityClass;

	/** Light melee swing. Defaults to UGSGA_SwordLight in the constructor so a fresh character
	 *  Blueprint can swing without anyone remembering to fill this in - the same reasoning that
	 *  put C++ defaults on GSTorchProjectile's FireVolumeClass. Per-weapon ability grants replace
	 *  this once DA_Weapon_* assets carry their own GrantedAbilities. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Abilities")
	TSubclassOf<UGameplayAbility> SwordLightAbilityClass;

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
