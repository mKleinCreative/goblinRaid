/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadProfilePreviewBuilder.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "RoadMeshBuild/RoadComputePipeline.h"
#include "RoadMeshBuild/ToolPropertySets.h"
#include "RoadMeshBuild/MetaRoadBuildSettings.h"
#include "RoadMeshBuild/MetaRoadBuildSettingsBase.h"
#include "Assets/RoadProfile.h"
#include "Utils/RoadUtils.h"
#include "MetaRoadActor.h"
#include "RoadSplineComponent.h"
#include "Components/SplineComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RoadProfilePreviewBuilder)

void URoadProfilePreviewBuilder::Initialize(UWorld* InWorld)
{
	PreviewWorld = InWorld;
}

void URoadProfilePreviewBuilder::EnsurePreviewActor()
{
	if (PreviewActor && PreviewSpline)
	{
		return;
	}

	UWorld* World = PreviewWorld.Get();
	if (!World)
	{
		return;
	}

	PreviewActor = RoadUtils::SpawnRoadActor(World, FTransform::Identity);
	PreviewSpline = RoadUtils::CreateSplineInActor(PreviewActor, /*bTransact=*/false, /*bSetAsRoot=*/true);

	// Build settings live on the (transient) preview actor's holder — the pipeline sources them via the
	// default GetBuildSettingsForActor. Transient actor → settings are not serialized (preview-only).
	if (UMetaRoadBuildSettings* Settings = UMetaRoadBuildSettings::GetForActor(PreviewActor, /*bCreateIfMissing=*/true))
	{
		Settings->EnsureDefaultPropertySets();
	}

	// Straight reference line along X, centered on the world origin so the road sits at world center.
	PreviewSpline->ClearSplinePoints(false);
	PreviewSpline->AddSplinePoint(FVector(-PreviewLengthCm * 0.5, 0.0, 0.0), ESplineCoordinateSpace::Local, false);
	PreviewSpline->AddSplinePoint(FVector(PreviewLengthCm * 0.5, 0.0, 0.0), ESplineCoordinateSpace::Local, false);
	PreviewSpline->UpdateSpline();

	// Raise the whole preview road ~20 cm above the floor.
	PreviewActor->SetActorLocation(FVector(0.0, 0.0, 20.0));
}

bool URoadProfilePreviewBuilder::PrepareSpline(URoadProfile* Profile)
{
	if (!Profile || !PreviewWorld.IsValid())
	{
		return false;
	}

	CurrentProfile = Profile;

	EnsurePreviewActor();
	if (!PreviewSpline)
	{
		return false;
	}

	// Refresh the spline layout so its road-graph visualization reflects profile edits.
	Profile->AssignToRoadSpline(PreviewSpline);
	PreviewSpline->MarkRenderStateDirty();
	return true;
}

void URoadProfilePreviewBuilder::EnsurePipelines()
{
	if (GetPipelines().Num() == 0)
	{
		SetSplineActors({ TWeakObjectPtr<AMetaRoad>(PreviewActor) });
		InitializePipelines();
	}
}

void URoadProfilePreviewBuilder::BuildFromProfile(URoadProfile* Profile)
{
	if (!PrepareSpline(Profile))
	{
		return;
	}

	// Run the mesh pipeline only when the generated mesh is actually shown — in Road Graph mode we
	// don't generate anything (only the spline is displayed).
	if (PreviewMode == ERoadProfilePreviewMode::GeneratedMesh)
	{
		EnsurePipelines();
		RequestRebuildAll();
	}

	ApplyPreviewMode();

	// Re-applying the profile resets the spline's selection — restore it.
	ApplySelectionToSpline();
}

void URoadProfilePreviewBuilder::BuildSynchronousFromProfile(URoadProfile* Profile)
{
	PreviewMode = ERoadProfilePreviewMode::GeneratedMesh;

	if (!PrepareSpline(Profile))
	{
		return;
	}

	EnsurePipelines();

	// Run the whole pipeline now, on this thread — every layer fills its preview mesh component.
	BuildAllSynchronous();

	ApplyPreviewMode(); // hide the spline; only the generated mesh remains
}

FBox URoadProfilePreviewBuilder::GetGeneratedBounds() const
{
	FBox Bounds(ForceInit);
	if (UWorld* World = PreviewWorld.Get())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			TArray<UDynamicMeshComponent*> Comps;
			It->GetComponents(Comps);
			for (UDynamicMeshComponent* Comp : Comps)
			{
				if (Comp && Comp->IsVisible() && Comp->GetDynamicMesh() && Comp->GetDynamicMesh()->GetTriangleCount() > 0)
				{
					Bounds += Comp->Bounds.GetBox();
				}
			}
		}
	}
	if (!Bounds.IsValid)
	{
		Bounds = FBox(FVector(-100.0), FVector(100.0));
	}
	return Bounds;
}

void URoadProfilePreviewBuilder::Tick(float DeltaTime)
{
	FRoadMeshBuildTickResult Result;
	TickPipelines(DeltaTime, Result);
}

void URoadProfilePreviewBuilder::SetPreviewMode(ERoadProfilePreviewMode Mode)
{
	if (PreviewMode == Mode)
	{
		ApplyPreviewMode();
		return;
	}
	PreviewMode = Mode;

	if (Mode == ERoadProfilePreviewMode::RoadGraph)
	{
		// Stop generating: cancel background work + destroy the preview meshes, then drop the pipeline.
		CancelAllPipelines();
		ResetPipelines();
	}
	else
	{
		// Generated Mesh: lanes are no longer pickable (spline hidden) — clear the selection.
		SetSelectedLane(NoSelection);
		if (CurrentProfile.IsValid())
		{
			BuildFromProfile(CurrentProfile.Get());
		}
	}

	ApplyPreviewMode();
}

void URoadProfilePreviewBuilder::SetSelectedLane(int32 LaneIndex)
{
	if (SelectedLaneIndex == LaneIndex)
	{
		return;
	}
	SelectedLaneIndex = LaneIndex;
	ApplySelectionToSpline();
	OnSelectionChanged.Broadcast();
}

void URoadProfilePreviewBuilder::ApplySelectionToSpline()
{
	if (!PreviewSpline)
	{
		return;
	}

	// Mirrors URoadSectionComponentVisualizerSelectionState::UpdateSplineSelection(): the spline's
	// scene proxy highlights the lane (SectionIndex, signed LaneIndex) with the selected material.
	if (SelectedLaneIndex != NoSelection && SelectedLaneIndex != MetaRoad::ZeroLaneIndex)
	{
		PreviewSpline->SetSelectedLane(0, SelectedLaneIndex);
	}
	else
	{
		PreviewSpline->SetSelectedLane(INDEX_NONE, MetaRoad::ZeroLaneIndex);
	}
}

void URoadProfilePreviewBuilder::ApplyPreviewMode()
{
	// The generated mesh exists only in Generated Mesh mode (built/destroyed on switch), so here we
	// only toggle the road spline's visibility.
	if (PreviewSpline)
	{
		const bool bShowGraph = (PreviewMode == ERoadProfilePreviewMode::RoadGraph);
		PreviewSpline->SetVisibility(bShowGraph, /*bPropagateToChildren=*/false);
	}
}

UWorld* URoadProfilePreviewBuilder::GetTargetWorld() const
{
	return PreviewWorld.Get();
}

UMetaRoadBuildSettings* URoadProfilePreviewBuilder::GetBuildSettings() const
{
	return UMetaRoadBuildSettings::GetForActor(PreviewActor, /*bCreateIfMissing=*/false);
}

void URoadProfilePreviewBuilder::NotifyMeshUpdated()
{
	OnPreviewUpdated.Broadcast();
}

void URoadProfilePreviewBuilder::BeginDestroy()
{
	CancelAllPipelines();
	ResetPipelines();
	Super::BeginDestroy();
}
