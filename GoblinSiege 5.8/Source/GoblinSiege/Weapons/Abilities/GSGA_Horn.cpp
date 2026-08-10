#include "Weapons/Abilities/GSGA_Horn.h"

#include "Combat/GSGameplayTags.h"
#include "Core/GSGameState.h"
#include "Horde/GSHordeSubsystem.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSHorn, Log, All);

UGSGA_Horn::UGSGA_Horn()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// ServerOnly, not ServerInitiated: the summon debits a pool that only the server owns, and
	// there is no client-side cosmetic worth predicting before the blast finishes.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// SetAssetTags, not the deprecated AbilityTags member - that one is C4996 in 5.8 and will not
	// compile after the next engine upgrade.
	FGameplayTagContainer Tags;
	Tags.AddTag(GSTags::State_Horn);
	SetAssetTags(Tags);

	// Held for the blast, and blocked on itself: one horn at a time. Without the block, holding the
	// button down would summon a wave per frame and empty a 20-goblin pool in well under a second.
	ActivationOwnedTags.AddTag(GSTags::State_Horn);
	ActivationBlockedTags.AddTag(GSTags::State_Horn);

	// A goblin with its arms around a pig cannot also raise a horn to its mouth. Same ruling that
	// blocks the torch toss while carrying (2026-08-04).
	ActivationBlockedTags.AddTag(GSTags::State_Carrying);
}

void UGSGA_Horn::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ACharacter* Character = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		if (UAnimMontage* Montage = HornMontage.LoadSynchronous())
		{
			Character->PlayAnimMontage(Montage);
		}
	}

	// The alarm goes up on the FIRST frame of the blast, not when it finishes. The horn being heard
	// is the point; a player who blows it and immediately dies has still committed the raid.
	//
	// Straight to Raid, not Suspicious. §2.5 calls this "the formal end of the quiet half", and
	// §2.6's Suspicious-tier horn is a PATROL's horn - a different event that happens to share a
	// noun. Reading those two as the same thing would make the player's horn a soft signal the town
	// could decay back out of.
	if (AGSGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AGSGameState>() : nullptr)
	{
		GameState->RequestAlarmPhase(EGSAlarmPhase::Raid, EGSAlarmSource::HornBlast);
	}

	// Goblins answer at the END of the blast, so they are responding to a horn the player has
	// heard rather than arriving on top of it.
	if (UAbilityTask_WaitDelay* Wait = UAbilityTask_WaitDelay::WaitDelay(this, BlastDurationSeconds))
	{
		Wait->OnFinish.AddDynamic(this, &UGSGA_Horn::OnBlastFinished);
		Wait->ReadyForActivation();
	}
	else
	{
		OnBlastFinished();
	}
}

void UGSGA_Horn::OnBlastFinished()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	AController* Summoner = nullptr;
	if (const APawn* Pawn = Cast<APawn>(Avatar))
	{
		Summoner = Pawn->GetController();
	}

	if (UGSHordeSubsystem* Horde = UGSHordeSubsystem::Get(this))
	{
		const int32 Answered = Horde->SummonWave(Summoner);

		// Zero is a legitimate outcome twice over - a dry pool ("when the pool is dry, the treeline
		// is silent") and a full active cap - so this is Log, not Warning. The subsystem has
		// already said which of the two it was.
		UE_LOG(LogGSHorn, Log, TEXT("Horn blown by %s: %d answered."), *GetNameSafe(Avatar), Answered);
	}
	else
	{
		UE_LOG(LogGSHorn, Error, TEXT("No UGSHordeSubsystem - the horn sounded and nothing could answer."));
	}

	EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, false);
}
