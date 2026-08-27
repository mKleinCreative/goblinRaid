// The windmill: the objective that has to be SOLVED rather than merely torched (design doc §6.3).
// Stone base, no purchase, and exterior fire alone won't take it - the goblin has to put a torch
// through an upper window into the flour dust inside, at which point the mill is on a fuse.
// Written 2026-07-28 for Block C.
//
// Sequence: Intact -> (torch through window) -> Smouldering [dust builds] -> Detonated -> state swap.
//
// The refusal is the teaching moment. Throwing fire at the outside does nothing mechanically but
// DOES fire OnExteriorIgnitionRefused, so the bark system can have the goblin mutter about the
// stone and the player learns to look up. A silent no-op would just read as a bug.
#pragma once

#include "CoreMinimal.h"
#include "Destruction/GSBurnObjectiveBase.h"
#include "GSMillObjective.generated.h"

class UStaticMeshComponent;
class UPrimitiveComponent;
class UBoxComponent;
class UGSBurnFXComponent;
class URotatingMovementComponent;

UENUM(BlueprintType)
enum class EGSMillStage : uint8
{
	Intact,
	Smouldering,
	Detonated
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnMillSimple);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnMillStageChanged, EGSMillStage, NewStage);

UCLASS()
class GOBLINSIEGE_API AGSMillObjective : public AGSBurnObjectiveBase
{
	GENERATED_BODY()

public:
	AGSMillObjective();

	/**
	 * Exterior fire. Does NOT light the mill - stone doesn't take, and the sails are too high to
	 * reach from the ground. Broadcasts the refusal so the goblin can say so out loud.
	 */
	virtual void IgniteAtLocation(const FVector& WorldLocation) override;

	/**
	 * The real verb: a lit torch has passed through a window and into the flour dust. Starts the
	 * fuse. Idempotent - a second torch through a second window changes nothing.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Mill")
	void IgniteInterior();

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Mill")
	EGSMillStage GetStage() const { return Stage; }

	/** Seconds until detonation, or 0 if not lit / already gone. Drives the audio build. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Mill")
	float GetSecondsToDetonation() const;

	/** Bark hook: "Won't take. Stone don't burn." */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Mill")
	FGSOnMillSimple OnExteriorIgnitionRefused;

	/** The fuse is lit - dust glow, interior light, rising rumble. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Mill")
	FGSOnMillSimple OnInteriorIgnited;

	/** The moment. Camera shake, the big VFX, and every alarm in the hamlet. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Mill")
	FGSOnMillSimple OnMillDetonated;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Mill")
	FGSOnMillStageChanged OnMillStageChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleCompleted() override;
	virtual void DrawDebugState() const override;

	void BindWindowVolumes();
	void Detonate();
	void SetStage(EGSMillStage NewStage);
	void TickBuildup();

	UFUNCTION()
	void OnWindowOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnRep_Stage();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Mill")
	TObjectPtr<UStaticMeshComponent> MillMesh;

	/**
	 * The sails, attached to the mill body. Assign a mesh in the Blueprint (the map's dressing
	 * mills use SM_Windmill_Sail); left unset the mill simply has no sails and none of the spin
	 * machinery does anything.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Mill|Visual")
	TObjectPtr<UStaticMeshComponent> SailMesh;

	/**
	 * Q-34 ruling (2026-08-01): the sails keep turning while the interior burns - Michael:
	 * "having the windmill spinning while burning would be a great sight" - and stop dead at
	 * detonation, which reads as the machine dying even before any ruin mesh lands.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Mill|Visual")
	TObjectPtr<URotatingMovementComponent> SailSpin;

	/**
	 * Sail turn rate, degrees per second. Which axis is right depends on how the sail mesh is
	 * authored; SM_Windmill_Sail turns about its local X, hence the default roll. Set all zeros
	 * in a Blueprint to park the sails entirely.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Visual")
	FRotator SailSpinRate = FRotator(0.f, 0.f, 25.f);

	/**
	 * Char driver (2026-07-31, Q-34). The mill scorches as its fuse burns down, delivering the
	 * burn-down spec's 0.33 / 0.66 / 1.0 char steps, WITH NO CHANGE TO ITS IGNITION RULES.
	 *
	 * The obvious way to get char on this actor would be to give it a UGSFlammableComponent, since
	 * that is what UGSBurnFXComponent normally reads. That is exactly the wrong move and it is
	 * worth saying so here, because it will look like an oversight to the next person: the mill
	 * deliberately has no flammable component. It runs its own Intact -> Smouldering -> Detonated
	 * dust-fuse state machine precisely so that exterior fire CANNOT take it (design doc §6.3 -
	 * "stone base, sails out of reach"), and a flammable component would hand it a second,
	 * contradictory ignition path: AGSFireVolume::SpreadTick and AGSMillObjective::Detonate both
	 * call Ignite() on every flammable in reach, so a hedge fire next door - or another mill - would
	 * light this one through a rule the design specifically refuses. It would also re-arm the town
	 * alarm twice and give the mill a second, unrelated completion clock in BurnDurationSeconds.
	 *
	 * So the FX component is driven DIRECTLY instead, from TickBuildup and OnRep_Stage, and the
	 * mill keeps exactly one definition of what "burning" means for it. UGSBurnFXComponent supports
	 * this: with no flammable present its BeginPlay logs once and skips the auto-drive, leaving
	 * SetBurnAmount as a pure material driver. Verified 2026-07-31 - it does not warn per frame and
	 * does not crash, so no bRequireFlammable opt-out was needed.
	 */
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Mill")
	TObjectPtr<UGSBurnFXComponent> BurnFXComponent;

	/** Swapped in on detonation. Set-dressing debris is the level's job; this is the silhouette. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Visual")
	TObjectPtr<UStaticMesh> DestroyedMesh;

	/**
	 * Any child primitive carrying this component tag is treated as a window: a torch actor
	 * overlapping it lights the interior. Add a box per window in the Blueprint and tag it.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Windows")
	FName WindowComponentTag = TEXT("Window");

	/**
	 * Actor tag a projectile must carry to count as a lit torch. Keeps the goblin's own body,
	 * thrown loot and stray physics props from setting off the mill.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Windows")
	FName LitTorchActorTag = TEXT("GS.LitTorch");

	// ---- the way in (2026-08-26, #323) ---------------------------------------------------------
	//
	// THE MILL WAS UNLIGHTABLE. Not stubborn - unlightable. IgniteAtLocation is a deliberate no-op
	// (stone base, sails out of reach), so IgniteInterior is the only route, and it is reached by an
	// overlap between a component tagged WindowComponentTag and an actor tagged LitTorchActorTag.
	// Measured on L_Tutorial_Island 2026-08-26: NEITHER placed windmill had a component carrying that
	// tag, and NOTHING in the entire project ever tagged an actor GS.LitTorch. Both halves of the
	// overlap were missing, so the only caller left was the GS.Burn debug command.
	//
	// It read as a design rule rather than a bug precisely because the refusal path still worked: a
	// torch to the outside fired OnExteriorIgnitionRefused, so the mill looked like it was saying no
	// on purpose while actually having no way to say yes.
	//
	// The trigger is created here rather than placed per-level for the same reason BurnFXComponent is
	// (see GSDestructibleObjective): hand-wiring it is exactly the step someone forgets on the one
	// the playtest happens to stand in front of - and it had been forgotten on both of them.

	/** The volume a lit torch must reach to light the mill. Tagged with WindowComponentTag on
	 *  construction, so the existing BeginPlay binding finds it with no special case. */
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Mill|Window")
	TObjectPtr<class UBoxComponent> WindowTrigger;

	/**
	 * Where the window sits, relative to the objective's origin.
	 *
	 * 1610 is MEASURED FROM THE ART, not derived from bounds. My first value was 2600, reasoned from
	 * "SM_WIndmill_Base spans z 0..4434, so the upper third is about here" - and it was nearly a
	 * metre and a half too high. Michael put a marker where it belongs: "right above the 1st round
	 * section", which is a thing you can see and cannot compute from a bounding box.
	 *
	 * That is the general lesson and it is worth more than the number: for anything positional in
	 * this project, put a marker in the world and let somebody look at it.
	 *
	 * EditAnywhere because the exact window is an art detail and this should be nudged by eye rather
	 * than argued about in code.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Window")
	FVector WindowTriggerOffset = FVector(0.f, 0.f, 1610.f);

	/**
	 * Half-size of the trigger. Wide enough to encircle the tower by default.
	 *
	 * A band rather than one face, deliberately, and it is a compromise worth naming: the fiction is
	 * "through an upper window", which is one side. But the two placed mills carry no window marker
	 * and are rotated differently, so a single-face volume would need per-instance authoring to be
	 * correct and would silently be wrong wherever nobody did it. A band is reachable from any side,
	 * which is slightly more forgiving than the fiction and never silently broken. Narrow it once
	 * somebody decides which face the window is on.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Window")
	FVector WindowTriggerExtent = FVector(950.f, 950.f, 400.f);

	// ---- the mill comes down (2026-08-26, #324) --------------------------------------------------
	//
	// REPLACES THE DETONATION BEAT. Michael, 2026-08-26: "our system looks good and a detonation is a
	// little much, I'd rather the top half just sink to the ground and it all be on fire."
	//
	// That supersedes GDD Ruling 7's "smashed-but-recognizable silhouette" mesh swap, and it is a
	// RULED beat being replaced rather than extended - it owes docs/decisions-ledger.md a row. Noted
	// here rather than done silently, because a design change that only exists in code is how two
	// documents come to disagree for months.
	//
	// The mesh-swap path below is left intact and simply unused when bSinkOnDetonation is true: the
	// mills on L_Tutorial_Island have no DestroyedMesh assigned anyway, so the swap was already a
	// no-op on both of them.

	/** Sink the tower instead of swapping to a ruin mesh. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Sink")
	bool bSinkOnDetonation = true;

	/**
	 * How much of the tower stays standing, as a fraction of its height.
	 *
	 * 0.61 is MEASURED, not reasoned. My first value was 0.45, argued from "the stonework ends lower
	 * than the midpoint" - and Michael's verdict on watching it was "too much of the bottom is still
	 * breaking". He put a marker at world z 3640 on a tower spanning 604..5596, which is 0.608.
	 *
	 * Same lesson as the window trigger on this class, which I also guessed a metre and a half too
	 * high: for anything positional here, put a marker in the world and let somebody look at it.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Sink", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float KeepStandingFraction = 0.61f;

	/** Where fracture assets live. Mesh SM_Foo resolves to GC_Foo in here. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Sink")
	FString CrumbleCollectionFolder = TEXT("/Game/Destruction");

	// ---- TWO collections, and why anchoring was abandoned ---------------------------------------
	//
	// The tower is fractured ONCE and then pruned into two assets: GC_WIndmill_Top (23 pieces, the
	// part that falls) and GC_WIndmill_Stump (27 pieces, the part that stays). The stump is spawned
	// visible and never released; the top is spawned and crumbled.
	//
	// This replaces four failed attempts at anchoring the base of a single collection - anchor then
	// crumble, anchor with no crumble, crumble then anchor, and anchor before going dynamic. Every one
	// applied the anchors successfully (the log said "anchored 27 of 49") and every one let the base
	// go anyway. Whatever is true about anchors here, it is not what four readings of the API implied.
	//
	// Which pieces exist is a FACT ABOUT THE ASSET. Facts do not have ordering bugs, and this is why
	// the two-asset version is the one that works.
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Sink")
	FName TopCollectionName = TEXT("GC_WIndmill_Top");

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Sink")
	FName StumpCollectionName = TEXT("GC_WIndmill_Stump");

	/**
	 * Downward shove on the cap as it is released.
	 *
	 * Needed because the cap now lands on an INTACT stump instead of falling through it, so under
	 * gravity alone it only settled a median of 70uu - measured, and Michael's read was that it barely
	 * moved. The stump standing is the feature; this is what stops that feature making the collapse
	 * look like nothing happened.
	 *
	 * Straight down, not inward: the tower is a cylinder and there is no footprint to fold into.
	 *
	 * 400,000 rather than the 3,000,000 tried first. This one IS a real mass-scaled impulse, but the
	 * cap's pieces are only a few thousand kg each, so 3M threw them a median of 3590uu with one
	 * piece reaching 338,706 - three and a half kilometres. Roughly 100 cm/s downward per piece is a
	 * collapse; anything above that is artillery.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Sink")
	float TopDropImpulse = 400000.f;

	/**
	 * Substring identifying the mill's own geometry among nearby actors.
	 *
	 * NEEDED BECAUSE THE OBJECTIVE OWNS NO MESH. Measured 2026-08-26: both placed windmills have
	 * MillMesh and SailMesh EMPTY, and actor bounds of zero - the visible mill is a separate
	 * StaticMeshActor sitting at 0uu from the objective (SM_WIndmill_Base2 and
	 * SM_WIndmill_Base_Blueprint). The class was written expecting to own its meshes; the level does
	 * it the other way round. So the sink has to find the geometry rather than assume it.
	 *
	 * "Windmill_Base" and NOT "Windmill", which was the first value and was wrong in a way that only
	 * showed up on the second mill. "Windmill" matches the SAILS too, and the hill mill's nearest
	 * matching actor IS a sail (110uu away, mesh SM_Windmill_Sail) while its actual tower sits 2213uu
	 * off. So that mill would have grabbed the sails, found no GC_Windmill_Sail, and quietly refused
	 * to sink - while the ground mill worked, because there the tower happens to be the nearer of the
	 * two. One working example proved nothing.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Sink")
	FString MillGeometryNameFilter = TEXT("Windmill_Base");

	/** 2600 rather than 1500: the hill mill's tower is 2213uu from its objective. Measured, not padded. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Sink", meta = (ClampMin = "0.0"))
	float MillGeometrySearchRadius = 2600.f;

	// ---- the sails come off with the cap ---------------------------------------------------------
	//
	// The sails are a SEPARATE ACTOR from the tower on both mills, so dropping the tower alone left
	// them hanging in mid-air. Michael chose the physics option over hiding them: the sail assembly
	// falls as a rigid body.
	//
	// No fracture needed and none wanted - a windmill's sails are a timber cross that should topple
	// and lie there, not shatter. This is the one place in the destruction system where a plain
	// simulating StaticMeshActor is the right answer rather than a geometry collection.

	/** Drop the sails as a physics body when the tower goes. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Sink")
	bool bDropSails = true;

	/**
	 * Every loose part of the mill that should come down with it, by mesh-name substring.
	 *
	 * A WINDMILL IS THREE ACTORS AND I ONLY EVER HANDLED TWO. Michael circled the conical roof:
	 * "this part I showed you here is the part that consistently doesn't render". It is
	 * SM_RoofTIles2, a separate actor sitting at horizontal offset 0 from the tower and spanning
	 * z 5389..7929 - so when the tower sank, the roof simply stayed in the sky. That is the whole of
	 * "the top of the mill isn't connected to the bottom, but it's still in the air", and the reason
	 * the same spot never moved however many times the collapse was retuned.
	 *
	 * Meanwhile I spent four measurements chasing unmoved indices INSIDE the collections, which are
	 * almost certainly invisible root nodes. The screenshot found in one glance what the numbers had
	 * been pointing away from all along.
	 *
	 * A LIST rather than one filter, so the next mill part that turns out to be its own actor is a
	 * one-line change and not another evening.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Sink")
	TArray<FString> LoosePartNameFilters = { TEXT("Windmill_Sail"), TEXT("RoofTIles") };

	/**
	 * Sideways nudge on the sails, in cm/s. THIS IS A VELOCITY, NOT AN IMPULSE.
	 *
	 * It is applied with bVelChange=true, which makes the magnitude a direct velocity change and
	 * ignores mass entirely. The first value here was 40000, chosen as if it were a force - so the
	 * sails left at 400 m/s. Michael: "what happened with the sails? can't see em". They had risen
	 * 42 metres and flown 341 metres sideways, most of the way across the map, and the drop had
	 * logged success the whole time.
	 *
	 * 250 cm/s is a shove you could give a gate. Gravity does the rest.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Sink")
	float SailDropImpulse = 20000.f;

	/** Fracture and release every loose part of the mill - sails, roof, anything else
	 *  matching LoosePartNameFilters. Server-only. */
	void DropSails();

	/** Surface fire spawned on the wreck - "and it all be on fire". */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Sink")
	TSoftObjectPtr<class UNiagaraSystem> WreckFireSystem;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Sink", meta = (ClampMin = "0"))
	int32 WreckFireCount = 6;

	/** Drop the tower: find the mill geometry, stand a fractured proxy in its place, release the top
	 *  half and set the wreck alight. Server-only; the proxy's own replicated release carries it. */
	void SinkTower();

	/** The fuse. Long enough to run, short enough to feel like a mistake if you don't. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Tuning")
	float DustBuildupSeconds = 9.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Mill|Tuning")
	float BuildupTickInterval = 0.1f;

	/** Detonation lights everything flammable nearby - this is how the mill takes the yard with it. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Tuning")
	float DetonationIgniteRadius = 1400.f;

	UPROPERTY(ReplicatedUsing = OnRep_Stage)
	EGSMillStage Stage = EGSMillStage::Intact;

	float BuildupElapsed = 0.f;
	FTimerHandle BuildupTimerHandle;
};
