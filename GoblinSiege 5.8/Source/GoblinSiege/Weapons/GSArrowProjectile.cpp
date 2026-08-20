#include "Weapons/GSArrowProjectile.h"
#include "Characters/GSCharacterBase.h"
#include "Combat/GSGE_WeaponDamage.h"
#include "Combat/GSGameplayTags.h"
#include "Weapons/Abilities/GSGA_SwordLight.h"   // GSCombatDebugEnabled() - see the note there (#167)
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"

// File-scope, not a member: see the note in the header. One warning per session for a broken mesh
// reference, rather than one per arrow.
static bool GArrowMeshResolveFailed = false;

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

	// The shaft itself. A soft path assigned on the CDO, resolved in BeginPlay - the reference is
	// recorded here where it cannot be left blank, without the constructor doing a package load.
	ArrowMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Game/_Import/Weapons/GS_Arrow.GS_Arrow")));

	// MEASURED, not guessed. GS_Arrow's bounds are extent (1.4, 1.6, 29.75) with the origin at
	// Z = +29.75 - so the shaft is 59.5uu long, its long axis is +Z, and its pivot sits at the BASE
	// of that span rather than in the middle.
	//
	// The actor's +X follows velocity (bRotationFollowsVelocity), so with the identity transform the
	// header used to default to, the shaft rendered at ninety degrees to its own flight path -
	// standing straight up out of the collision sphere, which is exactly what was reported.
	//
	//   Pitch -90  : swings the mesh's local +Z round onto the actor's +X, so the shaft lies along
	//                the direction of travel.
	//   X = -59.5  : the pivot is the tail, so after rotating, the whole shaft sat FORWARD of the
	//                collision sphere and the arrow appeared to be launched from its own midpoint.
	//                Pulling it back by its own length puts the head at the origin, where the 6uu
	//                collision sphere actually is - so the point of the arrow is the point that hits.
	//
	// If it ever flies tail-first, this is a 180 flip of Pitch and nothing else; the length and the
	// axis above are measured facts about the asset.
	//
	// The 1.25 is Michael's call on 2026-08-09 - the bow and its arrows read a little small in hand,
	// and DA_Weapon_Scout's bow and quiver were scaled to match in the same pass.
	//
	// THE OFFSET IS DERIVED FROM THE SCALE, not written next to it as a second constant. The head
	// sits on the collision sphere only because the pull-back equals the shaft's rendered length;
	// scaling the mesh without scaling the offset would slide the point off the sphere by a quarter
	// of its length and quietly undo the fix above. Change ArrowMeshScale and the rest follows.
	{
		constexpr float ArrowMeshLength = 59.5f;   // measured from GS_Arrow's bounds
		constexpr float ArrowMeshScale = 1.25f;

		ArrowMeshOffset = FTransform(
			FRotator(-90.f, 0.f, 0.f),
			FVector(-ArrowMeshLength * ArrowMeshScale, 0.f, 0.f),
			FVector(ArrowMeshScale));
	}

	// C++ default, matching UGSGA_SwordLight's DamageEffectClass and AGSTorchProjectile's
	// FireVolumeClass. A null here would produce an arrow that flies perfectly and deals nothing,
	// which reads as a balance decision rather than a broken reference.
	DamageEffectClass = UGSGE_WeaponDamage::StaticClass();
}

void AGSArrowProjectile::BeginPlay()
{
	Super::BeginPlay();
	CollisionSphere->OnComponentHit.AddDynamic(this, &AGSArrowProjectile::OnProjectileHit);

	// Never collide with the archer who loosed it - see AGSTorchProjectile::BeginPlay for the
	// full argument. The arrow had a milder version of the same bug: the damage block already
	// skipped OtherActor == Shooter, so it never shot its owner, but it still latched bHasHit,
	// stopped dead and attached. A self-hit therefore produced an arrow that harmlessly welded
	// itself to the goblin - which looks like a graphical glitch rather than a collision bug,
	// and is correspondingly harder to trace.
	AActor* Shooter = GetInstigator();
	if (!Shooter)
	{
		Shooter = GetOwner();
	}
	if (Shooter)
	{
		CollisionSphere->IgnoreActorWhenMoving(Shooter, true);
	}

	SetLifeSpan(MaxFlightSeconds);

	// Resolved here rather than in the constructor: the constructor runs on the CDO during module
	// load, which is the one place a synchronous package load is unwelcome. Warn once, then fly on
	// invisibly - the arrow's job is damage, not looks.
	if (ArrowMeshComponent && !GArrowMeshResolveFailed)
	{
		if (UStaticMesh* Mesh = ArrowMesh.IsNull() ? nullptr : ArrowMesh.LoadSynchronous())
		{
			ArrowMeshComponent->SetStaticMesh(Mesh);
			ArrowMeshComponent->SetRelativeTransform(ArrowMeshOffset);
		}
		else
		{
			GArrowMeshResolveFailed = true;

			// An empty reference is reported as loudly as a broken one. The previous guard was
			// `!ArrowMesh.IsNull() && ...`, so an UNSET mesh skipped the whole block and warned about
			// nothing - and unset was the shipping state. The bug that hid itself was the silence,
			// not the missing mesh: the log said the arrow system was healthy while every shot was
			// invisible.
			UE_LOG(LogTemp, Warning,
				TEXT("[GoblinSiege] %s has no usable arrow mesh (%s) - shots will be invisible in "
					 "flight. They still fly, still hit, and still deal damage."),
				*GetName(),
				ArrowMesh.IsNull() ? TEXT("reference is unset") : *ArrowMesh.ToString());
		}
	}
}

bool AGSArrowProjectile::IsHeadshot(const AActor* HitActor, const FHitResult& Hit) const
{
	// The arrow's collision sphere is BlockAllDynamic, so against a character it stops on the
	// CAPSULE, not the mesh, and a capsule hit reports BoneName = None. When the mesh IS hit
	// directly the bone name is authoritative and we use it.
	if (!Hit.BoneName.IsNone())
	{
		return IsHeadBone(Hit.BoneName);
	}

	const ACharacter* HitCharacter = Cast<ACharacter>(HitActor);
	const USkeletalMeshComponent* Mesh = HitCharacter ? HitCharacter->GetMesh() : nullptr;
	if (!Mesh)
	{
		return false;
	}

	// MEASURE THE DISTANCE TO THE HEAD - do not ask for the nearest bone.
	//
	// The first version used FindClosestBone(ImpactPoint) and it made EVERY arrow a headshot: an
	// impact on the capsule surface at roughly chest height is still nearer the neck/head chain
	// than to any other named bone on these rigs, so "closest bone" answered "head" essentially
	// always. Observed in PIE as archers doing a flat 50 damage (20 x 2.5) with every single shot.
	//
	// Distance to the actual head bone is the honest test: it asks "did this land ON the head",
	// which is the question, rather than "which bone is least far away", which is not.
	for (const FName& Fragment : HeadBoneFragments)
	{
		if (Fragment.IsNone())
		{
			continue;
		}

		// Fragments are substrings, so find the real bone whose name contains one.
		const FReferenceSkeleton& RefSkeleton = Mesh->GetSkinnedAsset()
			? Mesh->GetSkinnedAsset()->GetRefSkeleton()
			: FReferenceSkeleton();

		for (int32 i = 0; i < RefSkeleton.GetNum(); ++i)
		{
			const FName BoneName = RefSkeleton.GetBoneName(i);
			if (!BoneName.ToString().Contains(Fragment.ToString(), ESearchCase::IgnoreCase))
			{
				continue;
			}

			const FVector BoneLocation = Mesh->GetBoneLocation(BoneName, EBoneSpaces::WorldSpace);
			if (FVector::Dist(Hit.ImpactPoint, BoneLocation) <= HeadshotRadius)
			{
				return true;
			}
		}
	}

	return false;
}

bool AGSArrowProjectile::IsHeadBone(FName BoneName) const
{
	if (BoneName.IsNone())
	{
		// No bone means the arrow hit a plain capsule rather than a physics asset. Awarding a
		// headshot for that would make the bonus depend on collision setup rather than on aim.
		return false;
	}

	const FString Bone = BoneName.ToString();
	for (const FName& Fragment : HeadBoneFragments)
	{
		if (!Fragment.IsNone() && Bone.Contains(Fragment.ToString(), ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

void AGSArrowProjectile::OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bHasHit)
	{
		return;
	}

	// Before bHasHit is latched, for the same reason as the torch: an arrow that brushed its
	// own archer must carry on and still be able to hit the thing it was aimed at.
	if (OtherActor && (OtherActor == GetInstigator() || OtherActor == GetOwner()))
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

		// Same race, no damage - the rule UGSGA_SwordLight::…:271 already applies to every swing,
		// and which ranged was never brought in line with. Without it an archer firing into a melee
		// kills his own side, and once the horde lands (AGSHordeGoblin sets Race_Goblin in its
		// constructor) the player would be doing it constantly.
		//
		// IsHostileTo returns TRUE for anything that is not an AGSCharacterBase - a wall, a barrel,
		// a burnable - so this narrows nothing except character-on-character friendly fire, and an
		// unset race on either side still counts as hostile ("no opinion must not make a pawn
		// immune", GSCharacterBase.cpp).
		//
		// This skips only the DAMAGE. The arrow still stops and sticks in the ally, because bHasHit
		// and StopMovementImmediately are already done by the time we get here, and because making
		// an arrow pass through a friendly is a collision change rather than a damage one - it
		// needs IgnoreActorWhenMoving at a point where the ally is not yet known. Flagged for a
		// design call rather than guessed at.
		const AGSCharacterBase* ShooterChar = Cast<AGSCharacterBase>(Shooter);
		const bool bMayDamage = !ShooterChar || ShooterChar->IsHostileTo(OtherActor);

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
		if (SourceASC && TargetASC && OtherActor != Shooter && bMayDamage)
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
				// HEADSHOTS. Scaled here rather than in UGSDamageExecCalculation because this is the
				// only place that still has the FHitResult - by the time the effect is executing,
				// the bone that was struck is long gone. The arrow is also the right owner
				// conceptually: hitting a head is a property of the projectile's impact, not of the
				// damage pipeline's arithmetic.
				//
				// Matched by SUBSTRING, not an exact name, on purpose. This project has two
				// skeletons in play (GOB_Scout_v2 and SK_Human_Skeleton) with different retargeting
				// histories, and a hard-coded "head" would silently do nothing on whichever one
				// spells it differently - a headshot bonus that quietly never fires is worse than
				// none, because nobody would think to check.
				float FinalDamage = Damage;
				const bool bHeadshot = IsHeadshot(OtherActor, Hit);
				if (bHeadshot)
				{
					FinalDamage *= HeadshotMultiplier;
				}

				SpecHandle.Data->AddDynamicAssetTag(GSTags::Damage_Bow);
				SpecHandle.Data->SetSetByCallerMagnitude(GSTags::Damage_Bow, FinalDamage);

				if (GSCombatDebugEnabled())
				{
					UE_LOG(LogTemp, Warning, TEXT("[GS.Arrow] %s hit %s (bone '%s')%s -> %.1f damage"),
						*GetNameSafe(Shooter), *GetNameSafe(OtherActor), *Hit.BoneName.ToString(),
						bHeadshot ? TEXT(" HEADSHOT") : TEXT(""), FinalDamage);
				}

				SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, TargetASC);

				// Frenzy, "anything you attack" (GDD §2.5) - #071, supplement to #069. Announced on
				// the SHOOTER, not the arrow: the horde follows a summoner, and an arrow in flight is
				// nobody's summoner. bMayDamage has already excluded friendly fire, so shooting past
				// your own line cannot put a goblin on the threat list.
				if (AGSCharacterBase* ShooterAsCharacter = Cast<AGSCharacterBase>(Shooter))
				{
					ShooterAsCharacter->NotifyDealtDamage(OtherActor);
				}
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
