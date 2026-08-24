#include "Characters/GSPlayerCharacter.h"

#include "Raid/GSWarrenPlacementComponent.h"
#include "Characters/GSTargetingComponent.h"
#include "Attributes/GSAttributeSetBase.h"
#include "GameplayEffectExtension.h"
#include "Interaction/GSCarryComponent.h"
#include "Characters/GSStaminaComponent.h"
#include "Interaction/GSInteractionComponent.h"
#include "Weapons/GSWeaponComponent.h"
#include "Weapons/GSArrowProjectile.h"
#include "Weapons/Abilities/GSGA_Interact.h"
#include "Weapons/Abilities/GSGA_SwordLight.h"
#include "Weapons/Abilities/GSGA_Block.h"
#include "Weapons/Abilities/GSGA_Horn.h"
#include "Weapons/Abilities/GSGA_GrappleThrow.h"
#include "Weapons/Abilities/GSGA_BowShot.h"
#include "Weapons/Abilities/GSGA_TorchToss.h"
#include "Destruction/GSTorchProjectile.h"
#include "Combat/GSAimComponent.h"
#include "Weapons/GSBowTimingComponent.h"
#include "Combat/GSEngagementComponent.h"
#include "Combat/GSGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "UI/GSWeaponWheelWidget.h"
#include "UI/GSHordeOrderWheelWidget.h"
#include "Horde/GSHordeCommandComponent.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"

AGSPlayerCharacter::AGSPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WeaponComponent = CreateDefaultSubobject<UGSWeaponComponent>(TEXT("WeaponComponent"));
	TargetingComponent = CreateDefaultSubobject<UGSTargetingComponent>(TEXT("TargetingComponent"));
	InteractionComponent = CreateDefaultSubobject<UGSInteractionComponent>(TEXT("InteractionComponent"));
	CarryComponent = CreateDefaultSubobject<UGSCarryComponent>(TEXT("CarryComponent"));
	WarrenPlacementComponent = CreateDefaultSubobject<UGSWarrenPlacementComponent>(TEXT("WarrenPlacementComponent"));
	AimComponent = CreateDefaultSubobject<UGSAimComponent>(TEXT("AimComponent"));
	BowTimingComponent = CreateDefaultSubobject<UGSBowTimingComponent>(TEXT("BowTimingComponent"));
	HordeCommandComponent = CreateDefaultSubobject<UGSHordeCommandComponent>(TEXT("HordeCommandComponent"));

	// The player is a goblin, so allied goblins and the horde cannot cut him down by standing too
	// close. Set here rather than on the Blueprint for the same reason the ability classes are.
	RaceTag = GSTags::Race_Goblin;

	// THE ONE EXEMPTION FROM THE #132 CROWD LIMITS. Every NPC victim now grants 4 attack tokens to
	// 6 assigned attackers, because two swingers on a militiaman had a warband standing in a circle
	// waiting its turn. Pointed at the player those same numbers are the failure GSCharacterBase's
	// constructor comment names outright - "FOUR defenders deleting the player in a second" - so he
	// keeps the pre-#132 pair.
	//
	// The base class has already run CreateDefaultSubobject by the time this constructor body
	// executes, so the component exists to be configured. Michael's ruling of 2026-08-11.
	if (EngagementComponent)
	{
		EngagementComponent->ConfigureLimits(/*TokenBudget=*/ 2, /*MaxEngaged=*/ 3);
	}

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
	HornAbilityClass = UGSGA_Horn::StaticClass();
	GrappleThrowAbilityClass = UGSGA_GrappleThrow::StaticClass();
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

		// Publish the player to the avoidance manager. Until 2026-08-09 he was invisible to it
		// (bUseRVOAvoidance false, AvoidanceWeight 0), so NPCs did not steer around him at all - they
		// discovered him by walking into his capsule, which is a large part of why a melee reads as a
		// scrum. AvoidanceWeight 1.0 is deliberately the maximum: in UCharacterMovementComponent an
		// agent at full weight is published as an obstacle that others yield to while never yielding
		// itself, so the crowd parts around the player and the player's own movement is untouched.
		// That last part is why this is safe to do to a human-controlled pawn.
		MoveComp->bUseRVOAvoidance = true;
		MoveComp->AvoidanceWeight = 1.0f;
		MoveComp->AvoidanceConsiderationRadius = 600.f;

		// --- swimming -------------------------------------------------------------------------
		// bCanSwim is the half that is easy to miss: without it the movement component REFUSES the
		// switch to MOVE_Swimming and the pawn falls through a water volume as though it were empty
		// air - which looks identical to the volume not being there. Both this and
		// AGSWaterVolume::bWaterVolume are required.
		MoveComp->NavAgentProps.bCanSwim = true;

		// Deliberately slower than walking (BaseWalkSpeed 600). The GDD asks that horizontal
		// traversal beat swimming, so water is a decision rather than a shortcut.
		MoveComp->MaxSwimSpeed = 300.f;

		// Buoyancy 1.0 floats you at the surface, which is what "standard surface swimming" means.
		// Below 1 you sink while swimming, which needs a dive input to be legible and there is not
		// one.
		MoveComp->Buoyancy = 1.f;

		// Upward push when swimming out at an edge. The engine default is 0, which means a goblin
		// at the shoreline swims into the bank forever instead of climbing out.
		MoveComp->OutofWaterZ = 420.f;
	}

	StaminaComponent = CreateDefaultSubobject<UGSStaminaComponent>(TEXT("StaminaComponent"));
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

	ApplyViewPitchLimits();

	if (StaminaComponent)
	{
		StaminaComponent->OnExhausted.AddDynamic(this, &AGSPlayerCharacter::HandleStaminaExhausted);
	}

	// The wheel's on-screen half. Locally controlled only - a dedicated server or a remote pawn has
	// no viewport, and CreateWidget against one is a warning at best.
	//
	// Created ONCE and left in the viewport collapsed, rather than spawned per gesture: Q is pressed
	// often, and a construct/destruct cycle each time would rebind the component's delegates on every
	// press. The widget hides itself; see UGSWeaponWheelWidget::HandleWheelOpenChanged.
	if (WeaponWheelWidgetClass && IsLocallyControlled())
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			WeaponWheelWidget = CreateWidget<UGSWeaponWheelWidget>(PC, WeaponWheelWidgetClass);
			if (WeaponWheelWidget)
			{
				// Above the HUD: the wheel is momentary, and half a wheel behind the objective list
				// is worse than no wheel.
				WeaponWheelWidget->AddToViewport(10);
			}
		}
	}

	// The order wheel's on-screen half (#141). ZOrder 11 - above the weapon wheel's 10 and the HUD's
	// 0. They are mutually exclusive by construction (see Input_OrderWheelOpen), so the ordering only
	// matters for the frame in which one is closing as the other opens.
	if (HordeOrderWheelWidgetClass && IsLocallyControlled())
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			HordeOrderWheelWidget = CreateWidget<UGSHordeOrderWheelWidget>(PC, HordeOrderWheelWidgetClass);
			if (HordeOrderWheelWidget)
			{
				HordeOrderWheelWidget->AddToViewport(11);
			}
		}
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
		if (HornAbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(HornAbilityClass, 1, INDEX_NONE, this));
		}
		if (BowShotAbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(BowShotAbilityClass, 1, INDEX_NONE, this));
		}
		if (GrappleThrowAbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(GrappleThrowAbilityClass, 1, INDEX_NONE, this));
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

	// The guard can drop without the player releasing the button - a guard break, the recoil from a
	// blocked swing, death - and every one of those ends the block through the ability system rather
	// than through input. UpdateRotationMode is only called from the five input paths, so without
	// this the body would stay locked to the camera after a guard the player never lowered. Edge
	// -triggered rather than called every frame: the work is trivial but it also writes ASC tags.
	const bool bBlockingNow = IsBlocking();
	if (bBlockingNow != bWasBlockingLastFrame)
	{
		bWasBlockingLastFrame = bBlockingNow;
		UpdateRotationMode();
	}

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

	const bool bSettled = FMath::IsNearlyEqual(CameraAimAlpha, Target);

	// Settled AND fully hip: write nothing. This is why the blend is an explicit alpha rather than an
	// FInterpTo - an asymptotic interp never equals its target, so this early-out could never fire and
	// the boom would be rewritten every frame of the entire raid for no visible change.
	//
	// Settled-while-AIMING no longer returns, though: the pitch correction below has to track the
	// camera every frame, and it is only ever needed while aiming. So the "don't touch the boom all
	// raid" guarantee is kept exactly where it mattered and dropped exactly where it was wrong.
	if (bSettled && CameraAimAlpha <= 0.f)
	{
		return;
	}

	if (!bSettled)
	{
		const float Step = (AimBlendSeconds > 0.f) ? (DeltaSeconds / AimBlendSeconds) : 1.f;
		CameraAimAlpha = (Target > CameraAimAlpha)
			? FMath::Min(CameraAimAlpha + Step, Target)
			: FMath::Max(CameraAimAlpha - Step, Target);
	}

	// Eased on READ, not stored eased: storing the eased value would feed it back into the next
	// frame's easing and the blend would decelerate twice.
	const float Eased = FMath::InterpEaseInOut(0.f, 1.f, CameraAimAlpha, 2.f);

	// --- pitch correction ------------------------------------------------------------------------
	// Lobbing a torch means aiming UP, and a spring arm swings its far end DOWN by ArmLength*sin(pitch)
	// when you do. On the 250uu aim arm at 45 degrees that is ~177uu below the boom pivot, which is
	// below the ground - so bDoCollisionTest yanks the camera in against the goblin's back and the
	// shot becomes unaimable. Michael reported it as "aiming to arch it correctly brings the camera
	// into the ground", 2026-08-06, and it is the reason the torch is awkward to throw.
	//
	// Fixed by lifting the pivot UP - and LENGTHENING the arm - as pitch rises, rather than by
	// clamping how far up you may look. A clamp would cap the arc itself, and the arc is the weapon.
	//
	// The first version pulled the arm IN instead, which is the cheaper way to buy ground clearance
	// since the drop scales with arm length. It was the wrong trade: it put the goblin's back in the
	// frame at exactly the moment you are trying to read an arc past him. Clearance is the lift's job
	// now, and the arm grows so the model shrinks. Scaled by Eased as well as by pitch, so none of
	// this leaks into the hip camera.
	float PitchAlpha = 0.f;
	if (AimHighPitchDegrees > 0.f)
	{
		const float PitchDeg = FRotator::NormalizeAxis(GetControlRotation().Pitch);
		PitchAlpha = FMath::Clamp(PitchDeg / AimHighPitchDegrees, 0.f, 1.f);
	}
	const float Correction = PitchAlpha * Eased;

	FVector Socket = FMath::Lerp(HipSocketOffset, AimSocketOffset, Eased);
	Socket.Z += FMath::Lerp(0.f, AimHighPitchLift, Correction);

	CameraBoom->TargetArmLength =
		FMath::Lerp(HipArmLength, AimArmLength, Eased) * FMath::Lerp(1.f, AimHighPitchArmScale, Correction);
	CameraBoom->SocketOffset = Socket;
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
		else
		{
			// This branch is not hypothetical: InteractAction has been unset on BP_GSPlayerCharacter
			// since the day it was written (#061), so Input_InteractStart - the only caller of the
			// interact ability anywhere - has NEVER fired. Loot, takedown, foul-well, extract and the
			// whole carry state ride on it, which is five systems held shut by one empty CDO field.
			// It survived because a missing binding is silent: the guard skips, nothing warns, and
			// the framework reads as "written" forever. Say it loudly instead.
			UE_LOG(LogTemp, Warning, TEXT("[GS.Input] InteractAction is unset on %s - interact is DEAD. "
				"No loot, no takedown, no foul-well, no extract, no carry. Assign IA_Interact on the "
				"character Blueprint's Class Defaults (#061, #161)."),
				*GetName());
		}

		// Torch: hold to aim, release to throw. Canceled is bound as well as Completed because a
		// focus loss mid-hold fires Canceled only, and without it the goblin would be left aiming
		// forever with no way to resolve the throw.
		EIC->BindAction(ThrowTorchAction, ETriggerEvent::Started, this, &AGSPlayerCharacter::Input_ThrowTorchStart);
		EIC->BindAction(ThrowTorchAction, ETriggerEvent::Completed, this, &AGSPlayerCharacter::Input_ThrowTorchRelease);
		EIC->BindAction(ThrowTorchAction, ETriggerEvent::Canceled, this, &AGSPlayerCharacter::Input_ThrowTorchRelease);
		EIC->BindAction(SwapWeaponModeAction, ETriggerEvent::Started, this, &AGSPlayerCharacter::Input_SwapWeaponMode);
		EIC->BindAction(WeaponWheelAction, ETriggerEvent::Started, this, &AGSPlayerCharacter::Input_WheelOpen);
		EIC->BindAction(WeaponWheelAction, ETriggerEvent::Completed, this, &AGSPlayerCharacter::Input_WheelClose);
		EIC->BindAction(WeaponWheelAction, ETriggerEvent::Canceled, this, &AGSPlayerCharacter::Input_WheelClose);
		EIC->BindAction(AimAction, ETriggerEvent::Started, this, &AGSPlayerCharacter::Input_AimStart);
		EIC->BindAction(AimAction, ETriggerEvent::Completed, this, &AGSPlayerCharacter::Input_AimStop);
		EIC->BindAction(AimAction, ETriggerEvent::Canceled, this, &AGSPlayerCharacter::Input_AimStop);
		EIC->BindAction(CrouchAction, ETriggerEvent::Started, this, &AGSPlayerCharacter::Input_ToggleCrouch);

		// The war-horn (#069). Guarded, like the block above it and unlike the unguarded run from
		// ThrowTorchAction down - an unset TObjectPtr<UInputAction> here would crash on BindAction
		// rather than politely doing nothing, and this project has shipped an unset input action
		// twice already (InteractAction from the day it was written, JumpAction in #060).
		if (HornAction)
		{
			EIC->BindAction(HornAction, ETriggerEvent::Started, this, &AGSPlayerCharacter::Input_Horn);
			// Completed, NOT a Hold trigger on IA_Horn. #207/#208 cost a session to the fact that a Hold
			// trigger does not stop the Started pin firing; press-and-release as two bindings is the
			// shape that actually behaves, and it needs no change to IMC_Default.
			EIC->BindAction(HornAction, ETriggerEvent::Completed, this, &AGSPlayerCharacter::Input_HornReleased);
		}

		// Planting the Warren (#245). Same press-and-release shape as the horn, and guarded with an
		// else-branch for the same reason: BindAction does not assert on a null action in 5.8.
		if (PlaceWarrenAction)
		{
			EIC->BindAction(PlaceWarrenAction, ETriggerEvent::Started, this, &AGSPlayerCharacter::Input_PlaceWarren);
			EIC->BindAction(PlaceWarrenAction, ETriggerEvent::Completed, this, &AGSPlayerCharacter::Input_PlaceWarrenReleased);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[GoblinSiege] %s has no PlaceWarrenAction assigned - holding T will do NOTHING. "
					 "Assign IA_PlaceWarren on the Blueprint CDO."), *GetName());
		}

		// The order wheel (#141). Guarded and with an else-branch, for the reason spelled out on
		// JumpAction below: BindAction does not assert on a null action in 5.8, it registers a
		// binding that never fires, and this project has now shipped that bug three times.
		if (HordeOrderAction)
		{
			EIC->BindAction(HordeOrderAction, ETriggerEvent::Started, this, &AGSPlayerCharacter::Input_OrderWheelOpen);
			EIC->BindAction(HordeOrderAction, ETriggerEvent::Completed, this, &AGSPlayerCharacter::Input_OrderWheelClose);
			EIC->BindAction(HordeOrderAction, ETriggerEvent::Canceled, this, &AGSPlayerCharacter::Input_OrderWheelClose);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[GS.Input] HordeOrderAction is unset on %s - the horde order "
				"wheel is unreachable. Assign IA_HordeOrder on the character Blueprint (#141)."),
				*GetNameSafe(this));
		}

		// Jump straight to ACharacter's own handlers. Started/Completed rather than a single pin
		// because StopJumping is what ends the variable-height hold - bind only Started and every
		// jump is a full-height jump regardless of how briefly the key was tapped.
		//
		// Guarded for the reason the horn comment above gives, which named THIS property as one of
		// the two that had already shipped unset - and then bound it unguarded three lines later.
		// BindAction does not assert on a null action in 5.8; it registers a binding that never
		// resolves, so an unset JumpAction is a dead space bar with no log, no error and nothing to
		// search for. The warning turns the third occurrence of that bug into a one-line diagnosis.
		if (JumpAction)
		{
			EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
			EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
			EIC->BindAction(JumpAction, ETriggerEvent::Canceled, this, &ACharacter::StopJumping);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[GS.Input] JumpAction is unset on %s - the jump key will "
				"do nothing. Assign IA_Jump on the character Blueprint (see #060, #116)."),
				*GetNameSafe(this));
		}

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

void AGSPlayerCharacter::HandleStaminaExhausted()
{
	// Only water is lethal. Running the pool dry on land costs you your sprint (the exhaustion latch
	// pins you to a walk) and on a wall it costs you your grip - neither kills. The GDD is explicit
	// that the failure state differs by medium, and this is the fork.
	const UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (MoveComp && MoveComp->IsSwimming())
	{
		Drown();
	}

	// The climb's "lose grip and fall" is NOT handled here. It lives in the Blueprint's
	// ClimbStaminaExits graph, which already owns the montage and the movement-mode change; adding a
	// second exit path in C++ would mean two things deciding when a climb ends.
}

void AGSPlayerCharacter::Drown()
{
	if (!HasAuthority() || !IsAlive())
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[GoblinSiege] %s drowned."), *GetName());

	// Destroy the cargo BEFORE dying. AGSCharacterBase::HandleDeath fires
	// UGSCarryComponent::HandleOwnerDied, which calls PutDown() and leaves the sack floating at the
	// point of death - visible loot in deep water that the player may not be able to reach. Clearing
	// it first means the normal drop path finds nothing to drop.
	if (CarryComponent && CarryComponent->IsCarrying())
	{
		CarryComponent->DestroyCarried();
	}

	// Reuse the one death path rather than inventing a second. This zeroes Health, which flows
	// through HandleDeath -> AGSGameMode::HandleGoblinDeath -> LoseLife() -> respawn, or
	// EndRaid(OutOfLives) if that was the last one.
	KillOutright();
}

void AGSPlayerCharacter::ApplyViewPitchLimits()
{
	// Why a clamp exists at all, having been argued against in #042.
	//
	// #042 rejected clamping because "the arc IS the weapon" and capping the look angle caps the
	// throw. That is still true up to about 45 degrees, which is the maximum-RANGE launch angle -
	// past it you are lobbing shorter, not further. What the clamp actually costs is the near-vertical
	// throw nobody aims for; what it buys is the guarantee the pitch correction cannot make on its
	// own, because AimHighPitchLift stops growing at AimHighPitchDegrees while sin(pitch) keeps
	// climbing. Beyond ~60 degrees the camera falls back toward the wheat (158uu on
	// L_Tutorial_Island, ~275,000 instances) and the throw becomes unaimable again.
	//
	// So: correction handles the useful range, the clamp handles the tail. Michael asked for exactly
	// this after testing - "we need a maximum distance it'll pitch down".
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->PlayerCameraManager)
	{
		// Not locally controlled yet, or an AI-possessed pawn. Re-applied from PossessedBy.
		return;
	}

	// Engine default is +/-89.9. Positive pitch is looking UP, which is what swings the boom DOWN.
	PC->PlayerCameraManager->ViewPitchMax = ViewPitchMaxDegrees;
	PC->PlayerCameraManager->ViewPitchMin = ViewPitchMinDegrees;
}

void AGSPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// BeginPlay can run before a controller exists (spawn order is not contractually fixed, and the
	// raid director respawns the pawn), so the limits are applied from both ends. Setting them twice
	// is free; setting them never is a camera that dives into the wheat on every respawn.
	ApplyViewPitchLimits();
}

void AGSPlayerCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D LookInput = Value.Get<FVector2D>();

	// While the wheel is open the mouse is CHOOSING, not looking. Routing the existing look axis
	// rather than adding a second 2D action is what lets the wheel ship with one new digital input:
	// the delta is already here, already frame-scaled by Enhanced Input, and already the thing the
	// player's hand is doing. Swallowing the camera movement is the point - a wheel you have to
	// aim at while the world spins under you is unusable.
	if (WeaponComponent && WeaponComponent->IsWheelOpen())
	{
		WeaponComponent->AddWheelInput(LookInput);
		return;
	}

	// The order wheel drags off the same axis, for the same reason (#141). Two consumers is the
	// ceiling this arrangement handles cleanly: they are kept mutually exclusive in the two open
	// handlers below, so exactly one of these branches can ever be live. A THIRD wheel would want a
	// real input-mode concept rather than a third early-return - note that before adding one.
	if (HordeCommandComponent && HordeCommandComponent->IsWheelOpen())
	{
		HordeCommandComponent->AddWheelInput(LookInput);
		return;
	}

	if (Controller)
	{
		AddControllerYawInput(LookInput.X);
		AddControllerPitchInput(LookInput.Y);
	}
}

void AGSPlayerCharacter::Input_WheelOpen(const FInputActionValue& Value)
{
	// Not while the order wheel is up. Both drag off Input_Look, so allowing both would hand the
	// player a weapon swap he never asked for every time he issued an order.
	if (HordeCommandComponent && HordeCommandComponent->IsWheelOpen())
	{
		return;
	}

	if (WeaponComponent)
	{
		WeaponComponent->OpenWeaponWheel();
	}
}

void AGSPlayerCharacter::Input_WheelClose(const FInputActionValue& Value)
{
	if (WeaponComponent)
	{
		WeaponComponent->CloseWeaponWheel(true);
	}
}

void AGSPlayerCharacter::Input_OrderWheelOpen(const FInputActionValue& Value)
{
	// The mirror of the guard in Input_WheelOpen.
	if (WeaponComponent && WeaponComponent->IsWheelOpen())
	{
		return;
	}

	if (HordeCommandComponent)
	{
		// Returns false when the camera trace found nowhere to send anybody, and in that case no
		// wheel opens at all - see OpenOrderWheel. Nothing to do about it here; a wheel that refuses
		// to appear while you are staring at the sky is the designed behaviour, not an error.
		HordeCommandComponent->OpenOrderWheel();
	}
}

void AGSPlayerCharacter::Input_OrderWheelClose(const FInputActionValue& Value)
{
	if (HordeCommandComponent)
	{
		HordeCommandComponent->CloseOrderWheel(true);
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
	// The torch is a HELD weapon now (Michael, 2026-08-06), so ATTACK throws it exactly as ATTACK
	// looses the bow - one key whose meaning follows what is in your hand. Checked before the bow
	// because the two are mutually exclusive slots and this ordering keeps the bow branch below
	// byte-identical to what it was.
	// The grapple is checked FIRST, and for the same reason the torch is checked before the bow:
	// the slots are mutually exclusive, so ordering the newest one at the top leaves every branch
	// below byte-identical to what it was. Same shape as the torch throughout - suppress the heavy
	// charge (a melee verb whose 1.5s timer would otherwise fire a sword swing out of a raised
	// hook), then activate by class.
	if (WeaponComponent && WeaponComponent->GetCurrentSlot() == GSTags::WeaponSlot_Grapple
		&& GrappleThrowAbilityClass)
	{
		bAttackHeld = false;
		bHeavyFiredThisHold = false;
		AttackPressedTime = -1.f;
		GetWorldTimerManager().ClearTimer(HeavyChargeTimer);
		OnHeavyChargeChanged.Broadcast(0.f);

		// A raised hook ends the draw. CancelDraw rather than a sample: the player did not choose to
		// loose, so nothing should be scored and no arrow should carry a quality from it.
		if (BowTimingComponent)
		{
			BowTimingComponent->CancelDraw();
		}

		if (AbilitySystemComponent)
		{
			AbilitySystemComponent->TryActivateAbilityByClass(GrappleThrowAbilityClass);
		}
		return;
	}

	if (WeaponComponent && WeaponComponent->GetCurrentSlot() == GSTags::WeaponSlot_Torch
		&& TorchTossAbilityClass)
	{
		// Same heavy-charge suppression the bow needs, and for the same reason: the heavy is a melee
		// verb and its timer would otherwise fire a sword swing out of a raised torch at 1.5s.
		bAttackHeld = false;
		bHeavyFiredThisHold = false;
		AttackPressedTime = -1.f;
		GetWorldTimerManager().ClearTimer(HeavyChargeTimer);
		OnHeavyChargeChanged.Broadcast(0.f);

		// Delegates to the existing torch press so there is ONE torch aim path, not a second copy
		// that drifts. It handles the bTorchAimEnabled fallback and reads the projectile class off
		// the ability CDO.
		if (BowTimingComponent)
		{
			BowTimingComponent->CancelDraw();
		}

		Input_ThrowTorchStart(Value);
		return;
	}

	if (IsRangedAttackMode())
	{
		// No heavy charge in ranged mode. The heavy is a melee verb, and leaving its timer running
		// would fire a sword swing out of a drawn bow at the 1.5s mark.
		bAttackHeld = false;
		bHeavyFiredThisHold = false;
		AttackPressedTime = -1.f;
		GetWorldTimerManager().ClearTimer(HeavyChargeTimer);
		OnHeavyChargeChanged.Broadcast(0.f);

		// ---- AN EMPTY QUIVER DOES NOT DRAW ---------------------------------------------------
		//
		// Michael, 2026-08-24: "you shouldn't be able to engage firing an arrow when you have 0
		// arrows." Before this the draw ran a full sweep and the aim arc came up for a shot the
		// ability would then refuse, so an empty bow behaved exactly like a BROKEN one.
		//
		// This is NOT the cooldown gate that was removed below, and it must not grow into one. That
		// gate was wrong because a 1.5s recovery is transient - nothing appearing for a moment reads
		// as the bow being broken. An empty quiver is the opposite: a persistent state the player can
		// see on the HUD, so refusing to draw reads as "I have no arrows" rather than as a fault.
		// Checked here rather than inside the ability because the draw and the aim arc start on the
		// PRESS, and the ability does not activate until the release.
		if (!UGSGA_BowShot::HasAmmoFor(this))
		{
			// Cancel any draw already running - the last arrow can be spent mid-sweep.
			if (BowTimingComponent)
			{
				BowTimingComponent->CancelDraw();
			}
			UE_LOG(LogTemp, Log,
				TEXT("[GoblinSiege] %s pressed attack with the bow out and no arrows - not drawing."),
				*GetName());
			return;
		}

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

		// ---- THE DRAW ALWAYS STARTS ----------------------------------------------------------
		//
		// This was gated on UGSGA_BowShot::GetFireCooldownRemaining, to stop the bar running a full
		// sweep for a shot the ability would then silently refuse. Michael found what that actually
		// felt like: press again straight after a release and NOTHING appears, which reads as the
		// bow being broken rather than as a weapon still recovering.
		//
		// The gate was also solving a problem that barely exists. RangedAttackCooldownSeconds is 1.5s
		// on every weapon and red sits at 2.7s into the sweep, so the interval has long expired by
		// the time any deliberate shot is loosed. Only a near-instant tap could still be refused, and
		// that shot would have been a minimum-damage yellow anyway.
		//
		// So: the sweep always runs, and the fire interval keeps being enforced where it always was,
		// inside the ability.
		if (BowTimingComponent)
		{
			BowTimingComponent->BeginDraw();
		}
		return;
	}

	// Reached only when none of the branches above claimed the press, i.e. this is a melee swing.
	// A draw still in flight belongs to a bow that is no longer in hand.
	if (BowTimingComponent)
	{
		BowTimingComponent->CancelDraw();
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
	// Torch, gated on being mid-aim for the same reason the bow is below: a player who opens the
	// wheel and swaps mid-throw should still resolve the throw they started. Delegates to the one
	// torch release path rather than duplicating the push-aim / end-aim / activate sequence.
	if (AimComponent && AimComponent->GetAimMode() == EGSAimMode::Torch)
	{
		Input_ThrowTorchRelease(Value);
		return;
	}

	// Bow first, and gated on actually being mid-aim rather than on the current weapon mode: a player
	// who swaps to melee while the bow is drawn should still resolve the shot they started, not have
	// the release silently become a sword swing.
	if (AimComponent && AimComponent->GetAimMode() == EGSAimMode::Bow)
	{
		// Push the exact aim BEFORE activating - see Input_ThrowTorchRelease for why.
		AimComponent->PushAimRotationToServer();
		AimComponent->EndAim();

		// SAMPLED HERE, BEFORE THE ABILITY RUNS, AND DELIBERATELY.
		//
		// Two reasons. The honest moment is the one the player judged - UGSGA_BowShot puts
		// ReleaseDelaySeconds between this and the arrow spawning, and charging them for 80ms of
		// release they cannot see would make a perfect shot feel stolen. And FireArrow reads the
		// verdict through GetLastReleaseQuality, which only holds a value once this has run.
		//
		// The return value is discarded on purpose: the component keeps it, and a shot the ability
		// REFUSES resets it rather than leaving a perfect score lying around for the next arrow.
		if (BowTimingComponent)
		{
			BowTimingComponent->ConsumeReleaseQuality();
		}

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

void AGSPlayerCharacter::Input_Horn(const FInputActionValue& Value)
{
	if (!AbilitySystemComponent)
	{
		return;
	}
	if (!HornAbilityClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] Horn pressed but HornAbilityClass is unset on %s. It is C++-defaulted "
				 "to UGSGA_Horn, so an empty value here means a Blueprint cleared it."),
			*GetName());
		return;
	}

	// A refused activation is the normal case, not an error: UGSGA_Horn blocks on its own State.Horn
	// tag so a held button cannot summon a wave per frame, and it blocks on State.Carrying because a
	// goblin with a pig over its shoulder has no free hand for a horn.
	AbilitySystemComponent->TryActivateAbilityByClass(HornAbilityClass);
}

void AGSPlayerCharacter::Input_PlaceWarren(const FInputActionValue& Value)
{
	if (WarrenPlacementComponent)
	{
		WarrenPlacementComponent->BeginPlacement();
	}
}

void AGSPlayerCharacter::Input_PlaceWarrenReleased(const FInputActionValue& Value)
{
	if (WarrenPlacementComponent)
	{
		WarrenPlacementComponent->ConfirmPlacement();
	}
}

void AGSPlayerCharacter::Input_HornReleased(const FInputActionValue& Value)
{
	if (!AbilitySystemComponent || !HornAbilityClass)
	{
		return;
	}

	// Reach the LIVE instance, not the CDO. UGSGA_Horn is InstancedPerActor, so the spec's primary
	// instance is the object actually running the blast; writing the flag on the CDO would set it on
	// a template nobody is executing and the stream would never stop.
	const FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromClass(HornAbilityClass);
	if (!Spec)
	{
		return;
	}

	if (UGSGA_Horn* Horn = Cast<UGSGA_Horn>(Spec->GetPrimaryInstance()))
	{
		Horn->NotifyHornReleased();
	}
}

void AGSPlayerCharacter::Input_BlockStart(const FInputActionValue& Value)
{
	// One implementation for everybody. #069 hoisted the guard onto AGSCharacterBase so the player,
	// the defenders (via GrantCombatAbilities) and BTTask_Block all raise it the same way; these
	// handlers stay only because they are what the input bindings point at.
	StartBlocking();
}

void AGSPlayerCharacter::Input_BlockStop(const FInputActionValue& Value)
{
	// AGSCharacterBase::StopBlocking cancels by TAG, never CancelAbilities(nullptr) - nullptr means
	// "cancel everything", which would abort a swing already in flight every time the guard dropped.
	StopBlocking();
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
	if (!WeaponComponent)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] Weapon swap pressed on %s but there is no WeaponComponent."),
			*GetName());
		return;
	}

	// ToggleRangedMode reports its own three refusal reasons; this only adds the one gate it
	// cannot see, because the ability class lives on the character rather than the weapon.
	WeaponComponent->ToggleRangedMode();

	// The nastiest silent failure in the whole ranged path, and it is reported HERE - the moment
	// the player asked for the bow - rather than on the attack press, because on the attack press
	// it would fire once per swing forever. With no bow ability assigned, IsRangedAttackMode()
	// returns false and Input_AttackPressed falls straight through to the MELEE branch: you stand
	// there holding a bow, swinging a sword, and the symptom reads as "the swap did not work"
	// rather than as "an ability class is unset".
	if (WeaponComponent->IsInRangedMode() && !BowShotAbilityClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] %s is now in RANGED mode but BowShotAbilityClass is unset - the "
				 "attack button will swing the sword instead of drawing the bow. Point it at "
				 "UGSGA_BowShot (or a Blueprint child) on the character Blueprint."),
			*GetName());
	}
}

// RIGHT MOUSE FOLLOWS THE WEAPON, the same rule Input_Attack already applies to the left button:
// one key whose meaning is whatever the equipped slot makes it, rather than a key the player has to
// remember only applies half the time. Sword -> raise the guard. Bow or torch -> aim.
//
// Michael's call, 2026-08-10: with the sword out this is PURELY a block - no aim camera, no
// strafe-facing - and G was retired as the block key, so blocking is deliberately impossible while
// holding bow or torch. Input_Block* below stays bound to IA_Block so the ability keeps a rebindable
// path even though IMC_Default no longer maps a key to it.
bool AGSPlayerCharacter::IsSwordEquipped() const
{
	// No weapon component at all means the sword is the sensible assumption - it is CurrentSlot's
	// own default, and a character that cannot answer the question should not silently lose its
	// guard.
	return !WeaponComponent || WeaponComponent->GetCurrentSlot() == GSTags::WeaponSlot_Sword;
}

void AGSPlayerCharacter::Input_AimStart(const FInputActionValue& Value)
{
	if (IsSwordEquipped())
	{
		StartBlocking();
		return;
	}

	bIsAiming = true;
	UpdateRotationMode();
}

void AGSPlayerCharacter::Input_AimStop(const FInputActionValue& Value)
{
	// Release BOTH, unconditionally, and do not branch on the current slot. Swapping weapons with
	// the button still held would otherwise strand whichever state was entered under the old slot:
	// press with the sword, wheel to the bow, release -> the guard never comes down. Clearing both
	// costs nothing when only one was ever set.
	StopBlocking();

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

		// The bar goes with the arc, for the same reason. Note this does NOT cancel the shot - the
		// release handler still resolves it, as the comment above intends - but it resolves as an
		// ordinary arrow: the player put the bow away, so there is no draw left to score.
		if (BowTimingComponent)
		{
			BowTimingComponent->CancelDraw();
		}
	}

	UpdateRotationMode();
}

void AGSPlayerCharacter::UpdateRotationMode()
{
	// 2026-08-04: now includes the aim component, which is what makes aiming a TORCH turn the body.
	// It did not before, and the result was that the arc was drawn along the control rotation while
	// the goblin faced his movement direction - you aimed one way and the character pointed another.
	const bool bShouldFaceAim = WantsAimFacing();

	// BLOCKING STEERS TOO, but it is not aiming. Michael, 2026-08-10: "I'd still like for you to be
	// able to change the direction of the block and camera when you press rmb, just don't zoom in
	// like we're aiming."
	//
	// This is exactly the split WantsAimFacing / WantsAimCamera already exists to express, so the
	// block joins the FACING term only and never the camera one - RMB with a sword turns the body
	// to the camera and leaves the arm length alone. It is not cosmetic: GSDamageExecCalculation
	// tests the block arc against GetActorForwardVector, so steering the body IS steering which
	// attacks the guard catches.
	//
	// The State.Aiming tag below deliberately stays on bShouldFaceAim rather than this. Nothing in
	// C++ reads that tag, which means a Blueprint might - a reticle is the obvious candidate - and
	// raising a guard should not put an aiming reticle on screen.
	// bCameraRelativeMovement makes this permanent rather than aim/block-only - see the property's
	// comment for why it is gated on the eight-way blendspace existing.
	const bool bShouldFaceLock = bCameraRelativeMovement || bShouldFaceAim || IsBlocking();

	bUseControllerRotationYaw = bShouldFaceLock;
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = !bShouldFaceLock;
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
