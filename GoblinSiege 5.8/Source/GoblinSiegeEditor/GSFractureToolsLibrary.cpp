#include "GSFractureToolsLibrary.h"
#include "Engine/StaticMesh.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionEngineConversion.h"
#include "GeometryCollection/GeometryCollectionConvexUtility.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "FractureEngineFracturing.h"
#include "FractureEngineClustering.h"
#include "Dataflow/DataflowSelection.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/ARFilter.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSFracture, Log, All);

namespace
{
	// SM_MERGED_House_Small_03 -> GC_MERGED_House_Small_03 - the identical convention
	// AGSBuildingObjective::SpawnCollectionProxy already looks up by (GSBuildingObjective.cpp).
	// Duplicated rather than shared: that function is private to a runtime gameplay class, this is an
	// editor-only tool in a different module, and the naming rule is small enough that keeping them
	// independently readable beats a cross-module dependency for one string transform.
	FString MeshNameToCollectionAssetName(const FString& MeshName)
	{
		FString Base = MeshName;
		if (Base.StartsWith(TEXT("SM_")))
		{
			Base.RightChopInline(3);
		}
		return FString::Printf(TEXT("GC_%s"), *Base);
	}
}

UGeometryCollection* UGSFractureToolsLibrary::GenerateFractureAsset(UStaticMesh* SourceMesh,
	const FString& DestFolder, int32 NumVoronoiCells, bool bOverwriteExisting, int32 MaxSimulatedPieces,
	float MaxPieceReachRatio)
{
	if (!SourceMesh)
	{
		UE_LOG(LogGSFracture, Warning, TEXT("[GS.Fracture] GenerateFractureAsset called with a null mesh."));
		return nullptr;
	}

	const FString AssetName = MeshNameToCollectionAssetName(SourceMesh->GetName());
	const FString PackageName = DestFolder / AssetName;

	const FString ObjectPath = PackageName + TEXT(".") + AssetName;

	if (!bOverwriteExisting)
	{
		if (UGeometryCollection* Existing = LoadObject<UGeometryCollection>(nullptr, *ObjectPath))
		{
			return Existing;
		}
	}

	// Build directly on the destination asset via AppendStaticMesh, NOT the standalone
	// ConvertStaticMeshToGeometryCollection + manual NewAsset->Materials assignment the first version
	// of this function used. That version shipped a real bug, watched live (2026-08-31): every piece
	// rendered with a scrambled, effectively-random material from the array instead of its own -
	// ConvertStaticMeshToGeometryCollection builds its OWN per-face MaterialID indexing scheme, and
	// blindly assigning its OutMaterialInstances to NewAsset->Materials afterward does not reproduce
	// whatever internal-material/interior-face bookkeeping the asset's OWN population path expects.
	// AppendStaticMesh takes the destination UGeometryCollection directly and keeps both in sync by
	// construction - bAddInternalMaterials=true is what gives freshly-cut interior faces (created by
	// the fracture step below) their own correct material instead of inheriting a neighbour's.
	//
	// bOverwriteExisting on an asset that ALREADY has a .uasset on disk must NOT go through
	// CreatePackage(*PackageName). CreatePackage's own name lookup (FindObject<UPackage> before it
	// constructs anything new) hands back whatever UPackage of that name is already resident, and a
	// package that reaches this function's CreatePackage call via anything other than a completed
	// LoadPackage - the Asset Registry's header-only gather scan for thumbnails/tags is one, a
	// not-yet-collected package from a caller that just deleted the old asset is another - never had
	// UPackage::MarkAsFullyLoaded() called on it. UPackage::IsFullyLoaded() (Package.cpp) is a sticky
	// per-instance flag, not a recomputed fact, and it falls back to "does a .uasset with this name
	// exist on disk" when the flag is unset - true here, since we're regenerating - so SavePackage's
	// pre-save validation hits "cannot be saved as it has only been partially loaded" and fatally
	// errors (appError, not a catchable exception) every time. Confirmed live in headless
	// -ExecutePythonScript regeneration passes (2026-09): reproduced identically whether or not the
	// caller pre-deleted the asset and force-GC'd first, because deletion racing the Asset Registry's
	// background gather is exactly the scenario above, not the fix for it.
	//
	// The fix: LoadObject the existing asset for real (LinkerLoad's completed-load path is one of the
	// two call sites that actually flips MarkAsFullyLoaded - CreatePackage is never one of them), then
	// mutate that already-fully-loaded package/object IN PLACE instead of creating a new one. No
	// delete, no CreatePackage, no GC-timing race.
	UPackage* Package = nullptr;
	UGeometryCollection* NewAsset = nullptr;
	if (bOverwriteExisting)
	{
		NewAsset = LoadObject<UGeometryCollection>(nullptr, *ObjectPath);
	}

	if (NewAsset)
	{
		Package = NewAsset->GetOutermost();
	}
	else
	{
		Package = CreatePackage(*PackageName);
		NewAsset = NewObject<UGeometryCollection>(Package, FName(*AssetName), RF_Public | RF_Standalone);
	}
	NewAsset->SetGeometryCollection(MakeShared<FGeometryCollection, ESPMode::ThreadSafe>());

	TArray<UMaterialInterface*> MeshMaterials;
	for (const FStaticMaterial& Mat : SourceMesh->GetStaticMaterials())
	{
		MeshMaterials.Add(Mat.MaterialInterface);
	}

	const bool bAppended = FGeometryCollectionEngineConversion::AppendStaticMesh(
		SourceMesh, MeshMaterials, FTransform::Identity, NewAsset,
		/*bReindexMaterials*/ true, /*bAddInternalMaterials*/ true,
		/*bSplitComponents*/ false, /*bSetInternalFromMaterialIndex*/ false);

	TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Collection = NewAsset->GetGeometryCollection();
	const int32 NumTransforms = bAppended && Collection.IsValid()
		? Collection->NumElements(FGeometryCollection::TransformGroup) : 0;
	if (NumTransforms <= 0)
	{
		UE_LOG(LogGSFracture, Warning,
			TEXT("[GS.Fracture] %s produced no geometry on conversion - nothing to fracture."),
			*SourceMesh->GetName());
		return nullptr;
	}

	// Fracture: select the whole mesh, cut it into NumVoronoiCells pieces. The same call
	// FUniformFractureDataflowNode makes - DF_GS_HouseFracture (the Dataflow graph behind the one
	// hand-made GC_MERGED_House_Small_03) already wraps exactly this node.
	FDataflowTransformSelection Selection;
	Selection.Initialize(NumTransforms, true);

	FUniformFractureSettings Settings;
	Settings.Transform = FTransform::Identity;
	Settings.MinVoronoiSites = NumVoronoiCells;
	Settings.MaxVoronoiSites = NumVoronoiCells;
	Settings.InternalMaterialID = 0;
	Settings.RandomSeed = FMath::Rand();
	Settings.ChanceToFracture = 1.f;
	Settings.GroupFracture = true;
	Settings.SplitIslands = true;
	Settings.Grout = 0.f;
	Settings.AddSamplesForCollision = false;
	Settings.CollisionSampleSpacing = 50.f;
	// NoiseSettings left default-constructed - a straight Voronoi cut, no surface noise, for a first
	// pass. Michael can re-run with different NumVoronoiCells (this function's own parameter) per
	// mesh if a specific house needs a different look; that is a rerun, not a code change.

	FFractureEngineFracturing::UniformFracture(*Collection, Selection, Settings);

	if (Collection->NumElements(FGeometryCollection::TransformGroup) <= 1)
	{
		UE_LOG(LogGSFracture, Warning,
			TEXT("[GS.Fracture] %s fractured into only 1 piece - UniformFracture may need different ")
			TEXT("Voronoi site counts for this mesh's size/scale. Asset NOT saved (a 1-piece collection ")
			TEXT("is the exact stub AGSBuildingObjective::SpawnCollectionProxy already refuses)."),
			*SourceMesh->GetName());
		return nullptr;
	}

	// Cap simulated rigid bodies, NOT visual piece count (Michael, live playtest, 2026-09-01: "it's
	// really laggy right now" - measured via PerformanceService.frame_timing() as game-thread bound,
	// 182ms/frame at 5.5 FPS, plus an on-screen "VSM Nanite Marking Job Queue overflow" warning).
	// `SplitIslands` above multiplies leaf-piece count by however many disconnected chunks land in
	// each Voronoi cell, independent of NumVoronoiCells - real houses came out at 119-686 leaf pieces
	// from a NumVoronoiCells of 8, and every leaf is an independent Chaos rigid body the instant
	// CrumblePieces releases it. AutoCluster groups those leaves under MaxSimulatedPieces top-level
	// parents (ByNumber - a flat count regardless of mesh complexity, so a 686-piece house and a
	// 119-piece house both simulate the same number of bodies), which is what UGSCrumbleComponent's
	// own cluster-shove logic already expects to operate on - it was designed for a clustered
	// hierarchy, not hundreds of ungrouped leaves. IsGeometry() is the standard leaf test
	// (TransformToGeometryIndex != INDEX_NONE) - only fracture RESULTS get clustered, not the (already
	// non-leaf) transforms UniformFracture may have created for its own bookkeeping.
	TArray<int32> LeafIndices;
	const int32 NumTransformsAfterFracture = Collection->NumElements(FGeometryCollection::TransformGroup);
	for (int32 Index = 0; Index < NumTransformsAfterFracture; ++Index)
	{
		if (Collection->IsGeometry(Index))
		{
			LeafIndices.Add(Index);
		}
	}
	const int32 LeafPieceCount = LeafIndices.Num();

	if (MaxSimulatedPieces > 0 && LeafPieceCount > MaxSimulatedPieces)
	{
		FFractureEngineClustering::AutoCluster(*Collection, LeafIndices,
			EFractureEngineClusterSizeMethod::ByNumber,
			/*SiteCount*/ static_cast<uint32>(MaxSimulatedPieces), /*SiteCountFraction*/ 0.f,
			/*SiteSize*/ 0.f, /*bEnforceConnectivity*/ true, /*bAvoidIsolated*/ true,
			/*bEnforceSiteParameters*/ false);
	}

	// Collision: NOT generated by conversion or fracturing - confirmed live (2026-08-31), the first
	// version of this function never called anything from GeometryCollectionConvexUtility.h and every
	// resulting proxy had zero collision ("it has no collision, so I can just walk through it").
	// CreateNonOverlappingConvexHullData builds a convex hull per leaf piece, which is what
	// AGSBuildingObjective::SpawnCollectionProxy's "Destructible" collision profile needs something to
	// actually collide with.
	FGeometryCollectionConvexUtility::CreateNonOverlappingConvexHullData(Collection.Get());

	// ---- VALIDATE BEFORE SAVING (2026-09-09, #404) ------------------------------------------
	//
	// This tool can produce a geometrically corrupt collection, and it did: GC_MERGED_House_Medium_07
	// shipped with rest transforms 270,634 uu and 144,992 uu from a house whose source mesh measures
	// 2336 x 2588 x 2128. Chaos then solved constraints spanning kilometres between pieces of one
	// building and the accumulated impulse went NaN, which is ticket #399 - an ensure storm eight days
	// after the asset was made, with nothing pointing back to here.
	//
	// It is NOT deterministic. The same source mesh and the same parameters produced a corrupt asset
	// on 2026-09-01 and a clean one on 2026-09-09, so Voronoi site placement is the variable and
	// roughly 1 in 40 of that batch came out wrong. Every future regeneration is another roll, and
	// Content/Destruction is gitignored (these assets are regenerable by design), so there is no
	// committed copy to fall back on.
	//
	// #398 verified an 84-asset batch by file size, non-crashing and a successful repackage, and
	// concluded the assets were "structurally valid". They were - a corrupt-geometry collection
	// loads, cooks, packages and ships perfectly well. None of those checks looks at WHERE the pieces
	// are. This one does, and it is the only thing standing between a bad roll and another #399.
	{
		// MEASURE THE VERTICES, NOT THE TRANSFORMS.
		//
		// Two wrong versions preceded this one, and both reported success while measuring nothing:
		// GeometryCollectionAlgo::GlobalMatrices returned an EMPTY array here, and the raw Transform
		// array turned out to be 740 identity transforms. A freshly fractured collection keeps every
		// bone at the origin and puts the geometry in the vertex positions; the per-piece transforms
		// seen at runtime are derived later by the physics proxy. So the vertices are the only place
		// the corruption can actually be seen at this point in the pipeline.
		//
		// This is also why #399 was invisible for eight days: every check anyone ran - file size,
		// non-crashing, a successful cook and package - looked at the asset as a file rather than at
		// where its geometry sits.
		const TManagedArray<FVector3f>& Verts = Collection->Vertex;

		double WorstReach = 0.0;
		int32 WorstIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Verts.Num(); ++Index)
		{
			const FVector V(Verts[Index]);
			if (V.ContainsNaN())
			{
				WorstReach = TNumericLimits<double>::Max();
				WorstIndex = Index;
				break;
			}
			const double Reach = V.GetAbs().GetMax();
			if (Reach > WorstReach)
			{
				WorstReach = Reach;
				WorstIndex = Index;
			}
		}

		// The source mesh is the ground truth for how big this thing is allowed to be. A fracture
		// piece belongs inside the mesh it was cut from; some slack for pivot offsets and for pieces
		// whose origin sits at a corner, but not orders of magnitude.
		const double SourceReach = FMath::Max(1.0,
			static_cast<double>(SourceMesh->GetBoundingBox().GetExtent().GetMax()));
		const double Allowed = SourceReach * FMath::Max(1.0, static_cast<double>(MaxPieceReachRatio));

		// Always logged, not just on refusal: the first version of this gate measured the wrong thing
		// and passed a deliberately impossible ratio of 1.0 without a word. A validator that cannot
		// be seen working is indistinguishable from one that does nothing.
		UE_LOG(LogGSFracture, Log,
			TEXT("[GS.Fracture] %s reach check: worst vertex %.0f uu (index %d), source reach %.0f uu, ")
			TEXT("ratio %.2fx, limit %.0fx, over %d vertices."),
			*AssetName, WorstReach, WorstIndex, SourceReach,
			SourceReach > 0.0 ? WorstReach / SourceReach : 0.0, MaxPieceReachRatio, Verts.Num());

		if (WorstReach > Allowed)
		{
			UE_LOG(LogGSFracture, Error,
				TEXT("[GS.Fracture] REFUSED to save %s: vertex %d sits %.0f uu from the origin, but the ")
				TEXT("source mesh %s only reaches %.0f uu (limit %.0fx = %.0f). This is the corrupt-")
				TEXT("geometry failure from #399 - Chaos will NaN on it. Nothing was written; the ")
				TEXT("previous asset, if any, is untouched. Re-run to roll again: the fracture is ")
				TEXT("non-deterministic and a repeat usually succeeds."),
				*AssetName, WorstIndex, WorstReach, *SourceMesh->GetName(), SourceReach,
				MaxPieceReachRatio, Allowed);
			return nullptr;
		}
	}

	NewAsset->InitializeMaterials();
	NewAsset->RebuildRenderData();

	FAssetRegistryModule::AssetCreated(NewAsset);
	Package->MarkPackageDirty();

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	const FString PackageFileName =
		FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	UPackage::SavePackage(Package, NewAsset, *PackageFileName, SaveArgs);

	UE_LOG(LogGSFracture, Log,
		TEXT("[GS.Fracture] Generated %s from %s: %d visual pieces, clustered under %d simulated body(ies)."),
		*AssetName, *SourceMesh->GetName(), LeafPieceCount, FMath::Min(LeafPieceCount, MaxSimulatedPieces > 0 ? MaxSimulatedPieces : LeafPieceCount));

	return NewAsset;
}

int32 UGSFractureToolsLibrary::BulkGenerateMissingBuildingFractures(
	const FString& SourceFolder, const FString& DestFolder, const FString& NamePrefix,
	int32 NumVoronoiCells, int32 MaxSimulatedPieces, float MaxPieceReachRatio)
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*SourceFolder));
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());

	TArray<FAssetData> AssetDatas;
	AssetRegistry.GetAssets(Filter, AssetDatas);

	int32 Generated = 0;
	int32 Skipped = 0;
	for (const FAssetData& AssetData : AssetDatas)
	{
		const FString MeshName = AssetData.AssetName.ToString();
		if (!MeshName.StartsWith(NamePrefix))
		{
			continue;
		}

		const FString CollectionAssetName = MeshNameToCollectionAssetName(MeshName);
		const FString CollectionPackageName = DestFolder / CollectionAssetName;
		if (FPackageName::DoesPackageExist(CollectionPackageName))
		{
			// Idempotent on purpose - this is what makes it safe to re-run whenever new house meshes
			// are added, rather than a one-shot script that would clobber hand-tuned fractures.
			++Skipped;
			continue;
		}

		UStaticMesh* Mesh = Cast<UStaticMesh>(AssetData.GetAsset());
		if (!Mesh)
		{
			continue;
		}

		if (GenerateFractureAsset(Mesh, DestFolder, NumVoronoiCells, /*bOverwriteExisting*/ false, MaxSimulatedPieces, MaxPieceReachRatio))
		{
			++Generated;
		}
	}

	UE_LOG(LogGSFracture, Log,
		TEXT("[GS.Fracture] Bulk pass over '%s' (prefix '%s'): generated %d new fracture asset(s), ")
		TEXT("%d already had one."),
		*SourceFolder, *NamePrefix, Generated, Skipped);

	return Generated;
}
