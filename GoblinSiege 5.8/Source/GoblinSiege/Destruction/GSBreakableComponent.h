// Makes a prop breakable - and, on a window, makes it a way into a building.
// Written 2026-08-05 for Michael's flow: "a torch gets thrown into a window ... the torch flies
// into the window and causes the building to catch on fire."
//
// ---------------------------------------------------------------------------------------------
// WHAT THIS COMPONENT IS, AFTER THE ACF ADOPTION (2026-08-17, #165)
//
// It is the ADAPTER, not the destruction system. ACF's UACFDestructableComponent owns Chaos: it
// applies external strain to a registered geometry collection over a NetMulticast, derives piece
// velocity from impulse/mass, and forces the Destructible collision profile. That is the part we
// were going to write badly, and it already exists.
//
// What ACF has no opinion about, and what stays here:
//
//   - WHETHER a thing breaks. Hit points, idempotence, server authority.
//   - What breaking MEANS to this game: bOpensBuilding -> AGSBuildingObjective::IgniteInterior.
//     A broken window is a way in; that concept does not exist in ACF and cannot.
//   - The no-collection fallback. ACF's BeginPlay finds a geometry collection or logs a warning and
//     does nothing at all. 113 placed windows have no fracture asset and are not getting one, so
//     without this path they would simply stop working.
//
// So: Break() decides, then delegates. If the owner carries a UACFDestructableComponent it hands
// off to ACF; otherwise it hides the mesh and puffs FX, which is the intended treatment for windows.
//
// ---------------------------------------------------------------------------------------------
// THIS IS NOT UGSCrumbleComponent, AND MERGING THEM WOULD BREAK BOTH (2026-08-25, #317)
//
// They look like the same feature and are opposites:
//
//   Break (here)  - SHATTER IN PLACE. ACF applies strain NOW and the thing comes apart where it
//                   stands. Right for a window, a crate, a chest.
//   Crumble       - RELEASE. Un-pin a dormant collection and let it come apart under its own
//                   weight, on the way down and on landing. Right for a monument hauled over and a
//                   building that has burnt through.
//
// GSTopplableComponent.h has carried this warning since #193 for the concrete reason: routing a
// topple through Break() shatters the statue at the top of its lean instead of letting it fall, and
// the fall is the entire point. A prop may carry both components - smashed it shatters, destroyed
// it crumbles - but neither may call the other.
//
// ---------------------------------------------------------------------------------------------
// THE ONE THING THAT WILL BITE THE NEXT PERSON
//
// ACF NEVER CALLS SetSimulatePhysics. ApplyChaosDestructionAt only applies strain and breaking
// velocity, and bEnforceDestructibleCollisionSetup only sets a collision profile and clears body
// locks. A dormant collection - which is what you want for a cheap intact prop, and what
// AGSDestructibleObjective authors - will absorb the strain and visibly do nothing. Break() enables
// simulation immediately before delegating. Do not remove that line because it looks redundant.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSBreakableComponent.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UNiagaraSystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnBroken);
/** Fired on a hit that did NOT finish the prop off - for a flinch, a dust tick, a chip. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSOnSmashHit, int32, HitPointsRemaining, int32, MaxHitPoints);

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSBreakableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSBreakableComponent();

	/**
	 * Take a hit. Returns true if this hit BROKE it.
	 *
	 * The single entry point for everything that damages props: a thrown torch deals 1, a sword swing
	 * deals 1, and a prop with SmashHitPoints 1 dies to either. This replaced the torch calling
	 * Break() directly, which made "how many hits does this take" a question the torch could not ask.
	 *
	 * Server-only, like Break(). Calling it on a client is a no-op rather than an error - the client's
	 * swing is cosmetic and the server's copy is what counts.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Breakable")
	bool ApplySmash(int32 Damage, const FVector& ImpactPoint, const FVector& ImpactVelocity, AActor* Instigator);

	/**
	 * Break it outright, ignoring remaining hit points. Idempotent - a broken window stays broken, and
	 * a second torch through the same hole must not re-shatter it or re-light the building.
	 *
	 * @param ImpactPoint    where the hit landed, for strain origin and FX placement
	 * @param ImpactVelocity direction of travel, so shards fly INTO the room rather than outward
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Breakable")
	void Break(const FVector& ImpactPoint, const FVector& ImpactVelocity);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Breakable")
	bool IsBroken() const { return bBroken; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Breakable")
	int32 GetHitPointsRemaining() const { return HitPointsRemaining; }

	/**
	 * Should breaking this piece light the building behind it?
	 *
	 * True on windows. False on anything breakable that is not a way in - a fence, a crate - so the
	 * component can be reused for scenery without every broken plank torching a house.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Breakable")
	bool DoesOpenBuilding() const { return bOpensBuilding; }

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Breakable")
	FGSOnBroken OnBroken;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Breakable")
	FGSOnSmashHit OnSmashHit;

	/** Placement-time setter. A window opens its building; a crate or a fence does not. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Breakable")
	void SetOpensBuilding(bool bValue) { bOpensBuilding = bValue; }

	/** Placement-time setter, for the Python dressing pass. See IntactMeshComponentName. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Breakable")
	void SetIntactMeshComponent(UStaticMeshComponent* InMesh) { IntactMeshComponent = InMesh; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Broken();

	/** Hide the intact mesh and stop it blocking - so the torch that broke it can carry on through
	 *  instead of bouncing off a window that is visually gone. */
	void RetireIntactMesh();

	/** Which mesh RetireIntactMesh hides. Resolved once, at BeginPlay. */
	UStaticMeshComponent* ResolveIntactMesh() const;

	// ------------------------------------------------------------------ tuning

	/**
	 * How many hits it takes. Window and crate 1, chest 2, statue 4.
	 *
	 * A torch deals 1, so a window still dies to a single throw exactly as it did before hit points
	 * existed - which is the whole reason the default is 1.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Breakable", meta = (ClampMin = "1"))
	int32 SmashHitPoints = 1;

	/**
	 * The mesh to hide when this breaks. Leave unset and it resolves automatically at BeginPlay.
	 *
	 * This exists because the old code hid EVERY UStaticMeshComponent on the owner. That is right for
	 * a window, which is a StaticMeshActor with exactly one mesh, and catastrophic for a multi-mesh
	 * prop Blueprint - the whole actor vanishes, base and all, and it reads as a Chaos bug rather
	 * than a naming one.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Breakable")
	FName IntactMeshComponentName;

	/**
	 * Swap the intact mesh to this on break, instead of hiding it.
	 *
	 * The cheapest credible destruction in the project, and it costs no art at all where a pack
	 * already ships a broken variant. Measured 2026-08-17: SM_CrateSquare -> SM_CrateBroken and
	 * SM_Barrel_01 -> SM_BarrelBroken share a pivot, a base height and their bounds to within a
	 * centimetre, so the swap needs no offset and no per-prop tuning. They are authored swap pairs,
	 * not set dressing - which is worth recording, because they LOOK like set dressing in the content
	 * browser and were nearly dismissed as such.
	 *
	 * Unset (the normal case) and breaking hides the mesh, which is the right answer for a window.
	 *
	 * Soft, for the same reason the old fracture reference was: a level should not load a broken
	 * variant for every prop a player will probably never touch.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Breakable")
	TSoftObjectPtr<UStaticMesh> BrokenMesh;

	/** Debris puff. Carries the moment on props with no fracture asset, and adds dust on ones with. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Breakable")
	TSoftObjectPtr<UNiagaraSystem> BreakFX;

	/** Windows: true. Fences and crates: false. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Breakable")
	bool bOpensBuilding = true;

	/**
	 * On break, make the owner's UGSInteractableComponent available.
	 *
	 * This is what turns "smash" and "loot" into one verb chain instead of two unrelated ones: a
	 * crate ships with its interactable switched OFF and its lid on, you break the lid, and only then
	 * can you loot it. Set bIsAvailable=false on the interactable for that to mean anything - an
	 * already-available container will simply stay available and this flag does nothing.
	 *
	 * Cheaper than fracturing, and it reuses meshes that already ship: a crate is SM_CrateOpen with
	 * SM_CrateLid sitting on top, and breaking hides only the lid (see IntactMeshComponentName).
	 * Reserve real Chaos for things that should genuinely shatter, like the statue.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Breakable")
	bool bUnlockInteractableOnBreak = false;

	/** Push applied to the pieces, along the hit's direction of travel. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Breakable")
	float DebrisImpulse = 400.f;

	UPROPERTY(ReplicatedUsing = OnRep_Broken)
	bool bBroken = false;

	/** Server-side only; clients learn the outcome through bBroken, not the countdown. */
	int32 HitPointsRemaining = 0;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> IntactMeshComponent;
};
