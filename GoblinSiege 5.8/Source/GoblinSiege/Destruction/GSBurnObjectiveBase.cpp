#include "Destruction/GSBurnObjectiveBase.h"
#include "Core/GSGameState.h"
#include "Raid/GSRaidDirector.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

/**
 * GS.Burn.Debug 1
 *
 * The field's cells are data with no meshes and fire FX is a Blueprint concern that does not exist
 * yet, so without this overlay a perfectly spreading fire is completely invisible. This is the
 * instrument the burn tuning is actually judged with.
 */
static TAutoConsoleVariable<int32> CVarGSBurnDebug(
	TEXT("GS.Burn.Debug"),
	0,
	TEXT("Draw burn-objective debug overlays (field cells, mill stage, market stalls)."),
	ECVF_Cheat);

bool AGSBurnObjectiveBase::IsBurnDebugEnabled()
{
	return CVarGSBurnDebug.GetValueOnAnyThread() > 0;
}

AGSBurnObjectiveBase::AGSBurnObjectiveBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	// An AActor with NO RootComponent cannot be moved: SetActorLocation fails silently, the editor
	// gives it no transform gizmo, and GetActorTransform() is stuck at identity. For the field that
	// is fatal rather than cosmetic - the whole cell grid is derived from the actor transform, so
	// without a root every field in the game would be pinned to the world origin.
	ObjectiveRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ObjectiveRoot"));
	SetRootComponent(ObjectiveRoot);
}

void AGSBurnObjectiveBase::BeginPlay()
{
	Super::BeginPlay();

	// Debug polling runs on every instance, server and client, so a listen-server playtest draws
	// once rather than twice and a client still sees replicated state.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DebugTimerHandle, this,
			&AGSBurnObjectiveBase::DebugTick, 0.25f, true);
	}

	// Announce ourselves to the raid director (2026-08-05). It also sweeps the world at
	// OnWorldBeginPlay, so this is the second of two paths on purpose: the ordering between a world
	// subsystem's OnWorldBeginPlay and an actor's BeginPlay is not contractually fixed, and this is
	// the ONLY path by which a carrier spawned mid-raid can join the roster and learn it was born
	// Optional. RegisterCarrier is idempotent, so being registered twice costs one list scan.
	if (UGSRaidDirector* Director = UGSRaidDirector::Get(this))
	{
		Director->RegisterCarrier(this);
	}
}

void AGSBurnObjectiveBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DebugTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void AGSBurnObjectiveBase::DebugTick()
{
	if (IsBurnDebugEnabled())
	{
		DrawDebugState();
	}
}

void AGSBurnObjectiveBase::SetObjectiveIdentity(FGameplayTag InTypeTag, FText InDisplayName)
{
	// Placement-time only - see the header. Once BeginPlay has run, this carrier is already filed
	// under its old tag in UGSRaidDirector's per-type buckets, and re-typing it there would leave
	// the demotion pass and the win check disagreeing about what it is.
	if (HasActorBegunPlay())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] SetObjectiveIdentity refused on '%s': the raid has already begun. ")
			TEXT("Type tags are placement-time data."), *GetName());
		return;
	}

	ObjectiveTypeTag = InTypeTag;

	if (!InDisplayName.IsEmpty())
	{
		ObjectiveDisplayName = InDisplayName;
	}
}

void AGSBurnObjectiveBase::IgniteAtLocation(const FVector& /*WorldLocation*/)
{
	// Base does nothing. Subclasses that accept exterior fire override; the mill deliberately
	// does not (design doc §6.3 - "exterior fire alone won't take it").
}

bool AGSBurnObjectiveBase::ContainsWorldLocation(const FVector& /*WorldLocation*/) const
{
	// Default: an objective owns no ground. The mill relies on this - it is window-only.
	return false;
}

AGSBurnObjectiveBase* AGSBurnObjectiveBase::FindObjectiveAtLocation(
	const UObject* WorldContextObject, const FVector& WorldLocation)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Objectives are a handful of actors per level, so a direct iteration beats maintaining a
	// registry that has to survive PIE restarts and seamless travel.
	for (TActorIterator<AGSBurnObjectiveBase> It(World); It; ++It)
	{
		AGSBurnObjectiveBase* Objective = *It;
		if (Objective && !Objective->IsComplete() && Objective->ContainsWorldLocation(WorldLocation))
		{
			return Objective;
		}
	}

	return nullptr;
}

void AGSBurnObjectiveBase::SetCompletion01(float NewCompletion01)
{
	if (!HasAuthority() || bCompleted)
	{
		return;
	}

	const float Clamped = FMath::Clamp(NewCompletion01, 0.f, 1.f);

	// A value that REACHES THE THRESHOLD must always be processed, however small the step that got
	// it there. Skipping it is not a missed cosmetic update - it is a missed completion, forever.
	//
	// The windmill is the case that exposed this (2026-08-05). Its threshold is exactly 1.0 and its
	// progress is BuildupElapsed / DustBuildupSeconds, which lands on ~0.9999997 on the tick before
	// the ratio exceeds 1 and clamps. That is a difference of ~3e-7 - far inside KINDA_SMALL_NUMBER
	// (1e-4) - so the guard below returned early on that tick AND on every tick after it, because
	// the clamped value never moved again. The mill sat at "100%", stage Smouldering, fuse at 0.0s,
	// and never detonated: one of the three burn types was quietly impossible to complete, which
	// made the raid unwinnable without a single error anywhere.
	const bool bReachesThreshold = Clamped >= CompletionThreshold01;
	if (!bReachesThreshold && FMath::IsNearlyEqual(Clamped, Completion01, KINDA_SMALL_NUMBER))
	{
		return;
	}

	Completion01 = Clamped;
	OnRep_Completion01();

	if (Completion01 >= CompletionThreshold01)
	{
		bCompleted = true;
		OnRep_Completed();

		// 2026-07-31 (Q-37). A carrier that has burned is Complete in the objective list, always -
		// this is the one list-state transition the actor can decide entirely by itself, since it
		// is a statement about this carrier and nothing else. Required -> Optional, by contrast, is
		// a statement about the carrier's SIBLINGS and is not decided here at all; see the hook
		// below.
		SetListState(EGSObjectiveListState::Complete);

		// RAID LOOP HOOK - Q-32 demotion (Q-37). IMPLEMENTED 2026-08-05, and deliberately NOT here:
		// it lives in UGSRaidDirector, which binds OnBurnObjectiveCompleted below. This actor still
		// cannot see its siblings and still should not try - the director holds the per-type
		// completion set, runs the demotion, and answers "is the raid won" from the same state.
		// Nothing was added to this function; the delegate broadcast further down is the whole
		// coupling.

		if (UWorld* World = GetWorld())
		{
			if (AGSGameState* GS = World->GetGameState<AGSGameState>())
			{
				// DESIGN CALL (flagged to Michael 2026-07-28, awaiting ratification): completing an
				// objective promotes the town straight to Raid rather than merely arming the
				// unseen-fire fuse. Set bHardPromoteOnCompletion = false to fall back to the fuse.
				if (bHardPromoteOnCompletion)
				{
					GS->ReportFireSeenByHuman();
				}
				else
				{
					GS->ReportFireStarted();
				}

				GS->AddAlarm(AlarmOnCompletion, EGSAlarmSource::ObjectiveProgress);
			}
		}

		HandleCompleted();
	}
}

void AGSBurnObjectiveBase::HandleCompleted()
{
	// Subclass hook: collapse, detonate, state-swap.
}

void AGSBurnObjectiveBase::OnRep_Completion01()
{
	OnBurnObjectiveProgress.Broadcast(Completion01);
}

void AGSBurnObjectiveBase::OnRep_Completed()
{
	if (bCompleted)
	{
		OnBurnObjectiveCompleted.Broadcast(this);
	}
}

// ====================================================================== objective list state (Q-37)

void AGSBurnObjectiveBase::SetListState(EGSObjectiveListState NewState)
{
	// 2026-07-31 (Q-37). Authority-only, matching SetCompletion01 directly above: list state is
	// replicated, so a client writing it would be silently overwritten by the next update from the
	// server and would fire a spurious local transition in the meantime.
	if (!HasAuthority() || ListState == NewState)
	{
		return;
	}

	// Complete is TERMINAL. Guarded here rather than trusted to callers because the demotion pass
	// this is built for (Raid Loop, see SetCompletion01) will sweep every carrier of a type when
	// any one of them finishes, and the obvious implementation of that sweep - "set them all
	// Optional" - would demote the very carrier that just triggered it. A burned objective must
	// stay struck through in the HUD for the rest of the raid.
	if (ListState == EGSObjectiveListState::Complete)
	{
		return;
	}

	ListState = NewState;

	// Same pattern as Completion01/bCompleted: call the RepNotify by hand on the server so the
	// server's own listeners see the transition, since UE only invokes RepNotify on clients.
	OnRep_ListState();
}

void AGSBurnObjectiveBase::OnRep_ListState()
{
	OnObjectiveListStateChanged.Broadcast(ListState);
}

void AGSBurnObjectiveBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGSBurnObjectiveBase, Completion01);
	DOREPLIFETIME(AGSBurnObjectiveBase, bCompleted);

	// 2026-07-31 (Q-37). The HUD objective list runs on every machine and Required/Optional is what
	// it styles entries from, so this has to reach clients. Unconditional and cheap - an enum byte
	// that changes at most twice in a carrier's life.
	//
	// bReplicates is already true (set in the constructor), so no change was needed there.
	DOREPLIFETIME(AGSBurnObjectiveBase, ListState);
}
