#include "AI/Tasks/BTDecorator_HasAttackToken.h"

#include "AIController.h"
#include "AI/GSAIDebug.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Combat/GSEngagementComponent.h"
#include "GameFramework/Pawn.h"

namespace
{
	/** The victim's ledger, not the attacker's. Null for anything with no engagement component -
	 *  a breakable, a target dummy - which the callers treat as "unrationed". */
	UGSEngagementComponent* EngagementOf(const UBlackboardComponent* BB, const FName TargetKeyName)
	{
		if (!BB)
		{
			return nullptr;
		}
		AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKeyName));
		return IsValid(Target) ? Target->FindComponentByClass<UGSEngagementComponent>() : nullptr;
	}
}

UBTDecorator_HasAttackToken::UBTDecorator_HasAttackToken()
{
	NodeName = TEXT("Has Attack Token");

	// Both required: OnBecomeRelevant acquires, OnCeaseRelevant releases. Without the latter this
	// node is a token leak with extra steps.
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;

	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTDecorator_HasAttackToken, TargetKey),
		AActor::StaticClass());
	TargetKey.SelectedKeyName = TEXT("TargetActor");
}

void UBTDecorator_HasAttackToken::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	// Not optional - see the long note on UBTService_AcquireTarget::InitializeFromAsset. A selector
	// that is never resolved reports IsSet() false forever and the guard silently never fires.
	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		TargetKey.ResolveSelectedKey(*BBAsset);
	}
}

bool UBTDecorator_HasAttackToken::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory) const
{
	const AAIController* Controller = OwnerComp.GetAIOwner();
	APawn* Self = Controller ? Controller->GetPawn() : nullptr;
	if (!Self)
	{
		return false;
	}

	UGSEngagementComponent* Engagement =
		EngagementOf(OwnerComp.GetBlackboardComponent(), TargetKey.SelectedKeyName);

	// No component means nothing to ration against. Practice dummies and breakables must stay
	// hittable rather than becoming invulnerable because they own no ledger.
	if (!Engagement)
	{
		return true;
	}

	// ACQUIRE HERE, not in OnBecomeRelevant.
	//
	// The first version only reported on a reservation it expected OnBecomeRelevant to have made,
	// and deadlocked: this condition gates ENTRY to the branch, so returning false meant the branch
	// was never entered, so OnBecomeRelevant never ran, so no token was ever acquired, so the
	// condition was false forever. In PIE that read as a 10v10 in which nobody threw a punch.
	//
	// Acquiring in the condition is safe for the shape this node is used in: the parent is a
	// Selector, which enters the first child whose decorators pass, so a true here means the branch
	// runs. Releasing stays in OnCeaseRelevant, which is what covers every abort path.
	if (Engagement->HoldsToken(Self))
	{
		return true;
	}
	return Engagement->TryAcquireToken(Self, TokenCost);
}

void UBTDecorator_HasAttackToken::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	const AAIController* Controller = OwnerComp.GetAIOwner();
	APawn* Self = Controller ? Controller->GetPawn() : nullptr;
	UGSEngagementComponent* Engagement =
		EngagementOf(OwnerComp.GetBlackboardComponent(), TargetKey.SelectedKeyName);
	if (!Self || !Engagement)
	{
		return;
	}

	// The condition above already acquired; this only re-stamps it so the watchdog measures the
	// branch's life rather than the evaluation that preceded it, and reports what happened.
	const bool bGranted = Engagement->TryAcquireToken(Self, TokenCost);
	if (GSAIDebug::IsLogging())
	{
		GSAIDebug::Log(Self, FString::Printf(
			TEXT("token %s (cost %d, %d/%d reserved, %d attacker(s))"),
			bGranted ? TEXT("GRANTED") : TEXT("REFUSED"), TokenCost,
			Engagement->GetReservedWeight(), Engagement->GetTokenBudget(),
			Engagement->GetAttackerCount()));
	}
}

void UBTDecorator_HasAttackToken::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// THE release path. Runs when the branch stops being relevant for ANY reason - completion,
	// abort, target change, the tree restarting - which is precisely why the acquire lives on the
	// decorator rather than in the attack task.
	const AAIController* Controller = OwnerComp.GetAIOwner();
	APawn* Self = Controller ? Controller->GetPawn() : nullptr;
	if (UGSEngagementComponent* Engagement =
			EngagementOf(OwnerComp.GetBlackboardComponent(), TargetKey.SelectedKeyName))
	{
		if (Self)
		{
			Engagement->ReleaseToken(Self);
		}
	}

	Super::OnCeaseRelevant(OwnerComp, NodeMemory);
}

FString UBTDecorator_HasAttackToken::GetStaticDescription() const
{
	return FString::Printf(TEXT("Reserve %d attack weight from %s"),
		TokenCost, *TargetKey.SelectedKeyName.ToString());
}
