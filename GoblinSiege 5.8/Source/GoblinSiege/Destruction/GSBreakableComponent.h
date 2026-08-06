// Makes a kit piece breakable - and, on a window, makes it a way into a building.
// Written 2026-08-05 for Michael's flow: "a torch gets thrown into a window ... the torch flies
// into the window and causes the building to catch on fire."
//
// ---------------------------------------------------------------------------------------------
// WHY A WINDOW IS THE ONE THING WORTH FRACTURING
//
// Whole buildings are a bad Chaos candidate: a house here is 20-40 kitbashed pieces of 12-780
// triangles, and fracturing all of them means thousands of rigid bodies on a game thread that has
// no room. A window is the opposite case and it is worth being precise about why:
//
//   - it is small (82-432 triangles across the 18 window meshes on this map)
//   - it breaks exactly once, and never un-breaks
//   - only the window the player actually hit ever simulates
//   - the debris falls, settles, and sleeps within a couple of seconds
//
// So the cost is bounded by how many windows the player has personally thrown a torch through,
// which is a small number, rather than by how many exist.
//
// ---------------------------------------------------------------------------------------------
// THE FALLBACK IS NOT A COMPROMISE, IT IS THE SHIPPING ORDER
//
// BrokenCollection is OPTIONAL. With one assigned you get a real fracture; without one the window
// hides, drops its collision and puffs debris. Both paths open the building identically.
//
// That split is deliberate. The GDD deferred breaking meshes ("anything burnable chars black +
// smoulders ... breaking meshes later"), and authoring 18 geometry collections is real art time.
// Gating the GAMEPLAY on that art would mean the torch-through-a-window flow could not be played or
// tuned until every asset existed. This way the flow works today and each fracture asset upgrades
// one window type whenever someone makes it, with no code change.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSBreakableComponent.generated.h"

class UGeometryCollection;
class UStaticMeshComponent;
class UNiagaraSystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnBroken);

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSBreakableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSBreakableComponent();

	/**
	 * Break it. Idempotent - a broken window stays broken, and a second torch through the same hole
	 * must not re-shatter it or re-light the building.
	 *
	 * @param ImpactPoint    where the torch struck, for debris impulse and FX placement
	 * @param ImpactVelocity the torch's velocity, so shards fly INTO the room rather than outward
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Breakable")
	void Break(const FVector& ImpactPoint, const FVector& ImpactVelocity);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Breakable")
	bool IsBroken() const { return bBroken; }

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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Broken();

	/** Hide the intact mesh and stop it blocking - so the torch that broke it can carry on through
	 *  instead of bouncing off a window that is visually gone. */
	void RetireIntactMesh();

	/** Spawn the fractured version, if one was authored. Returns false when there is none, which is
	 *  the normal case until the art exists. */
	bool SpawnFracture(const FVector& ImpactPoint, const FVector& ImpactVelocity);

	// ------------------------------------------------------------------ tuning

	/**
	 * The fractured version of this piece. Optional - see the header. Soft, because a level holding
	 * 113 windows should not load 113 geometry collections that most players will never break.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Breakable")
	TSoftObjectPtr<UGeometryCollection> BrokenCollection;

	/** Debris puff. Carries the moment when there is no fracture asset, and adds dust when there is. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Breakable")
	TSoftObjectPtr<UNiagaraSystem> BreakFX;

	/** Windows: true. Fences and crates: false. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Breakable")
	bool bOpensBuilding = true;

	/** Push applied to the shards, along the torch's direction of travel. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Breakable")
	float DebrisImpulse = 400.f;

	/** Debris is litter, not physics you keep paying for. Cleared after this long. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Breakable")
	float DebrisLifeSeconds = 12.f;

	UPROPERTY(ReplicatedUsing = OnRep_Broken)
	bool bBroken = false;
};
