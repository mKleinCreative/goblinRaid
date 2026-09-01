// The thrown hook: arcs, sticks on impact, tells UGSGrappleHaulComponent it bit something, and
// lays its own rope back to the thrower. Replaces BP_GrappleHook (#390, 2026-08-31).
//
// BP_GrappleHook is retired, not patched. #388's live playtest found its HookMesh/RopeISM/RopeMesh
// nested under a Sphere root in the Blueprint's SCS, in a shape the engine's SCS validator silently
// DROPS on every load - the same corruption class that hit BP_Statue_Warrior, except here the
// dropped components fed real EventGraph logic (rope segment placement, `Set Start and End`), so
// the grapple has likely been losing its own rope/hook visuals on every load for a while. Native
// UPROPERTY subobjects created in a constructor cannot suffer that failure mode - there is no SCS
// tree for the validator to walk. Shaped on AGSTorchProjectile deliberately: same collision-sphere
// root, same soft-mesh-resolved-in-BeginPlay-with-a-warn-once-latch degrade, same
// stick-on-impact-and-report-to-a-component pattern.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSGrappleHookProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UProjectileMovementComponent;
class UStaticMesh;
class UMaterialInterface;

UCLASS()
class GOBLINSIEGE_API AGSGrappleHookProjectile : public AActor
{
	GENERATED_BODY()

public:
	AGSGrappleHookProjectile();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	/**
	 * Re-traces along the incoming path to find the point the rope should anchor on.
	 *
	 * UGSGrappleHaulComponent::NotifyHookAttached's own header measured the raw collision hit sitting
	 * proud of the visible art by a median of 40uu and up to 451uu, agreeing within 20uu only 37% of
	 * the time - anchoring on Hit.ImpactPoint directly would put the rope inside or floating outside
	 * whatever it stuck to. A short complex-collision trace through the impact, restricted to the
	 * struck actor, finds where the rendered surface actually is; falls back to Hit.ImpactPoint if
	 * that trace finds nothing (simple-collision-only geometry has no complex hull to find).
	 */
	FVector FindVisualAnchorPoint(const FHitResult& Hit) const;

	/**
	 * Lays NumSegments instances of RopeSegmentMeshAsset end to end from From to To.
	 *
	 * SM_Rope_Segment01's real bounding box was read back live via VibeUE Python (2026-08-31):
	 * 7.32 x 7.32 x 50.0 - its long axis is local Z. An earlier version of this function assumed
	 * local +X and 100uu, both wrong; that assumption oriented each instance's true 50uu-long axis
	 * straight up instead of along the rope, which is what a live playtest saw as "a row of sticks"
	 * planted in the grass, not a rope. RopeSegmentLengthUU below is now the read-back value, and the
	 * .cpp rotates local Z onto the rope direction to match.
	 *
	 * The pivot is at one END of that axis, not centered - also read back live (bounding box runs
	 * Z 0..50, not -25..25) after a first attempt assumed centered and left visible gaps between
	 * segments (a playtest screenshot showed why). Segments are positioned so the pivot sits at each
	 * interval's START, matching that.
	 */
	void RebuildRopeSegments(const FVector& From, const FVector& To);

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Grapple")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Grapple|Visual")
	TObjectPtr<UStaticMeshComponent> HookMesh;

	/** The rope. Instanced rather than a single stretched mesh so a curved/segmented look stays
	 *  available later without a rewrite - today's rope is straight (see
	 *  UGSGrappleHaulComponent::ConstrainToRope, horizontal-only and taut), so it renders as one
	 *  straight run of segments, but the component itself does not assume that. */
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Grapple|Visual")
	TObjectPtr<UInstancedStaticMeshComponent> RopeSegments;

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Grapple")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/**
	 * Hook head mesh. Unset by default - the old BP_GrappleHook's HookMesh reference could not be
	 * read (never open a .uasset directly; the editor/VibeUE MCP was not connected while this was
	 * written). Assign a mesh here (or on a thin Blueprint child of this class) before shipping;
	 * until then the hook is a functional but invisible collision volume, same degrade
	 * AGSTorchProjectile uses for TorchMesh.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Grapple|Visual")
	TSoftObjectPtr<UStaticMesh> HookMeshAsset;

	/** C++-defaulted to the project's existing rope prop (Content/Props/Rope) - the one dedicated
	 *  rope asset already in the project, and the same reasoning as AGSTorchProjectile::FireVolumeClass
	 *  for defaulting it here rather than leaving it for a content folder to lose. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Grapple|Visual")
	TSoftObjectPtr<UStaticMesh> RopeSegmentMeshAsset;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Grapple|Visual")
	TSoftObjectPtr<UMaterialInterface> RopeMaterialAsset;

	/** Authored length of RopeSegmentMeshAsset along local Z, in uu - read back live via VibeUE
	 *  (2026-08-31): bounding box Z 0..50. NOTE: the axis/pivot fixes in RebuildRopeSegments landed
	 *  in the same session this default was supposed to change to 50 and did not - it sat at the old
	 *  guessed 100 through two rebuilds while everything ELSE got fixed, which is why segments still
	 *  showed visible gaps after both the axis and pivot bugs were already corrected: at 100 here
	 *  against a real 50uu mesh, each segment only scales to ~60% of the spacing between them
	 *  regardless of how correctly it is placed or oriented. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Grapple|Visual", meta = (ClampMin = "1.0"))
	float RopeSegmentLengthUU = 50.f;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Grapple|Visual", meta = (ClampMin = "0.01"))
	float RopeThicknessScale = 1.f;

	/** Segments are stretched slightly past their true spacing so adjoining ends overlap rather than
	 *  exactly abut - cheap insurance against the centered-pivot assumption on RopeSegmentMeshAsset
	 *  being wrong (see RebuildRopeSegments), which reads as a visible gap between segments, i.e.
	 *  "a row of separate sticks" instead of one rope. 1.0 = no overlap. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Grapple|Visual", meta = (ClampMin = "1.0"))
	float RopeSegmentOverlapFactor = 1.2f;

	/** Where the rope's free end reads from on the thrower while attached - roughly hand height,
	 *  matching the fallback muzzle offset UGSGA_GrappleThrow uses when UGSAimComponent is absent. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Grapple|Visual")
	float RopeSourceHeightOffsetUU = 50.f;

	/**
	 * The rope's thrower-side end is smoothed toward the thrower's current position rather than
	 * snapped to it every tick (Michael, live playtest 2026-08-31: "the rope is going up and down").
	 * A taut rigid line faithfully redraws every frame's raw actor location, and
	 * UGSGrappleHaulComponent::ConstrainToRope deliberately leaves Z unclamped (see its own comment -
	 * clamping vertically would hoist the hauler toward the anchor) - so ordinary ground-snap /
	 * footstep micro-motion in CharacterMovementComponent shows up as visible vertical bounce along
	 * the whole rope. Higher = snappier and closer to the raw position; lower = smoother but laggier.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Grapple|Visual", meta = (ClampMin = "0.1"))
	float RopeSourceSmoothingSpeed = 8.f;

	bool bAttached = false;

	/** The goblin who threw this hook. Weak: nothing here should crash if the thrower dies mid-flight
	 *  or mid-haul - UGSGrappleHaulComponent::HandleOwnerHealthChanged already drops the haul in that
	 *  case, and this actor is destroyed along with it (see ReleaseHook). */
	TWeakObjectPtr<AActor> ThrowingOwner;

	FVector AttachedAnchorPoint = FVector::ZeroVector;

	/** Smoothed thrower-side rope end - see RopeSourceSmoothingSpeed. Seeded from the raw position on
	 *  the first tick after attach so the rope does not lerp in from the origin. */
	FVector SmoothedRopeSource = FVector::ZeroVector;
	bool bRopeSourceSeeded = false;
};
