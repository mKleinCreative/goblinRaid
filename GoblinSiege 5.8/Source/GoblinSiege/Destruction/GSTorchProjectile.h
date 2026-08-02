// The thrown torch: arcs, sticks on impact, ignites what it hits, and spawns an AGSFireVolume at
// the impact point (design doc §7). Reconstructed 2026-07-19.
//
// 2026-08-01 - CLOSED THE TWO NULL LINKS AND GAVE IT A BODY. FireVolumeClass was null, so a torch
// that hit the ground correctly ignited flammables and correctly asked the burn objectives about
// the impact point, and then spawned no fire - which meant fire damage, the ONLY closed damage loop
// in the project, never actually closed. It now C++-defaults to AGSFireVolume, and the projectile
// has a visible mesh instead of being an invisible 8uu collision sphere.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSTorchProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UStaticMesh;
class UStaticMeshComponent;
class AGSFireVolume;

UCLASS()
class GOBLINSIEGE_API AGSTorchProjectile : public AActor
{
	GENERATED_BODY()

public:
	AGSTorchProjectile();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Torch")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Torch")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** The thing you actually see flying. Created unconditionally and left empty when TorchMesh is
	 *  unset - an empty UStaticMeshComponent draws nothing and costs nothing, and having it always
	 *  exist means the mesh can be dropped in from the details panel mid-PIE. */
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Torch|Visual")
	TObjectPtr<UStaticMeshComponent> TorchMeshComponent;

	/**
	 * The thrown torch prop, resolved in BeginPlay and warned about exactly once on failure.
	 *
	 * Soft and unset by default like every other content reference in this module (see
	 * AGSFireVolume::FireSystem, UGSBurnFXComponent::SmolderSystem): the mesh does not exist yet,
	 * and a torch nobody can see must still arc, still stick, still ignite and still spawn its fire
	 * volume. Invisible but functional - never a load failure.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Torch|Visual")
	TSoftObjectPtr<UStaticMesh> TorchMesh;

	/**
	 * What to spawn at the impact point. C++-DEFAULTED TO AGSFireVolume (2026-08-01) - it was null
	 * until today, which is why a torch could hit dry wheat and set nothing alight.
	 *
	 * The precedent is AGSFireVolume itself, which C++-defaults its own FireDamageEffectClass to
	 * UGSGE_FireDamage in its constructor for exactly this reason: a C++ default CANNOT GO MISSING
	 * FROM A CONTENT FOLDER. A Blueprint subclass assigned in the editor is one asset rename, one
	 * bad merge, or one un-migrated folder away from being null again, and null here is silent -
	 * the torch lands, sticks, and simply fails to start a fire, which looks like a gameplay balance
	 * decision rather than a broken reference. Override this with a BP subclass when a torch fire
	 * needs to differ from the default one; do not clear it.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Torch")
	TSubclassOf<AGSFireVolume> FireVolumeClass;

	/** One-shot warn latch for TorchMesh, matching the rest of the module's soft-asset pattern. */
	bool bTorchMeshResolveFailed = false;

	bool bStuck = false;
};
