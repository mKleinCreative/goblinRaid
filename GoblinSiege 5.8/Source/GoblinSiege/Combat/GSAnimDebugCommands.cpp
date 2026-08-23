// GS.Anim.Snapshot - the runtime readout animation never had.
//
// Headerless on purpose, same as GSDebugCommands.cpp and GSBurnDebugCommands.cpp; this file
// registers a console command and exports nothing.
//
// ---- why this exists (#137) ---------------------------------------------------------------------
// On 2026-08-11 an agent (me) spent a session changing animation and handed Michael four broken
// passes at the same feature, every one of them reported as verified. An inventory taken afterwards
// found the reason: this project had 18 console variables and 26 console commands, and NOT ONE of
// them reported anything about animation. There is no UAnimInstance subclass in the module, and no
// line of code in it had ever printed a speed. Combat is richly instrumented - GS.Combat.LogAI,
// LogDamage, CrowdWatch, CrowdStats - and animation had nothing at all, so every defect had to be
// found by a human looking at the screen. Michael found all four.
//
// ---- why a SNAPSHOT and not a log ---------------------------------------------------------------
// GS.Combat.LogAI is an event stream: it prints when a decision changes. That is the right shape for
// decisions and the wrong shape for this, because the failure being hunted is a pawn STANDING STILL
// IN THE WRONG POSE. A goblin frozen in its reference pose generates no events, so an event log is
// silent at exactly the moment you need it to shout. This command answers "what is true right now,
// for every pawn" instead.
//
// ---- why it prints EVERY pawn -------------------------------------------------------------------
// The most expensive single mistake in that session was testing through a path that could not reach
// the change: the player pawn runs bOrientRotationToMovement, which pins the blendspace Direction
// input near zero, so playing as the goblin exercised one of five direction columns and the other
// four were broken. Printing one pawn on request would let that happen again. Printing all of them
// makes coverage structural - player, defender and horde land in the same table whether or not the
// person reading it remembered they were different.

#include "CoreMinimal.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
// NOT "Kismet/KismetAnimationLibrary.h" - despite the class prefix and despite every sibling
// Kismet library living under a Kismet/ folder, this one sits directly in AnimGraphRuntime/Public.
#include "KismetAnimationLibrary.h"
#include "ReferenceSkeleton.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSAnim, Log, All);

namespace
{
	/** The world the GAME is in, not the world the console was typed in. Copied deliberately from
	 *  GSDebugCommands.cpp - the editor's Output Log console is not the PIE world, and four attempts
	 *  at GS.Raid.GotoActor produced zero output before that was understood (#027). Every debug
	 *  command in this project re-resolves; one that does not is a command that silently prints
	 *  nothing. */
	static UWorld* GSAnimGameWorld(UWorld* Fallback)
	{
		if (GEngine)
		{
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				if ((Ctx.WorldType == EWorldType::PIE || Ctx.WorldType == EWorldType::Game) && Ctx.World())
				{
					return Ctx.World();
				}
			}
		}
		return Fallback;
	}

	/** How far a bone must be from its reference-pose rotation before the pose counts as "posed".
	 *  Small: a genuinely animated pose moves the limbs by tens of degrees, while a reference pose
	 *  is bit-identical, so anything in between is noise from a blend that is barely weighted. */
	constexpr float GSRefPoseToleranceRad = 0.02f;

	/** THE COLUMN THIS COMMAND EXISTS FOR.
	 *
	 *  Every visible animation defect in the session that prompted this file presented as the same
	 *  thing - a T-pose, then "no locomotion at all", then an A-pose - and all three were one
	 *  underlying condition: the pose being evaluated was the mesh's reference pose. Nothing in the
	 *  project could say that, so it had to be seen. This says it.
	 *
	 *  Compares the component's live local-space bone transforms against the reference skeleton.
	 *  Rotation only: translation is a poor discriminator here because the human clips animate
	 *  translation on the Hips alone, so a fully-working pose still matches the reference pose on
	 *  every other bone's translation.
	 *
	 *  Deliberately samples the whole chain rather than a hand-picked bone. A hand-picked bone is how
	 *  #092 shipped a detection heuristic that could only ever answer yes. */
	// Non-const Mesh: GetBoneSpaceTransforms() is a non-const accessor that returns BY VALUE, so the
	// live pose is copied into Live rather than referenced.
	static bool GSIsInReferencePose(USkeletalMeshComponent* Mesh)
	{
		if (!Mesh || !Mesh->GetSkeletalMeshAsset())
		{
			return false;
		}

		const FReferenceSkeleton& RefSkel = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
		const TArray<FTransform> Live = Mesh->GetBoneSpaceTransforms();
		const TArray<FTransform>& Ref = RefSkel.GetRefBonePose();

		if (Live.Num() == 0 || Live.Num() != Ref.Num())
		{
			return false;
		}

		// Start at 1: bone 0 is the root, which a great many clips legitimately leave at identity.
		for (int32 i = 1; i < Live.Num(); ++i)
		{
			const float AngleRad = Live[i].GetRotation().AngularDistance(Ref[i].GetRotation());
			if (AngleRad > GSRefPoseToleranceRad)
			{
				return false;
			}
		}
		return true;
	}

	/** What the anim instance is actually playing. Montages first, because a montage on a slot is
	 *  exactly what keeps working while the locomotion underneath it has collapsed - the symptom
	 *  Michael described as "stuck in their A pose until they attack". */
	static FString GSDescribeAnim(const USkeletalMeshComponent* Mesh)
	{
		const UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
		if (!Anim)
		{
			return TEXT("<no anim instance>");
		}
		if (const UAnimMontage* Montage = Anim->GetCurrentActiveMontage())
		{
			return FString::Printf(TEXT("montage %s"), *Montage->GetName());
		}
		return FString::Printf(TEXT("graph %s"), *Anim->GetClass()->GetName());
	}
}

static FAutoConsoleCommandWithWorldAndArgs GSAnimSnapshotCmd(
	TEXT("GS.Anim.Snapshot"),
	TEXT("GS.Anim.Snapshot [radius=5000] - one row per live pawn: speed, movement direction, "
		 "rotation mode, what is playing, and whether the pose has collapsed to the reference pose. "
		 "Radius is measured from the player pawn; 0 means the whole level."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* InWorld)
		{
			UWorld* World = GSAnimGameWorld(InWorld);
			if (!World)
			{
				UE_LOG(LogGSAnim, Warning, TEXT("[GS.Anim] GS.Anim.Snapshot: no game world."));
				return;
			}

			const float Radius = (Args.Num() > 0) ? FCString::Atof(*Args[0]) : 5000.f;

			FVector Origin = FVector::ZeroVector;
			if (const APlayerController* PC = World->GetFirstPlayerController())
			{
				if (const APawn* PlayerPawn = PC->GetPawn())
				{
					Origin = PlayerPawn->GetActorLocation();
				}
			}

			UE_LOG(LogGSAnim, Warning,
				TEXT("[GS.Anim] --- snapshot: pawn | ctrl | speed | dir | rotmode | REFPOSE | playing | target ---"));

			int32 Rows = 0;
			int32 Collapsed = 0;
			for (TActorIterator<APawn> It(World); It; ++It)
			{
				APawn* Pawn = *It;
				if (!IsValid(Pawn))
				{
					continue;
				}
				if (Radius > 0.f && !Origin.IsZero()
					&& FVector::Dist(Pawn->GetActorLocation(), Origin) > Radius)
				{
					continue;
				}

				const ACharacter* Char = Cast<ACharacter>(Pawn);
				USkeletalMeshComponent* Mesh = Char ? Char->GetMesh() : Pawn->FindComponentByClass<USkeletalMeshComponent>();
				const UCharacterMovementComponent* Move = Char ? Char->GetCharacterMovement() : nullptr;

				const FVector Velocity = Pawn->GetVelocity();
				const float Speed = FVector(Velocity.X, Velocity.Y, 0.f).Size();

				// The SAME call the anim blueprints use to drive a blendspace's Direction axis, so
				// this column is the number the graph actually sees rather than one invented here.
				const float Direction =
					UKismetAnimationLibrary::CalculateDirection(Velocity, Pawn->GetActorRotation());

				// Why Direction is what it is. The player runs OrientToMovement, which pins Direction
				// near zero and therefore exercises one column of a directional blendspace; every AI
				// pawn here runs ControllerYaw instead and spans the full range. Printing the mode
				// beside the value is what makes that legible without reading any CDO.
				FString RotMode = TEXT("-");
				if (Move)
				{
					RotMode = FString::Printf(TEXT("%s%s%s"),
						Move->bOrientRotationToMovement ? TEXT("OrientToMove ") : TEXT(""),
						Move->bUseControllerDesiredRotation ? TEXT("DesiredRot ") : TEXT(""),
						Pawn->bUseControllerRotationYaw ? TEXT("CtrlYaw") : TEXT(""));
					RotMode.TrimEndInline();
					if (RotMode.IsEmpty()) { RotMode = TEXT("free"); }
				}

				const bool bRefPose = GSIsInReferencePose(Mesh);
				if (bRefPose) { ++Collapsed; }

				FString TargetName = TEXT("-");
				FString CtrlName = TEXT("<unpossessed>");
				if (const AController* C = Pawn->GetController())
				{
					CtrlName = C->GetClass()->GetName();
					if (const AAIController* AI = Cast<AAIController>(C))
					{
						if (const UBlackboardComponent* BB = AI->GetBlackboardComponent())
						{
							TargetName = GetNameSafe(Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor"))));
						}
					}
				}

				UE_LOG(LogGSAnim, Warning, TEXT("[GS.Anim] %-28s | %-26s | %6.0f | %7.1f | %-32s | %-7s | %-34s | %s"),
					*Pawn->GetName(), *CtrlName, Speed, Direction, *RotMode,
					bRefPose ? TEXT("*YES*") : TEXT("no"),
					*GSDescribeAnim(Mesh), *TargetName);
				++Rows;
			}

			const FString Summary = (Rows == 0)
				? FString(TEXT("GS.Anim.Snapshot: no pawns in range."))
				: FString::Printf(
					TEXT("GS.Anim.Snapshot: %d pawn(s), %d in REFERENCE POSE%s"),
					Rows, Collapsed,
					Collapsed > 0
						? TEXT(" <- these are not animating; the pose evaluated to nothing.")
						: TEXT("."));

			UE_LOG(LogGSAnim, Warning, TEXT("[GS.Anim] %s"), *Summary);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 10.f,
					Collapsed > 0 ? FColor::Red : FColor::Green, Summary);
			}
		}));

// =================================================================================================
// GS.AI.LogLocomotion - is the velocity a SQUARE WAVE, or is it smooth with a jittering heading?
//
// ---- why this exists (#246) ---------------------------------------------------------------------
// Michael watched Erika "take a step forward with one foot" while stuttering, and said the castle
// guards have always done it too. Two investigations then agreed on a mechanism: the archer's
// MoveTo re-executes continuously against a hold point that slides with its target, producing ~0.15s
// dashes, and ABP_Human turns each dash into a fragment of a walk cycle because Idle->Walk and
// Walk->Idle BOTH pivot on HU_Speed = 10.0 with no hysteresis between them.
//
// That is a diagnosis, not a measurement, and this project has a costly history of shipping
// diagnoses. So this command exists to try to REFUTE it before anything is changed.
//
// ---- the two hypotheses, and how this tells them apart ------------------------------------------
//   A (the diagnosis): speed is a SQUARE WAVE - short pulses separated by flat zeros. The pawn is
//       repeatedly told to travel ~80uu and brakes before it reaches walk speed.
//   B (the alternative): speed is CONTINUOUS and non-zero, and it is the HEADING that jitters. That
//       would mean the RVO/separation stack is the cause and the behaviour tree is innocent.
// Both are measured here, side by side, from the same capture. A high crossing rate with a large
// share of frames at rest is A; near-zero time at rest with a high heading-change rate is B.
//
// ---- why it also SIMULATES the proposed fix ------------------------------------------------------
// The last column replays the captured speed through the PROPOSED Idle<->Walk rule (enter above 60,
// leave below 25) and counts how many state changes survive. That turns "these two numbers should
// fix it" from a claim into a prediction made against real data, before anyone edits an AnimBP -
// and if the proposed rule does not collapse the crossing count, it is the wrong fix and this says
// so while it is still cheap to find out.
// =================================================================================================

namespace
{
	/** The live Idle<->Walk boundary in ABP_Human. Both directions use this same value today, which
	 *  is the defect: the Walk<->Run pair in the same state machine correctly uses 500/450. */
	constexpr float GSLocoCurrentThreshold = 10.f;

	/** The proposed replacement, mirroring the shape Walk<->Run already uses. */
	constexpr float GSLocoProposedEnter = 60.f;
	constexpr float GSLocoProposedLeave = 25.f;

	struct FGSLocoTrack
	{
		TWeakObjectPtr<APawn> Pawn;
		FString Name;
		bool bPlayerControlled = false;

		TArray<float> Speeds;          // uu/s, XY only - the same quantity HU_Speed receives
		TArray<float> Times;           // seconds since capture start
		TArray<float> HeadingsDeg;     // yaw of the velocity vector; only meaningful while moving

		int32 CurrentCrossings = 0;    // Idle<->Walk changes under the LIVE rule
		int32 ProposedCrossings = 0;   // ...and under the proposed hysteresis rule
		bool bCurrentWalking = false;
		bool bProposedWalking = false;
	};

	static TArray<FGSLocoTrack> GSLocoTracks;
	static FTSTicker::FDelegateHandle GSLocoTickHandle;
	static TWeakObjectPtr<UWorld> GSLocoWorld;
	static float GSLocoElapsed = 0.f;
	static float GSLocoDuration = 0.f;
	static float GSLocoRadius = 0.f;

	static FGSLocoTrack& GSLocoTrackFor(APawn* Pawn)
	{
		for (FGSLocoTrack& T : GSLocoTracks)
		{
			if (T.Pawn.Get() == Pawn)
			{
				return T;
			}
		}
		FGSLocoTrack New;
		New.Pawn = Pawn;
		New.Name = Pawn->GetName();
		New.bPlayerControlled = Pawn->IsPlayerControlled();
		return GSLocoTracks[GSLocoTracks.Add(MoveTemp(New))];
	}

	/** A speed trace drawn in text, because the shape IS the evidence. A square wave and a smooth
	 *  ramp are instantly distinguishable by eye and tedious to distinguish from summary statistics
	 *  alone - and if the shape does not match the prediction, that has to be impossible to miss. */
	static FString GSLocoSparkline(const TArray<float>& Speeds, float MaxSpeed, int32 Columns)
	{
		static const TCHAR* Ramp = TEXT(" .:-=+*#");
		if (Speeds.Num() == 0 || MaxSpeed <= KINDA_SMALL_NUMBER)
		{
			return FString();
		}

		FString Out;
		Out.Reserve(Columns);
		for (int32 c = 0; c < Columns; ++c)
		{
			// Peak-hold rather than average across the bucket: averaging a 0.15s pulse into a wider
			// bucket is exactly how a square wave gets smoothed into the ramp we are testing for.
			const int32 Begin = (c * Speeds.Num()) / Columns;
			const int32 End = FMath::Max(Begin + 1, ((c + 1) * Speeds.Num()) / Columns);
			float Peak = 0.f;
			for (int32 i = Begin; i < End && i < Speeds.Num(); ++i)
			{
				Peak = FMath::Max(Peak, Speeds[i]);
			}
			const int32 Level = FMath::Clamp(FMath::RoundToInt((Peak / MaxSpeed) * 7.f), 0, 7);
			Out.AppendChar(Ramp[Level]);
		}
		return Out;
	}

	static void GSLocoReport();

	static bool GSLocoTick(float DeltaTime)
	{
		UWorld* World = GSLocoWorld.Get();
		if (!World)
		{
			GSLocoReport();
			return false;
		}

		GSLocoElapsed += DeltaTime;

		FVector Origin = FVector::ZeroVector;
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			if (const APawn* PlayerPawn = PC->GetPawn())
			{
				Origin = PlayerPawn->GetActorLocation();
			}
		}

		for (TActorIterator<APawn> It(World); It; ++It)
		{
			APawn* Pawn = *It;
			if (!IsValid(Pawn))
			{
				continue;
			}
			if (GSLocoRadius > 0.f && !Origin.IsZero()
				&& FVector::Dist(Pawn->GetActorLocation(), Origin) > GSLocoRadius)
			{
				continue;
			}

			const FVector V = Pawn->GetVelocity();
			const FVector Flat(V.X, V.Y, 0.f);
			const float Speed = Flat.Size();

			FGSLocoTrack& T = GSLocoTrackFor(Pawn);
			T.Speeds.Add(Speed);
			T.Times.Add(GSLocoElapsed);
			T.HeadingsDeg.Add(Speed > GSLocoCurrentThreshold
				? FMath::RadiansToDegrees(FMath::Atan2(Flat.Y, Flat.X))
				: TNumericLimits<float>::Max());   // sentinel: heading is undefined at rest

			// The live rule: one threshold, both directions.
			const bool bNowWalking = Speed > GSLocoCurrentThreshold;
			if (bNowWalking != T.bCurrentWalking)
			{
				T.bCurrentWalking = bNowWalking;
				++T.CurrentCrossings;
			}

			// The proposed rule: enter high, leave low.
			if (T.bProposedWalking ? (Speed < GSLocoProposedLeave) : (Speed > GSLocoProposedEnter))
			{
				T.bProposedWalking = !T.bProposedWalking;
				++T.ProposedCrossings;
			}
		}

		if (GSLocoElapsed >= GSLocoDuration)
		{
			GSLocoReport();
			return false;   // unregister
		}
		return true;
	}

	static void GSLocoReport()
	{
		GSLocoTickHandle.Reset();

		UE_LOG(LogGSAnim, Warning,
			TEXT("[GS.Loco] --- %.1fs capture, %d pawn(s). Idle<->Walk today = %.0f both ways; ")
			TEXT("proposed = enter %.0f / leave %.0f ---"),
			GSLocoElapsed, GSLocoTracks.Num(),
			GSLocoCurrentThreshold, GSLocoProposedEnter, GSLocoProposedLeave);
		UE_LOG(LogGSAnim, Warning,
			TEXT("[GS.Loco] %-28s | %5s | %6s | %6s | %7s | %7s | %8s | %8s"),
			TEXT("pawn"), TEXT("smpls"), TEXT("%rest"), TEXT("maxspd"),
			TEXT("flick/s"), TEXT("prop/s"), TEXT("turn d/s"), TEXT("verdict"));

		int32 SquareWavePawns = 0;
		int32 SmoothJitterPawns = 0;
		const FGSLocoTrack* Worst = nullptr;

		for (const FGSLocoTrack& T : GSLocoTracks)
		{
			if (T.Speeds.Num() < 2 || GSLocoElapsed <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			int32 AtRest = 0;
			float MaxSpeed = 0.f;
			for (const float S : T.Speeds)
			{
				if (S <= GSLocoCurrentThreshold) { ++AtRest; }
				MaxSpeed = FMath::Max(MaxSpeed, S);
			}
			const float RestFraction = static_cast<float>(AtRest) / static_cast<float>(T.Speeds.Num());

			// Heading change per second, measured ONLY across consecutive moving samples. Sampling
			// across a stop would report the heading before and after a pause as a "turn", which is
			// the one thing that could make hypothesis B look true when it is not.
			float TotalTurn = 0.f;
			float TurnSeconds = 0.f;
			for (int32 i = 1; i < T.HeadingsDeg.Num(); ++i)
			{
				if (T.HeadingsDeg[i] == TNumericLimits<float>::Max()
					|| T.HeadingsDeg[i - 1] == TNumericLimits<float>::Max())
				{
					continue;
				}
				TotalTurn += FMath::Abs(FMath::FindDeltaAngleDegrees(T.HeadingsDeg[i - 1], T.HeadingsDeg[i]));
				TurnSeconds += (T.Times[i] - T.Times[i - 1]);
			}
			const float TurnRate = (TurnSeconds > KINDA_SMALL_NUMBER) ? (TotalTurn / TurnSeconds) : 0.f;

			const float FlickerPerSec = static_cast<float>(T.CurrentCrossings) / GSLocoElapsed;
			const float ProposedPerSec = static_cast<float>(T.ProposedCrossings) / GSLocoElapsed;

			// A pawn that never moved is not evidence either way, and saying so keeps a still
			// bystander out of the verdict count.
			FString Verdict = TEXT("still");
			if (MaxSpeed > GSLocoCurrentThreshold)
			{
				if (FlickerPerSec >= 1.f && RestFraction > 0.2f)
				{
					Verdict = TEXT("SQUARE");
					++SquareWavePawns;
					if (!Worst || T.CurrentCrossings > Worst->CurrentCrossings) { Worst = &T; }
				}
				else if (RestFraction < 0.1f && TurnRate > 90.f)
				{
					Verdict = TEXT("turnjit");
					++SmoothJitterPawns;
				}
				else
				{
					Verdict = TEXT("smooth");
				}
			}

			UE_LOG(LogGSAnim, Warning,
				TEXT("[GS.Loco] %-28s | %5d | %5.0f%% | %6.0f | %7.1f | %7.1f | %8.0f | %8s%s"),
				*T.Name, T.Speeds.Num(), RestFraction * 100.f, MaxSpeed,
				FlickerPerSec, ProposedPerSec, TurnRate, *Verdict,
				T.bPlayerControlled ? TEXT("  <- PLAYER") : TEXT(""));
		}

		if (Worst && Worst->Speeds.Num() > 0)
		{
			float PeakSpeed = 0.f;
			for (const float S : Worst->Speeds) { PeakSpeed = FMath::Max(PeakSpeed, S); }
			UE_LOG(LogGSAnim, Warning, TEXT("[GS.Loco] speed trace, %s (0 to %.0f uu/s over %.1fs):"),
				*Worst->Name, PeakSpeed, GSLocoElapsed);
			UE_LOG(LogGSAnim, Warning, TEXT("[GS.Loco] [%s]"),
				*GSLocoSparkline(Worst->Speeds, PeakSpeed, 100));
		}

		// ---- DO NOT LET ONE POPULATION OUTVOTE ANOTHER (fixed after the first real capture) ------
		//
		// The first version picked a single winner by comparing the two counts, and on 2026-08-21 it
		// printed "THE BEHAVIOUR-TREE DIAGNOSIS IS WRONG" off the back of 7 horde goblins - which run
		// a different skeleton and a different AnimBP entirely - outvoting the 3 humans that were the
		// actual subject. A majority vote across unrelated animation setups is not evidence about
		// either of them.
		//
		// So: any SQUARE pawn at all means start/stop is real for SOMETHING, and the row that matters
		// is the one whose name you came here to read. The counts are reported side by side and the
		// reader picks the population; only a capture with ZERO square-wave pawns refutes it.
		FString Summary;
		if (SquareWavePawns > 0)
		{
			Summary = FString::Printf(
				TEXT("GS.AI.LogLocomotion: %d SQUARE (start/stop), %d turnjit (continuous, heading ")
				TEXT("jitter). READ THE ROW FOR THE PAWN YOU CARE ABOUT - different skeletons run ")
				TEXT("different AnimBPs and do not vote on each other. Where flick/s equals prop/s, ")
				TEXT("the proposed hysteresis would change nothing for that pawn."),
				SquareWavePawns, SmoothJitterPawns);
		}
		else if (SmoothJitterPawns > 0)
		{
			Summary = FString::Printf(
				TEXT("GS.AI.LogLocomotion: %d pawn(s) move CONTINUOUSLY with a jittering heading. ")
				TEXT("THE BEHAVIOUR-TREE DIAGNOSIS IS WRONG - look at RVO avoidance and the separation ")
				TEXT("steer instead."),
				SmoothJitterPawns);
		}
		else
		{
			Summary = TEXT("GS.AI.LogLocomotion: nothing moved enough to judge. Capture during a fight, ")
					  TEXT("with an archer in range.");
		}

		UE_LOG(LogGSAnim, Warning, TEXT("[GS.Loco] %s"), *Summary);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 15.f,
				SquareWavePawns > 0 ? FColor::Yellow : FColor::Green, Summary);
		}

		GSLocoTracks.Empty();
		GSLocoWorld.Reset();
	}
}

static FAutoConsoleCommandWithWorldAndArgs GSAILogLocomotionCmd(
	TEXT("GS.AI.LogLocomotion"),
	TEXT("GS.AI.LogLocomotion [seconds=5] [radius=3000] - sample every pawn's ground speed each "
		 "frame, then report whether the motion is a start/stop square wave or continuous travel "
		 "with a jittering heading. Also counts how many Idle<->Walk animation flickers the current "
		 "threshold produces, and how many would survive the proposed hysteresis."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* InWorld)
		{
			UWorld* World = GSAnimGameWorld(InWorld);
			if (!World)
			{
				UE_LOG(LogGSAnim, Warning, TEXT("[GS.Loco] GS.AI.LogLocomotion: no game world."));
				return;
			}

			if (GSLocoTickHandle.IsValid())
			{
				UE_LOG(LogGSAnim, Warning,
					TEXT("[GS.Loco] A capture is already running (%.1fs of %.1fs). Ignoring."),
					GSLocoElapsed, GSLocoDuration);
				return;
			}

			GSLocoDuration = (Args.Num() > 0) ? FMath::Max(0.5f, FCString::Atof(*Args[0])) : 5.f;
			GSLocoRadius = (Args.Num() > 1) ? FCString::Atof(*Args[1]) : 3000.f;
			GSLocoElapsed = 0.f;
			GSLocoTracks.Empty();
			GSLocoWorld = World;

			GSLocoTickHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateStatic(&GSLocoTick));

			UE_LOG(LogGSAnim, Warning,
				TEXT("[GS.Loco] capturing %.1fs within %.0fuu of the player - keep the fight going."),
				GSLocoDuration, GSLocoRadius);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, GSLocoDuration, FColor::Cyan,
					FString::Printf(TEXT("GS.AI.LogLocomotion: capturing %.0fs..."), GSLocoDuration));
			}
		}));
