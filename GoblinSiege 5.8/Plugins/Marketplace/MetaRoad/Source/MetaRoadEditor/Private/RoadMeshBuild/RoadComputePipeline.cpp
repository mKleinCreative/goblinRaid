/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadMeshBuild/RoadComputePipeline.h"
#include "RoadMeshBuild/IRoadMeshBuildHost.h"
#include "RoadMeshBuild/ToolPropertySets.h"
#include "RoadMeshBuild/MetaRoadBuildSettings.h"
#include "RoadMeshBuild/GenericDataBackgroundCompute.h"
#include "RoadMeshBuild/Ops/TriangulateRoadOp.h"
#include "RoadMeshBuild/RoadComputeFactoryRegistry.h"
#include "MetaRoadModule.h"
#include "MetaRoadActor.h" // AMetaRoad (TargetActor weak ptr complete type)
#include "Engine/World.h"          // UWorld::LineTraceSingleByChannel (Snap to Ground)
#include "CollisionQueryParams.h"  // FCollisionQueryParams / SCENE_QUERY_STAT

using namespace UE::Geometry;
using namespace MetaRoad;

namespace
{
	UE::Geometry::EBackgroundComputeTaskStatus GetLastComputeStatus(TGenericDataBackgroundCompute<MetaRoad::FRoadTriangulationData>& Compute)
	{
		class FAccessor : public TGenericDataBackgroundCompute<MetaRoad::FRoadTriangulationData>
		{
		public:
			UE::Geometry::EBackgroundComputeTaskStatus GetLastComputeStatus() const { return this->BackgroundCompute->CheckStatus().TaskStatus; }
		};

		return static_cast<FAccessor*>(&Compute)->GetLastComputeStatus();
	}
}

// -----------------------------------------------------------------------------------------------------
// FRoadComputePipeline
// -----------------------------------------------------------------------------------------------------

FRoadComputePipeline::FRoadComputePipeline(TWeakObjectPtr<AMetaRoad> InTargetActor, FName InSubGroup,
                                               TArray<TWeakObjectPtr<URoadSplineComponent>> InGroupSplines)
	: TargetActor(InTargetActor)
	, SubGroup(InSubGroup)
	, GroupSplines(MoveTemp(InGroupSplines))
{
}

void FRoadComputePipeline::Initialize(IRoadMeshBuildHost& Host)
{
	OwningHost = &Host;

	// Lambda factory for the base (Delaunay) operator. Runs on a background thread; captures the host
	// raw (safe by ownership — the host owns this pipeline which owns the factory) and a weak self.
	TriangulationOpFactory = MakeUnique<TLambdaGenericDataFactory<FRoadTriangulationData>>(
		[HostPtr = &Host, WeakSelf = AsWeak()]() -> TUniquePtr<TGenericDataOperator<FRoadTriangulationData>>
		{
			TUniquePtr<FRoadTriangulationOp> Op = MakeUnique<FRoadTriangulationOp>();
			auto Scope = WeakSelf.Pin();
			// Per-actor triangulation params: source from the target actor's build settings when present
			// (level preview/bake — the holder + props are created on the game thread before build), and
			// fall back to the host's for actors without a holder (e.g. the profile preview's synthetic
			// actor). Read-only here — no NewObject on this background thread.
			UMetaRoadBuildSettings* Settings = Scope ? HostPtr->GetBuildSettingsForActor(Scope->GetTargetActor()) : nullptr;
			UTriangulateRoadToolProperties* P = Settings ? Settings->Find<UTriangulateRoadToolProperties>() : nullptr;
			if (!P)
			{
				// Safety net — holders are normally ensured on the game thread before any build runs.
				P = GetMutableDefault<UTriangulateRoadToolProperties>();
			}
			Op->OverlapStrategy = P->OverlapStrategy;
			Op->OverlapRadius = P->OverlapRadius;
			Op->Params.ChordToleranceSq = P->ErrorTolerance * P->ErrorTolerance;
			Op->SidewalkCapToleranceSq = P->SidewalkCapErrorTolerance * P->SidewalkCapErrorTolerance;
			Op->Params.MinSegmentLength = P->MinSegmentLength;
			Op->Params.bSplitBySections = P->bSplitBySections;
			Op->Params.MergeSectionsAreaThreshold = P->MergeSectionsAreaThreshold * 100 * 100;
			Op->VertexSnapTol = P->VertexSnapTol;
			Op->Params.UV0ScaleFactor = P->UV0Scale;
			Op->Params.UV1ScaleFactor = P->UV1Scale;
			Op->Params.UV2ScaleFactor = P->UV2Scale;
			Op->Params.UVMaxSize = P->UVMaxSize;
			Op->bSmooth = P->bSmooth;
			Op->SmoothSpeed = P->SmoothSpeed;
			Op->Smoothness = P->Smoothness;
			// Boundaries are rendered live from the result (UMetaRoadPreviewManager::RenderDebugLines),
			// so the op never needs to fill the (now-unused) DebugDraw buffer.
			Op->bDrawBoundaries = false;
			// Scope/actor may be gone by the time this runs on a worker thread — SetActorWithRoads is null-safe.
			static const TArray<TWeakObjectPtr<URoadSplineComponent>> EmptyFilter;
			Op->SetActorWithRoads(Scope ? Scope->GetTargetActor() : nullptr,
			                      Scope ? Scope->GetGroupSplines() : EmptyFilter);
			return Op;
		});

	TriangulationCompute = MakeUnique<TGenericDataBackgroundCompute<MetaRoad::FRoadTriangulationData>>();
	TriangulationCompute->Setup(TriangulationOpFactory.Get());
	TriangulationCompute->OnResultUpdated.AddLambda(
		[this](const TUniquePtr<MetaRoad::FRoadTriangulationData>& Data)
		{
			// The delegate is owned by TriangulationCompute, itself owned by this scope, which is
			// owned by the host, so OwningHost is always valid when the callback fires.
			if (OwningHost)
			{
				OwningHost->NotifyMeshUpdated();
			}
			if (TriangulationCompute->HaveValidResult())
			{
				TriangulationResult = TSharedPtr<MetaRoad::FRoadTriangulationData>(TriangulationCompute->Shutdown().Release());
				GenerationResultInfo = TriangulationResult->ResultInfo;
				// Game-thread Snap to Ground pass — must run before the layers consume the result.
				if (TriangulationResult && !TriangulationResult->ResultInfo.HasFailed())
				{
					ApplyGroundSnap(*TriangulationResult);
				}
			}
			else
			{
				TriangulationResult.Reset();
				GenerationResultInfo = { EGeometryResultType::Failure };
			}
			for (auto& Layer : LayerComputes)
			{
				Layer->InvalidateResult();
			}
		}
	);

	auto& RoadComputeFactories = FRoadComputeFactoryRegistry::Get().GetFactories();
	LayerComputes.Reserve(RoadComputeFactories.Num());
	for (auto& [FactoryName, Factory] : RoadComputeFactories)
	{
		IRoadOpCompute* RoadCompute = Factory.Execute(&Host, AsWeak());
		check(RoadCompute);
		check(RoadCompute->AsObject()); // every IRoadOpCompute is a UObject
		TStrongScriptInterface<IRoadOpCompute> Interface;
		Interface.SetObject(RoadCompute->AsObject());
		Interface.SetInterface(RoadCompute);
		LayerComputes.Add(Interface);
	}
}

bool FRoadComputePipeline::Tick(float DeltaTime)
{
	if (bIsDirty)
	{
		bRebuildAllPending = true;
		bIsDirty = false;
	}

	TriangulationCompute->Tick(DeltaTime);

	bool bStartedRebuild = false;

	if (bRebuildAllPending)
	{
		DoRebuildAll();
		bRebuildAllPending = false;
		PendingRebuildOps.Reset();
		bStartedRebuild = true;
	}

	for (IRoadOpCompute* Op : PendingRebuildOps)
	{
		DoRebuildOne(*Op);
		bStartedRebuild = true;
	}
	PendingRebuildOps.Reset();

	for (auto& Layer : LayerComputes)
	{
		Layer->Tick(DeltaTime);
	}

	return bStartedRebuild;
}

void FRoadComputePipeline::BuildSynchronous()
{
	// Triangulation, synchronously — mirrors the OnResultUpdated path (line ~86) without Shutdown()/threads.
	TriangulationResult.Reset();
	if (TriangulationOpFactory)
	{
		TUniquePtr<TGenericDataOperator<FRoadTriangulationData>> Op = TriangulationOpFactory->MakeNewOperator();
		if (Op)
		{
			Op->CalculateResult(nullptr);
			TriangulationResult = TSharedPtr<FRoadTriangulationData>(Op->ExtractResult().Release());
		}
	}

	if (!TriangulationResult)
	{
		GenerationResultInfo = { EGeometryResultType::Failure };
		return;
	}
	GenerationResultInfo = TriangulationResult->ResultInfo;
	if (TriangulationResult->ResultInfo.HasFailed())
	{
		return;
	}

	// Game-thread Snap to Ground pass — must run before the layers consume the result.
	ApplyGroundSnap(*TriangulationResult);

	// Each layer, synchronously (reads TriangulationResult through its MakeOp closure).
	for (auto& Layer : LayerComputes)
	{
		Layer->ComputeSynchronous();
	}
}

void FRoadComputePipeline::ApplyGroundSnap(FRoadTriangulationData& Data)
{
	// Resolve the triangulation properties for this actor (mirrors the op factory's lookup in Initialize).
	UTriangulateRoadToolProperties* P = nullptr;
	if (OwningHost)
	{
		if (UMetaRoadBuildSettings* Settings = OwningHost->GetBuildSettingsForActor(GetTargetActor()))
		{
			P = Settings->Find<UTriangulateRoadToolProperties>();
		}
	}
	if (!P)
	{
		P = GetMutableDefault<UTriangulateRoadToolProperties>();
	}

	if (P->OverlapStrategy != ERoadOverlapStrategy::SnapToGround)
	{
		return;
	}

	UWorld* World = OwningHost ? OwningHost->GetTargetWorld() : nullptr;
	if (!World)
	{
		return; // e.g. the road-profile preview/thumbnail world — keep the op's fallback Z
	}

	// Ignore the road's own geometry so the downward trace can't snap the road onto itself.
	FCollisionQueryParams Query(SCENE_QUERY_STAT(MetaRoadSnapToGround), P->bSnapTraceComplex);
	if (AMetaRoad* Actor = GetTargetActor())
	{
		Query.AddIgnoredActor(Actor);
		if (AActor* Gen = Actor->LastGeneratedActor)
		{
			Query.AddIgnoredActor(Gen);
		}
	}

	const double HeightAbove = P->SnapTraceHeightAbove;
	const double DepthBelow = P->SnapTraceDepthBelow;
	const double GroundOffset = P->SnapGroundOffset;
	const ECollisionChannel Channel = P->SnapTraceChannel.GetValue();

	// Vertices are in world space — trace start/end build directly, no transform needed.
	for (FArrangementVertex3d& V : Data.Vertices3d)
	{
		const FVector Start = V.Vertex + FVector(0.0, 0.0, HeightAbove);
		const FVector End = V.Vertex - FVector(0.0, 0.0, DepthBelow);

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Start, End, Channel, Query))
		{
			V.Vertex.Z = Hit.ImpactPoint.Z + GroundOffset;
		}
		// miss -> keep the op-computed fallback Z
	}

	// Z changed -> recompute normals (+ optional smoothing) and rebuild the AABB tree on the game thread.
	Data.FinalizeVertexGeometry(P->bSmoothAfterSnap, P->SmoothSpeed, P->Smoothness, /*bRebuildSpatial*/ true, nullptr);
}

void FRoadComputePipeline::DoRebuildAll()
{
	TriangulationCompute->Cancel();

	for (auto& Layer : LayerComputes)
	{
		Layer->CancelCompute();
	}

	GenerationResultInfo = EGeometryResultType::InProgress;
	TriangulationResult.Reset();
	bNeedGenerateReport = true;
	TriangulationCompute->InvalidateResult();
}

void FRoadComputePipeline::DoRebuildOne(IRoadOpCompute& LayerCompute)
{
	bNeedGenerateReport = true;
	GenerationResultInfo = { EGeometryResultType::InProgress };
	LayerCompute.CancelCompute();
	LayerCompute.InvalidateResult();
}

void FRoadComputePipeline::HandlePropertyModified(const FProperty& Property)
{
	static const FName RebuildAll = "RebuildAll";

	if (Property.HasMetaData(RebuildAll))
	{
		RequestRebuildAll();
		return;
	}

	if (bRebuildAllPending)
	{
		return; // a full rebuild is already queued
	}

	for (auto& Layer : LayerComputes)
	{
		for (const FName& Tag : Layer->GetRebuildTags())
		{
			if (Property.HasMetaData(Tag))
			{
				PendingRebuildOps.Add(Layer.GetInterface());
				break;
			}
		}
	}
}

void FRoadComputePipeline::SetWireframe(bool bEnable)
{
	for (auto& Layer : LayerComputes)
	{
		Layer->EnableWireframe(bEnable);
	}
}

void FRoadComputePipeline::CancelAll()
{
	if (TriangulationCompute)
	{
		TriangulationCompute->Cancel();
	}
	for (auto& Layer : LayerComputes)
	{
		Layer->Cancel();
	}
}

void FRoadComputePipeline::CancelActiveComputes()
{
	// TriangulationCompute is a data compute (no preview mesh) — Cancel() just aborts the task.
	if (TriangulationCompute)
	{
		TriangulationCompute->Cancel();
	}
	// Layers MUST use CancelCompute() (aborts the background task, keeps PreviewMesh) rather than
	// Cancel() (which nulls PreviewMesh — the next Tick() would then dereference null and crash).
	for (auto& Layer : LayerComputes)
	{
		Layer->CancelCompute();
	}

	// Drop any queued rebuild + pending report so the next Tick() idles and no stale report is flushed.
	bIsDirty = false;
	bRebuildAllPending = false;
	PendingRebuildOps.Reset();
	bNeedGenerateReport = false;
}

bool FRoadComputePipeline::GenerateAssets(AActor* OutputActor, const FTransform3d& ActorToWorld)
{
	bool bAllWritesOk = true;
	for (auto& Layer : LayerComputes)
	{
		bAllWritesOk &= Layer->ShutdownAndGenerateAssets(OutputActor, ActorToWorld);
	}
	return bAllWritesOk;
}

bool FRoadComputePipeline::CanAccept() const
{
	for (auto& Layer : LayerComputes)
	{
		if (Layer->HaveValidNonEmptyResult())
		{
			return true;
		}
	}
	return false;
}

int FRoadComputePipeline::CountActiveTasks() const
{
	auto IsActive = [](EBackgroundComputeTaskStatus Status)
	{
		return Status != EBackgroundComputeTaskStatus::NotComputing;
	};

	int Count = 0;
	if (TriangulationCompute && IsActive(GetLastComputeStatus(*TriangulationCompute)))
	{
		++Count;
	}
	for (auto& Layer : LayerComputes)
	{
		if (IsActive(Layer->GetLastComputeStatus()))
		{
			++Count;
		}
	}
	return Count;
}

bool FRoadComputePipeline::TryFlushReport()
{
	if (bNeedGenerateReport && CountActiveTasks() == 0)
	{
		ShowReport();
		bNeedGenerateReport = false;
		return true;
	}
	return false;
}

void FRoadComputePipeline::AppendResultInfo(const FGeometryResult& InResult)
{
	GenerationResultInfo.Errors.Append(InResult.Errors);
	GenerationResultInfo.Warnings.Append(InResult.Warnings);
	GenerationResultInfo.Result = FMath::Max(GenerationResultInfo.Result, InResult.Result);
}

void FRoadComputePipeline::ShowReport() const
{
	if (!TargetActor.IsValid()) return;

	int StatNumTriangles = 0;
	int StatNewVertices = 0;
	auto AddStat = [&StatNumTriangles, &StatNewVertices](const IRoadOpCompute& RoadOpCompute)
	{
		if (RoadOpCompute.HaveValidNonEmptyResult())
		{
			StatNewVertices += RoadOpCompute.GetNumVertices();
			StatNumTriangles += RoadOpCompute.GetNumTriangles();
		}
	};

	for (auto& It : LayerComputes)
	{
		AddStat(*It);
	}

	UE_LOG(LogMetaRoad, Log, TEXT("----------------- Generation Report: %s ----------------"), *TargetActor->GetActorLabel());
	UE_LOG(LogMetaRoad, Log, TEXT("\t\t Num triangles: %i"), StatNumTriangles);
	UE_LOG(LogMetaRoad, Log, TEXT("\t\t Num vertices: %i"), StatNewVertices);
	if (GenerationResultInfo.Errors.Num())
	{
		UE_LOG(LogMetaRoad, Log,  TEXT("\t\t Error messages: "));
		for (auto& It : GenerationResultInfo.Errors)
		{
			UE_LOG(LogMetaRoad, Log, TEXT("\t\t\t %s"), *It.Message.ToString());
		}
	}
	if (GenerationResultInfo.Warnings.Num())
	{
		UE_LOG(LogMetaRoad, Log, TEXT("\t\t Warning messages: "));
		for (auto& It : GenerationResultInfo.Warnings)
		{
			UE_LOG(LogMetaRoad, Log, TEXT("\t\t\t %s"), *It.Message.ToString());
		}
	}
}
