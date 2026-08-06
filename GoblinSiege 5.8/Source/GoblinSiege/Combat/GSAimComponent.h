// The one place the game answers "where does the shot come from, and where does it go".
//
// 2026-08-04. Before this existed, the torch throw answered that question TWICE: once in
// UGSGA_TorchToss::ThrowTorch (which spawned the projectile) and once in
// AGSPlayerCharacter::DrawTorchAimArc (which drew the preview), with a comment on the second
// reading "Must match UGSGA_TorchToss::ThrowTorch exactly, including the +50 hand-height fudge."
// Two copies of a spawn transform, kept in sync by a comment, is a bug with a delivery date - and
// the bug it delivers is the worst kind an aim indicator can have, which is lying about where the
// thing lands. Both callers now ask GetMuzzleTransform().
//
// It also carries the ARC VISUAL, which is why the preview can ship. The old one was
// PredictProjectilePath's own EDrawDebugTrace output plus a DrawDebugCircle, and debug draw is
// compiled out of a shipping build entirely - the "feature" was a playtest tool wearing a feature's
// clothes. This draws spline meshes and a decal, which are real primitives that survive packaging.
//
// One component serves the torch and the bow because they differ only in the numbers, and the
// numbers are read off the projectile's own CDO (see UpdatePrediction). A third ranged verb costs a
// projectile class and nothing here.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSAimComponent.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USplineMeshComponent;
class UStaticMesh;

/** Which ranged verb is being aimed. Replicated so remote clients can pose correctly - the ARC is
 *  never replicated, only the fact that someone is aiming and with what. */
UENUM(BlueprintType)
enum class EGSAimMode : uint8
{
	None	UMETA(DisplayName = "Not aiming"),
	Torch	UMETA(DisplayName = "Torch toss"),
	Bow		UMETA(DisplayName = "Bow shot"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnAimStateChanged, bool, bIsAiming);

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSAimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSAimComponent();

	virtual void TickComponent(float DeltaSeconds, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Start aiming a ranged verb. ProjectileClass is what the arc is predicted FROM - pass the exact
	 * class the ability will spawn, or the preview and the shot describe different objects.
	 *
	 * Safe to call repeatedly with the same mode (it re-arms the projectile class and nothing else),
	 * because the input handlers that call it are bound to Started on keys the player can mash.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Aim")
	void BeginAim(EGSAimMode Mode, TSubclassOf<AActor> ProjectileClass);

	/** Stop aiming and hide the arc. Idempotent - a release with no matching press is a no-op, which
	 *  matters because Enhanced Input fires Canceled as well as Completed. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Aim")
	void EndAim();

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Aim")
	bool IsAiming() const { return AimMode != EGSAimMode::None; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Aim")
	EGSAimMode GetAimMode() const { return AimMode; }

	/**
	 * Where the projectile is born, and pointing where. THE single definition - both the arc and
	 * every ranged ability's spawn call this, so they cannot disagree.
	 *
	 * Rotation is GetAimRotation(), which is not always the control rotation - read that function's
	 * comment before assuming it is.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Aim")
	FTransform GetMuzzleTransform() const;

	/**
	 * The direction the shot should actually travel.
	 *
	 * On the owning client this is simply the control rotation. ON THE SERVER, for a pawn driven by
	 * a REMOTE client, it is the rotation that client explicitly sent us - because
	 * APawn::RemoteViewPitch, which is what the server's GetControlRotation() falls back on for a
	 * remote pawn, is quantised to a single byte. That is about 1.4 degrees per step, and over a
	 * three-second torch lob 1.4 degrees is metres of error between the arc the player aimed with
	 * and the spot the torch lands on. Yaw survives replication at full precision; pitch does not,
	 * and pitch is the entire game of a lobbed projectile.
	 *
	 * Falls back to the control rotation whenever no explicit rotation has been received, so
	 * single-player, listen-server hosts and AI pawns all take the old path unchanged.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Aim")
	FRotator GetAimRotation() const;

	/** Sends the current control rotation to the server so it can spawn along the exact direction
	 *  this client aimed. Call immediately BEFORE activating a ranged ability, not after. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Aim")
	void PushAimRotationToServer();

	/** Fired when aiming starts and stops. Drives the character's camera blend and rotation mode, so
	 *  nothing has to poll IsAiming() every frame to notice a change. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Aim")
	FGSOnAimStateChanged OnAimStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Runs PredictProjectilePath from the muzzle using the armed projectile's own speed and gravity,
	 *  then hands the result to the visual. Called every frame while aiming, on the owning client
	 *  only - nobody else needs a prediction. */
	void UpdatePrediction();

	// ---- arc visual ----------------------------------------------------------------------
	// Spline meshes and a decal rather than DrawDebug*, so the preview exists in a packaged build.
	// Created lazily on the first aim and REUSED for the life of the component, never destroyed and
	// rebuilt per aim: this is the same reuse-don't-churn argument UGSWeaponComponent makes for the
	// weapon meshes, and it applies harder here because the player taps this key constantly.

	/** Points the ribbon at Path, showing exactly as many segments as it needs and hiding the rest.
	 *  Also places the landing decal, or hides it when the shot lands on nothing. */
	void UpdateArcVisual(const TArray<FVector>& Path, bool bHit, const FVector& ImpactPoint,
		const FVector& ImpactNormal);

	/** Hides every segment and the decal without destroying anything. */
	void HideArcVisual();

	/** Creates the pool and the decal on first use. Returns false if this is not the locally
	 *  controlled pawn, which is the case the whole visual must never run in. */
	bool EnsureArcVisual();

	/** Tint for the current mode, applied to the ribbon and decal dynamic materials. */
	FLinearColor GetArcColourForMode() const;

	UFUNCTION(Server, Unreliable, WithValidation)
	void Server_SetAimRotation(FRotator NewAimRotation);

	// ---- muzzle -------------------------------------------------------------------------

	/** Forward distance from the pawn's origin to the muzzle. Was a literal 80 in two files. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Aim|Muzzle")
	float MuzzleForwardOffset = 80.f;

	/** Height above the pawn's origin - roughly hand height on the goblin rig. Was a literal 50 in
	 *  two files, one of which called it a "fudge" in a comment. It is still a fudge; it is now a
	 *  fudge in one place, and one a designer can move without a rebuild. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Aim|Muzzle")
	float MuzzleHeightOffset = 50.f;

	// ---- prediction ---------------------------------------------------------------------

	/** How far ahead to simulate. A torch at 1400uu/s under full gravity is done well inside 3s;
	 *  an arrow at 6000uu/s under 0.2 gravity is still travelling, and that is fine - the arc simply
	 *  ends off in the distance, which is an honest picture of a shot that hits nothing. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Aim|Prediction", meta = (ClampMin = "0.2"))
	float MaxSimSeconds = 3.f;

	/** Simulation steps per second. Higher is smoother and costs a trace each. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Aim|Prediction", meta = (ClampMin = "5.0"))
	float SimFrequency = 15.f;

	/** Radius of the swept prediction trace. Should approximate the projectile's collision sphere -
	 *  8 for the torch, 6 for the arrow - so the arc stops where the projectile would. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Aim|Prediction", meta = (ClampMin = "0.0"))
	float PredictionTraceRadius = 8.f;

	// ---- visual ---------------------------------------------------------------------------

	/** Mesh tiled along the arc. A unit cylinder (/Engine/BasicShapes/Cylinder) works; anything whose
	 *  local +X runs its length will. Unset means no ribbon and a warning, once. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Aim|Visual")
	TSoftObjectPtr<UStaticMesh> ArcSegmentMesh;

	/** Applied to every arc segment. Wants to be unlit and translucent with a vector parameter named
	 *  by ArcColourParameterName. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Aim|Visual")
	TSoftObjectPtr<UMaterialInterface> ArcMaterial;

	/** Deferred decal material for the landing ring. A DECAL and not a flat mesh on purpose: the
	 *  DrawDebugCircle this replaces drew a horizontal disc, which floats above or sinks into any
	 *  ground that is not level - and a landing indicator that is wrong on a hillside is wrong
	 *  exactly where the player needed it. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Aim|Visual")
	TSoftObjectPtr<UMaterialInterface> LandingDecalMaterial;

	/** Vector parameter tinted on both materials. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Aim|Visual")
	FName ArcColourParameterName = TEXT("Colour");

	/** Ceiling on the segment pool. Path points beyond this are dropped, which shortens the drawn
	 *  arc rather than dropping frames. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Aim|Visual", meta = (ClampMin = "4"))
	int32 MaxArcSegments = 40;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Aim|Visual", meta = (ClampMin = "0.1"))
	float ArcSegmentWidth = 4.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Aim|Visual")
	FVector LandingDecalSize = FVector(40.f, 55.f, 55.f);

	/** Torch orange - carried over verbatim from AGSPlayerCharacter::TorchAimArcColour, which this
	 *  component replaced. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Aim|Visual")
	FLinearColor TorchArcColour = FLinearColor(1.f, 0.45f, 0.1f, 1.f);

	/** Pale and cold, so a bow shot never reads as "this will set something on fire". */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Aim|Visual")
	FLinearColor BowArcColour = FLinearColor(0.75f, 0.85f, 0.95f, 1.f);

	/** Replicated so a remote client can play an aim pose. The arc itself is never replicated. */
	UPROPERTY(ReplicatedUsing = OnRep_AimMode)
	EGSAimMode AimMode = EGSAimMode::None;

	UFUNCTION()
	void OnRep_AimMode();

private:
	/** The class the current aim is predicting. Weak in spirit - cleared by EndAim. */
	UPROPERTY(Transient)
	TSubclassOf<AActor> ArmedProjectileClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> ArcSegments;

	UPROPERTY(Transient)
	TObjectPtr<UDecalComponent> LandingDecal;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ArcMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LandingMID;

	/** Resolved once in EnsureArcVisual. Null with bVisualResolveFailed set means "we tried, we
	 *  warned, we are not trying again" - LoadSynchronous does not cache a failure, and retrying it
	 *  every frame while aiming would be a package lookup per frame. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> ResolvedArcMesh;

	bool bVisualResolveFailed = false;
	bool bVisualBuilt = false;

	/** Set by Server_SetAimRotation. Only meaningful on the server for a remotely-controlled pawn;
	 *  see GetAimRotation. */
	FRotator ReplicatedAimRotation = FRotator::ZeroRotator;
	bool bHasReplicatedAimRotation = false;

	/** True on the machine that owns this pawn's input. The arc is built only here. */
	bool IsLocallyControlledPawn() const;

	/** Reused across frames so a per-frame prediction does not churn an allocation. */
	TArray<FVector> ScratchPathPoints;
};
