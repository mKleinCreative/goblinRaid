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
//
// ---------------------------------------------------------------------------------------------
// WHAT THIS CLASS STILL OWNS, AFTER UGSCrumbleComponent (2026-08-25, #317)
//
// The release itself - swap, promote, un-anchor, shove a frame later - moved to UGSCrumbleComponent,
// because a building burning down needs the identical sequence and this file's copy was one of
// three. Michael named the commonality: "the commonality is they get destroyed, and on the destroyed
// state after being on fire, or being torn down, they crumble into pieces like the statue."
//
// What stays here is everything that makes a topple a TOPPLE rather than a collapse: the rope, the
// heave, the direction the monument falls, the ward it holds while it stands, and what bringing it
// down is worth. bToppled therefore no longer needs a RepNotify - it is gameplay state read on the
// server, and the visuals every client has to see are the crumble component's replicated release.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GSTopplableComponent.generated.h"

class UGSCrumbleComponent;

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
	 * While this monument STANDS, no portal may be summoned in the land it seals.
	 *
	 * Michael's fiction is the rule: "the seal that prevents chaos magic from happening IN THE LAND".
	 * Not in a circle around itself - in the land. So the default is bWardsEntireLevel, and a standing
	 * warded monument refuses Warren placement anywhere on the map. Casting it down is what opens the
	 * ground to a gate, wherever the player chooses to plant it.
	 *
	 * THE FIRST VERSION WAS A RADIUS AND IT WAS USELESS, which is worth writing down rather than
	 * quietly replacing: the statue sits 12,475 uu from the player's spawn and the ward was 6,000, so
	 * the player could plant a gate at spawn without ever walking to the statue and the whole opening
	 * beat was decorative. A rule that the player can trivially stand outside is not a rule.
	 *
	 * WardRadius survives for levels with several shrines, where "this one guards the mill" is a real
	 * distinction. It is only consulted when bWardsEntireLevel is false.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Topple|Ward")
	bool bWardsEntireLevel = true;

	/** Only used when bWardsEntireLevel is false. 0 wards nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Topple|Ward",
		meta = (ClampMin = "0.0", EditCondition = "!bWardsEntireLevel"))
	float WardRadius = 0.f;

	/** True while this monument is standing AND seals anything at all. The one question the placement
	 *  rule asks; kept here so the rule cannot drift from the component that owns the state. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Topple|Ward")
	bool IsWarding() const { return !bToppled && (bWardsEntireLevel || WardRadius > 0.f); }

	/** Does this monument's ward reach Spot? Global wards reach everywhere. */
	bool WardReaches(const FVector& Spot) const;

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

	/**
	 * Which win-condition type this monument satisfies when it falls. Invalid means it counts for
	 * nothing, which is right for scenery.
	 *
	 * EXISTS BECAUSE THE STATUE COUNTED FOR NOTHING. Michael, 2026-08-25, named the required set as
	 * "a percentage of houses, the market stalls, the Statue and the field" - and the raid director
	 * only ever swept AGSBurnObjectiveBase, so the statue could be hauled down to no effect at all on
	 * the win. AGSObjective_ToppleStatue looks like the answer and is not: it hard-casts to
	 * AGSDestructibleObjective, which completes by BURNING, so its total stayed 0 and its condition
	 * was true on the first broadcast.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Topple|Objective")
	FGameplayTag ObjectiveTypeTag;

	/** The type this monument satisfies, or an invalid tag. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Topple|Objective")
	FGameplayTag GetObjectiveTypeTag() const { return ObjectiveTypeTag; }

	/** What the HUD calls this monument. Empty falls back to the type tag's leaf, so a monument that
	 *  counts is never a nameless row. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Topple|Objective")
	FText ObjectiveDisplayName;

	/** The HUD name, resolved: the authored one, else the tag leaf, else "Monument". */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Topple|Objective")
	FText GetObjectiveDisplayName() const;

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

	// ---- score (#196) ---------------------------------------------------------------------------
	//
	// Until now, bringing down a monument scored NOTHING. OnToppled had no subscribers anywhere in the
	// project, UGSScoreSubsystem binds only to AGSBurnObjectiveBase, and the sole trace a toppled idol
	// left behind was one line in the log. The raid's whole point is what you did to the place, and the
	// most dramatic thing a goblin can do to it did not register.
	//
	// Rates match the existing objective scale rather than inventing one (GSScoreSubsystem.cpp:20-21):
	// 100 for the first of its type, 40 for each after. The Statue is one of the three REQUIRED
	// objectives in the GDD roster, so the full rate is the right one.

	/** Deeds for the first monument of this type. Matches DeedsPerRequiredObjective. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Topple|Score", meta = (ClampMin = "0"))
	int32 ToppleDeeds = 100;

	/** Deeds for each further monument of the same type, once one is already down. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Topple|Score", meta = (ClampMin = "0"))
	int32 DuplicateToppleDeeds = 40;

	/**
	 * Score bucket this monument counts towards.
	 *
	 * Deliberately EMPTY by default and left to be set in the editor. The obvious value is
	 * `Objective.Statue`, but that string is currently only an ACTOR tag - there is no gameplay tag for
	 * it, and GSGameplayTags.h is claimed by another agent's open ticket (#179), so declaring one here
	 * would collide. An unset tag still scores; it simply does not break down by type in
	 * GetDeedsForType, which is a reporting nicety rather than the feature.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Topple|Score")
	FGameplayTag DeedTypeTag;

	/**
	 * Gameplay state, not a visual. No RepNotify on purpose: what a client must SEE is the crumble
	 * component's replicated release, and this only answers "is the ward still up, has it scored".
	 *
	 * It used to be both, and that was the co-op bug - every visual sat behind the authority
	 * early-return in Topple(), so the idol fell on the host and stood untouched on every client.
	 */
	UPROPERTY(Replicated)
	bool bToppled = false;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

};
