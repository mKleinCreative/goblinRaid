// Bulk Chaos fracture generation (#393).
//
// WHY THIS EXISTS: only 1 of 42 `SM_MERGED_House_*` meshes had a matching `GC_<name>` fracture asset
// (see AGSBuildingObjective::SpawnCollectionProxy) - authoring the other 41 by hand in Fracture
// Mode's UI is real per-asset work (~2-5 minutes each, ~1.5-3.5 hours total) with no Python surface
// to drive it (unreal.FractureEditorLibrary does not exist; UGeometryCollection/UDataflow expose no
// Evaluate/Rebuild entry point for a Dataflow graph, only SetDataflowAsset). Michael asked for this
// to be a process, not a one-off - runnable again whenever new house meshes get added, not just a
// fix for today's 41.
//
// This bypasses the Editor UI/Dataflow-graph surface entirely and calls the underlying C++ the
// Dataflow nodes themselves wrap: FGeometryCollectionEngineConversion::
// ConvertStaticMeshToGeometryCollection (StaticMesh -> a one-piece FGeometryCollection, the same
// call FStaticMeshToCollectionDataflowNode_v2 makes) then FFractureEngineFracturing::UniformFracture
// (the same call FUniformFractureDataflowNode makes - DF_GS_HouseFracture, the Dataflow graph behind
// the one hand-made GC_MERGED_House_Small_03, uses exactly this node). Both are real, exported,
// linkable entry points confirmed by reading the installed engine's headers directly - not a
// guess - see GoblinSiegeEditor.Build.cs for the module dependency reasoning.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSFractureToolsLibrary.generated.h"

class UStaticMesh;
class UGeometryCollection;

UCLASS()
class UGSFractureToolsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Converts SourceMesh into a fractured UGeometryCollection and saves it as GC_<mesh name minus
	 * SM_ prefix> under DestFolder - the exact naming convention AGSBuildingObjective::
	 * SpawnCollectionProxy already looks up by.
	 *
	 * Returns the existing asset unchanged if one is already there and bOverwriteExisting is false -
	 * this is the property that makes BulkGenerateMissingBuildingFractures below idempotent and safe
	 * to re-run.
	 *
	 * MaxSimulatedPieces caps how many TOP-LEVEL rigid bodies the result ever simulates at once
	 * (via FFractureEngineClustering::AutoCluster, run after fracturing) - NOT how many pieces are
	 * fractured. `SplitIslands` in the Voronoi fracture step multiplies piece count by however many
	 * disconnected chunks fall inside each Voronoi cell, independent of NumVoronoiCells, so a real
	 * house produced 119-686 leaf pieces from a NumVoronoiCells of 8 (measured live, 2026-08-31/
	 * 2026-09-01). Every leaf piece is an independent Chaos rigid body once released - at those counts,
	 * one collapsing house alone was enough to drop a live PIE session to ~5 FPS (confirmed via
	 * PerformanceService.frame_timing(): game-thread bound, 182ms frame, 165ms over the 60 FPS budget)
	 * and to trigger an on-screen "VSM Nanite Marking Job Queue overflow" warning from the sheer count
	 * of individually-rendered pieces. Clustering leaves the visual piece count untouched but groups
	 * them under a small number of simulated parents, which is what actually bounds the physics/
	 * rendering cost.
	 *
	 * MaxPieceReachRatio is a SANITY GATE on the result, not a tuning knob. The generated collection
	 * is measured against the source mesh before it is saved, and refused if any piece sits further
	 * from the origin than the source's own extent x this. Fracturing is non-deterministic and can
	 * produce a geometrically corrupt collection - GC_MERGED_House_Medium_07 came out with pieces
	 * 270,634 uu from a house whose source extent is 1,294 uu, roughly 209x, and Chaos NaN'd on it
	 * eight days later (#399) with nothing pointing back here.
	 *
	 * The check measures VERTEX positions against the source mesh's extent, because a freshly
	 * fractured collection keeps every bone transform at the origin and puts the geometry in the
	 * vertices - two earlier versions of this gate measured transforms instead and silently passed a
	 * deliberately impossible ratio of 1.0 while reporting success.
	 *
	 * Measured: a healthy result comes out at **1.25x**, and the corrupt one would have been ~231x.
	 * 4 is therefore wide enough never to fire on a good asset and tight enough to catch that by two
	 * orders of magnitude. Verified both ways - at a limit of 1x the same healthy asset is refused
	 * and nothing is written; at 4x it saves. On a refusal any existing asset is left untouched;
	 * re-run to roll again, since the fracture is non-deterministic.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "GoblinSiege|Fracture")
	static UGeometryCollection* GenerateFractureAsset(UStaticMesh* SourceMesh,
		const FString& DestFolder = TEXT("/Game/Destruction"),
		int32 NumVoronoiCells = 8,
		bool bOverwriteExisting = false,
		int32 MaxSimulatedPieces = 24,
		float MaxPieceReachRatio = 4.f);

	/**
	 * Scans SourceFolder (recursive) for UStaticMesh assets whose name starts with NamePrefix, and
	 * calls GenerateFractureAsset on every one that has no matching GC_ asset in DestFolder yet.
	 *
	 * THE reusable process Michael asked for (2026-08-31): re-running this after new house meshes are
	 * added only generates fractures for the NEW ones - existing GC_ assets (hand-tuned or previously
	 * generated) are left alone. Returns the number of NEW assets generated.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "GoblinSiege|Fracture")
	static int32 BulkGenerateMissingBuildingFractures(
		const FString& SourceFolder = TEXT("/Game"),
		const FString& DestFolder = TEXT("/Game/Destruction"),
		const FString& NamePrefix = TEXT("SM_MERGED_House_"),
		int32 NumVoronoiCells = 8,
		int32 MaxSimulatedPieces = 24,
		float MaxPieceReachRatio = 4.f);
};
