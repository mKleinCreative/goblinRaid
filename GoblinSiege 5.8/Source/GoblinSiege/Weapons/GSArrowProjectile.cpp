#include "Weapons/GSArrowProjectile.h"
#include "Combat/GSGE_WeaponDamage.h"
#include "Combat/GSGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"

AGSArrowProjectile::AGSArrowProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(6.f);
	CollisionSphere->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionSphere->SetNotifyRigidBodyCollision(true);
	RootComponent = CollisionSphere;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->InitialSpeed = 6000.f;
	ProjectileMovement->MaxSpeed = 6000.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	// Not zero, on purpose. A perfectly flat arrow is a hitscan shot wearing a projectile's costume:
	// nothing to read, nothing to lead, and an aim arc with no shape to it. 0.2 gives a droop that is
	// invisible across a courtyard and very much not invisible across the fields, which is exactly
	// where a bow should start asking something of the player.
	ProjectileMovement->ProjectileGravityScale = 0.2f;

	ArrowMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowMesh"));
	ArrowMeshComponent->SetupAttachment(RootComponent);
	// No collision of its own: the 6uu sphere above is the arrow's whole physical presence. A second
	// colliding body on the same actor is how AGSTorchProjectile would have spawned two fire volumes
	// from one throw, and here it would be two damage applications from one arrow.
	ArrowMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArrowMeshComponent->SetGenerateOverlapEvents(false);

	// C++ default, matching UGSGA_SwordLight's DamageEffectClass and AGSTorchProjectile's
	// FireVolumeClass. A null here would produce an arrow that flies perfectly and deals nothing,
	// which reads as a balance decision rather than a broken reference.
	DamageEffectClass = UGSGE_WeaponDamage::StaticClass();
}

void AGSArrowProjectile::BeginPlay()
{
	Super::BeginPlay();
	CollisionSphere->OnComponentHit.AddDynamic(this, &AGSArrowProjectile::OnProjectileHit);

	SetLifeSpan(MaxFlightSeconds);

	// Resolved here rather than in the constructor: the constructor runs on the CDO during module
	// load, which is the one place a synchronous package load is unwelcome. Warn once, then fly on
	// invisibly - the arrow's job is damage, not looks.
	if (!ArrowMesh.IsNull() && !bArrowMeshResolveFailed && ArrowMeshComponent)
	{
		if (UStaticMesh* Mesh = ArrowMesh.LoadSynchronous())
		{
			ArrowMeshComponent->SetStaticMesh(Mesh);
		}
		else
		{
			bArrowMeshResolveFailed = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[GoblinSiege] %s could not load its arrow mesh (%s) - the shot will be "
					 "invisible in flight. It still flies, still hits, and still deals damage."),
				*GetName(), *ArrowMesh.ToString());
		}
	}
}

void AGSArrowProjectile::OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bHasHit)
	{
		return;
	}
	bHasHit = true;

	// Stop dead the moment it lands, on every machine. Doing this outside the authority check means a
	// client sees the arrow stop on contact instead of skating on for a round trip.
	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->ProjectileGravityScale = 0.f;
	SetActorEnableCollision(false);

	if (HasAuthority() && DamageEffectClass)
	{
		AActor* Shooter = GetInstigator() ? static_cast<AActor*>(GetInstigator()) : GetOwner();

		// The damage application below is UGSGA_SwordLight's, verbatim in shape. Reused rather than
		// reinvented specifically so the arrow goes through UGSDamageExecCalculation's armour,
		// blocking and frontal-arc rules on the same path a sword does - an arrow that ignored a
		// raised shield would be a rule the player could never see the logic of.
		UAbilitySystemComponent* SourceASC = nullptr;
		if (const IAbilitySystemInterface* SourceASI = Cast<IAbilitySystemInterface>(Shooter))
		{
			SourceASC = SourceASI->GetAbilitySystemComponent();
		}

		UAbilitySystemComponent* TargetASC = nullptr;
		if (const IAbilitySystemInterface* TargetASI = Cast<IAbilitySystemInterface>(OtherActor))
		{
			TargetASC = TargetASI->GetAbilitySystemComponent();
		}

		// Both null is the overwhelmingly common case - an arrow in a wall - and is not an error.
		if (SourceASC && TargetASC && OtherActor != Shooter)
		{
			FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
			Context.AddInstigator(Shooter, this);
			Context.AddSourceObject(this);

			const FGameplayEffectSpecHandle SpecHandle =
				SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.f, Context);
			if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
			{
				// The Damage.* tag is BOTH the type marker and the SetByCaller key, and it must go on
				// the SPEC - the same tag sitting in a Blueprint effect's asset tags is invisible to
				// UGSDamageExecCalculation and silently deals zero.
				SpecHandle.Data->AddDynamicAssetTag(GSTags::Damage_Bow);
				SpecHandle.Data->SetSetByCallerMagnitude(GSTags::Damage_Bow, Damage);

				SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, TargetASC);
			}
		}
	}

	// Stick briefly so the hit reads, then go. Shorter than the torch's 15s because a torch is
	// meant to be found burning and an arrow is just litter.
	SetLifeSpan(5.f);

	if (OtherComp)
	{
		AttachToComponent(OtherComp, FAttachmentTransformRules::KeepWorldTransform);
	}
}
