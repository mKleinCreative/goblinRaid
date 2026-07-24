/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RoadMeshBuild/IRoadMeshBuildHost.h"
#include "MetaRoadPreviewManager.generated.h"

class UWorld;
class UMetaRoadBuildSettingsBase;
class UMaterialInterface;
class UMetaRoadBuildSettings;
class AActor;
class AMetaRoad;
class FProperty;

/** Aggregated live-preview build status, shown by the toolkit's preview status icon button. */
enum class EMetaRoadPreviewStatus : uint8
{
	Idle,
	InProgress,
	Error,
	Warning,
	Success
};

/**
 * UMetaRoadPreviewManager
 *
 * Mode-owned IRoadMeshBuildHost that drives live road-mesh preview for the selected AMetaRoad
 * actors directly in the level viewport while UMetaRoadEditorMode is in Preview display mode.
 *
 * Mirrors URoadProfilePreviewBuilder, but targets real level actors instead of a synthetic preview
 * actor. It never writes assets (GenerateAssets()/Shutdown() are never called) — committing the
 * result is the separate Bake action. Build settings are sourced per-AMetaRoad via
 * GetBuildSettingsForActor (the actor's own UMetaRoadBuildSettings, or a working copy in Preset mode).
 */
UCLASS()
class UMetaRoadPreviewManager : public UObject, public IRoadMeshBuildHost
{
	GENERATED_BODY()

public:
	/** Set the level world and create the default triangulation properties. */
	void Initialize(UWorld* InWorld);

	/** Set the AMetaRoad actors the preview follows and reconcile the state. An empty array clears both the
	 *  mesh pipelines and the preset working copies. */
	void SetPreviewActors(const TArray<TWeakObjectPtr<AMetaRoad>>& Actors);

	/** Toggle whether the mesh-preview pipelines are built (driven by the mode's Schematic/Preview view).
	 *  Turning it off tears the pipelines down but KEEPS the preset-editing session (working copies) alive,
	 *  so the Preset panel + preset dropdown stay functional in Schematic view. */
	void SetMeshPreviewEnabled(bool bEnable);
	bool IsMeshPreviewEnabled() const { return bMeshPreviewEnabled; }

	/** Full teardown (mode Exit): drops the pipeline stack, the preset working copies, and the actor set. */
	void ClearPreview();

	/** Cancel the in-progress background build but keep the (possibly partial) preview meshes and pipeline
	 *  stack — clears the InProgress status so the toolkit reverts the Cancel button back to Update. */
	void CancelPreviewBuild();

	/** Drive the compute pipelines one frame — call from UMetaRoadEditorMode::Tick. */
	void Tick(float DeltaTime);

	bool HasPreview() const { return GetPipelines().Num() > 0; }

	/** Draw each preview pipeline's debug geometry — called from the mode's Render every Preview frame.
	 *  Triangulation boundaries are drawn only when bDrawBoundaries is set; the DebugDraw buffer (e.g. the
	 *  spline-mesh "Draw reference splines" lines) is always drained. Mirrors the former
	 *  UTriangulateRoadTool::Render. */
	void RenderDebugLines(class FPrimitiveDrawInterface* PDI, bool bDrawBoundaries) const;

	/** Toggle wireframe display on every preview pipeline's mesh (global "Show Wireframe" debug option). */
	void SetWireframe(bool bEnable);

	/** Aggregated build status of the current preview (drives the toolkit's preview status icon button). */
	EMetaRoadPreviewStatus GetPreviewStatus() const { return PreviewStatus; }

	// --- Preset editing (the "Preset" Edit sub-mode) ---
	// While enabled, each previewed actor's preview is built from a transient working copy of its build
	// settings (snapshot of the real settings) instead of the actor's real holder. Editing the working
	// copy (in the Preset panel Details) re-previews without touching the actor; ApplyWorkingToSelected
	// commits the working copies into the actors' real settings.

	/** Enable/disable Preset editing for the currently previewed actors (rebuilds the preview). */
	void SetPresetEditing(bool bEnable);
	bool IsPresetEditing() const { return bPresetEditing; }

	/** The working-copy holders currently edited — shown in the Preset panel's Details view. */
	TArray<UObject*> GetWorkingSettingsObjects() const;

	/** Commit the working copies into each corresponding actor's real MetaRoadBuildSettings (+ dirty). */
	void ApplyWorkingToSelected();

	// IRoadMeshBuildHost
	virtual UWorld* GetTargetWorld() const override;
	virtual void NotifyMeshUpdated() override;
	virtual UMaterialInterface* GetWorkingMaterial() override { return nullptr; }
	virtual UMetaRoadBuildSettings* GetBuildSettingsForActor(AMetaRoad* Actor) const override;

	// UObject
	virtual void BeginDestroy() override;

private:
	/** Routed from each previewed actor's build-settings property sets (GetOnModified) — rebuilds that
	 *  actor's preview pipeline for the changed layer (mirrors UTriangulateRoadTool::OnPropertyModified). */
	void OnBuildSettingModified(UObject* PropertySet, FProperty* Property);

	/** Drop all GetOnModified subscriptions. */
	void UnsubscribeFromBuildSettings();

	/** Reconcile the manager to its current state (PreviewedActors + bPresetEditing + bMeshPreviewEnabled):
	 *  refreshes the preset working copies (independent of the view) and, only while mesh preview is enabled,
	 *  (re)builds the pipelines. Tears down the previous pipelines/subscriptions first. */
	void Reconcile();
	void TeardownPipelines();

	/** Reconcile the working copies with PreviewedActors, preserving existing copies (and their in-progress
	 *  edits) for actors still previewed — so the preset session survives Schematic<->Preview toggles. */
	void SyncWorkingCopies();
	void ClearWorkingCopies();
	void RefreshSubscriptions();

	/** Build-settings property sets we are currently listening to (real holders' or working copies'). */
	TArray<TWeakObjectPtr<UMetaRoadBuildSettingsBase>> SubscribedPropertySets;

	/** Actors currently previewed (the shared input to Reconcile — drives both the preset working copies and,
	 *  while mesh preview is enabled, the pipeline stack). */
	TArray<TWeakObjectPtr<AMetaRoad>> PreviewedActors;

	bool bPresetEditing = false;

	/** Whether the mesh-preview pipelines are currently built. Driven by the mode's view mode (true in
	 *  Preview, false in Schematic). Independent of bPresetEditing, which drives the working copies. */
	bool bMeshPreviewEnabled = false;

	/** Transient working-copy holders (one per previewed AMetaRoad) while Preset editing; index-aligned
	 *  with WorkingActors. UPROPERTY so the duplicated holders + their instanced property sets are rooted. */
	UPROPERTY()
	TArray<TObjectPtr<UMetaRoadBuildSettings>> WorkingSettings;
	TArray<TWeakObjectPtr<AMetaRoad>> WorkingActors;

	UPROPERTY()
	TWeakObjectPtr<UWorld> PreviewWorld;

	// Subscription to FMetaRoadDelegates::OnRoadComponentDirtyDelegate so spline edits auto-rebuild
	// the affected preview (mirrors UTriangulateRoadTool).
	FDelegateHandle RoadDirtyHandle;

	EMetaRoadPreviewStatus PreviewStatus = EMetaRoadPreviewStatus::Idle;
	bool bOpWasJustUpdated = false;
};
