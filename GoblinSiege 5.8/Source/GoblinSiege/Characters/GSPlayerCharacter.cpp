#include "Characters/GSPlayerCharacter.h"
#include "Characters/GSTargetingComponent.h"
#include "Attributes/GSAttributeSetBase.h"
#include "GameplayEffectExtension.h"
#include "Interaction/GSCarryComponent.h"
#include "Interaction/GSInteractionComponent.h"
#include "Weapons/GSWeaponComponent.h"
#include "Weapons/GSArrowProjectile.h"
#include "Weapons/Abilities/GSGA_Interact.h"
#include "Weapons/Abilities/GSGA_SwordLight.h"
#include "Weapons/Abilities/GSGA_Block.h"
#include "Weapons/Abilities/GSGA_BowShot.h"
#include "Weapons/Abilities/GSGA_TorchToss.h"
#include "Destruction/GSTorchProjectile.h"
#include "Combat/GSAimComponent.h"
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
	InteractionComponent = CreateDefaultSubobject<UGSInteractionComponent>(TEXT("InteractionComponent"));
	CarryComponent = CreateDefaultSubobject<UGSCarryComponent>(TEXT("CarryComponent"));
	AimComponent = CreateDefaultSubobject<UGSAimComponent>(TEXT("AimComponent"));

	// The player is a goblin, so allied goblins and the horde cannot cut him down by standing too
	// close. Set here rather than on the Blueprint for the same reason the ability classes are.
	RaceTag = GSTags::Race_Goblin;

	// Respawn must not be refusable. The default handling aborts the spawn on any overlap, and the
	// thing most likely to be overlapping a PlayerStart is the pack of defenders that just killed
	// you standing on it - observed 2026-08-04: "SpawnActor failed because of collision at the spawn
	// location for [BP_GSPlayerCharacter_C]", after which the session had no player pawn at all.
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// Defaulted in C++ so a character Blueprint swings out of the box. An unset ability class is a
	// silent failure - you press attack, nothing happens, and nothing tells you why.
	SwordLightAbilityClass = UGSGA_SwordLight::StaticClass();
	BlockAbilityClass = UGSGA_Block::StaticClass();
	InteractAbilityClass = UGSGA_Interact::StaticClass();
	// SwordHeavyAbilityClass is deliberately NOT defaulted: it and the light share a class, so a
	// C++ default would silently give the heavy the light's stage array and the two would feel
	// identical for no visible reason. Better to have the heavy do nothing until it is pointed at
	// its own Blueprint - and the input handler says so in the log.

	// Tick drives the aim-camera blend and the heavy-charge broadcast, and early-outs of both when
	// there is nothing to do. (It used to draw the torch aim arc too; that moved to UGSAimComponent,
	// which ticks only while aiming.)
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
	// The SetRelativeRotation(-10 pitch) that used to sit here was removed on 2026-08-04. It was
	// dead: bUsePawnControlRotation makes USpringArmComponent overwrite the boom's rotation from the
	// controller every single tick, so the "low pitch by default" its comment promised had never
	// once been in effect. Deleting it rather than leaving it while real camera code went in next to
	// it - a line that looks load-bearing and is not is worse than no line.
	//
	// Note for anyone tuning the aim rig: SocketOffset is applied AFTER the spring arm's collision
	// probe, so the -55 left offset while aiming can push the camera into a wall the probe already
	// cleared. Standard UE behaviour, and Epic's own templates live with it; it is simply more
	// noticeable on the tighter 250 aim arm than at 450.

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

	// BaseWalkSpeed capture, the MoveSpeedMultiplier binding and the first ApplyMoveSpeed all happen
	// in AGSCharacterBase::BeginPlay above - every character needs them, not just the player.
	UpdateRotationMode();

	// Capture the hip rig from the components themselves, not from constants. A designer who retunes
	// the boom on BP_GSPlayerCharacter would otherwise have their value silently replaced by the C++
	// number the first time the player aimed and let go.
	if (CameraBoom)
	{
		HipArmLength = CameraBoom->TargetArmLength;
		HipSocketOffset = CameraBoom->SocketOffset;
	}
	if (FollowCamera)
	{
		HipFOV = FollowCamera->FieldOfView;
	}

	if (WeaponComponent)
	{
		WeaponComponent->OnWeaponModeChanged.AddDynamic(this, &AGSPlayerCharacter::HandleWeaponModeChanged);
	}

	if (AimComponent)
	{
		AimComponent->OnAimStateChanged.AddDynamic(this, &AGSPlayerCharacter::HandleAimStateChanged);
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
		if (BowShotAbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(BowShotAbilityClass, 1, INDEX_NONE, this));
		}
	}
}

void AGSPlayerCharacter::ApplyMoveSpeed()
{
	Super::ApplyMoveSpeed();

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp || !AbilitySystemComponent)
	{
		return;
	}

	// The crouch speed is the player's own addition to the base derivation. Deriving it here rather
	// than in OnStartCrouch is what stops a crouch from wiping an active carry or block slow.
	MoveComp->MaxWalkSpeedCrouched = BaseWalkSpeed * CrouchSpeedMultiplier
		* AbilitySystemComponent->GetNumericAttribute(UGSAttributeSetBase::GetMoveSpeedMultiplierAttribute());
}

void AGSPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateAimCamera(DeltaSeconds);

	// A 1.5s hold threshold with no visible fill is guesswork for the player - "I held it and got
	// a light attack" is the complaint that follows. Broadcast every frame while charging so a HUD
	// can draw the ring; stop once the heavy has fired so the ring doesn't sit full afterwards.
	if (bAttackHeld && !bHeavyFiredThisHold)
	{
		OnHeavyChargeChanged.Broadcast(GetHeavyChargeAlpha());
	}
}

bool AGSPlayerCharacter::WantsAimFacing() const
{
	// Which way the BODY points. Merely having the bow out is enough - that is the pre-existing
	// tech-doc §16 rule and it is unchanged; the aim component is the new term, and it is the fix
	// for the torch arc being drawn along the control rotation while the goblin faced his movement
	// direction.
	return bIsAiming
		|| (AimComponent && AimComponent->IsAiming())
		|| (WeaponComponent && WeaponComponent->IsInRangedMode());
}

bool AGSPlayerCharacter::WantsAimCamera() const
{
	// Deliberately NOT the same predicate as WantsAimFacing, and the difference is the ranged-mode
	// term. Facing can follow the equipped weapon all day - it costs the player nothing to strafe
	// with a bow out. The CAMERA cannot: pulling to a 250 arm at 70 degrees the moment the bow is
	// drawn, and holding it there for as long as the bow is the active weapon, would mean a player
	// who swapped to ranged never got to look at the hamlet again. Zoom is a thing you do while
	// actually aiming a shot, not a property of what you are holding.
	return bIsAiming || (AimComponent && AimComponent->IsAiming());
}

void AGSPlayerCharacter::UpdateAimCamera(float DeltaSeconds)
{
	if (!CameraBoom || !FollowCamera)
	{
		return;
	}

	const float Target = WantsAimCamera() ? 1.f : 0.f;

	// Arrived: write nothing. This is why the blend is an explicit alpha rather than an FInterpTo -
	// an asymptotic interp never equals its target, so this early-out could never fire and the boom
	// would be rewritten every frame of the entire raid for no visible change.
	if (FMath::IsNearlyEqual(CameraAimAlpha, Target))
	{
		return;
	}

	const float Step = (AimBlendSeconds > 0.f) ? (DeltaSeconds / AimBlendSeconds) : 1.f;
	CameraAimAlpha = (Target > CameraAimAlpha)
		? FMath::Min(CameraAimAlpha + Step, Target)
		: FMath::Max(CameraAimAlpha - Step, Target);

	// Eased on READ, not stored eased: storing the eased value would feed it back into the next
	// frame's easing and the blend would decelerate twice.
	const float Eased = FMath::InterpEaseInOut(0.f, 1.f, CameraAimAlpha, 2.f);

	CameraBoom->TargetArmLength = FMath::Lerp(HipArmLength, AimArmLength, Eased);
	CameraBoom->SocketOffset = FMath::Lerp(HipSocketOffset, AimSocketOffset, Eased);
	FollowCamera->SetFieldOfView(FMath::Lerp(HipFOV, AimFOV, Eased));
}

void AGSPlayerCharacter::HandleAimStateChanged(bool bIsAimingNow)
{
	// Aiming has to turn the body, not just the camera. This binding is the fix for a real bug: the
	// torch aim used to set its own flag and nothing else, so the arc was computed from the control
	// rotation while the goblin carried on facing wherever he was walking.
	UpdateRotationMode();
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

bool AGSPlayerCharacter::IsRangedAttackMode() const
{
	return WeaponComponent && WeaponComponent->IsInRangedMode() && BowShotAbilityClass != nullptr;
}

void AGSPlayerCharacter::Input_AttackPressed(const FInputActionValue& Value)
{
	// Ranged mode reinterprets the attack button rather than adding a second one: press to draw and
	// aim, release to loose. That is what "sword <-> bow, live-swap" has to mean for a player - one
	// key whose meaning follows the weapon, not a key they have to remember only applies half the
	// time.
	if (IsRangedAttackMode())
	{
		// No heavy charge in ranged mode. The heavy is a melee verb, and leaving its timer running
		// would fire a sword swing out of a drawn bow at the 1.5s mark.
		bAttackHeld = false;
		bHeavyFiredThisHold = false;
		AttackPressedTime = -1.f;
		GetWorldTimerManager().ClearTimer(HeavyChargeTimer);
		OnHeavyChargeChanged.Broadcast(0.f);

		if (AimComponent)
		{
			TSubclassOf<AActor> ProjectileClass = AGSArrowProjectile::StaticClass();
			if (const UGSGA_BowShot* BowCDO = Cast<UGSGA_BowShot>(BowShotAbilityClass->GetDefaultObject()))
			{
				if (BowCDO->GetArrowProjectileClass())
				{
					ProjectileClass = BowCDO->GetArrowProjectileClass();
				}
			}
			AimComponent->BeginAim(EGSAimMode::Bow, ProjectileClass);
		}
		return;
	}

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
	// Bow first, and gated on actually being mid-aim rather than on the current weapon mode: a player
	// who swaps to melee while the bow is drawn should still resolve the shot they started, not have
	// the release silently become a sword swing.
	if (AimComponent && AimComponent->GetAimMode() == EGSAimMode::Bow)
	{
		// Push the exact aim BEFORE activating - see Input_ThrowTorchRelease for why.
		AimComponent->PushAimRotationToServer();
		AimComponent->EndAim();
		if (AbilitySystemComponent && BowShotAbilityClass)
		{
			AbilitySystemComponent->TryActivateAbilityByClass(BowShotAbilityClass);
		}
		return;
	}

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
	if (!bTorchAimEnabled || !AimComponent)
	{
		Input_ThrowTorch(Value); // aiming disabled: behave exactly as the old press-to-throw
		return;
	}

	// Arm the arc with the exact class the ability will spawn, so the preview and the throw describe
	// the same object. Reaching for the ability CDO rather than a second copy of the projectile class
	// on the character: one place owns "which torch", and it is the ability that throws it.
	TSubclassOf<AActor> ProjectileClass = AGSTorchProjectile::StaticClass();
	if (TorchTossAbilityClass)
	{
		if (const UGSGA_TorchToss* TossCDO = Cast<UGSGA_TorchToss>(TorchTossAbilityClass->GetDefaultObject()))
		{
			if (TossCDO->GetTorchProjectileClass())
			{
				ProjectileClass = TossCDO->GetTorchProjectileClass();
			}
		}
	}

	AimComponent->BeginAim(EGSAimMode::Torch, ProjectileClass);
}

void AGSPlayerCharacter::Input_ThrowTorchRelease(const FInputActionValue& Value)
{
	if (!AimComponent || !AimComponent->IsAiming())
	{
		return; // release without a matching press (e.g. aiming was disabled) - nothing to resolve
	}

	// Push the exact aim BEFORE activating, not after: the ability spawns inside TryActivate on the
	// server, and a rotation that arrives afterwards is a rotation for the next throw. See
	// UGSAimComponent::GetAimRotation for why the server cannot just read the control rotation.
	AimComponent->PushAimRotationToServer();
	AimComponent->EndAim();
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
	// Putting the bow away has to take the arc with it. Without this, swapping to the sword mid-draw
	// leaves a bow trajectory hanging in the air with no way to resolve or dismiss it - the release
	// handler would still fire the shot (which is correct, and deliberate), but a player who swapped
	// and then never released would be stuck looking at a preview for a weapon they are not holding.
	if (!bRangedMode && AimComponent && AimComponent->GetAimMode() == EGSAimMode::Bow)
	{
		AimComponent->EndAim();
	}

	UpdateRotationMode();
}

void AGSPlayerCharacter::UpdateRotationMode()
{
	// 2026-08-04: now includes the aim component, which is what makes aiming a TORCH turn the body.
	// It did not before, and the result was that the arc was drawn along the control rotation while
	// the goblin faced his movement direction - you aimed one way and the character pointed another.
	const bool bShouldFaceAim = WantsAimFacing();

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
