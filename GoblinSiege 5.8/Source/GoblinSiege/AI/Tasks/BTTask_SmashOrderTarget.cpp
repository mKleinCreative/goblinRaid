#include "AI/Tasks/BTTask_SmashOrderTarget.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Characters/GSCharacterBase.h"
#include "Destruction/GSBreakableComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSSmashOrder, Log, All);

UBTTask_SmashOrderTarget::UBTTask_SmashOrderTarget()
{
	NodeName = TEXT("Smash Ordered Target (Break)");

	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_SmashOrderTarget, TargetKey),
		AActor::StaticClass());
	TargetKey.SelectedKeyName = TEXT("OrderSubject");
}

void UBTTask_SmashOrderTarget::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		TargetKey.ResolveSelectedKey(*BBAsset);
	}
}

EBTNodeResult::Type UBTTask_SmashOrderTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();
	if (!BB || !Controller)
	{
		return EBTNodeResult::Failed;
	}

	AGSCharacterBase* Self = Cast<AGSCharacterBase>(Controller->GetPawn());
	AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));
	if (!Self || !IsValid(Target))
	{
		UE_LOG(LogGSSmashOrder, Verbose, TEXT("[GS.Smash] %s: no self or no valid OrderSubject."),
			*GetNameSafe(Controller->GetPawn()));
		return EBTNodeResult::Failed;
	}

	UGSBreakableComponent* Breakable = Target->FindComponentByClass<UGSBreakableComponent>();
	if (!Breakable || Breakable->IsBroken())
	{
		// Not breakable, or somebody already broke it. Failing here is what lets an Attack order on a
		// living guard fall straight through to the combat branches below - this node is the FIRST
		// thing an Attack order tries, and "that is not a prop" is its normal answer.
		UE_LOG(LogGSSmashOrder, Verbose, TEXT("[GS.Smash] %s: '%s' has %s - nothing to smash."),
			*Self->GetName(), *Target->GetName(), !Breakable ? TEXT("no GSBreakableComponent") : TEXT("already broken"));
		return EBTNodeResult::Failed;
	}

	const float Dist = FVector::Dist2D(Target->GetActorLocation(), Self->GetActorLocation());
	if (Dist > SmashRange)
	{
		UE_LOG(LogGSSmashOrder, Log, TEXT("[GS.Smash] %s: '%s' is %.0fuu away, need <= %.0fuu - not in range yet."),
			*Self->GetName(), *Target->GetName(), Dist, SmashRange);
		return EBTNodeResult::Failed;
	}

	// Swing first, so the break reads as a goblin breaking it rather than a crate spontaneously
	// exploding as somebody walks past. A refused activation means a swing is already running, which
	// GAS treats as the combo buffer (see UBTTask_MeleeAttack) - fail out and the tree comes back
	// around in a tick or two.
	if (!Self->TryLightAttack())
	{
		UE_LOG(LogGSSmashOrder, Verbose, TEXT("[GS.Smash] %s: TryLightAttack refused (already swinging?)."),
			*Self->GetName());
		return EBTNodeResult::Failed;
	}

	// KNOWN TIMING COMPROMISE: this fires at the START of the swing, not on the contact frame. The
	// honest version routes through the montage's hit notify the way UGSGA_SwordLight's sweep does,
	// which means an ability that can target a non-ASC actor - a bigger change than this ticket, and
	// one that belongs with the general "melee damages props" feature rather than with the order
	// wheel. The visible cost is that a crate bursts a few frames early.
	const FVector Impact = Target->GetActorLocation();
	const FVector Direction = (Target->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal();
	Breakable->Break(Impact, Direction * DebrisImpulse);

	UE_LOG(LogGSSmashOrder, Log, TEXT("[GS.Smash] %s smashed '%s' open."), *Self->GetName(), *Target->GetName());

	return EBTNodeResult::Succeeded;
}
