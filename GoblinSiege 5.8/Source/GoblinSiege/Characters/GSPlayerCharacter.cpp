#include "Characters/GSPlayerCharacter.h"
#include "Characters/GSTargetingComponent.h"
#include "Weapons/GSWeaponComponent.h"
#include "Combat/GSGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"

AGSPlayerCharacter::AGSPlayerCharacter()
{
	WeaponComponent = CreateDefaultSubobject<UGSWeaponComponent>(TEXT("WeaponComponent"));
	TargetingComponent = CreateDefaultSubobject<UGSTargetingComponent>(TEXT("TargetingComponent"));

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	// Third-person over-the-shoulder rig (tech doc §16): ~450 arm, slight right-shoulder offset,
	// low pitch by default - goblins are short, the world should loom. Free-look rides the
	// controller's rotation (bUsePawnControlRotation); the character mesh's own rotation is a
	// separate concern handled below (movement-facing by default, aim-facing on demand - see
	// UpdateRotationMode).
	CameraBoom->TargetArmLength = 450.f;
	CameraBoom->SocketOffset = FVector(0.f, 55.f, 65.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->SetRelativeRotation(FRotator(-10.f, 0.f, 0.f));

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// The character body doesn't snap to controller yaw/pitch/roll directly - UpdateRotationMode
	// switches bOrientRotationToMovement vs. bUseControllerRotationYaw depending on aim state.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = true;
		MoveComp->RotationRate = FRotator(0.f, FMath::RadiansToDegrees(TurnRateRadPerSec), 0.f);
		MoveComp->NavAgentProps.bCanCrouch = true;
	}
}

void AGSPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		BaseWalkSpeed = MoveComp->MaxWalkSpeed;
	}
	UpdateRotationMode();

	if (WeaponComponent)
	{
		WeaponComponent->OnWeaponModeChanged.AddDynamic(this, &AGSPlayerCharacter::HandleWeaponModeChanged);
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}

	if (AbilitySystemComponent)
	{
		if (TorchTossAbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(TorchTossAbilityClass, 1, INDEX_NONE, this));
		}
		if (DodgeAbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(DodgeAbilityClass, 1, INDEX_NONE, this));
		}
	}
}

void AGSPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AGSPlayerCharacter::Input_Move);
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AGSPlayerCharacter::Input_Look);
		EIC->BindAction(DodgeAction, ETriggerEvent::Started, this, &AGSPlayerCharacter::Input_Dodge);
		EIC->BindAction(ThrowTorchAction, ETriggerEvent::Started, this, &AGSPlayerCharacter::Input_ThrowTorch);
		EIC->BindAction(SwapWeaponModeAction, ETriggerEvent::Started, this, &AGSPlayerCharacter::Input_SwapWeaponMode);
		EIC->BindAction(AimAction, ETriggerEvent::Started, this, &AGSPlayerCharacter::Input_AimStart);
		EIC->BindAction(AimAction, ETriggerEvent::Completed, this, &AGSPlayerCharacter::Input_AimStop);
		EIC->BindAction(AimAction, ETriggerEvent::Canceled, this, &AGSPlayerCharacter::Input_AimStop);
		EIC->BindAction(CrouchAction, ETriggerEvent::Started, this, &AGSPlayerCharacter::Input_ToggleCrouch);

		// Light/heavy attack and the "E" ability are bound by the currently-granted ability set's
		// own AbilityTask_WaitInputPress/Release (standard GAS pattern), not hardcoded here, so
		// swapping weapons doesn't require touching this character class.
	}
}

void AGSPlayerCharacter::Input_Move(const FInputActionValue& Value)
{
	const FVector2D MoveInput = Value.Get<FVector2D>();
	if (!MoveInput.IsNearlyZero())
	{
		LastMoveInput = MoveInput;
	}

	if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(GSTags::State_Dodging))
	{
		return; // committed recovery - the roll owns movement until it ends (design doc §7)
	}

	if (!Controller || MoveInput.IsNearlyZero())
	{
		return;
	}

	const FRotator ControlRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(ControlRot).GetUnitAxis(EAxis::X), MoveInput.Y);
	AddMovementInput(FRotationMatrix(ControlRot).GetUnitAxis(EAxis::Y), MoveInput.X);

	// Facing itself is handled by CharacterMovementComponent::bOrientRotationToMovement +
	// RotationRate (set from TurnRateRadPerSec - see SetTurnRateRadPerSec) rather than here, so
	// aim-facing (UpdateRotationMode) can override it without touching this function.
}

void AGSPlayerCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D LookInput = Value.Get<FVector2D>();
	if (Controller)
	{
		AddControllerYawInput(LookInput.X);
		AddControllerPitchInput(LookInput.Y);
	}
}

void AGSPlayerCharacter::Input_Dodge(const FInputActionValue& Value)
{
	if (AbilitySystemComponent && DodgeAbilityClass)
	{
		AbilitySystemComponent->TryActivateAbilityByClass(DodgeAbilityClass);
	}
}

void AGSPlayerCharacter::Input_ThrowTorch(const FInputActionValue& Value)
{
	if (AbilitySystemComponent && TorchTossAbilityClass)
	{
		AbilitySystemComponent->TryActivateAbilityByClass(TorchTossAbilityClass);
	}
}

void AGSPlayerCharacter::Input_SwapWeaponMode(const FInputActionValue& Value)
{
	if (WeaponComponent)
	{
		WeaponComponent->ToggleRangedMode();
	}
}

void AGSPlayerCharacter::Input_AimStart(const FInputActionValue& Value)
{
	bIsAiming = true;
	UpdateRotationMode();
}

void AGSPlayerCharacter::Input_AimStop(const FInputActionValue& Value)
{
	bIsAiming = false;
	UpdateRotationMode();
}

void AGSPlayerCharacter::Input_ToggleCrouch(const FInputActionValue& Value)
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void AGSPlayerCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeedCrouched = BaseWalkSpeed * CrouchSpeedMultiplier;
	}
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(GSTags::State_Crouching);
	}
}

void AGSPlayerCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(GSTags::State_Crouching);
	}
}

void AGSPlayerCharacter::HandleWeaponModeChanged(bool bRangedMode)
{
	UpdateRotationMode();
}

void AGSPlayerCharacter::UpdateRotationMode()
{
	const bool bShouldFaceAim = bIsAiming || (WeaponComponent && WeaponComponent->IsInRangedMode());

	bUseControllerRotationYaw = bShouldFaceAim;
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = !bShouldFaceAim;
	}

	if (AbilitySystemComponent)
	{
		const bool bHasTag = AbilitySystemComponent->HasMatchingGameplayTag(GSTags::State_Aiming);
		if (bShouldFaceAim && !bHasTag)
		{
			AbilitySystemComponent->AddLooseGameplayTag(GSTags::State_Aiming);
		}
		else if (!bShouldFaceAim && bHasTag)
		{
			AbilitySystemComponent->RemoveLooseGameplayTag(GSTags::State_Aiming);
		}
	}
}

FVector AGSPlayerCharacter::GetDodgeDirection() const
{
	if (!LastMoveInput.IsNearlyZero() && Controller)
	{
		const FRotator ControlRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
		const FVector Forward = FRotationMatrix(ControlRot).GetUnitAxis(EAxis::X);
		const FVector Right = FRotationMatrix(ControlRot).GetUnitAxis(EAxis::Y);
		return (Forward * LastMoveInput.Y + Right * LastMoveInput.X).GetSafeNormal();
	}
	return GetActorForwardVector();
}
