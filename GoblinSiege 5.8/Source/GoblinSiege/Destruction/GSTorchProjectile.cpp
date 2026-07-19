#include "Destruction/GSTorchProjectile.h"
#include "Destruction/GSFireVolume.h"
#include "Destruction/GSFlammableComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

AGSTorchProjectile::AGSTorchProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(8.f);
	CollisionSphere->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionSphere->SetNotifyRigidBodyCollision(true);
	RootComponent = CollisionSphere;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->InitialSpeed = 1400.f;
	ProjectileMovement->MaxSpeed = 1400.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->ProjectileGravityScale = 1.f; // torches arc - readable, dodgeable
}

void AGSTorchProjectile::BeginPlay()
{
	Super::BeginPlay();
	CollisionSphere->OnComponentHit.AddDynamic(this, &AGSTorchProjectile::OnProjectileHit);
}

void AGSTorchProjectile::OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bStuck)
	{
		return;
	}
	bStuck = true;

	// "Sticks on impact" - freeze in place and ride along with whatever we hit.
	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->ProjectileGravityScale = 0.f;
	SetActorEnableCollision(false);
	if (OtherComp)
	{
		AttachToComponent(OtherComp, FAttachmentTransformRules::KeepWorldTransform);
	}

	if (HasAuthority())
	{
		if (OtherActor)
		{
			if (UGSFlammableComponent* Flammable = OtherActor->FindComponentByClass<UGSFlammableComponent>())
			{
				Flammable->Ignite();
			}
		}

		if (FireVolumeClass)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			GetWorld()->SpawnActor<AGSFireVolume>(FireVolumeClass, Hit.ImpactPoint,
				FRotator::ZeroRotator, SpawnParams);
		}
	}

	SetLifeSpan(15.f); // burnt-out torch prop despawns after a beat
}
