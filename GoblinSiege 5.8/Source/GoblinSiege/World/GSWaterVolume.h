// Water you can swim in. Added 2026-08-06.
//
// Until now the village "water" was `Plane2` - a /Engine/BasicShapes/Plane with MI_VillageWater on
// it, left ECR_BLOCK to Pawn. You walked on it. There is no water simulation anywhere in this
// project: Epic's Water plugin is not enabled, there are no water bodies, and no swim code ever
// touched Buoyancy or MaxSwimSpeed.
//
// This is deliberately the SMALLEST thing that makes swimming real: an APhysicsVolume with
// bWaterVolume set. UCharacterMovementComponent already knows how to swim - it switches to
// MOVE_Swimming on entering a water volume with no custom movement mode, no FSavedMove subclass, and
// no network prediction work. A bespoke swim mode would be weeks of that for the same result.
//
// Why a subclass at all, rather than placing a bare APhysicsVolume and ticking the box:
//   1. It carries the swim tuning, so the numbers live with the thing they describe.
//   2. It is findable. A bare APhysicsVolume in a level is invisible to grep and to code that wants
//      to ask "am I in water" - and this project has been bitten twice by settings that existed only
//      inside a .umap where nobody could see them (the fog cards, and Plane2 itself).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PhysicsVolume.h"
#include "GSWaterVolume.generated.h"

UCLASS()
class GOBLINSIEGE_API AGSWaterVolume : public APhysicsVolume
{
	GENERATED_BODY()

public:
	AGSWaterVolume();

	/** Stamina per second while swimming at normal speed. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Water")
	float GetSwimDrainRate() const { return SwimDrainRate; }

	/** Stamina per second while dashing. Replaces the normal rate, it does not add to it. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Water")
	float GetSwimDashDrainRate() const { return SwimDashDrainRate; }

protected:
	/**
	 * Lower than sprint's 10/s: you can cross the village river without thinking, and cannot cross
	 * the wide water without planning. The GDD's ask is that horizontal traversal beats swimming,
	 * which is a speed statement - the drain is what makes a long swim a commitment.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Water|Tuning", meta = (ClampMin = "0.0"))
	float SwimDrainRate = 6.f;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Water|Tuning", meta = (ClampMin = "0.0"))
	float SwimDashDrainRate = 18.f;
};
