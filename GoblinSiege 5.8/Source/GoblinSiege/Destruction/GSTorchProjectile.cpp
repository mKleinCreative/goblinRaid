#include "Destruction/GSTorchProjectile.h"
#include "Destruction/GSFireVolume.h"
#include "Destruction/GSFlammableComponent.h"
#include "Destruction/GSBurnObjectiveBase.h"
#include "Destruction/GSBuildingObjective.h"
#include "Destruction/GSBreakableComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

// Warn-once latches, FILE-SCOPE STATIC rather than members (2026-08-06, code review).
//
// A torch projectile is spawned FRESH FOR EVERY THROW, so a per-instance latch latches nothing -
// each new torch starts with it false, retries the failed synchronous package load, and warns
// again. The documented "warn once, then fly on" was therefore a per-throw package lookup plus a
// log line, in exactly the situation where the asset is missing and the player is spamming the
// throw. Process-wide is the right scope: "this soft path does not resolve" is a fact about the
// build, not about one projectile.
static bool GSTorchMeshResolveFailed = false;
static bool GSTorchFlameResolveFailed = false;

AGSTorchProjectile::AGSTorchProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// A THROWN TORCH IS A LIT TORCH, AND UNTIL NOW NOTHING SAID SO (2026-08-26, #323).
	//
	// AGSMillObjective only lights when an actor carrying this tag overlaps its window, and the tag
	// was referenced in exactly two places in the whole project: the mill's own property, and the
	// check that reads it. Nothing ever applied it. So the windmill's one gameplay ignition route
	// tested a condition that could never be true, and the mill could only be lit by a console
	// command.
	//
	// Tagged on the projectile rather than on the held torch on purpose: the mill wants "something
	// burning arrived through the window", which is the thrown thing, not the one still in a hand.
	Tags.Add(FName(TEXT("GS.LitTorch")));

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(8.f);
	CollisionSphere->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionSphere->SetNotifyRigidBodyCollision(true);
	RootComponent = CollisionSphere;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	// 2026-08-06: 1400 -> 2400, because the throw did not read as a throw. RANGE GOES AS THE SQUARE
	// OF SPEED (v^2/g at 45 degrees), so this is not a 70% improvement - it is 20m to 59m, nearly
	// triple. 1400 put the torch on the ground about two house-lengths away, which looks like a
	// drop rather than a throw.
	//
	// Gravity stays at 1.0 deliberately. The lob is the read: it is what makes the arc worth
	// previewing, what lets a defender see it coming, and what makes lighting a distant roof a
	// skill rather than a straight line. Flattening the trajectory to get range would buy distance
	// by deleting the interesting part.
	ProjectileMovement->InitialSpeed = 2400.f;
	ProjectileMovement->MaxSpeed = 2400.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->ProjectileGravityScale = 1.f; // torches arc - readable, dodgeable

	// The visible prop. No collision of its own: the 8uu sphere above is the projectile's whole
	// physical presence, and a second colliding body on the same actor would produce a second hit
	// event and a second fire volume from one throw.
	TorchMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TorchMesh"));
	TorchMeshComponent->SetupAttachment(RootComponent);
	TorchMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TorchMeshComponent->SetGenerateOverlapEvents(false);

	// The flame that makes the throw trackable. Auto-activate is OFF: it is switched on in BeginPlay
	// once the system actually resolves, so a missing asset leaves a dormant component rather than
	// an activated one with nothing in it.
	FlameFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FlameFX"));
	FlameFX->SetupAttachment(RootComponent);
	FlameFX->bAutoActivate = false;

	// C++ default, matching FireVolumeClass above and AGSFireVolume's own FireSystem/SmokeSystem.
	FlameSystem = TSoftObjectPtr<UNiagaraSystem>(
		FSoftObjectPath(TEXT("/Game/VFX/NS_GS_TorchFlame.NS_GS_TorchFlame")));

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

	// Light it. Same resolve-here-not-in-the-constructor rule as the mesh below, and the same
	// warn-once-then-carry-on degradation: an unlit torch is harder to follow but still lands,
	// still ignites and still spawns its fire volume.
	if (!FlameSystem.IsNull() && !GSTorchFlameResolveFailed && FlameFX)
	{
		if (UNiagaraSystem* Flame = FlameSystem.LoadSynchronous())
		{
			FlameFX->SetAsset(Flame);
			FlameFX->Activate(true);
		}
		else
		{
			GSTorchFlameResolveFailed = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[GoblinSiege] %s could not load its flame system (%s) - the throw will be "
					 "hard to follow in the air. It still lands, ignites and spawns its fire."),
				*GetName(), *FlameSystem.ToString());
		}
	}

	// Resolved here rather than in the constructor: the constructor runs on the CDO during module
	// load, which is the one place a synchronous package load is genuinely unwelcome. Warn once,
	// then fly on invisibly - the torch's job is fire, not looks.
	if (!TorchMesh.IsNull() && !GSTorchMeshResolveFailed && TorchMeshComponent)
	{
		if (UStaticMesh* Mesh = TorchMesh.LoadSynchronous())
		{
			TorchMeshComponent->SetStaticMesh(Mesh);
		}
		else
		{
			GSTorchMeshResolveFailed = true;
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

	// DIAGNOSTIC (#366, temporary): what did the torch actually hit? Added because a mill-exterior
	// throw was reported hard to land, with a live theory that the torch is colliding with
	// something OTHER than the mill's own geometry before ever reaching it. This prints on every
	// single throw, so grep the log for "TORCH HIT" after a test pass rather than guessing blind.
	UE_LOG(LogTemp, Warning,
		TEXT("[GoblinSiege] TORCH HIT: actor='%s' component='%s' impact=(%.0f, %.0f, %.0f)"),
		OtherActor ? *OtherActor->GetName() : TEXT("<none>"),
		OtherComp ? *OtherComp->GetName() : TEXT("<none>"),
		Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z);

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
		// The mill used to answer false unconditionally here (window-only) - retired 2026-08-30,
		// #361/#362. It now accepts a torch anywhere on its own resolved geometry; see
		// AGSMillObjective::ContainsWorldLocation. What actually gates whether IgniteAtLocation is
		// reached for ANY objective is ContainsWorldLocation, not that function's own body - see
		// AGSBurnObjectiveBase's header comment on both, which #362 exists to explain in full.
		if (AGSBurnObjectiveBase* Objective =
			AGSBurnObjectiveBase::FindObjectiveAtLocation(this, Hit.ImpactPoint))
		{
			// DIAGNOSTIC (#366, temporary).
			UE_LOG(LogTemp, Warning, TEXT("[GoblinSiege] TORCH HIT: objective found at impact point: '%s'"),
				*Objective->GetName());
			Objective->IgniteAtLocation(Hit.ImpactPoint);
		}
		else
		{
			// DIAGNOSTIC (#366, temporary).
			UE_LOG(LogTemp, Warning, TEXT("[GoblinSiege] TORCH HIT: no burn objective claims this impact point."));
		}

		// ---------------------------------------------------------------- buildings (2026-08-05)
		//
		// Two ways into a house, and nothing else works - a torch against a plaster wall is a torch
		// against a wall. AGSBuildingObjective::ContainsWorldLocation returns false precisely so the
		// sweep above cannot light a building by splashing its facade; getting in is decided here.
		//
		// Order matters. The window is tried first because a window IS a wall piece as far as the
		// roof/entry naming goes, and breaking it is the more specific outcome: it opens the
		// building AND removes the pane, so the torch is not left stuck to glass that no longer
		// exists.
		if (OtherActor)
		{
			// The torch's own velocity is already spent by the time we get here (the hit is what
			// stopped it), so fall back to the surface normal reversed - which IS the direction of
			// travel for anything that hit a flat pane square on.
			FVector ImpactVelocity = GetVelocity();
			if (ImpactVelocity.IsNearlyZero())
			{
				ImpactVelocity = -Hit.ImpactNormal * 500.f;
			}

			if (UGSBreakableComponent* Breakable = OtherActor->FindComponentByClass<UGSBreakableComponent>())
			{
				// ImpactVelocity, not the hit normal: shards and the torch should carry on the way
				// the throw was going, into the room.
				// One damage, not an outright Break (#165). A window has one hit point, so
					// this is still a single-throw kill and the flow is unchanged - but a crate or
					// a statue can cost several hits now, and the torch need not know which it hit.
					Breakable->ApplySmash(1, Hit.ImpactPoint, ImpactVelocity, GetInstigator());
			}
			else if (AGSBuildingObjective* Building =
				AGSBuildingObjective::FindBuildingOwning(this, OtherActor))
			{
				// Not a window, but it belongs to a building - so it only counts if it is the roof.
				// Anything else is a wall and is refused, which is what keeps "torch the outside of
				// a house" from being a valid strategy.
				if (Building->IsRoofPiece(OtherActor))
				{
					Building->IgniteInterior(EGSBuildingIgnitionSource::Roof);
				}
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
