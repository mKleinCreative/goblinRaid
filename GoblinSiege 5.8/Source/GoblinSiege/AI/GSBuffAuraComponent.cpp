#include "AI/GSBuffAuraComponent.h"
#include "Characters/GSEnemyCharacter.h"
#include "AbilitySystemComponent.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"

UGSBuffAuraComponent::UGSBuffAuraComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGSBuffAuraComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AuraEffectClass && GetOwner() && GetOwner()->HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(AuraTimerHandle, this,
			&UGSBuffAuraComponent::AuraTick, ReapplyIntervalSeconds, true);
	}
}

void UGSBuffAuraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AuraTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void UGSBuffAuraComponent::AuraTick()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(AuraRadius);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Owner->GetActorLocation(), FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), Sphere);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		// Same-team = fellow defenders; the aura carrier doesn't buff itself.
		AGSEnemyCharacter* Ally = Cast<AGSEnemyCharacter>(Overlap.GetActor());
		if (!Ally || Ally == Owner)
		{
			continue;
		}

		if (UAbilitySystemComponent* TargetASC = Ally->GetAbilitySystemComponent())
		{
			FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
			Context.AddSourceObject(Owner);
			FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(AuraEffectClass, 1.f, Context);
			if (SpecHandle.IsValid())
			{
				TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			}
		}
	}
}
