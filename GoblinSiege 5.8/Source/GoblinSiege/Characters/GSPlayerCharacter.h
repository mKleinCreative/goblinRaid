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
class UGSBowTimingComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UGSWarrenPlacementComponent;
class UGameplayAbility;

/** 0..1, reaching 1 at HeavyHoldSeconds. Broadcast every frame while the attack button is held. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnHeavyChargeChanged, float, ChargeAlpha);

UCLASS()
class GOBLINSIEGE_API AGSPlayerCharacter : public AGSCharacterBase
{
	GENERATED_BODY()

public:
	// AACFCharacter has NO default constructor - it takes an FObjectInitializer (ACFCharacter.h:54),
	// so the whole chain must pass one down (#223).
	AGSPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Weapon")
	UGSWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Warren")
	UGSWarrenPlacementComponent* GetWarrenPlacementComponent() const { return WarrenPlacementComponent; }

	/**
	 * Renamed from GetTargetingComponent in #223. AACFCharacter declares
	 * `UFUNCTION(BlueprintPure) UATSBaseTargetComponent* GetTargetingComponent() const`, and after the
	 * Phase 2a reparent ours became a same-named override with a DIFFERENT return type, which UHT
	 * rejects. Renaming rather than dropping the UFUNCTION keeps this callable from Blueprint and
	 * avoids hiding ACF's accessor in C++ - and it cost nothing: this had zero callers in Source and
	 * zero references in any .uasset.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Targeting")
	UGSTargetingComponent* GetGSTargeting() const { return TargetingComponent; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Interaction")
	UGSInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Interaction")
	UGSCarryComponent* GetCarryComponent() const { return CarryComponent; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Aim")
	UGSAimComponent* GetAimComponent() const { return AimComponent; }

	/** The bow's timing minigame. PLAYER ONLY, and that is the whole AI safety story - UGSGA_BowShot
	 *  is shared with BP_ErikaArcher, so anything added inside the ability would change every
	 *  defender archer too. FireArrow asks the avatar for this component and falls back to a
	 *  multiplier of 1.0 when it is absent, which it always is on AI. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Bow")
	UGSBowTimingComponent* GetBowTimingComponent() const { return BowTimingComponent; }

	/** World-space direction the dodge roll should launch toward: the last held movement input
	 *  resolved against camera yaw, or forward if the player dodges from a standstill. Used by
	 *  UGSGA_DodgeRoll so the roll reads as "where I was going," not always forward. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Movement")
	FVector GetDodgeDirection() const;

protected:
	virtual void BeginPlay() override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	/** The player's coin pouch is UGSScoreSubsystem's carried Loot total, not LootSackDropValue - a
	 *  guard drops a fixed purse, the player drops (and loses, unless recovered) whatever they've
	 *  actually accumulated. Reads AND resets the score, so a second death before the last pouch is
	 *  recovered does not double-drop the same points (#382 addendum, 2026-08-30 morning). */
	virtual int32 ConsumeLootSackDropValue() override;

	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	virtual void Tick(float DeltaSeconds) override;

	void Input_Dodge(const FInputActionValue& Value);

	/** Light and heavy share one button (2026-08-03 ruling: tap for light, hold 1.5s for heavy).
	 *  Pressed fires the light IMMEDIATELY and starts the charge timer; Released only tidies up.
	 *
	 *  THE LIGHT USED TO RESOLVE ON RELEASE (#345), and that single fact was most of what made
	 *  melee feel mushy: press-to-contact was "however long the player held the button" plus
	 *  WindupSeconds, so the swing appeared to lag the input by an amount the player themselves
	 *  varied. The old comment here called release-resolution "the unavoidable cost of putting two
	 *  attacks on one key". It is avoidable: the press starts a light, and the hold upgrades the
	 *  FOLLOW-UP rather than the swing already in flight, so nothing has to be predicted and no
	 *  visible animation is ever cancelled. */
	void Input_AttackPressed(const FInputActionValue& Value);
	void Input_AttackReleased(const FInputActionValue& Value);

	/** Fired by the charge timer at HeavyHoldSeconds, while the button is still down. */
	void TriggerHeavyAttack();

	/** The running light swing, or null when nothing is swinging. One lookup shared by the combo
	 *  buffer and the heavy hand-off, so "which instance owns the chain" is answered in one place. */
	class UGSGA_SwordLight* FindActiveSwing() const;

	/** Delivers a heavy queued mid-swing once the light ability that was in flight ends. Bound to
	 *  the ASC's OnAbilityEnded in BeginPlay.
	 *
	 *  Why a queue rather than activating the heavy on the spot: the heavy is a DIFFERENT ability
	 *  class from the light, so GAS would happily run both at once and the player would get two
	 *  overlapping damage windows out of one button. */
	void HandleAbilityEnded(const struct FAbilityEndedData& EndedData);

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

	/** Q down: open the radial wheel. While it is open, Input_Look feeds the drag instead of the
	 *  camera - see the comment there. */
	void Input_ToggleMap(const FInputActionValue& Value);

	void Input_WheelOpen(const FInputActionValue& Value);

	/** Q up: commit whatever sector is highlighted. Bound to Completed AND Canceled, so losing focus
	 *  mid-drag closes the wheel rather than leaving it stuck open eating the mouse. */
	void Input_WheelClose(const FInputActionValue& Value);

	/**
	 * R down: latch what the crosshair is on, then open the horde order wheel (#141).
	 *
	 * MUTUAL EXCLUSION WITH THE WEAPON WHEEL LIVES HERE, in the character, rather than as a
	 * dependency between the two components. Neither component learns about the other; the pawn that
	 * owns both is the only thing that has any business deciding that two wheels must not be open at
	 * once. Both wheels feed off Input_Look, and two live drag accumulators over one axis would give
	 * the player a weapon swap he did not ask for every time he issued an order.
	 */
	void Input_OrderWheelOpen(const FInputActionValue& Value);

	/** R up: commit the highlighted verb against the latched target. Bound to Completed AND Canceled,
	 *  for the reason Input_WheelClose gives. */
	void Input_OrderWheelClose(const FInputActionValue& Value);

	/** True when the right mouse button should mean BLOCK rather than AIM. Sword, or no weapon
	 *  component at all - see the comment on Input_AimStart. */
	bool IsSwordEquipped() const;

	/** Edge-detect for the guard, so UpdateRotationMode also runs when a block ENDS without the
	 *  player releasing the button (guard break, recoil, death). See Tick. */
	bool bWasBlockingLastFrame = false;

	void Input_AimStart(const FInputActionValue& Value);
	void Input_AimStop(const FInputActionValue& Value);
	void Input_ToggleCrouch(const FInputActionValue& Value);

	/** Blow the war-horn. Activates UGSGA_Horn, which owns the alarm promotion and the summon -
	 *  this only presses the button. */
	void Input_Horn(const FInputActionValue& Value);

	/** Middle mouse came up. Tells the live UGSGA_Horn instance to stop streaming goblins; a tap
	 *  that lands here before the wind-up finishes still delivers exactly one. */
	void Input_HornReleased(const FInputActionValue& Value);

	/** T down / T up. Two bindings rather than a Hold trigger, for the reason spelled out on the
	 *  horn: a Hold trigger does not stop the Started pin firing (#207/#208). */
	void Input_PlaceWarren(const FInputActionValue& Value);
	void Input_PlaceWarrenReleased(const FInputActionValue& Value);


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

	/**
	 * Camera-relative movement. TRUE (the default) makes the body follow the camera at all times, so
	 * the goblin strafes and backpedals instead of turning to face wherever it is walking: you keep
	 * looking at what you are looking at, and change facing with the mouse rather than with WASD.
	 *
	 * FALSE restores the previous behaviour - bOrientRotationToMovement, body turns to face travel,
	 * and aim/block are the only things that ever face-lock.
	 *
	 * THIS DEPENDS ON THE EIGHT-WAY LOCOMOTION BLENDSPACE. With movement-facing, Direction is pinned
	 * near zero and a single forward clip is always correct. With camera-facing it is not: the pawn
	 * genuinely moves sideways and backwards, and without directional locomotion it slides while
	 * playing a forward walk. Do not turn this on against a forward-only blendspace.
	 *
	 * It also changes combat, not just the camera: UGSDamageExecCalculation tests the block arc
	 * against GetActorForwardVector, so a permanently camera-locked body means the guard always
	 * points where the camera points (see the note above UpdateRotationMode's face-lock term).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GoblinSiege|Camera",
		meta = (AllowPrivateAccess = "true"))
	bool bCameraRelativeMovement = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Weapon")
	TObjectPtr<UGSWeaponComponent> WeaponComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Targeting")
	TObjectPtr<UGSTargetingComponent> TargetingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Interaction")
	TObjectPtr<UGSInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Interaction")
	TObjectPtr<UGSCarryComponent> CarryComponent;

	/** Plants the Warren on a held T (#245). A C++ default subobject rather than a Blueprint-added
	 *  component, so every player has one without anyone remembering to add it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Warren")
	TObjectPtr<UGSWarrenPlacementComponent> WarrenPlacementComponent;

	/** Owns the aim state, the predicted trajectory and the arc ribbon for every ranged verb. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Aim")
	TObjectPtr<UGSAimComponent> AimComponent;

	/** See GetBowTimingComponent. Ticks only while a draw is in flight. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Bow")
	TObjectPtr<UGSBowTimingComponent> BowTimingComponent;

	/** The order wheel and its one Server RPC (#141). On the pawn rather than on
	 *  UGSHordeSubsystem because a UWorldSubsystem has no NetRole and cannot host an RPC - see the
	 *  component's header for the other two reasons. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Horde")
	TObjectPtr<class UGSHordeCommandComponent> HordeCommandComponent;

	/**
	 * The stamina pool. BlueprintReadOnly so BP_GSPlayerCharacter's sprint and traversal graphs can
	 * call TryConsume / SetDrainRate on it instead of owning a float of their own.
	 *
	 * On AGSPlayerCharacter rather than AGSCharacterBase: only the player spends stamina today. AI
	 * melee is paced by UBTTask_MeleeAttack's own cooldown, and giving every militiaman a ticking
	 * stamina component would be per-frame work for a number nothing reads.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Stamina")
	TObjectPtr<class UGSStaminaComponent> StaminaComponent;

	/** Drowning: OnExhausted fires while swimming. Bound in BeginPlay. */
	UFUNCTION()
	void HandleStaminaExhausted();

	/**
	 * Drain stamina while the goblin is out of his depth. Called every frame.
	 *
	 * MICHAEL'S RULE, 2026-08-25: "anything further than waist deep will drown you. We can also make
	 * it canon that Goblins notoriously hate water."
	 *
	 * SO THE GATE IS IMMERSION, NOT MOVEMENT MODE. The engine puts a character into MOVE_Swimming the
	 * moment it touches a water volume at any depth, so draining on IsSwimming() would punish ankles
	 * in the village river. ImmersionDepth() is a 0..1 fraction of the capsule, so 0.5 is literally
	 * the waist and the rule reads exactly as he said it.
	 */
	void UpdateWaterDrain();

	/** How deep is too deep, as a fraction of the capsule. 0.5 is the waist. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Water", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DrowningImmersion01 = 0.5f;

	/** True while WE are the ones draining the pool, so leaving the water clears our drain and never
	 *  somebody else's - sprint sets this same rate from its own path. */
	bool bDrainingFromWater = false;

	/**
	 * Kills the goblin and destroys whatever they were carrying.
	 *
	 * Separate from a normal death because the loss rule differs: dying on land drops your sack
	 * where you fell (UGSCarryComponent::HandleOwnerDied -> PutDown), and drowning takes it with you.
	 * Michael's call, 2026-08-06.
	 */
	void Drown();

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

	/**
	 * How long the attack button must be held before the follow-up becomes a heavy.
	 *
	 * 0.35s, down from 1.5s (#345). The old value was chosen when the light resolved on release, so
	 * the hold had to be long enough that a normal tap could never reach it. Now the light has
	 * already gone out by the time this timer is running, the threshold only has to be longer than
	 * a deliberate tap - and 1.5s of nothing happening is what made the heavy read as "not bound to
	 * anything". Keep it comfortably below the light chain's own stage length or the heavy will
	 * always arrive as the third swing rather than the second.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input", meta = (ClampMin = "0.1"))
	float HeavyHoldSeconds = 0.35f;

	/** Hold to guard. Bound to Started and Completed/Canceled - the guard is up exactly as long as
	 *  the button is down. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> BlockAction;

	/** Guard break - X. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> GuardBreakAction;

	/**
	 * Radial weapon wheel - Q. Hold, drag a direction, release to commit (Michael's design, settled).
	 *
	 * A plain digital action: the DIRECTION comes from the look axis while the wheel is open, so
	 * there is no second 2D action to create and no cursor to warp. Leave it unset and the wheel is
	 * simply unreachable - every other input keeps working, including the sword/bow swap key.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> WeaponWheelAction;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> ThrowTorchAction;

	/**
	 * M. Opens and closes the objective board, and later the map it turns into (#311).
	 *
	 * Tab was the obvious key and is already IA_SwapWeaponMode - Michael: "right now, tab goes
	 * betweens weapons". Leave this unset and the board simply never opens; nothing else changes.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> MapAction;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> SwapWeaponModeAction;

	/** Hold to aim - forces aim-facing rotation while held (tech doc §16). */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> AimAction;

	/** The stealth stance toggle - universal kit (design doc §7). */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> CrouchAction;

	/**
	 * The war-horn - universal kit (GDD §2.3), bound to MIDDLE MOUSE.
	 *
	 * Not G, which both design docs say: G is IA_Block since the 2026-08-06 movement remap (#058),
	 * which was Michael's own explicit ask, and IMC_Default has 17 rows across 17 distinct keys with
	 * nothing doubled up. Michael's ruling 2026-08-07 put the horn on the mouse rather than move a
	 * binding he had just set. The GDD's "G" is an erratum.
	 *
	 * MUST BE ASSIGNED ON THE BLUEPRINT CDO. A TObjectPtr<UInputAction> does not fill itself in, and
	 * an unset one here means the horn silently never fires - the exact failure InteractAction and
	 * JumpAction (#060) both shipped with.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> HornAction;

	/**
	 * Hold T to plant the Warren. MUST BE ASSIGNED ON THE BLUEPRINT CDO, same as HornAction - an
	 * unset TObjectPtr<UInputAction> does not assert in 5.8, it registers a binding that never
	 * fires, and this project has shipped that exact bug three times.
	 *
	 * T, not X: X is IA_GuardBreak and always was.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> PlaceWarrenAction;

	/**
	 * Jump - SpaceBar. STRICTLY a jump: never a vault, never a mantle, never a climb (Michael,
	 * 2026-08-06). Those three moved to E, where the Blueprint's existing 120/217 height tiers
	 * already choose between them.
	 *
	 * Bound straight to ACharacter::Jump / StopJumping. No ability, no montage, no stamina cost - a
	 * jump you have to afford is a different design, and the GDD only prices the CLIMB jump.
	 *
	 * TUNING LIVES ELSEWHERE, and this is the trap: distance is `JumpZVelocity` (which sets airtime)
	 * multiplied by the horizontal speed you carry in, plus `AirControl` for mid-air steering. Both
	 * are on BP_GSPlayerCharacter's CharacterMovement, which OVERRIDES the C++ defaults - the
	 * Blueprint holds JumpZVelocity 490.5 where the engine default is 420, which proves it is an
	 * override. Setting either in code changes nothing while that override stands. Measured
	 * 2026-08-06: at 490.5 / AirControl 0.05 a sprint-jump covers ~819uu and steers barely at all;
	 * 620 / 0.35 gives ~1035uu and is the value Michael was testing when he asked for more distance.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> JumpAction;

	/** Hold-E interact (GDD §8). Bound to Started and Completed/Canceled - the channel runs exactly
	 *  as long as the key is down. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> InteractAction;

	/**
	 * The horde order wheel (#141) - hold to open, drag to pick a verb, release to commit. Bound to R.
	 *
	 * R rather than a mouse button: LMB/RMB/MMB are attack, block and the horn, and the horn is the
	 * thing you press immediately BEFORE you want this - putting them on the same button would make
	 * "summon then order" one gesture with two meanings. Q is the weapon wheel, so R sits next to it
	 * and reads as its sibling.
	 *
	 * MUST BE ASSIGNED ON THE BLUEPRINT CDO, and SetupPlayerInputComponent logs loudly if it is not.
	 * An unset TObjectPtr<UInputAction> does not crash BindAction in 5.8 - it registers a binding that
	 * never fires - which is a dead key with no error and nothing to search for. This project has
	 * shipped that exact bug three times (InteractAction, JumpAction in #060, again in #116).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> HordeOrderAction;

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

	// BlockAbilityClass and GuardBreakAbilityClass were declared HERE until #069 (2026-08-07) and
	// are now INHERITED from AGSCharacterBase, which grew the shared combat-ability slots when the
	// verbs were hoisted off AGSEnemyCharacter. Re-declaring them here would not merely double-grant
	// - two UPROPERTYs of one name in a class hierarchy is a UnrealHeaderTool error, so this is not
	// optional tidying. Every use in GSPlayerCharacter.cpp resolves to the inherited property
	// unchanged, and BP_GSPlayerCharacter's saved values follow the name, not the declaring class.
	//
	// Guard break is still not C++-defaulted, for the reason the old comment gave: a default would
	// silently hand it the light attack's stage array and it would behave like a second light swing.
	//
	// NOTE the player deliberately keeps its OWN SwordLightAbilityClass / SwordHeavyAbilityClass
	// above rather than using the base's LightAttackAbilityClass / HeavyAttackAbilityClass - the
	// names differ, so there is no collision, and the player's two are driven by the weapon wheel
	// rather than by GrantCombatAbilities().

	/** Universal interact channel - C++-defaulted to UGSGA_Interact, same reasoning as
	 *  SwordLightAbilityClass: a framework verb nobody remembers to fill in is a framework nobody
	 *  can test. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Abilities")
	TSubclassOf<UGameplayAbility> InteractAbilityClass;

	/** The war-horn. C++-defaulted to UGSGA_Horn for the same reason as the interact channel: it is
	 *  universal kit every class carries (GDD §2.3), so leaving it for a Blueprint to remember is
	 *  how it ends up unset on the one character that matters. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Abilities")
	TSubclassOf<UGameplayAbility> HornAbilityClass;

	/** The Scout's ranged half. Routed from the ATTACK button while the weapon component reports
	 *  ranged mode - "sword <-> bow, live-swap" means one attack key whose meaning follows the mode,
	 *  not a second key the player has to remember. Not C++-defaulted, matching SwordHeavyAbilityClass:
	 *  point it at UGSGA_BowShot or a Blueprint child of it. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Abilities")
	TSubclassOf<UGameplayAbility> BowShotAbilityClass;

	/** The fourth wheel slot's verb (2026-08-17). Routed from the ATTACK button while the weapon
	 *  component reports the Grapple slot, exactly as the torch is - one attack key whose meaning
	 *  follows what is in your hand, rather than a fourth key to remember.
	 *
	 *  C++-defaulted, unlike BowShotAbilityClass, because the grapple has no second home: the bow
	 *  at least fails loudly into the sword, whereas an unset grapple class makes the newest wheel
	 *  slot do nothing at all. That is #048 and #088, twice. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Abilities")
	TSubclassOf<UGameplayAbility> GrappleThrowAbilityClass;

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
	 * Control pitch, in degrees, at which the aim camera's pitch correction is fully applied.
	 *
	 * A lobbed torch is aimed UP, and a spring arm answers that by swinging its far end DOWN by
	 * ArmLength * sin(pitch). At the 250uu aim arm, 45 degrees puts the camera ~177uu below the boom
	 * pivot - underground - so the collision probe drags it in against the goblin's back. 45 rather
	 * than 90 because a 45-degree launch is already the maximum-range throw; past that you are
	 * lobbing shorter, not further, so full correction by then is the useful shape.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Camera", meta = (ClampMin = "1.0", ClampMax = "89.0"))
	float AimHighPitchDegrees = 45.f;

	/**
	 * Arm-length multiplier at AimHighPitchDegrees. Above 1 LENGTHENS the arm as you look up.
	 *
	 * This was 0.5 for about ten minutes, on the reasoning that the camera's drop is proportional to
	 * arm length so pulling in is the cheapest way to keep it off the floor. That is true and it was
	 * the wrong trade: Michael, 2026-08-06 - "the player model crowds the camera a little too much" -
	 * and halving the arm makes the goblin fill the frame at exactly the moment you are trying to
	 * read an arc past him. Ground clearance now comes from AimHighPitchLift alone, and the arm grows
	 * instead, so the goblin gets SMALLER as you aim higher.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Camera", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float AimHighPitchArmScale = 1.15f;

	/**
	 * Extra SocketOffset.Z at AimHighPitchDegrees. This is now the whole ground-clearance mechanism.
	 *
	 * At the 250uu aim arm the camera falls 177uu at 45 degrees, from a pivot 165uu up - so it needs
	 * roughly 150uu of lift to sit at chest height instead of underfoot. Applied AFTER the collision
	 * probe (see the constructor's note), which is safe going UP in a way the -55 lateral offset is
	 * not: raising the camera vertically cannot push it through the wall behind the player.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Camera", meta = (ClampMin = "0.0"))
	float AimHighPitchLift = 190.f;

	/**
	 * Hard ceiling on how far UP the player may look, in degrees. Engine default is 89.9.
	 *
	 * Looking up is what swings the spring arm DOWN, and AimHighPitchLift stops growing at
	 * AimHighPitchDegrees while sin(pitch) does not - so past roughly 60 degrees the camera falls
	 * back toward the wheat canopy (158uu on L_Tutorial_Island, ~275,000 instances) and the throw
	 * stops being aimable. 55 keeps it clear with margin and costs only the near-vertical lob, which
	 * is past the max-range angle anyway.
	 *
	 * #042 argued against a clamp on the grounds that the arc IS the weapon. That holds up to the
	 * max-range angle; beyond it a clamp costs nothing real and is the only thing that bounds the
	 * tail, because the lift cannot keep growing forever without putting the camera in orbit.
	 *
	 * Applies to the hip camera too, which has the same geometry and a LONGER arm (450), so it dives
	 * harder for the same pitch - it has simply never been complained about, because the collision
	 * probe hides it as a pull-in rather than a clip.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Camera", meta = (ClampMin = "5.0", ClampMax = "89.0"))
	float ViewPitchMaxDegrees = 55.f;

	/** Floor on how far DOWN the player may look. Looking down RAISES the camera, so this is not a
	 *  ground-clearance concern - it only stops the boom swinging far enough overhead to stare
	 *  through the goblin's own skull. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Camera", meta = (ClampMin = "-89.0", ClampMax = "-5.0"))
	float ViewPitchMinDegrees = -70.f;

	/** Pushes the two limits above onto the player camera manager. Called from BeginPlay AND
	 *  PossessedBy, because either can run first. */
	void ApplyViewPitchLimits();

	/**
	 * The radial wheel's on-screen half. Set to WBP_WeaponWheel on BP_GSPlayerCharacter.
	 *
	 * Created once on BeginPlay for the locally controlled pawn and left in the viewport for the
	 * raid, collapsed until the wheel opens - the widget hides itself rather than being created and
	 * destroyed per gesture, because Q is pressed often and a construct/destruct cycle per press
	 * would rebind the delegates every time.
	 *
	 * Leave it unset and the wheel keeps working exactly as it does today: selection, mesh swap and
	 * the torch prop are all component-side. You simply drag blind, which is the state #039-#041
	 * shipped in.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|UI")
	TSubclassOf<class UGSWeaponWheelWidget> WeaponWheelWidgetClass;

	/** The live instance, kept so it is not garbage collected out from under the viewport. */
	UPROPERTY(Transient)
	TObjectPtr<class UGSWeaponWheelWidget> WeaponWheelWidget;

	/** The order wheel's on-screen half. Set to WBP_HordeOrderWheel on BP_GSPlayerCharacter.
	 *  Same lifetime rules as the weapon wheel above: created once, left collapsed in the viewport.
	 *  Leave it unset and orders still work perfectly - you simply drag blind. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|UI")
	TSubclassOf<class UGSHordeOrderWheelWidget> HordeOrderWheelWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<class UGSHordeOrderWheelWidget> HordeOrderWheelWidget;

	virtual void PossessedBy(AController* NewController) override;

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

	/** Set when the charge matured while a light swing was still in flight; the heavy is delivered
	 *  by HandleAbilityEnded when that swing finishes. Cleared on every fresh press, so a new input
	 *  supersedes an undelivered heavy rather than stacking behind it. */
	bool bHeavyQueuedThisHold = false;

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
	 *  can draw a charge ring - without some feedback the threshold is pure guesswork for the
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
