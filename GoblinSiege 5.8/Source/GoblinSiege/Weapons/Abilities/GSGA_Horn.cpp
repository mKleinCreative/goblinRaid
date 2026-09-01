#include "Weapons/Abilities/GSGA_Horn.h"

#include "Combat/GSGameplayTags.h"
#include "Core/GSGameState.h"
#include "Horde/GSHordeSubsystem.h"
#include "Weapons/GSWeaponComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "TimerManager.h"

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

	// Held for the blast, and blocked on itself: one horn at a time. The block matters more now
	// than it did, not less - a held horn lives for as long as the button is down, and a second
	// activation on top of it would run two streaming timers against one pool.
	ActivationOwnedTags.AddTag(GSTags::State_Horn);
	ActivationBlockedTags.AddTag(GSTags::State_Horn);

	// A goblin with its arms around a pig cannot also raise a horn to its mouth. Same ruling that
	// blocks the torch toss while carrying (2026-08-04).
	ActivationBlockedTags.AddTag(GSTags::State_Carrying);

	// THE GESTURE, defaulted here rather than left for a designer, because there is nowhere for a
	// designer to put it: the horn is the only ability in the project with no Blueprint subclass
	// (GA_GS_Block, GA_GS_Dodge, GA_GS_SwordLight and the rest all have one), and
	// AGSPlayerCharacter hard-defaults HornAbilityClass to this C++ class.
	//
	// CUT DOWN 2026-08-30 (Michael) to a single plain looping montage - see HornMontage's own
	// comment for why. There is no actual horn animation anywhere in the project; this is a
	// placeholder taunt clip and stays one until an animator authors a real one.
	//
	// Still EditDefaultsOnly and still soft: a Blueprint subclass overrides this the day one exists,
	// and the path costs no package load at module time.
	HornMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(
		TEXT("/Game/Characters/ScoutV2/Montages/AM_GS_HornBlast.AM_GS_HornBlast")));

	// THE VOICE, defaulted here for the same reason as the montage: there is no CDO for a designer
	// to set it on. Nothing in the project's 2000-asset sound bundle is a horn - it has animals,
	// crafting, vocalisations, weapons and spells and no brass of any kind - so these assets are
	// authored for the project rather than found in it.
	HornSoundStart = TSoftObjectPtr<USoundBase>(
		FSoftObjectPath(TEXT("/Game/Audio/Horn/SW_GS_HornBlast_Start.SW_GS_HornBlast_Start")));
	HornSoundLoop = TSoftObjectPtr<USoundBase>(
		FSoftObjectPath(TEXT("/Game/Audio/Horn/SW_GS_HornBlast_Loop.SW_GS_HornBlast_Loop")));
	HornSoundEnd = TSoftObjectPtr<USoundBase>(
		FSoftObjectPath(TEXT("/Game/Audio/Horn/SW_GS_HornBlast_End.SW_GS_HornBlast_End")));
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

	// Down until told otherwise. A tap lowers this before the wind-up finishes and still gets its
	// one goblin; a hold keeps it up and the stream continues. See NotifyHornReleased.
	bHornHeld = true;
	SummonedThisBlast = 0;

	StartBlast();

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

void UGSGA_Horn::NotifyHornReleased()
{
	// Only lowers the flag. It deliberately does NOT end the ability: if the wind-up is still
	// running, the first goblin has not been delivered yet, and a tap has to be worth exactly one
	// goblin rather than none. SummonNext ends it on the next delivery instead.
	bHornHeld = false;
}

void UGSGA_Horn::OnBlastFinished()
{
	// The wind-up is over: the first goblin is owed. From here SummonNext re-arms itself for as
	// long as the button is down.
	SummonNext();
}

void UGSGA_Horn::StartBlast()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ACharacter* Character = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!Character)
	{
		return;
	}

	// The horn appears in the hand for the length of the blast. Without this the goblin mimes it:
	// SM_HuntingHorn_Signal01 has existed in the project unattached to anything since the horn was
	// written, and no arrangement of an empty hand near a mouth reads as blowing a horn.
	//
	// FindComponentByClass rather than AGSPlayerCharacter::GetWeaponComponent(), for the same reason
	// UGSGA_TorchToss gives: the horn is universal kit (GDD 2.3) and horde goblins are not
	// AGSPlayerCharacters. A pawn with no weapon component blows an invisible horn - a degradation,
	// not an error, and the summon still works.
	if (UGSWeaponComponent* WeaponComp = Character->FindComponentByClass<UGSWeaponComponent>())
	{
		WeaponComp->SetHornRaised(true);
	}

	// A blast started while a previous outro is still lowering the horn must cancel that lower, or
	// the prop would vanish out of the new blast's hand when the old timer fires.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HornLowerTimerHandle);
	}

	// Straight into the looping gesture - no separate raise/hold/lower phases. See HornMontage's
	// comment for why: this project has no horn animation, only a taunt clip standing in for one,
	// and trying to choreograph a raise/hold/lower shape out of it is exactly what broke twice in
	// one evening (2026-08-30). PlayLooping no-ops safely if HornMontage fails to load.
	PlayLooping(HornMontage.LoadSynchronous());

	StartHornVoice();
}

void UGSGA_Horn::PlayLooping(UAnimMontage* Montage)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ACharacter* Character = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!Montage || !Character || !Anim)
	{
		return;
	}

	ActiveHornMontage = Montage;
	Character->PlayAnimMontage(Montage);

	// UGSGA_Block's trick, for the reason its comment gives: a montage marked looping in the ASSET
	// would loop everywhere it is ever used, and montage sections cannot be authored from Python -
	// only chained at runtime like this.
	const FName FirstSection = Montage->GetSectionName(0);
	if (!FirstSection.IsNone())
	{
		Anim->Montage_SetNextSection(FirstSection, FirstSection, Montage);
	}
}

void UGSGA_Horn::LowerHornProp()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ACharacter* Character = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		if (UGSWeaponComponent* WeaponComp = Character->FindComponentByClass<UGSWeaponComponent>())
		{
			WeaponComp->SetHornRaised(false);
		}
	}
}

namespace
{
	/** Every horn component is spawned the same way, and the two arguments that matter are easy to
	 *  get wrong in one of three call sites, so they are stated once here.
	 *
	 *  bStopWhenAttachedToDestroyed = true - a goblin that dies mid-blast takes its horn with it.
	 *  Without it the note finishes playing over the corpse, which is funny exactly once. */
	UAudioComponent* SpawnHornVoice(USoundBase* Sound, USceneComponent* Attach)
	{
		if (!Sound || !Attach)
		{
			return nullptr;
		}
		// Attached rather than fire-and-forget at a location: the horn travels with a goblin that
		// is free to keep running while it blows, and the component is the only handle the release
		// path has to fade it out.
		return UGameplayStatics::SpawnSoundAttached(Sound, Attach, NAME_None,
			FVector::ZeroVector, EAttachLocation::SnapToTarget, /*bStopWhenAttachedToDestroyed*/ true);
	}
}

USceneComponent* UGSGA_Horn::GetVoiceAttachPoint() const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ACharacter* Character = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	return Character ? Character->GetMesh() : nullptr;
}

void UGSGA_Horn::StartHornVoice()
{
	USceneComponent* Attach = GetVoiceAttachPoint();
	if (!Attach)
	{
		return;
	}

	USoundBase* Start = HornSoundStart.LoadSynchronous();
	AttackAudio = SpawnHornVoice(Start, Attach);

	// Hand over when the attack is spent. Read the duration off the asset rather than hard-coding
	// it, so re-authoring the attack cannot silently leave a gap or an overlap at the join - the
	// two files were rendered to butt together sample-accurately and this is the only thing
	// holding them to that.
	// IsLooping() rather than testing GetDuration() against INDEFINITELY_LOOPING_DURATION: that
	// constant lives in AudioMixerCore, which this module does not depend on, and USoundBase
	// already asks the question for us.
	const float Handoff = Start ? Start->GetDuration() : 0.0f;
	if (Start && !Start->IsLooping() && Handoff > KINDA_SMALL_NUMBER)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(VoiceHandoffTimerHandle, this,
				&UGSGA_Horn::BeginHornSustain, Handoff, false);
		}
	}
	else
	{
		// No attack asset, or one that claims to loop forever: go straight to the sustain rather
		// than leaving the horn silent behind a timer that will never fire.
		BeginHornSustain();
	}
}

void UGSGA_Horn::BeginHornSustain()
{
	// The button may have come up during the attack. A tap should not leave a sustain running
	// behind it, and EndAbility may not have arrived yet - SummonNext owns the end, and it does not
	// run until the next summon interval.
	if (!bHornHeld)
	{
		return;
	}

	if (USceneComponent* Attach = GetVoiceAttachPoint())
	{
		LoopAudio = SpawnHornVoice(HornSoundLoop.LoadSynchronous(), Attach);
	}
}

void UGSGA_Horn::StopHornVoice()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(VoiceHandoffTimerHandle);
	}

	const bool bWasSounding = AttackAudio.IsValid() || LoopAudio.IsValid();

	// A short fade rather than a hard stop, and short rather than long: the fall-off sample IS the
	// end of the note, so a slow fade here would blur underneath it instead of handing over to it.
	if (UAudioComponent* Loop = LoopAudio.Get())
	{
		Loop->FadeOut(0.06f, 0.0f);
	}
	LoopAudio = nullptr;

	// The attack is deliberately NOT cut. On a tap short enough that the sustain never started,
	// clipping it would swallow the whole horn and the player would hear a click for their press.
	AttackAudio = nullptr;

	if (bWasSounding)
	{
		if (USceneComponent* Attach = GetVoiceAttachPoint())
		{
			SpawnHornVoice(HornSoundEnd.LoadSynchronous(), Attach);
		}
	}
}

void UGSGA_Horn::StopBlast()
{
	StopHornVoice();

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ACharacter* Character = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		if (ActiveHornMontage)
		{
			// Unhook the self-loop BEFORE stopping. InstancedPerActor reuses this ability object,
			// but the section chaining lives on the ANIM INSTANCE and outlives the montage - leave
			// it hooked and the next thing to play this montage on this goblin loops forever with
			// nobody left holding a button.
			if (UAnimInstance* Anim = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
			{
				const FName FirstSection = ActiveHornMontage->GetSectionName(0);
				if (!FirstSection.IsNone())
				{
					Anim->Montage_SetNextSection(FirstSection, NAME_None, ActiveHornMontage);
				}
				Anim->Montage_Stop(MontageBlendOutSeconds, ActiveHornMontage);
			}
		}

		// No outro clip any more - see HornMontage's comment. The blend-out above IS the lowering
		// motion now; hide the prop once it has had time to finish rather than on this frame, so it
		// doesn't pop out of the hand mid-blend.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(HornLowerTimerHandle, this,
				&UGSGA_Horn::LowerHornProp, FMath::Max(MontageBlendOutSeconds, 0.01f), false);
		}
	}
	ActiveHornMontage = nullptr;
}

void UGSGA_Horn::SummonNext()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;

	AController* Summoner = nullptr;
	if (const APawn* Pawn = Cast<APawn>(Avatar))
	{
		Summoner = Pawn->GetController();
	}

	UGSHordeSubsystem* Horde = UGSHordeSubsystem::Get(this);
	if (!Horde)
	{
		UE_LOG(LogGSHorn, Error, TEXT("No UGSHordeSubsystem - the horn sounded and nothing could answer."));
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, false);
		return;
	}

	const int32 Answered = Horde->SummonOne(Summoner);
	SummonedThisBlast += Answered;

	// Four reasons to stop, and they are not interchangeable:
	//   the button came up      - the player asked for this many and no more
	//   nothing answered        - a dry pool or a full active cap; the subsystem has already said which
	//   the per-hold ceiling    - MaxSummonsPerHold, 0 meaning "until the cap"
	//   nothing left to summon  - asked BEFORE the next interval elapses, so a held horn over an
	//                             empty pool ends on the beat rather than ticking silently
	const bool bCeilingHit = MaxSummonsPerHold > 0 && SummonedThisBlast >= MaxSummonsPerHold;
	const bool bNothingLeft = Horde->GetSummonableNow() <= 0;

	if (!bHornHeld || Answered == 0 || bCeilingHit || bNothingLeft)
	{
		UE_LOG(LogGSHorn, Log, TEXT("Horn blown by %s: %d answered (%s)."),
			*GetNameSafe(Avatar), SummonedThisBlast,
			!bHornHeld ? TEXT("released")
				: Answered == 0 ? TEXT("nothing answered")
				: bCeilingHit ? TEXT("per-hold ceiling")
				: TEXT("squad full or pool dry"));

		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, false);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(SummonTimerHandle, this, &UGSGA_Horn::SummonNext,
			SummonIntervalSeconds, false);
	}
}

void UGSGA_Horn::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// InstancedPerActor means this object is REUSED for the next blast, so every piece of blast
	// state has to be cleared here rather than initialised at activation. A surviving timer would
	// keep summoning after the ability ended; a surviving SummonedThisBlast would make the second
	// hold stop early against MaxSummonsPerHold.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SummonTimerHandle);
	}
	StopBlast();
	bHornHeld = false;
	SummonedThisBlast = 0;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
