// The thrown torch: arcs, sticks on impact, ignites what it hits, and spawns an AGSFireVolume at
// the impact point (design doc §7). Reconstructed 2026-07-19.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSTorchProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
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

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Torch")
	TSubclassOf<AGSFireVolume> FireVolumeClass;

	bool bStuck = false;
};
