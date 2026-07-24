/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadMeshBuild/SplineMeshOpHelpers.h"
#include "RoadMeshBuild/IRoadMeshBuildHost.h"
#include "Engine/World.h" // UWorld complete for TWeakObjectPtr<UWorld> in Setup()
#include "UObject/WeakInterfacePtr.h"
#include "RoadMeshBuild/Ops/TriangulateRoadOp.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "MetaRoadModule.h"

#define LOCTEXT_NAMESPACE "MeshOpPreviewHelpers"

using namespace UE::Geometry;

namespace UE::Private::MeshOpPreviewLocal
{
	static void DisplayCriticalWarningMessage(const FText& InMessage, float ExpireDuration = 5.0f)
	{
#if WITH_EDITOR
		FNotificationInfo Info(InMessage);
		Info.ExpireDuration = ExpireDuration;
		FSlateNotificationManager::Get().AddNotification(Info);
#endif

		UE_LOG(LogMetaRoad, Warning, TEXT("%s"), *InMessage.ToString());
	}

	int32 MaxActiveBackgroundTasksWithOverride(int32 MaxWithoutOverride)
	{
		return 0;
	}
}


void USplineMeshOpPreviewWithBackgroundCompute::Setup(UWorld* InWorld)
{
	PreviewMesh = NewObject<USplineMeshPreview>(this, TEXT("PreviewMesh"));
	PreviewMesh->CreateInWorld(InWorld, FTransform::Identity);
	PreviewWorld = InWorld;
	bResultValid = false;
	bMeshInitialized = false;
}

void USplineMeshOpPreviewWithBackgroundCompute::Setup(UWorld* InWorld, MetaRoad::ISplineMeshOperatorFactory* OpGenerator)
{
	Setup(InWorld);
	BackgroundCompute = MakeUnique<MetaRoad::FBackgroundSplineMeshComputeSource>(OpGenerator);
	BackgroundCompute->MaxActiveTaskCount = UE::Private::MeshOpPreviewLocal::MaxActiveBackgroundTasksWithOverride(MaxActiveBackgroundTasks);
}

void USplineMeshOpPreviewWithBackgroundCompute::ChangeOpFactory(MetaRoad::ISplineMeshOperatorFactory* OpGenerator)
{
	CancelCompute();
	BackgroundCompute = MakeUnique<MetaRoad::FBackgroundSplineMeshComputeSource>(OpGenerator);
	BackgroundCompute->MaxActiveTaskCount = UE::Private::MeshOpPreviewLocal::MaxActiveBackgroundTasksWithOverride(MaxActiveBackgroundTasks);
	bResultValid = false;
	bMeshInitialized = false;
}

void USplineMeshOpPreviewWithBackgroundCompute::ClearOpFactory()
{
	CancelCompute();
	BackgroundCompute = nullptr;
	bResultValid = false;
	bMeshInitialized = false;
}


MetaRoad::FSplineMeshOpResult USplineMeshOpPreviewWithBackgroundCompute::Shutdown()
{
	CancelCompute();

	MetaRoad::FSplineMeshOpResult Result;
	Result.MeshSegments = PreviewMesh->ExtractMeshSegments();
	Result.Transform = FTransformSRT3d(PreviewMesh->GetTransform());

	PreviewMesh->SetVisible(false);
	PreviewMesh->Disconnect();
	PreviewMesh = nullptr;

	PreviewWorld = nullptr;

	return Result;
}

void USplineMeshOpPreviewWithBackgroundCompute::CancelCompute()
{
	if (BackgroundCompute)
	{
		BackgroundCompute->CancelActiveCompute();
	}
}

void USplineMeshOpPreviewWithBackgroundCompute::Cancel()
{
	CancelCompute();

	PreviewMesh->SetVisible(false);
	PreviewMesh->Disconnect();
	PreviewMesh = nullptr;
}

void USplineMeshOpPreviewWithBackgroundCompute::Tick(float DeltaTime)
{
	if (BackgroundCompute)
	{
		BackgroundCompute->Tick(DeltaTime);
		UpdateResults();
	}

}

void USplineMeshOpPreviewWithBackgroundCompute::ComputeSynchronous()
{
	// Mirror UpdateResults()/the static URoadMeshOpPreviewWithBackgroundCompute::ComputeSynchronous, but run the
	// operator directly on the calling thread (no FBackgroundSplineMeshComputeSource).
	bResultValid = false;
	if (!OwnedFactory || !PreviewMesh)
	{
		return;
	}

	TUniquePtr<MetaRoad::FSplineMeshOperator> Op = OwnedFactory->MakeNewOperator();
	if (!Op)
	{
		return;
	}

	Op->CalculateResult(nullptr);
	OnOpSplineMeshCompleted.Broadcast(Op.Get());

	if (!Op->GetResultInfo().HasResult())
	{
		return;
	}

	TUniquePtr<MetaRoad::FSplineMeshSegments> ResultMesh = Op->ExtractResult();
	const bool bHasSegments = ResultMesh.IsValid() && ResultMesh->Segments.Num() > 0;

	PreviewMesh->SetTransform((FTransform)Op->GetResultTransform());
	PreviewMesh->UpdatePreview(MoveTemp(ResultMesh));
	bMeshInitialized = true;
	PreviewMesh->SetVisible(bVisible);
	bResultValid = bHasSegments;
}

void USplineMeshOpPreviewWithBackgroundCompute::SetMaxActiveBackgroundTasks(int32 InMaxActiveBackgroundTasks)
{
	MaxActiveBackgroundTasks = InMaxActiveBackgroundTasks;
	if (BackgroundCompute)
	{
		BackgroundCompute->MaxActiveTaskCount = UE::Private::MeshOpPreviewLocal::MaxActiveBackgroundTasksWithOverride(MaxActiveBackgroundTasks);
	}
}

void USplineMeshOpPreviewWithBackgroundCompute::UpdateResults()
{
	if (BackgroundCompute == nullptr)
	{
		LastComputeStatus = EBackgroundComputeTaskStatus::NotComputing;
		return;
	}

	MetaRoad::FBackgroundSplineMeshComputeSource::FStatus Status = BackgroundCompute->CheckStatus();
	LastComputeStatus = Status.TaskStatus;

	if (LastComputeStatus == EBackgroundComputeTaskStatus::ValidResultAvailable || (bAllowDirtyResultUpdates && LastComputeStatus == EBackgroundComputeTaskStatus::DirtyResultAvailable))
	{
		TUniquePtr<MetaRoad::FSplineMeshOperator> MeshOp = BackgroundCompute->ExtractResult();
		OnOpSplineMeshCompleted.Broadcast(MeshOp.Get());

		TUniquePtr<MetaRoad::FSplineMeshSegments> ResultMesh = MeshOp->ExtractResult();
		PreviewMesh->SetTransform((FTransform)MeshOp->GetResultTransform());


		PreviewMesh->UpdatePreview(MoveTemp(ResultMesh));
		bMeshInitialized = true;

		PreviewMesh->SetVisible(bVisible);
		bResultValid = (LastComputeStatus == EBackgroundComputeTaskStatus::ValidResultAvailable);
		ValidResultComputeTimeSeconds = Status.ElapsedTime;

		OnMeshUpdated.Broadcast(this);

		bWaitingForBackgroundTasks = false;
	}
	else if (int WaitingTaskCount; BackgroundCompute->IsWaitingForBackgroundTasks(WaitingTaskCount))
	{
		if (!bWaitingForBackgroundTasks)
		{
			UE::Private::MeshOpPreviewLocal::DisplayCriticalWarningMessage(LOCTEXT("TooManyBackgroundTasks", "Too many background tasks: Cancelling earlier tasks before generating new preview."));
			bWaitingForBackgroundTasks = true;
		}
	}
	else
	{
		bWaitingForBackgroundTasks = false;
	}
}

void USplineMeshOpPreviewWithBackgroundCompute::InvalidateResult()
{
	if (BackgroundCompute)
	{
		BackgroundCompute->NotifyActiveComputeInvalidated();
	}
	bResultValid = false;
}

void USplineMeshOpPreviewWithBackgroundCompute::SetVisibility(bool bVisibleIn)
{
	bVisible = bVisibleIn;
	PreviewMesh->SetVisible(bVisible);
}

void USplineMeshOpPreviewWithBackgroundCompute::Setup(IRoadMeshBuildHost* Host, TWeakPtr<MetaRoad::FRoadComputePipeline> RoadComputePipeline, TFunction<TUniquePtr<MetaRoad::FSplineMeshOperator>()> MakeOp)
{
	OwnedFactory = MakeUnique<MetaRoad::FLambdaSplineMeshOperatorFactory>(MoveTemp(MakeOp));
	Setup(Host->GetTargetWorld(), OwnedFactory.Get());
	OnOpSplineMeshCompleted.AddLambda(
		[this, RoadComputePipeline](const MetaRoad::FSplineMeshOperator* Op)
		{
			if (!Op)
			{
				return;
			}

			auto& ActorData = *RoadComputePipeline.Pin().Get();
			ActorData.AppendResultInfo(Op->GetResultInfo());

			if (!Op->GetResultInfo().HasResult())
			{
				return;
			}
		}
	);
	OnMeshUpdated.AddLambda(
		[WeakHost = TWeakInterfacePtr<IRoadMeshBuildHost>(Host)](const USplineMeshOpPreviewWithBackgroundCompute* UpdatedPreview)
		{
			if (WeakHost.IsValid())
			{
				WeakHost->NotifyMeshUpdated();
			}
		}
	);
}

bool USplineMeshOpPreviewWithBackgroundCompute::ShutdownAndGenerateAssets(AActor* TargetActor, const FTransform3d& ActorToWorld)
{
	if (!HaveValidNonEmptyResult())
	{
		Cancel();
		return true; // nothing to write — not an error
	}

	auto OpResult = Shutdown();
	if (OpResult.MeshSegments.IsValid())
	{
		OpResult.MeshSegments->ApplyTransform(OpResult.Transform);
		OpResult.MeshSegments->ApplyTransformInverse(ActorToWorld);
		OpResult.MeshSegments->BuildComponents(TargetActor, false);
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
