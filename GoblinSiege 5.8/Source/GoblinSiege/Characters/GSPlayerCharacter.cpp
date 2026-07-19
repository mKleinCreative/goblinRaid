#include "Characters/GSPlayerCharacter.h"
#include "Weapons/GSWeaponComponent.h"
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

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	// Top-down/isometric-leaning boom to match the browser prototype's readable, wide-view combat.
	CameraBoom->TargetArmLength = 900.f;
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->bInheritRoll = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = false; // turn rate is handled explicitly per design doc
}

void AGSPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

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

		// Light/heavy attack and the "E" ability are bound by the currently-granted ability set's
		// own AbilityTask_WaitInputPress/Release (standard GAS pattern), not hardcoded here, so
		// swapping weapons doesn't require touching this character class.
	}
}

void AGSPlayerCharacter::Input_Move(const FInputActionValue& Value)
{
	const FVector2D MoveInput = Value.Get<FVector2D>();
	if (!Controller || MoveInput.IsNearlyZero())
	{
		return;
	}

	const FRotator ControlRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(ControlRot).GetUnitAxis(EAxis::X), MoveInput.Y);
	AddMovementInput(FRotationMatrix(ControlRot).GetUnitAxis(EAxis::Y), MoveInput.X);

	// Facing is eased toward the movement/aim direction at TurnRateRadPerSec (per-weapon value from
	// GSWeaponDataAsset) in Tick or a dedicated movement component - see design doc "Turn rate".
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
