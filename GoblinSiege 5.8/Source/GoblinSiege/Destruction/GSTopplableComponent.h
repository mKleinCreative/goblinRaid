// A monument that can be pulled over, as opposed to chipped down.
//
// Michael, 2026-08-18: "I want to give people the idea of tearing down false idols in favour of their
// dark lord." That sentence is the spec. It rules out the obvious implementation - hitting a statue
// four times with a sword until it pops - because that reads as demolition work, not iconoclasm. What
// sells it is resistance: a rope, a heave, a slow lean, and then the thing coming down under its own
// weight.
//
// ---------------------------------------------------------------------------------------------
// WHY THIS IS NOT PART OF UGSBreakableComponent
//
// The breakable answers "how much damage until it is destroyed". Toppling is not damage. Nothing
// hits the statue, no hit points are spent, and the moment it goes over it is not yet broken - it
// breaks when it LANDS. Folding that into the hit-point model would mean inventing a damage number
// for a rope, and then explaining why the statue is undamaged at the top of its arc.
//
// So this is a sibling: the breakable still owns being smashed, and a statue may carry both.
//
// ---------------------------------------------------------------------------------------------
// THE ORDER THAT MATTERS, AND THE ONE THIS PROJECT ALREADY GOT WRONG ONCE
//
// Topple() does NOT call Break(), and must not. Break() delegates to ACF's ForceDestruction, which
// shatters the collection where it stands - a statue that explodes at the top of its lean instead of
// falling. The fall is the point. So Topple only promotes the collection to dynamic and pushes it;
// the SHATTER is left to bEnableDamageFromCollision when it hits the ground.
//
// That is also why GC_Statue_Warrior ships Chaos_Object_Static. It was authored Dynamic first, and a
// dynamic collection with collision damage on simply fell over and smashed itself at level start,
// untouched - which was briefly, and wrongly, recorded as the feature working.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSTopplableComponent.generated.h"

class UGeometryCollectionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnToppled, AActor*, Toppler);

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSTopplableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTopplableComponent();

	/**
	 * Pull it over. Server-authoritative and idempotent - a monument comes down once.
	 *
	 * @param Toppler        who did it, for the objective and the score
	 * @param PullDirection  the direction it should fall, normally hauler-minus-statue so it comes
	 *                       down towards the player who pulled it
	 * @param AnchorPoint    where the rope was attached, in world space. The push is applied HERE
	 *                       rather than at the centre of mass, which is what makes it rotate instead
	 *                       of sliding.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Topple")
	bool Topple(AActor* Toppler, const FVector& PullDirection, const FVector& AnchorPoint);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Topple")
	bool IsToppled() const { return bToppled; }

	/**
	 * Stand it back up. The exact inverse of Topple(), for testing.
	 *
	 * A monument comes down ONCE, which is right for the game and miserable for iteration: every retry
	 * costs a full PIE restart. That cost is not hypothetical - it is why the "second hook does
	 * nothing" report took a whole workflow to explain, because nobody could cheaply re-run the case.
	 *
	 * Exposed as `GS.Topple.ResetAll` as well, so it is one console line rather than a rebuild.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "GoblinSiege|Topple")
	void ResetTopple();

	/** How long the player must keep the rope taut. Read by UGSGrappleHaulComponent, which owns the
	 *  haul; kept here so it is a property of the monument, not of the rope. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Topple")
	float GetHaulSeconds() const { return HaulSeconds; }

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Topple")
	FGSOnToppled OnToppled;

protected:
	/**
	 * Seconds of sustained tension before it goes over.
	 *
	 * Long enough to be a deliberate act rather than a side effect of walking away, short enough not
	 * to be a chore. This is the "heave" - the whole reason the feature is a haul and not a button.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Topple", meta = (ClampMin = "0.1"))
	float HaulSeconds = 2.5f;

	/** Sideways shove along the pull direction, applied at the rope's anchor. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Topple")
	float ToppleImpulse = 90000.f;

	/**
	 * Extra lift added to the push.
	 *
	 * A purely horizontal shove on something standing on the ground tends to scrape it sideways,
	 * because the ground contact resists the rotation. A little up-and-over gets the mass past its
	 * balance point so gravity finishes the job, which is what toppling actually looks like.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Topple")
	float ToppleLift = 0.35f;

	/**
	 * The intact statue: an ordinary static mesh that is swapped for the geometry collection at the
	 * moment it goes over.
	 *
	 * Michael's design, and it is a better one than what came before: "the me throwing a hook activates
	 * the ability for it to begin to topple. when it finally topples after the bar is filled, we swap
	 * the model to the version that shatters."
	 *
	 * This is the same pattern the crate and barrel already use (UGSBreakableComponent::BrokenMesh), and
	 * it removes an entire class of bug rather than fixing instances of it. A standing monument that is
	 * just a static mesh CANNOT crumble at level start, cannot be caught in the wrong Chaos object
	 * state, and costs nothing to render. The collection is inert and hidden until the instant it is
	 * supposed to come apart - at which point it is created dynamic, so no promotion is needed and
	 * SetSimulatePhysics has nothing to argue with.
	 *
	 * Named rather than guessed, for the reason GSBreakableComponent gives at length: hiding "every
	 * static mesh on the actor" is right for a single-mesh prop and catastrophic for anything else.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Topple")
	FName IntactMeshComponentName = FName("IntactMesh");

	UPROPERTY(Replicated)
	bool bToppled = false;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The collection this topples. Resolved from the owner; null is survivable and logs. */
	UGeometryCollectionComponent* ResolveCollection() const;

	/**
	 * Logs whether the collection ACTUALLY MOVED, half a second after the shove.
	 *
	 * Michael asked the right question - "is there a way to check in the logs that the mesh swapped vs
	 * it actually toppling?" - and the answer was no, which is the whole problem. The old single line
	 * announced a successful topple the moment the impulse was dispatched, and said exactly that three
	 * times while the statue stood there kinematic and unmoved. A log that reports intent rather than
	 * outcome is worse than no log: it actively misdirects.
	 *
	 * So this reads the velocity back after the physics has had a chance to run, and says MOVED or
	 * DID NOT MOVE. The swap is reported separately, because "the mesh changed" and "the monument fell"
	 * are two different successes and only one of them is the feature.
	 */
	void ReportToppleOutcome();

	FTimerHandle ToppleOutcomeTimer;
};
