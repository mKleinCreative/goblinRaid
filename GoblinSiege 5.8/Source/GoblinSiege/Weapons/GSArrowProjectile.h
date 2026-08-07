// The Scout's arrow - the second half of "sword <-> bow" (AGENT_STATE.md, canonical class decision).
// Added 2026-08-04. Until today the ranged half of the Scout's kit was scaffolding with nothing
// behind it: UGSWeaponComponent could toggle ranged mode, move the bow to the hand socket and wear a
// quiver, the Damage.Bow tag was declared and the damage exec already handled it - and pressing
// attack in ranged mode did nothing at all.
//
// Deliberately built to AGSTorchProjectile's shape rather than as a hitscan trace. Three reasons,
// in order of how much they matter:
//   1. It can be dodged. A raid whose archers cannot be juked is a raid where cover is decoration.
//   2. The SAME UGSAimComponent prediction draws its path, because the arc reads speed and gravity
//      off this class's own ProjectileMovement CDO. A hitscan shot would need a second, separate
//      preview path, and two preview paths is how a preview starts lying.
//   3. AI archers get it free later - BP_ErikaArcher already exists and wants exactly this.
//
// It is fast and nearly flat (6000uu/s at 0.2 gravity) rather than a torch's lazy lob, so the arc it
// draws is a gentle droop instead of a rainbow. That difference is entirely data on this class; no
// code anywhere branches on "is this an arrow".
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSArrowProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UGameplayEffect;

UCLASS()
class GOBLINSIEGE_API AGSArrowProjectile : public AActor
{
	GENERATED_BODY()

public:
	AGSArrowProjectile();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Arrow")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Arrow")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** The shaft you see in flight. Always created, empty until ArrowMesh resolves - same reasoning
	 *  as AGSTorchProjectile::TorchMeshComponent. */
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Arrow|Visual")
	TObjectPtr<UStaticMeshComponent> ArrowMeshComponent;

	/**
	 * The shaft mesh, C++-defaulted to /Game/_Import/Weapons/GS_Arrow.
	 *
	 * It was previously soft AND unset, "like every other content reference in this project" - but
	 * the comparison was wrong. The bow, quiver, sword and torch meshes are unset here because they
	 * are per-weapon kit identity chosen by UGSWeaponDataAsset, which really does assign them. Nothing
	 * anywhere assigned this one, so the arrow shipped invisible from the day it was written: a shot
	 * you cannot see is a shot you cannot lead, aim off, or learn the droop of.
	 *
	 * Defaulted in C++ for the same reason DamageEffectClass on this class is - the arrow's appearance
	 * belongs to the projectile, not to the weapon that fired it, and a default in code cannot be left
	 * blank by a data asset nobody edited. Override it on a Blueprint subclass when an arrow needs to
	 * look different; do not clear it.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Arrow|Visual")
	TSoftObjectPtr<UStaticMesh> ArrowMesh;

	/**
	 * Applied to the mesh component after the mesh resolves, to line the shaft up with the direction
	 * of travel.
	 *
	 * ProjectileMovement has bRotationFollowsVelocity, so the ACTOR's +X always points along the
	 * flight path; whether the mesh's own long axis agrees is a property of the FBX and not of
	 * anything in code. Identity is the default because GS_Arrow's authored orientation has not been
	 * confirmed in flight - if the arrow flies sideways or tail-first, this is the knob, and it needs
	 * a Blueprint subclass to be editable at all since the spawned class is this C++ one.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Arrow|Visual")
	FTransform ArrowMeshOffset;

	/**
	 * Damage on hit, passed as the SetByCaller magnitude under GSTags::Damage_Bow.
	 *
	 * The mitigation - armour, blocking, the frontal-arc check - is entirely
	 * UGSDamageExecCalculation's business, exactly as it is for a sword swing. This number is the
	 * raw figure that goes in, not the figure that lands.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Arrow", meta = (ClampMin = "0.0"))
	float Damage = 20.f;

	/**
	 * Carries UGSDamageExecCalculation. C++-defaulted to UGSGE_WeaponDamage in the constructor, the
	 * same default UGSGA_SwordLight sets on its own DamageEffectClass - one damage effect, many
	 * callers, with the Damage.* tag on the SPEC deciding the type.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Arrow")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** Seconds before an arrow that hit nothing gives up and despawns. Without it, a shot into the
	 *  sky is an actor that lives until the raid ends. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Arrow", meta = (ClampMin = "0.1"))
	float MaxFlightSeconds = 8.f;

	// The warn-once latch for a failed mesh resolve is a file-scope static in the .cpp, NOT a member.
	// An arrow is spawned fresh for every shot, so a member starts false on each one and warns again
	// on every arrow forever - the exact bug ticket #030 fixed on AGSTorchProjectile's two latches.
	// This was the third twin.

	/** Latched on the first contact. A single arrow deals damage once, even if the engine reports
	 *  two contacts on the same frame - the same guard AGSTorchProjectile::bStuck provides for its
	 *  fire volume, and for the same reason. */
	bool bHasHit = false;
};
