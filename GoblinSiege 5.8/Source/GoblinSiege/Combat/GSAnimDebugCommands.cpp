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
