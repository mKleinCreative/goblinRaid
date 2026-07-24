#include "Characters/GSTargetingComponent.h"
#include "Characters/GSCharacterBase.h"
#include "Characters/GSEnemyCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"

UGSTargetingComponent::UGSTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = ScanIntervalSeconds;
	SetIsReplicatedByDefault(false); // client-side aim-assist flavor only; server trusts hit traces, not this
}

void UGSTargetingComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// Re-apply in case a Blueprint changed ScanIntervalSeconds from the C++ default the
	// constructor baked in.
	PrimaryComponentTick.TickInterval = ScanIntervalSeconds;
}

void UGSTargetingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshSoftTarget();
}

void UGSTargetingComponent::RefreshSoftTarget()
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	APlayerController* PC = OwnerCharacter ? Cast<APlayerController>(OwnerCharacter->GetController()) : nullptr;
	if (!PC)
	{
		return;
	}

	FVector CameraLocation = OwnerCharacter->GetActorLocation();
	FRotator CameraRotation = PC->GetControlRotation();
	if (const UCameraComponent* Camera = OwnerCharacter->FindComponentByClass<UCameraComponent>())
	{
		CameraLocation = Camera->GetComponentLocation();
		CameraRotation = Camera->GetComponentRotation();
	}
	const FVector CameraForward = CameraRotation.Vector();

	TArray<AActor*> Candidates;
	UGameplayStatics::GetAllActorsOfClass(this, AGSEnemyCharacter::StaticClass(), Candidates);

	AActor* Best = nullptr;
	float BestScore = -1.f;
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(ConeHalfAngleDegrees));

	for (AActor* Candidate : Candidates)
	{
		const AGSCharacterBase* CandidateChar = Cast<AGSCharacterBase>(Candidate);
		if (!CandidateChar || !CandidateChar->IsAlive())
		{
			continue;
		}

		const FVector ToCandidate = Candidate->GetActorLocation() - CameraLocation;
		const float Distance = ToCandidate.Size();
		if (Distance <= KINDA_SMALL_NUMBER || Distance > MaxTargetDistance)
		{
			continue;
		}

		const float Cos = FVector::DotProduct(CameraForward, ToCandidate / Distance);
		if (Cos < CosHalfAngle)
		{
			continue; // outside the soft-lock cone
		}

		// Prefer tightly-centered candidates; distance only breaks near-ties.
		const float Score = Cos - (Distance / MaxTargetDistance) * 0.1f;
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}

	if (Best != CurrentSoftTarget.Get())
	{
		CurrentSoftTarget = Best;
		OnSoftTargetChanged.Broadcast(Best);
	}
}
