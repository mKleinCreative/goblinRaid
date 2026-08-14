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
	 * anything in code.
	 *
	 * CONFIRMED 2026-08-09 by measuring the asset rather than by eye: GS_Arrow is 59.5uu long, its
	 * long axis is +Z, and its pivot is at the tail. The default in the constructor is the
	 * correction for exactly that, and the reasoning is written out there. It was identity until
	 * now, which is why arrows flew standing straight up out of their own collision sphere.
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
	 * Damage multiplier for an arrow that strikes the head.
	 *
	 * The reward for the shot the bow is actually for. It also sharpens the knight answer from GDD
	 * §217 - a bow already ignores plate, and a headshot turns "a bow shot placed at the gaps" from
	 * a way through his armour into a way to end him.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Arrow", meta = (ClampMin = "1.0"))
	float HeadshotMultiplier = 2.5f;

	/**
	 * Bone-name fragments that count as a head, matched case-insensitively as SUBSTRINGS.
	 *
	 * Substrings because this project runs two skeletons with different retargeting histories
	 * (GOB_Scout_v2 and SK_Human_Skeleton), and Mixamo-derived rigs prefix bones - "mixamorig:Head"
	 * and "head" both have to match. An exact name would silently never fire on one of the two
	 * skeletons, and a headshot bonus that quietly does nothing is worse than not having one.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Arrow")
	TArray<FName> HeadBoneFragments = { TEXT("head"), TEXT("neck") };

	/** True when BoneName contains any HeadBoneFragments entry. An empty/None bone - a hit on a
	 *  simple collision capsule rather than a physics asset - is never a headshot, which is the
	 *  right default: a body with no bones to aim at should not award a bonus for luck. */
	bool IsHeadBone(FName BoneName) const;

	/** Did this land on the head? Uses the reported bone when the mesh was hit directly, and
	 *  otherwise measures the impact point against the head bone's world position - because the
	 *  arrow usually stops on the CAPSULE, which carries no bone name. */
	bool IsHeadshot(const AActor* HitActor, const FHitResult& Hit) const;

	/** How close to the head bone an impact must land to count.
	 *
	 *  This is a head-sized number, and it has to be: the earlier version asked the skeleton for its
	 *  NEAREST bone instead, and a chest-height hit on the capsule is nearer the neck than anything
	 *  else on these rigs - so every arrow was a headshot and archers did a flat 50 damage. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Arrow", meta = (ClampMin = "1.0"))
	float HeadshotRadius = 22.f;

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
