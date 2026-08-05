#include "Characters/GSPlayerCharacter.h"
#include "Characters/GSTargetingComponent.h"
#include "Attributes/GSAttributeSetBase.h"
#include "GameplayEffectExtension.h"
#include "Interaction/GSCarryComponent.h"
#include "Interaction/GSInteractionComponent.h"
#include "Weapons/GSWeaponComponent.h"
#include "Weapons/Abilities/GSGA_Interact.h"
#include "Weapons/Abilities/GSGA_SwordLight.h"
#include "Weapons/Abilities/GSGA_Block.h"
#include "Destruction/GSTorchProjectile.h"
#include "Combat/GSGameplayTags.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
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
	InteractionComponent = CreateDefaultSubobject<UGSInteractionComponent>(TEXT("InteractionComponent"));
	CarryComponent = CreateDefaultSubobject<UGSCarryComponent>(TEXT("CarryComponent"));

	// Defaulted in C++ so a character Blueprint swings out of the box. An unset ability class is a
	// silent failure - you press attack, nothing happens, and nothing tells you why.
	SwordLightAbilityClass = UGSGA_SwordLight::StaticClass();
	BlockAbilityClass = UGSGA_Block::StaticClass();
	InteractAbilityClass = UGSGA_Interact::StaticClass();
	// SwordHeavyAbilityClass is deliberately NOT defaulted: it and the light share a class, so a
	// C++ default would silently give the heavy the light's stage array and the two would feel
	// identical for no visible reason. Better to have the heavy do nothing until it is pointed at
	// its own Blueprint - and the input handler says so in the log.

	// Tick exists only to draw the torch aim arc, and early-outs when not aiming.
	PrimaryActorTick.bCanEverTick = true;

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
		if (SwordLightAbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(SwordLightAbilityClass, 1, INDEX_NONE, this));
		}
		if (SwordHeavyAbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(SwordHeavyAbilityClass, 1, INDEX_NONE, this));
		}
		if (BlockAbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(BlockAbilityClass, 1, INDEX_NONE, this));
		}
		if (GuardBreakAbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(GuardBreakAbilityClass, 1, INDEX_NONE, this));
		}
		if (InteractAbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(InteractAbilityClass, 1, INDEX_NONE, this));
		}

		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UGSAttributeSetBase::GetMoveSpeedMultiplierAttribute())
			.AddUObject(this, &AGSPlayerCharacter::HandleMoveSpeedMultiplierChanged);
	}

	// After BaseWalkSpeed is captured above: every slow in the game is a GameplayEffect on
	// MoveSpeedMultiplier now, and this is the one place that turns that attribute into walk speeds.
	ApplyMoveSpeed();
}

void AGSPlayerCharacter::HandleMoveSpeedMultiplierChanged(const FOnAttributeChangeData& /*Data*/)
{
	ApplyMoveSpeed();
}

void AGSPlayerCharacter::ApplyMoveSpeed()
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp || !AbilitySystemComponent)
	{
		return;
	}

	// UGSAttributeSetBase::PreAttributeChange already clamps this to [0.1, 3.0], so a stack of slows
	// can never hard-freeze the goblin.
	const float Multiplier = AbilitySystemComponent->GetNumericAttribute(
		UGSAttributeSetBase::GetMoveSpeedMultiplierAttribute());

	MoveComp->MaxWalkSpeed = BaseWalkSpeed * Multiplier;
	MoveComp->MaxWalkSpeedCrouched = BaseWalkSpeed * CrouchSpeedMultiplier * Multiplier;
}

void AGSPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bAimingTorch && bTorchAimEnabled)
	{
		DrawTorchAimArc();
	}

	// A 1.5s hold threshold with no visible fill is guesswork for the player - "I held it and got
	// a light attack" is the complaint that follows. Broadcast every frame while charging so a HUD
	// can draw the ring; stop once the heavy has fired so the ring doesn't sit full afterwards.
	if (bAttackHeld && !bHeavyFiredThisHold)
	{
		OnHeavyChargeChanged.Broadcast(GetHeavyChargeAlpha());
	}
}

void AGSPlayerCharacter::DrawTorchAimArc()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Read the flight parameters off the projectile CDO instead of duplicating them here. If the
	// torch's speed or gravity is ever retuned, the aim line follows automatically - an aim
	// indicator that lies about where the thing lands is worse than no indicator.
	float Speed = 1400.f;
	float GravityScale = 1.f;
	if (const AGSTorchProjectile* TorchCDO = GetDefault<AGSTorchProjectile>())
	{
		if (const UProjectileMovementComponent* Move =
				TorchCDO->FindComponentByClass<UProjectileMovementComponent>())
		{
			Speed = Move->InitialSpeed > 0.f ? Move->InitialSpeed : Speed;
			GravityScale = Move->ProjectileGravityScale;
		}
	}

	// Must match UGSGA_TorchToss::ThrowTorch exactly, including the +50 hand-height fudge.
	const FRotator AimRotation = GetControlRotation();
	const FVector Start = GetActorLocation() + AimRotation.Vector() * 80.f + FVector(0.f, 0.f, 50.f);

	FPredictProjectilePathParams Params(12.f, Start, AimRotation.Vector() * Speed, TorchAimMaxSimSeconds);
	Params.OverrideGravityZ = World->GetGravityZ() * GravityScale;
	Params.bTraceWithCollision = true;
	Params.bTraceComplex = false;
	Params.ActorsToIgnore.Add(this);
	Params.DrawDebugType = EDrawDebugTrace::ForOneFrame;
	Params.DrawDebugTime = 0.f;
	Params.SimFrequency = 15.f;

	FPredictProjectilePathResult Result;
	const bool bHit = UGameplayStatics::PredictProjectilePath(this, Params, Result);

	// The engine's own debug draw gives the arc; the landing ring is what the player actually
	// reads, so draw that ourselves in the torch colour and make it obvious when it hits nothing.
	if (bHit)
	{
		DrawDebugCircle(World, Result.HitResult.ImpactPoint + FVector(0.f, 0.f, 3.f), 55.f, 24,
			TorchAimArcColour.ToFColor(true), false, -1.f, 0, 3.f,
			FVector(1, 0, 0), FVector(0, 1, 0), false);
	}
	else if (Result.PathData.Num() > 0)
	{
		DrawDebugSphere(World, Result.PathData.Last().Location, 30.f, 10,
			FColor::Silver, false, -1.f, 0, 2.f);
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
		if (AttackAction)
		{
			// Tap for light, hold for heavy - so the press only starts a clock and the release
			// decides. Canceled is bound as well as Completed: a focus loss mid-hold fires only
			// Canceled, and without it the charge timer would keep running against a button that
			// is no longer down.
			EIC->BindAction(AttackAction, ETriggerEvent::Started, this, &AGSPlayerCharacter::Input_AttackPressed);
			EIC->BindAction(AttackAction, ETriggerEvent::Completed, this, &AGSPlayerCharacter::Input_AttackReleased);
			EIC->BindAction(AttackAction, ETriggerEvent::Canceled, this, &AGSPlayerCharacter::Input_AttackReleased);
		}
		if (HeavyAttackAction)
		{
			EIC->BindAction(HeavyAttackAction, ETriggerEvent::Started, this, &AGSPlayerCharacter::Input_HeavyAttack);
		}
		if (GuardBreakAction)
		{
			EIC->BindAction(GuardBreakAction, ETriggerEvent::Started, this, &AGSPlayerCharacter::Input_GuardBreak);
		}
		if (BlockAction)
		{
			EIC->BindAction(BlockAction, ETriggerEvent::Started, this, &AGSPlayerCharacter::Input_BlockStart);
			EIC->BindAction(BlockAction, ETriggerEvent::Completed, this, &AGSPlayerCharacter::Input_BlockStop);
			EIC->BindAction(BlockAction, ETriggerEvent::Canceled, this, &AGSPlayerCharacter::Input_BlockStop);
		}
		if (InteractAction)
		{
			EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &AGSPlayerCharacter::Input_InteractStart);
			EIC->BindAction(InteractAction, ETriggerEvent::Completed, this, &AGSPlayerCharacter::Input_InteractStop);
			EIC->BindAction(InteractAction, ETriggerEvent::Canceled, this, &AGSPlayerCharacter::Input_InteractStop);
		}

		// Torch: hold to aim, release to throw. Canceled is bound as well as Completed because a
		// focus loss mid-hold fires Canceled only, and without it the goblin would be left aiming
		// forever with no way to resolve the throw.
		EIC->BindAction(ThrowTorchAction, ETriggerEvent::Started, this, &AGSPlayerCharacter::Input_ThrowTorchStart);
		EIC->BindAction(ThrowTorchAction, ETriggerEvent::Completed, this, &AGSPlayerCharacter::Input_ThrowTorchRelease);
		EIC->BindAction(ThrowTorchAction, ETriggerEvent::Canceled, this, &AGSPlayerCharacter::Input_ThrowTorchRelease);
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

void AGSPlayerCharacter::Input_Attack(const FInputActionValue& Value)
{
	if (!AbilitySystemComponent || !SwordLightAbilityClass)
	{
		return;
	}

	// GAS refuses to re-activate an already-active InstancedPerActor ability, and that refusal is
	// exactly what turns a second press into a COMBO input rather than a competing swing. So the
	// failure path is not an error here - it is the interesting case.
	if (AbilitySystemComponent->TryActivateAbilityByClass(SwordLightAbilityClass))
	{
		return;
	}

	// Already swinging: hand the press to the running instance as a buffered follow-up.
	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (!Spec.IsActive() || !Spec.Ability || !Spec.Ability->IsA(SwordLightAbilityClass))
		{
			continue;
		}
		if (UGSGA_SwordLight* Swing = Cast<UGSGA_SwordLight>(Spec.GetPrimaryInstance()))
		{
			Swing->BufferComboInput();
			return;
		}
	}
}

float AGSPlayerCharacter::GetHeavyChargeAlpha() const
{
	if (!bAttackHeld || AttackPressedTime < 0.f || HeavyHoldSeconds <= 0.f)
	{
		return 0.f;
	}
	const UWorld* World = GetWorld();
	return World ? FMath::Clamp((World->GetTimeSeconds() - AttackPressedTime) / HeavyHoldSeconds, 0.f, 1.f) : 0.f;
}

void AGSPlayerCharacter::Input_AttackPressed(const FInputActionValue& Value)
{
	bAttackHeld = true;
	bHeavyFiredThisHold = false;
	AttackPressedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	// The heavy fires ON the threshold while the button is still down, rather than waiting for
	// release. Holding past 1.5s and then having to let go before anything happens feels like the
	// input was dropped; firing at the moment the charge completes is what makes the hold legible.
	GetWorldTimerManager().SetTimer(HeavyChargeTimer, this,
		&AGSPlayerCharacter::TriggerHeavyAttack, HeavyHoldSeconds, false);
}

void AGSPlayerCharacter::Input_AttackReleased(const FInputActionValue& Value)
{
	GetWorldTimerManager().ClearTimer(HeavyChargeTimer);
	bAttackHeld = false;
	AttackPressedTime = -1.f;
	OnHeavyChargeChanged.Broadcast(0.f);

	// A release after the heavy already went off is just the end of that input, not a second
	// attack. Without this guard every heavy would be chased by a light swing.
	if (bHeavyFiredThisHold)
	{
		bHeavyFiredThisHold = false;
		return;
	}

	Input_Attack(Value);
}

void AGSPlayerCharacter::TriggerHeavyAttack()
{
	bHeavyFiredThisHold = true;
	OnHeavyChargeChanged.Broadcast(1.f);
	Input_HeavyAttack(FInputActionValue());
}

void AGSPlayerCharacter::Input_HeavyAttack(const FInputActionValue& Value)
{
	if (!AbilitySystemComponent)
	{
		return;
	}
	if (!SwordHeavyAbilityClass)
	{
		// Loud on purpose. The heavy shares a class with the light, so a null here is invisible in
		// the details panel - it looks configured because the class column is populated elsewhere.
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] Heavy attack pressed but SwordHeavyAbilityClass is unset on %s. "
				 "Point it at a UGSGA_SwordLight Blueprint child with its own Stages array."),
			*GetName());
		return;
	}
	AbilitySystemComponent->TryActivateAbilityByClass(SwordHeavyAbilityClass);
}

void AGSPlayerCharacter::Input_GuardBreak(const FInputActionValue& Value)
{
	if (!AbilitySystemComponent)
	{
		return;
	}
	if (!GuardBreakAbilityClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] Guard break pressed but GuardBreakAbilityClass is unset on %s. "
				 "Point it at a UGSGA_SwordLight Blueprint child with bBreaksGuard on its stage."),
			*GetName());
		return;
	}
	AbilitySystemComponent->TryActivateAbilityByClass(GuardBreakAbilityClass);
}

void AGSPlayerCharacter::Input_BlockStart(const FInputActionValue& Value)
{
	if (AbilitySystemComponent && BlockAbilityClass)
	{
		AbilitySystemComponent->TryActivateAbilityByClass(BlockAbilityClass);
	}
}

void AGSPlayerCharacter::Input_BlockStop(const FInputActionValue& Value)
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// Cancel by TAG, not by passing nullptr - nullptr means "cancel everything", which would abort
	// a swing already in flight every time the guard came down. UGSGA_Block carries State.Blocking
	// as an ability tag precisely so this filter can find it and nothing else.
	FGameplayTagContainer BlockTags;
	BlockTags.AddTag(GSTags::State_Blocking);
	AbilitySystemComponent->CancelAbilities(&BlockTags);
}

void AGSPlayerCharacter::Input_ThrowTorchStart(const FInputActionValue& Value)
{
	if (!bTorchAimEnabled)
	{
		Input_ThrowTorch(Value); // aiming disabled: behave exactly as the old press-to-throw
		return;
	}
	bAimingTorch = true;
}

void AGSPlayerCharacter::Input_ThrowTorchRelease(const FInputActionValue& Value)
{
	if (!bAimingTorch)
	{
		return; // release without a matching press (e.g. aiming was disabled) - nothing to resolve
	}
	bAimingTorch = false;
	Input_ThrowTorch(Value);
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

void AGSPlayerCharacter::Input_InteractStart(const FInputActionValue& /*Value*/)
{
	if (AbilitySystemComponent && InteractAbilityClass)
	{
		AbilitySystemComponent->TryActivateAbilityByClass(InteractAbilityClass);
	}
}

void AGSPlayerCharacter::Input_InteractStop(const FInputActionValue& /*Value*/)
{
	// Interruptible always (stealth spec): letting go cancels, including on an alt-tab (Canceled).
	if (InteractionComponent)
	{
		InteractionComponent->ReleaseInteractInput();
	}
}

void AGSPlayerCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	// Was: MaxWalkSpeedCrouched = BaseWalkSpeed * CrouchSpeedMultiplier. That reassignment ignored
	// any active slow, so crouch-walking while carrying a sack silently cancelled the carry penalty
	// and putting the sack down then restored a stale value. ApplyMoveSpeed folds the multiplier in.
	ApplyMoveSpeed();

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
