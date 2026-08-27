// The destroyed state, and the only thing that knows how to reach it.
//
// Michael, 2026-08-24, naming the commonality this component exists to hold:
//
//     "we don't want to burn statues, we break them with a grapple, we break and destroy buildings
//      with a torch. the commonality is they get destroyed, and on the destroyed state after being
//      on fire, or being torn down, they crumble into pieces like the statue."
//
// One verb, three entrances. Fire is how a BUILDING reaches destroyed. A rope is how a STATUE
// reaches destroyed. What happens next is the same thing every time, and before this component it
// was written out three times in three files - correctly once, incorrectly once, and not at all in
// the third.
//
// ---------------------------------------------------------------------------------------------
// CRUMBLE IS RELEASE, NOT SHATTER. THE DIFFERENCE IS THE WHOLE FEATURE.
//
// This does NOT break the collection where it stands. It un-pins a dormant collection and lets it
// come apart under its own weight, optionally with a shove. The pieces separate on the way down and
// on impact, through bEnableDamageFromCollision.
//
// UGSBreakableComponent::Break() is the other thing - ACF's ForceDestruction applies strain NOW and
// shatters in place. That is right for a window and wrong for a monument, and the two must not be
// merged: GSTopplableComponent.h has carried that warning since #193, because a statue that
// shatters at the top of its lean instead of falling is the bug the warning exists to prevent.
//
// So: smashed things shatter (Break). Destroyed things crumble (here). A prop may have both.
//
// ---------------------------------------------------------------------------------------------
// THE FOUR STEPS, AND WHY EVERY ONE OF THEM IS LOAD-BEARING
//
// All four were found by measurement in PIE on 2026-08-19, after the statue logged three successful
// topples and never moved. Each one looks redundant. None is. ApplyRelease() carries the detail;
// the short version:
//
//   1. Hide the intact mesh AND its collision, reveal the collection. Both in one frame.
//   2. ObjectType = Chaos_Object_Dynamic BEFORE SetSimulatePhysics. SetSimulatePhysics does not
//      change ObjectType, and a kinematic body discards impulses silently.
//   3. RemoveAllAnchors(). An anchored collection is pinned no matter what else is true of it.
//   4. Impulse ONE FRAME LATER. Chaos does not switch the proxy's object state until the next
//      physics tick.
//
// ---------------------------------------------------------------------------------------------
// MULTIPLAYER
//
// The release is replicated, the simulation is not. Each machine runs its own crumble off the same
// authoritative release and its own physics decides where the rubble lands - which is why the
// impulse travels with it. Without it a client watches the monument sink straight down while it
// topples sideways on the host.
//
// Debris ends up in slightly different places on different machines. That is the intended trade:
// nobody interacts with rubble, it costs one small replicated struct once per object ever, and it
// is late-join correct. Replicating the collection itself would cost about 720 bytes and, worse,
// make the prop unbreakable on clients - a replicated collection marks its particles
// SetUnbreakable on proxy creation, because the client is not in control of it.
//
// This is also the co-op bug it was written to close: UGSTopplableComponent::bToppled was
// replicated with no RepNotify and every visual sat behind the authority early-return, so the idol
// fell on the host and stood untouched on every client.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSCrumbleComponent.generated.h"

class UGeometryCollectionComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnCrumbled);

/**
 * The authoritative fact that a thing has been released, and the shove that released it.
 *
 * One struct rather than a bare bool because the impulse has to reach the clients too - see the
 * multiplayer note in the class comment. Plain FVectors, not quantized: this replicates once per
 * object for the lifetime of the level, so the twelve bytes are not worth a quantization foot-gun.
 */
USTRUCT()
struct FGSCrumbleRelease
{
	GENERATED_BODY()

	UPROPERTY()
	bool bReleased = false;

	/** World-space shove applied at ImpulseAt. Zero is legal and means "just let go" - which is what
	 *  a building burning down wants, and what a statue pulled over does not. */
	UPROPERTY()
	FVector Impulse = FVector::ZeroVector;

	/** Where the shove lands, in world space. Off the centre of mass on purpose: applied at the
	 *  rope's anchor high on a statue it has a moment arm and the thing rotates about its base;
	 *  applied at the centre of mass it would simply slide. */
	UPROPERTY()
	FVector ImpulseAt = FVector::ZeroVector;
};

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSCrumbleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSCrumbleComponent();

	/**
	 * Let it come apart. Server-authoritative and idempotent - a thing is destroyed once.
	 *
	 * @param Impulse    world-space shove, or zero to simply release it and let gravity do the work
	 * @param ImpulseAt  where that shove is applied, in world space. Ignored when Impulse is zero.
	 *
	 * Returns false if it had already crumbled, if this is not the server, or if there is no
	 * geometry collection to release. All three are survivable, and the last one logs, because a
	 * destroyed thing that does not come apart is indistinguishable from the trigger never firing.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Crumble")
	bool Crumble(const FVector& Impulse, const FVector& ImpulseAt);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Crumble")
	bool HasCrumbled() const { return Release.bReleased; }

	/**
	 * Put it back. The exact inverse, for iteration.
	 *
	 * A thing is destroyed ONCE, which is right for the game and miserable for testing: every retry
	 * otherwise costs a full PIE restart. Inherited wholesale from UGSTopplableComponent::ResetTopple,
	 * which exists for exactly this reason and whose console command now routes through here.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "GoblinSiege|Crumble")
	void ResetCrumble();

	/**
	 * Find this actor's crumble component, adding one if it has none.
	 *
	 * Needed because the things that crumble are largely NOT Blueprints. A house is a placed
	 * StaticMeshActor out of a modular kit and a monument may be one too, so there is often no asset
	 * to hand-wire a component onto. Mirrors AGSBuildingObjective::EnsurePiecesFlammable, which
	 * force-adds UGSFlammableComponent to plain kit actors for the same reason.
	 *
	 * Server-side only: a component added at runtime on a client is not the replicated one.
	 */
	static UGSCrumbleComponent* FindOrAdd(AActor* Actor);

	/**
	 * Spawn a dormant AGeometryCollectionActor standing in for Source, resolving its fracture by mesh
	 * name (SM_Foo -> GC_Foo in Folder). Returns null, silently, when no fracture exists for that mesh.
	 *
	 * A COLLECTION WANTS TO BE AN ACTOR'S ROOT, NOT A PASSENGER. Bolting a collection component onto
	 * an existing StaticMeshActor failed four different ways on 2026-08-26 - static mobility, the
	 * attachment driving its transform, a Custom collision profile that let it fall through the world,
	 * and finally the pieces landing 1.3 km away - because Chaos builds a collection's proxy once, at
	 * registration, from the actor it belongs to. Spawning the shape that works is the fix, and it
	 * retired three of those four patches.
	 *
	 * The caller hides the source. Server-only.
	 */
	static AActor* SpawnProxyFor(AActor* Source, const FString& Folder,
		FName AssetNameOverride = NAME_None);

	/** Fires on every machine the instant the release lands, server included. The hook for dust,
	 *  sound, and anything that wants to react to the collapse rather than cause it. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Crumble")
	FGSOnCrumbled OnCrumbled;

	/**
	 * The intact mesh to retire, by component name. Leave unset and it resolves automatically at
	 * BeginPlay, which is what a plain one-mesh kit actor needs.
	 *
	 * Named rather than guessed wherever it can be, for the reason UGSBreakableComponent gives at
	 * length: hiding every static mesh on the owner is right for a single-mesh prop and catastrophic
	 * for a multi-mesh Blueprint, where the whole actor vanishes and it reads as a Chaos bug rather
	 * than a naming one. The automatic path therefore only fires when there is exactly ONE candidate
	 * and so no guess to get wrong.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Crumble")
	FName IntactMeshComponentName = FName("IntactMesh");

	// ---- the collapse shape (tuned live with Michael, 2026-08-26) -------------------------------
	//
	// Everything below defaults to OFF, which is the statue's behaviour and must stay that way: a
	// monument is pulled over by a rope, and the rope's impulse is the whole reason it comes apart.
	// A BUILDING has no rope. Released with nothing but gravity it does not fall at all - measured in
	// PIE on a 32-chunk merged house: 0.0 uu/s, bounds unchanged, still standing. A well-formed
	// collection resting on the ground is a single rigid body and stays one.
	//
	// So a building sets these, and they encode what was actually watched rather than what sounded
	// right. AGSBuildingObjective::CrumblePieces is the caller.

	/**
	 * How many times to break the cluster bonds on release.
	 *
	 * ZERO for a monument. THREE for a building, and the number is not arbitrary: the collection
	 * carries a damage threshold PER CLUSTER LEVEL (measured [500000, 50000, 5000]), and one
	 * CrumbleActiveClusters call only breaks the top bond. With a single pass Michael watched the
	 * roof come down as a bonded slab; with three, "part of the roof broke up".
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Crumble|Collapse", meta = (ClampMin = "0", ClampMax = "8"))
	int32 ClusterCrumblePasses = 0;

	/**
	 * Number of shoves arranged around the roof. Zero means "use the single Impulse the caller
	 * passed", which is the statue.
	 *
	 * A ring of downward-and-inward shoves is what makes a building fold into its own footprint
	 * instead of being pushed over. One shove at the centre spread it sideways (2139 x 2829);
	 * four angled inward held it to 1980 x 2002, barely wider than the intact house.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Crumble|Collapse", meta = (ClampMin = "0", ClampMax = "16"))
	int32 CollapseShoveCount = 0;

	/** Magnitude of each shove. Scaled against a measured 100,000 kg house. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Crumble|Collapse", meta = (ClampMin = "0.0"))
	float CollapseShoveMagnitude = 5000000.f;

	/**
	 * How much of each shove points INWARD versus straight down (0 = pure drop, 1 = 45 degrees in).
	 *
	 * This is the dial that decides "controlled demolition" versus "shoved over", and it is the one
	 * to reach for before changing magnitude.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Crumble|Collapse", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float CollapseInwardRatio = 0.6f;

	/**
	 * Height of the shove ring as a fraction of the owner's bounds, and its radius as a fraction of
	 * the footprint.
	 *
	 * The height matters more than it looks. Shoves were first applied above the roof, at a height
	 * with no geometry under it - the topmost piece origin sat at 63% of the height - so they landed
	 * on nothing. Dropping the ring onto the actual roof pieces is what started breaking the roof.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Crumble|Collapse", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CollapseShoveHeightFraction = 0.63f;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Crumble|Collapse", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CollapseShoveRadiusFraction = 0.65f;

	/**
	 * Keep everything below this fraction of the owner's height ANCHORED, so only the upper part
	 * comes down.
	 *
	 * Zero (the default) releases the whole thing, which is what a house and a monument want.
	 *
	 * Michael's brief for the windmill, 2026-08-26: "I'd rather the top half just sink to the ground
	 * and it all be on fire." That is not a weaker collapse - it is a different one, and Chaos already
	 * has the mechanism. A geometry collection can be anchored per-particle, and an anchored particle
	 * is pinned no matter what else is true of it. That fact has been a trap all through this system:
	 * it is why the statue would not fall until RemoveAllAnchors was found (#193, four wrong guesses),
	 * and it is exactly the behaviour wanted here. Same lever, opposite intent.
	 *
	 * So: release everything, then re-anchor the base. A 44m stone tower keeps its stump and drops
	 * its cap, which reads as a building giving way rather than a demolition.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Crumble|Collapse", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float KeepAnchoredBelowFraction = 0.f;

	/**
	 * Char the released collection to this amount, 0..1. Zero (the default) leaves it pristine.
	 *
	 * THE RUBBLE IS A NEW OBJECT AND HAS NEVER BEEN ON FIRE. Michael watched the windmill sink,
	 * 2026-08-26: "there was a stone stub, everything fell down, it was still on fire, but it didn't
	 * have the char on it." The standing mill blackens as its fuse burns, then it is hidden and a
	 * freshly spawned fracture takes its place - and that fracture has pristine materials, because
	 * nothing ever burned IT. The burn state does not survive the swap unless it is carried across.
	 *
	 * The same applies to a burnt-out house, which drops clean debris for exactly the same reason.
	 *
	 * Drives GS_BurnAmount, the single scalar UGSBurnFXComponent uses. Pushing it at a material that
	 * has never heard of that parameter is a safe no-op in UE, so this is harmless on a kit that has
	 * not opted in. A monument pulled over by a rope leaves this at zero: it was never burnt, and
	 * charred rubble under an idol would be a lie.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Crumble|Collapse", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CharAmountOnRelease = 0.f;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Release();

	/** The four steps. Runs identically on the server and on every client. */
	void ApplyRelease();

	/**
	 * The ring of downward-and-inward shoves that makes a building fold rather than topple.
	 *
	 * Derived from the owner's bounds, so it is one setting for a kit of differently-sized houses,
	 * and deterministic - every client computes the identical ring from replicated state, which is
	 * why the ring costs no bandwidth beyond the release itself.
	 */
	void ApplyCollapseRing(UGeometryCollectionComponent* Collection);

	/** The collection this releases. Resolved from the owner; null is survivable and logs. */
	UGeometryCollectionComponent* ResolveCollection() const;

	/** The mesh ApplyRelease retires: the named one, else the only one, else null. */
	UStaticMeshComponent* ResolveIntactMesh() const;

	/**
	 * Logs whether the collection ACTUALLY MOVED, half a second after the release.
	 *
	 * Michael asked the right question - "is there a way to check in the logs that the mesh swapped
	 * vs it actually toppling?" - and the answer was no, which was the whole problem. The old line
	 * announced success the moment the impulse was dispatched, and said so three times while the
	 * statue stood there kinematic and unmoved. A log that reports intent rather than outcome is
	 * worse than no log: it actively misdirects.
	 */
	void ReportCrumbleOutcome();

	/**
	 * Two seconds after release, knock loose anything still hanging in the air.
	 *
	 * A backstop, and deliberately a dumb one. Three pieces survived every principled fix - explicit
	 * unpinning, three cluster-crumble passes, removing all anchors - and sat in mid-air with a
	 * displacement of 0, 1 and 24 uu while everything around them fell. I do not know what holds
	 * them, and four theories about Chaos anchoring have already been wrong today.
	 *
	 * So this does not theorise: it finds pieces that did not move and are still above the wreck, and
	 * applies strain to break them free. If that fails they are hidden, because a fragment hanging in
	 * the sky is worse than a fragment that is not there.
	 */
	void SweepStragglers();

	FTimerHandle SweepTimer;
	int32 SweepPassesRun = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Release)
	FGSCrumbleRelease Release;

	FTimerHandle OutcomeTimer;
};
