/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadMeshBuild/IRoadMeshBuildHost.h"
#include "RoadMeshBuild/RoadComputePipeline.h"
#include "RoadMeshBuild/MetaRoadBuildSettings.h"
#include "RoadSplineComponent.h"
#include "MetaRoadActor.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IRoadMeshBuildHost)

using namespace MetaRoad;

UMetaRoadBuildSettings* IRoadMeshBuildHost::GetBuildSettingsForActor(AMetaRoad* Actor) const
{
	return UMetaRoadBuildSettings::GetForActor(Actor, /*bCreateIfMissing=*/false);
}

void IRoadMeshBuildHost::SetSplineActors(const TArray<TWeakObjectPtr<AMetaRoad>>& SplineActors)
{
	for (auto& ActorWeak : SplineActors)
	{
		AMetaRoad* Actor = ActorWeak.Get();
		if (!Actor) continue;

		TArray<URoadSplineComponent*> AllSplines;
		Actor->GetComponents(AllSplines);

		TMap<FName, TArray<URoadSplineComponent*>> ByGroup;
		for (auto* S : AllSplines)
			ByGroup.FindOrAdd(S->SubGroup).Add(S);

		if (ByGroup.Num() <= 1)
		{
			// Single sub-group (or no splines) — legacy path, no filtering needed
			RoadComputePipelines.Add(MakeShared<FRoadComputePipeline>(ActorWeak, NAME_None, TArray<TWeakObjectPtr<URoadSplineComponent>>{}));
		}
		else
		{
			// Multiple sub-groups — one scope per sub-group, each with its own spline subset
			for (auto& [SubGroup, Splines] : ByGroup)
			{
				TArray<TWeakObjectPtr<URoadSplineComponent>> GroupSplines;
				GroupSplines.Reserve(Splines.Num());
				for (auto* S : Splines)
					GroupSplines.Add(S);
				RoadComputePipelines.Add(MakeShared<FRoadComputePipeline>(ActorWeak, SubGroup, MoveTemp(GroupSplines)));
			}
		}
	}
}

void IRoadMeshBuildHost::InitializePipelines()
{
	for (auto& Pipeline : RoadComputePipelines)
	{
		Pipeline->Initialize(*this);
	}
}

void IRoadMeshBuildHost::RequestRebuildAll()
{
	for (auto& Pipeline : RoadComputePipelines)
	{
		Pipeline->RequestRebuildAll();
	}
}

void IRoadMeshBuildHost::BuildAllSynchronous()
{
	for (auto& Pipeline : RoadComputePipelines)
	{
		Pipeline->BuildSynchronous();
	}
}

void IRoadMeshBuildHost::MarkActorDirty(UActorComponent* Component)
{
	if (!Component) return;

	auto* Found = RoadComputePipelines.FindByPredicate([Component](auto& It)
	{
		return It->GetTargetActor() == Component->GetOwner();
	});

	if (Found)
	{
		(*Found)->MarkDirty();
	}
}

bool IRoadMeshBuildHost::TickPipelines(float DeltaTime, FRoadMeshBuildTickResult& Out)
{
	Out = {};

	for (auto& Pipeline : RoadComputePipelines)
	{
		Out.bAnyRebuildStarted |= Pipeline->Tick(DeltaTime);
	}

	for (auto& Pipeline : RoadComputePipelines)
	{
		Out.TotalActiveTasks += Pipeline->CountActiveTasks();

		if (Pipeline->TryFlushReport())
		{
			Out.bReportShown = true;
		}

		const UE::Geometry::FGeometryResult& Info = Pipeline->GetResultInfo();
		if (Info.HasFailed() || Info.Errors.Num())
		{
			Out.bHasFailed = true;
		}
		if (Info.Warnings.Num())
		{
			Out.bHasWarnings = true;
		}
	}

	return Out.bAnyRebuildStarted;
}

void IRoadMeshBuildHost::CancelAllPipelines()
{
	for (auto& Pipeline : RoadComputePipelines)
	{
		Pipeline->CancelAll();
	}
}
