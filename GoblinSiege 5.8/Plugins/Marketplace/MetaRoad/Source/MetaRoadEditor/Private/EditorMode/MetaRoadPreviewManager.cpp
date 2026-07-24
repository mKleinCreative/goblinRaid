/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "EditorMode/MetaRoadPreviewManager.h"
#include "Engine/World.h"
#include "Editor.h"
#include "MetaRoadActor.h"
#include "RoadMeshBuild/RoadComputePipeline.h"
#include "RoadMeshBuild/RoadTriangulationData.h" // FRoadTriangulationData::DebugDraw
#include "RoadMeshBuild/ToolPropertySets.h"
#include "RoadMeshBuild/MetaRoadBuildSettings.h"
#include "SceneManagement.h" // FPrimitiveDrawInterface, SDPG_World
#include "RoadMeshBuild/MetaRoadBuildSettingsBase.h" // UMetaRoadBuildSettingsBase::GetOnModified
#include "MetaRoadTypes.h" // FMetaRoadDelegates::OnRoadComponentDirtyDelegate

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaRoadPreviewManager)

void UMetaRoadPreviewManager::Initialize(UWorld* InWorld)
{
	PreviewWorld = InWorld;

	// Auto-rebuild the affected preview when a road component's data changes (spline edits, etc.).
	RoadDirtyHandle = FMetaRoadDelegates::OnRoadComponentDirtyDelegate.AddUObject(this, &UMetaRoadPreviewManager::MarkActorDirty);
}

void UMetaRoadPreviewManager::SetPreviewActors(const TArray<TWeakObjectPtr<AMetaRoad>>& Actors)
{
	PreviewedActors = Actors;
	Reconcile();
}

void UMetaRoadPreviewManager::SetPresetEditing(bool bEnable)
{
	if (bPresetEditing == bEnable)
	{
		return;
	}
	bPresetEditing = bEnable;
	Reconcile();
}

void UMetaRoadPreviewManager::SetMeshPreviewEnabled(bool bEnable)
{
	if (bMeshPreviewEnabled == bEnable)
	{
		return;
	}
	bMeshPreviewEnabled = bEnable;
	// Reconcile rebuilds or tears down the pipelines to match, while leaving the preset working copies (and
	// their in-progress edits) untouched — so toggling the view does not disturb the preset-editing session.
	Reconcile();
}

void UMetaRoadPreviewManager::Reconcile()
{
	// Two independent concerns share the selected-actor set:
	//   * Preset session (working copies) — alive while Preset editing, INDEPENDENT of the view.
	//   * Mesh preview pipelines — built only while mesh preview (Preview view) is enabled.
	// Keeping them separate lets the Preset panel (and its preset dropdown) keep working after the user
	// toggles the view to Schematic: only the pipelines are torn down; the working copies survive.

	TeardownPipelines();

	if (PreviewedActors.Num() == 0 || !PreviewWorld.IsValid())
	{
		ClearWorkingCopies();
		return;
	}

	// Ensure each previewed AMetaRoad has its real per-actor holder + property sets on the game thread
	// (so the background triangulation factory only ever reads them).
	for (const TWeakObjectPtr<AMetaRoad>& Actor : PreviewedActors)
	{
		if (UMetaRoadBuildSettings* Settings = UMetaRoadBuildSettings::GetForActor(Actor.Get(), /*bCreateIfMissing=*/true))
		{
			Settings->EnsureDefaultPropertySets();
		}
	}

	// Preset editing previews from transient working copies; otherwise from the actors' real holders.
	if (bPresetEditing)
	{
		SyncWorkingCopies();
	}
	else
	{
		ClearWorkingCopies();
	}

	RefreshSubscriptions();

	// The mesh pipelines only exist while the Preview view is showing them.
	if (bMeshPreviewEnabled)
	{
		SetSplineActors(PreviewedActors);
		InitializePipelines();
		RequestRebuildAll();
	}
}

void UMetaRoadPreviewManager::ClearPreview()
{
	TeardownPipelines();
	ClearWorkingCopies();
	PreviewedActors.Reset();
	bMeshPreviewEnabled = false;
}

void UMetaRoadPreviewManager::CancelPreviewBuild()
{
	// Stop the background computes but keep the pipelines (and their preview meshes) alive so the preview
	// isn't torn down. Must NOT use CancelAllPipelines() here — that calls Cancel() on each layer, which
	// destroys its PreviewMesh; the next Tick() would then dereference the null mesh and crash. Use the
	// per-pipeline CancelActiveComputes() (CancelCompute() under the hood, mesh-preserving) instead.
	for (const TSharedPtr<MetaRoad::FRoadComputePipeline>& Pipeline : GetPipelines())
	{
		if (Pipeline.IsValid())
		{
			Pipeline->CancelActiveComputes();
		}
	}

	// Force the status off InProgress — Tick only settles the status while work is active, so without this
	// the throbber/Cancel button would linger. Idle hides the status icon and reverts the toolkit button to
	// Update; a subsequent RequestRebuildAll re-drives the pipelines from scratch.
	PreviewStatus = EMetaRoadPreviewStatus::Idle;
	bOpWasJustUpdated = false;
}

void UMetaRoadPreviewManager::TeardownPipelines()
{
	UnsubscribeFromBuildSettings();
	CancelAllPipelines();
	ResetPipelines();
	PreviewStatus = EMetaRoadPreviewStatus::Idle;
	bOpWasJustUpdated = false;
}

void UMetaRoadPreviewManager::SyncWorkingCopies()
{
	// Reconcile the working copies with PreviewedActors, PRESERVING existing copies (and any in-progress
	// preset edits) for actors that are still previewed. Only newly-added actors get a fresh transient copy;
	// copies for removed actors are dropped. Preserving copies is what lets the preset-editing session (and
	// the user's unsaved edits) survive a Schematic<->Preview view toggle, which tears down/rebuilds the mesh
	// pipelines around it. WorkingSettings/WorkingActors are kept index-aligned.
	TArray<TObjectPtr<UMetaRoadBuildSettings>> NewSettings;
	TArray<TWeakObjectPtr<AMetaRoad>> NewActors;
	NewSettings.Reserve(PreviewedActors.Num());
	NewActors.Reserve(PreviewedActors.Num());

	for (const TWeakObjectPtr<AMetaRoad>& ActorWeak : PreviewedActors)
	{
		AMetaRoad* Road = ActorWeak.Get();
		if (!Road)
		{
			continue;
		}

		// Reuse the existing working copy for this actor if we already have one (keeps its edits).
		const int32 ExistingIdx = WorkingActors.IndexOfByKey(Road);
		if (WorkingSettings.IsValidIndex(ExistingIdx) && WorkingSettings[ExistingIdx])
		{
			NewSettings.Add(WorkingSettings[ExistingIdx]);
			NewActors.Add(Road);
			continue;
		}

		UMetaRoadBuildSettings* Real = UMetaRoadBuildSettings::GetForActor(Road, /*bCreateIfMissing=*/true);
		if (!Real)
		{
			continue;
		}
		Real->EnsureDefaultPropertySets();

		// Transient deep copy (outered to this manager) — edited + previewed without touching the actor.
		UMetaRoadBuildSettings* Working = DuplicateObject<UMetaRoadBuildSettings>(Real, this);
		NewSettings.Add(Working);
		NewActors.Add(Road);
	}

	WorkingSettings = MoveTemp(NewSettings);
	WorkingActors = MoveTemp(NewActors);
}

void UMetaRoadPreviewManager::ClearWorkingCopies()
{
	WorkingSettings.Reset();
	WorkingActors.Reset();
}

void UMetaRoadPreviewManager::RefreshSubscriptions()
{
	UnsubscribeFromBuildSettings();

	// Listen for Details edits of the active source so the preview rebuilds (the manager is not a
	// UInteractiveTool, so it gets no OnPropertyModified from the tools framework).
	auto SubscribeHolder = [this](UMetaRoadBuildSettings* Holder)
	{
		if (!Holder)
		{
			return;
		}
		for (const TObjectPtr<UMetaRoadBuildSettingsBase>& PropertySet : Holder->PropertySets)
		{
			if (PropertySet)
			{
				PropertySet->GetOnModified().AddUObject(this, &UMetaRoadPreviewManager::OnBuildSettingModified);
				SubscribedPropertySets.Add(PropertySet);
			}
		}
	};

	if (bPresetEditing)
	{
		for (const TObjectPtr<UMetaRoadBuildSettings>& Working : WorkingSettings)
		{
			SubscribeHolder(Working);
		}
	}
	else
	{
		for (const TWeakObjectPtr<AMetaRoad>& Actor : PreviewedActors)
		{
			SubscribeHolder(UMetaRoadBuildSettings::GetForActor(Actor.Get(), /*bCreateIfMissing=*/false));
		}
	}
}

TArray<UObject*> UMetaRoadPreviewManager::GetWorkingSettingsObjects() const
{
	TArray<UObject*> Objects;
	for (const TObjectPtr<UMetaRoadBuildSettings>& Working : WorkingSettings)
	{
		if (Working)
		{
			Objects.Add(Working);
		}
	}
	return Objects;
}

void UMetaRoadPreviewManager::ApplyWorkingToSelected()
{
	for (int32 Index = 0; Index < WorkingActors.Num(); ++Index)
	{
		AMetaRoad* Road = WorkingActors[Index].Get();
		UMetaRoadBuildSettings* Working = WorkingSettings.IsValidIndex(Index) ? WorkingSettings[Index].Get() : nullptr;
		if (!Road || !Working)
		{
			continue;
		}

		// Commit: replace the actor's real settings with a copy of the working settings + flag for save.
		Road->Modify();
		Road->BuildSettings = DuplicateObject<UMetaRoadBuildSettings>(Working, Road);
		Road->MarkPackageDirty();
	}
}

UMetaRoadBuildSettings* UMetaRoadPreviewManager::GetBuildSettingsForActor(AMetaRoad* Actor) const
{
	if (bPresetEditing)
	{
		for (int32 Index = 0; Index < WorkingActors.Num(); ++Index)
		{
			if (WorkingActors[Index].Get() == Actor)
			{
				return WorkingSettings.IsValidIndex(Index) ? WorkingSettings[Index].Get() : nullptr;
			}
		}
	}
	return IRoadMeshBuildHost::GetBuildSettingsForActor(Actor);
}

void UMetaRoadPreviewManager::UnsubscribeFromBuildSettings()
{
	for (const TWeakObjectPtr<UMetaRoadBuildSettingsBase>& PropertySet : SubscribedPropertySets)
	{
		if (PropertySet.IsValid())
		{
			PropertySet->GetOnModified().RemoveAll(this);
		}
	}
	SubscribedPropertySets.Reset();
}

void UMetaRoadPreviewManager::OnBuildSettingModified(UObject* PropertySet, FProperty* Property)
{
	if (!PropertySet || !Property)
	{
		return;
	}

	// Resolve the actor that owns the modified property set: via the working-copy map while Preset editing,
	// otherwise via the holder's actor outer. Rebuild only that actor's pipeline for the changed layer.
	AActor* OwningActor = nullptr;
	if (bPresetEditing)
	{
		UMetaRoadBuildSettings* Holder = Cast<UMetaRoadBuildSettings>(PropertySet->GetOuter());
		const int32 Index = WorkingSettings.IndexOfByKey(Holder);
		if (WorkingActors.IsValidIndex(Index))
		{
			OwningActor = WorkingActors[Index].Get();
		}
	}
	else
	{
		OwningActor = PropertySet->GetTypedOuter<AActor>();
	}

	if (!OwningActor)
	{
		return;
	}

	for (const TSharedPtr<MetaRoad::FRoadComputePipeline>& Pipeline : GetPipelines())
	{
		if (Pipeline.IsValid() && Pipeline->GetTargetActor() == OwningActor)
		{
			Pipeline->HandlePropertyModified(*Property);
		}
	}
}

void UMetaRoadPreviewManager::SetWireframe(bool bEnable)
{
	for (const TSharedPtr<MetaRoad::FRoadComputePipeline>& Pipeline : GetPipelines())
	{
		if (Pipeline.IsValid())
		{
			Pipeline->SetWireframe(bEnable);
		}
	}
}

void UMetaRoadPreviewManager::Tick(float DeltaTime)
{
	FRoadMeshBuildTickResult Result;
	TickPipelines(DeltaTime, Result);

	// Map the aggregate tick result to a status (mirrors UTriangulateRoadTool::OnTick).
	if (GetPipelines().Num() == 0)
	{
		PreviewStatus = EMetaRoadPreviewStatus::Idle;
	}
	else if (Result.bAnyRebuildStarted)
	{
		PreviewStatus = EMetaRoadPreviewStatus::InProgress;
	}
	else if (bOpWasJustUpdated || Result.bReportShown)
	{
		if (Result.TotalActiveTasks > 0)
		{
			PreviewStatus = EMetaRoadPreviewStatus::InProgress;
		}
		else if (Result.bHasFailed)
		{
			PreviewStatus = EMetaRoadPreviewStatus::Error;
		}
		else if (Result.bHasWarnings)
		{
			PreviewStatus = EMetaRoadPreviewStatus::Warning;
		}
		else
		{
			PreviewStatus = EMetaRoadPreviewStatus::Success;
		}
	}

	bOpWasJustUpdated = false;
}

void UMetaRoadPreviewManager::RenderDebugLines(FPrimitiveDrawInterface* PDI, bool bDrawBoundaries) const
{
	if (!PDI)
	{
		return;
	}
	for (const TSharedPtr<MetaRoad::FRoadComputePipeline>& Pipeline : GetPipelines())
	{
		if (!Pipeline.IsValid())
		{
			continue;
		}
		const TSharedPtr<MetaRoad::FRoadTriangulationData> Data = Pipeline->GetTriangulationResult();
		if (!Data.IsValid())
		{
			continue;
		}

		// Boundaries: drawn directly from the (persistent) result — independent of the DebugDraw buffer — so
		// toggling the option redraws immediately without rebuilding the preview. Mirrors the former
		// UTriangulateRoadTool::Render: translucent foreground lines (blue, thickness 4) + white endpoint points.
		if (bDrawBoundaries)
		{
			const auto& Vertices = Data->Vertices3d;
			for (const auto& Boundary : Data->Boundaries)
			{
				for (const auto& Edge : Boundary)
				{
					if (Vertices.IsValidIndex(Edge.A) && Vertices.IsValidIndex(Edge.B))
					{
						const FVector A = Vertices[Edge.A].Vertex;
						const FVector B = Vertices[Edge.B].Vertex;
						PDI->DrawTranslucentLine(A, B, FLinearColor::Blue, SDPG_Foreground, 4.0f, 10000.0f, true);
						PDI->DrawPoint(A, FColor::White, 10.0f, SDPG_Foreground);
						PDI->DrawPoint(B, FColor::White, 10.0f, SDPG_Foreground);
					}
				}
			}
		}

		// DebugDraw buffer: always drained. Operators only fill it when their own debug toggle is on (e.g.
		// FSplineMeshOp's bDrawRefSplines), so this is empty otherwise. Each batch carries its own color/thickness.
		Data->DebugDraw.ForEach([PDI](const auto& Batch)
		{
			const FLinearColor LineColor(Batch.Color);
			for (const TPair<FVector, FVector>& Line : Batch.Lines)
			{
				PDI->DrawTranslucentLine(Line.Key, Line.Value, LineColor, SDPG_Foreground, Batch.Thickness, 10000.0f, true);
			}
		});
	}
}

UWorld* UMetaRoadPreviewManager::GetTargetWorld() const
{
	return PreviewWorld.Get();
}

void UMetaRoadPreviewManager::NotifyMeshUpdated()
{
	bOpWasJustUpdated = true;
	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports(false);
	}
}

void UMetaRoadPreviewManager::BeginDestroy()
{
	FMetaRoadDelegates::OnRoadComponentDirtyDelegate.Remove(RoadDirtyHandle);
	RoadDirtyHandle.Reset();

	TeardownPipelines();
	ClearWorkingCopies();
	Super::BeginDestroy();
}
