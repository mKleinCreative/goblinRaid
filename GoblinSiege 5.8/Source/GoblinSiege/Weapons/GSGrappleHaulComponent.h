// The heave. Owns "am I pulling on a rope, and how hard", and nothing about what is on the far end.
//
// Michael chose the input, 2026-08-18: walking AWAY from the anchor is what builds tension. No new
// binding, and it puts the player's body into it - you lean away from the idol and it comes down on
// you. Holding a button would have read as winching, which is a different and duller image.
//
// ---------------------------------------------------------------------------------------------
// WHY THE HAUL LIVES ON THE PLAYER AND NOT ON THE STATUE
//
// Distance is a property of the pair, but PROGRESS is a property of the player: it drives their HUD
// ring, it must survive the statue being destroyed mid-haul, and a second goblin hauling a second
// monument must not share a counter with the first. The statue owns what toppling MEANS
// (UGSTopplableComponent); this owns the act of pulling.
//
// It is added on BP_GSPlayerCharacter rather than in AGSPlayerCharacter's constructor, deliberately:
// GSPlayerCharacter.h/.cpp were claimed by another agent (#186) at the time this was written, and the
// HUD finds this the same way it finds the interaction and command components - FindComponentByClass.
// Nothing here needs to be a native subobject.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSGrappleHaulComponent.generated.h"

class UGSTopplableComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSOnHaulStarted, AActor*, Target, float, DurationSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnHaulProgress, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnHaulEnded, bool, bCompleted);
/** A hook that bit something it cannot haul - today, a monument already lying in pieces. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnHaulRefused, AActor*, Target);

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSGrappleHaulComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSGrappleHaulComponent();

	/**
	 * Called by AGSGrappleHookProjectile the moment the hook bites.
	 *
	 * @param HitActor    what the hook stuck into
	 * @param AnchorPoint where it stuck, in world space. MUST be the hook's re-traced point, not the
	 *                    raw ImpactPoint: the grapple handover measured collision hulls sitting proud
	 *                    of the visible art by a median of 40uu and up to 451uu, agreeing within 20uu
	 *                    only 37% of the time. Anchoring on the raw impact would put the rope inside
	 *                    or outside the statue, and every tension reading below would be measured from
	 *                    a point that is not where the rope appears to be.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Grapple")
	void NotifyHookAttached(AActor* HookActor, AActor* HitActor, const FVector& AnchorPoint);

	/** Rope cut, hook recalled, target gone. Safe to call when not hauling. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Grapple")
	void NotifyHookDetached();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Grapple")
	bool IsHauling() const { return bHauling; }

	/** True whenever a hook is out - stuck in scenery as a movement anchor, or actively hauling a
	 *  monument. The release input (right-click, GSPlayerCharacter::Input_AimStart) checks this
	 *  first so the player can always drop the rope, not just while a haul is in progress - the
	 *  grapple is a movement tool first (see the class comment) and most throws never haul anything. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Grapple")
	bool IsHookAttached() const { return HookActor.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Grapple")
	float GetHaulProgress() const { return HaulProgress; }

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Grapple")
	FGSOnHaulStarted OnHaulStarted;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Grapple")
	FGSOnHaulProgress OnHaulProgress;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Grapple")
	FGSOnHaulEnded OnHaulEnded;

	/**
	 * The hook bit a valid monument that has nothing left to give.
	 *
	 * Michael: "the 2nd time I try and hook the statue, nothing happens." It was refusing correctly and
	 * saying nothing at all - no log, no delegate, and it did not even drop the hook, so a rope hung
	 * off a pile of rubble. A refusal the player cannot perceive is indistinguishable from a broken
	 * feature, which is the third time that exact shape has cost this project a debugging session.
	 */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Grapple")
	FGSOnHaulRefused OnHaulRefused;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/**
	 * How far past the attach distance counts as "taut".
	 *
	 * Not zero, and that is the whole reason this property exists: the player is never perfectly
	 * still, so a zero threshold would tick progress from idle sway and the statue would come down
	 * because somebody shuffled. You have to visibly lean away from it.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Grapple", meta = (ClampMin = "0.0"))
	float TautSlackUU = 120.f;

	/** Progress lost per second while the rope is slack. Faster than it fills, so backing off reads
	 *  as giving up rather than pausing - but not instant, so a stumble does not reset the heave. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Grapple", meta = (ClampMin = "0.0"))
	float SlackDecayPerSecond = 0.6f;

	/**
	 * How far past the attach distance the rope will let you go before it stops you.
	 *
	 * Michael, first playtest: "we need it so the player is also anchored to the statue. right now I
	 * fell off the platform trying to topple the statue." Without this the rope is a progress bar with
	 * no physicality - you are attached to a monument by a line that lets you walk into the void.
	 *
	 * Must be comfortably larger than TautSlackUU, or the rope stops you before tension can build and
	 * the statue can never come down. 260 against 120 gives 140uu of genuine leaning room: you feel
	 * the line go tight, then you feel it hold.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Grapple", meta = (ClampMin = "0.0"))
	float MaxRopeStretchUU = 260.f;

	/**
	 * How far away the hook may bite. Beyond this it simply does not hold.
	 *
	 * Michael: "The grappling hook is supposed to have a maximum distance anyways." Until now there
	 * was no rule at all - the hook flew until its arc gave out, so range was an accident of
	 * InitialSpeed 2600 against ProjectileGravityScale 0.45. Observed attach distances across one
	 * session ran 760, 948, 1245, 1264 and 1561uu, which means the same throw is a short tether or a
	 * rope across the arena depending on the angle you happened to use.
	 *
	 * 1200 is chosen against those measurements: it comfortably allows the close and mid throws that
	 * felt right and rejects the two that read as absurdly long.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Grapple", meta = (ClampMin = "0.0"))
	float MaxGrappleDistanceUU = 1200.f;

	/** Hold the hauler inside the rope's reach. Horizontal only - see the .cpp for why. */
	void ConstrainToRope(AActor* Owner);

	void EndHaul(bool bCompleted);

	/**
	 * Drop the rope and despawn the hook.
	 *
	 * Michael found the gap: nothing ever let go. NotifyHookDetached was only reachable when a second
	 * throw superseded the first, the hook actor was never destroyed, and there was no release input -
	 * so once you threw, you were tethered for the rest of the raid. Strafing simply orbited you at
	 * rope length forever, which is how he ran into it.
	 */
	void ReleaseHook();

	/** Damage breaks your grip (Michael's ruling): a guard landing a hit costs you the monument,
	 *  which makes toppling one in a live raid a risk rather than a quiet chore. */
	UFUNCTION()
	void HandleOwnerHealthChanged(float NewHealth, float MaxHealth, float Delta);

	/** The thrown hook, so the rope can be despawned with it. Weak: the hook can be destroyed by
	 *  anything at any time, and a haul that outlives it must not crash. */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> HookActor;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> HaulTarget;

	UPROPERTY(Transient)
	TWeakObjectPtr<UGSTopplableComponent> HaulTopplable;

	FVector HaulAnchorPoint = FVector::ZeroVector;

	/** Distance from the anchor at the instant the hook bit. Tension is measured against THIS, not
	 *  against the statue's pivot - so hooking a monument from close up still requires walking the
	 *  same distance back as hooking it from far away. */
	float AnchorDistanceAtAttach = 0.f;

	float HaulProgress = 0.f;
	float HaulDuration = 0.f;
	bool bHauling = false;
};
