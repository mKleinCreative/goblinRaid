#include "Destruction/GSFireVolume.h"
#include "Destruction/GSFlammableComponent.h"
#include "Combat/GSGameplayTags.h"
#include "Core/GSGameState.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/OverlapResult.h"

AGSFireVolume::AGSFireVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	DamageSphere = CreateDefaultSubobject<USphereComponent>(TEXT("DamageSphere"));
	DamageSphere->InitSphereRadius(DamageRadius);
	DamageSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	RootComponent = DamageSphere;

	FireFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FireFX"));
	FireFX->SetupAttachment(RootComponent);
}

void AGSFireVolume::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(DamageTickHandle, this, &AGSFireVolume::DamageTick, DamageTickInterval, true);
		GetWorldTimerManager().SetTimer(SpreadTimerHandle, this, &AGSFireVolume::SpreadTick, SpreadDelaySeconds, false);

		if (AGSGameState* GS = Cast<AGSGameState>(UGameplayStatics::GetGameState(this)))
		{
			GS->AddAlarm(1.5f, EGSAlarmSource::FireDamage);
		}

		SetLifeSpan(LifetimeSeconds);
	}
}

void AGSFireVolume::DamageTick()
{
	TArray<AActor*> OverlappingActors;
	DamageSphere->GetOverlappingActors(OverlappingActors, APawn::StaticClass());

	for (AActor* Actor : OverlappingActors)
	{
		// Friendly fire is intentionally ON (design doc §4) - no ally/goblin exclusion check here.
		// TODO: apply a GameplayEffect tagged Damage.Fire with SetByCaller = DamagePerTick *
		// FriendlyFireScalar (scalar only matters when the target is another goblin/ally; enemy
		// targets always take full damage) via GSDamageExecCalculation.
	}
}

void AGSFireVolume::SpreadTick()
{
	TArray<FOverlapResult> Overlaps;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(SpreadRadius);
	GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity,
		FCollisionObjectQueryParams(ECC_WorldStatic), Sphere);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (AActor* OverlappedActor = Overlap.GetActor())
		{
			if (UGSFlammableComponent* Flammable = OverlappedActor->FindComponentByClass<UGSFlammableComponent>())
			{
				Flammable->Ignite();
			}
		}
	}
}
