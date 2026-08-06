#include "Destruction/GSTorchProjectile.h"
#include "Destruction/GSFireVolume.h"
#include "Destruction/GSFlammableComponent.h"
#include "Destruction/GSBurnObjectiveBase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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

	// The visible prop. No collision of its own: the 8uu sphere above is the projectile's whole
	// physical presence, and a second colliding body on the same actor would produce a second hit
	// event and a second fire volume from one throw.
	TorchMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TorchMesh"));
	TorchMeshComponent->SetupAttachment(RootComponent);
	TorchMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TorchMeshComponent->SetGenerateOverlapEvents(false);

	// C++ DEFAULT, 2026-08-01. See the header for the full argument; the short version is the
	// precedent set three lines into AGSFireVolume's own constructor
	// (FireDamageEffectClass = UGSGE_FireDamage::StaticClass()): a C++ default cannot go missing
	// from a content folder, and this reference being null is what stopped the project's only
	// closed damage loop from closing.
	FireVolumeClass = AGSFireVolume::StaticClass();
}

void AGSTorchProjectile::BeginPlay()
{
	Super::BeginPlay();
	CollisionSphere->OnComponentHit.AddDynamic(this, &AGSTorchProjectile::OnProjectileHit);

	// NEVER collide with the goblin who threw it (2026-08-05). The collision profile is
	// BlockAllDynamic, which blocks the Pawn channel, so before this the thrower was just
	// another thing to stick to - and the projectile's whole hit response is "attach to what
	// you hit", so a torch that clipped its thrower welded itself to him for its full 15s
	// lifespan. Note the aim arc has ignored the owner since it was written
	// (Params.ActorsToIgnore.Add(Owner)); this makes the actual throw agree with the preview,
	// which is the property the whole aim framework exists to guarantee.
	AActor* Thrower = GetInstigator();
	if (!Thrower)
	{
		Thrower = GetOwner();
	}
	if (Thrower)
	{
		CollisionSphere->IgnoreActorWhenMoving(Thrower, true);
	}

	// Resolved here rather than in the constructor: the constructor runs on the CDO during module
	// load, which is the one place a synchronous package load is genuinely unwelcome. Warn once,
	// then fly on invisibly - the torch's job is fire, not looks.
	if (!TorchMesh.IsNull() && !bTorchMeshResolveFailed && TorchMeshComponent)
	{
		if (UStaticMesh* Mesh = TorchMesh.LoadSynchronous())
		{
			TorchMeshComponent->SetStaticMesh(Mesh);
		}
		else
		{
			bTorchMeshResolveFailed = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[GoblinSiege] %s could not load its torch mesh (%s) - the throw will be "
					 "invisible in flight. It still sticks, still ignites, and still spawns its "
					 "fire volume."),
				*GetName(), *TorchMesh.ToString());
		}
	}
}

void AGSTorchProjectile::OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bStuck)
	{
		return;
	}

	// Belt and braces against the same bug the IgnoreActorWhenMoving in BeginPlay covers.
	// That call only suppresses SWEEP hits from this component's own movement; a hit
	// generated the other way round - the goblin's capsule sweeping into a torch that is
	// already in flight, which is exactly what running forward after a throw does - still
	// arrives here. Returning BEFORE bStuck is set matters: latching it would leave a torch
	// that brushed its thrower permanently unable to stick to anything else.
	if (OtherActor && (OtherActor == GetInstigator() || OtherActor == GetOwner()))
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

		// Hit-actor lookup alone can never light a field: a torch thrown into wheat hits the
		// LANDSCAPE, and the field objective's cells are data with no collision of their own. So
		// ask the objectives whether any of them owns this impact point.
		//
		// The mill answers false by design and is unreachable this way - it wants a torch through
		// a window, which its own overlap volumes handle.
		if (AGSBurnObjectiveBase* Objective =
			AGSBurnObjectiveBase::FindObjectiveAtLocation(this, Hit.ImpactPoint))
		{
			Objective->IgniteAtLocation(Hit.ImpactPoint);
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
