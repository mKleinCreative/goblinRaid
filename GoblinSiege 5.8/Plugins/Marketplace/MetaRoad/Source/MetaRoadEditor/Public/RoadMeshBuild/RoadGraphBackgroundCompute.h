/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "BackgroundModelingComputeSource.h"
#include "ModelingOperators.h"
#include "RoadGraphComponent.h"
#include "IRoadOpCompute.h"
#include "RoadMeshBuild/RoadComputePipeline.h"
#include "RoadMeshBuild/IRoadMeshBuildHost.h"
#include "UObject/WeakInterfacePtr.h"
#include "RoadGraphBackgroundCompute.generated.h"

/**
 * URoadGraphBackgroundCompute
 *
 * UObject IRoadOpCompute implementation for the road-graph data operator: produces an FRoadGraphScope
 * (ZoneGraph navigation data) rather than a preview mesh. Wraps TBackgroundModelingComputeSource and
 * exposes an OnShutdownAndGenerateAssets multicast delegate consumed on the Accept path.
 *
 * Created with NewObject and held as TStrongScriptInterface<IRoadOpCompute> inside
 * FRoadComputePipeline::LayerComputes[] — GC-rooted, no manual lifetime management.
 *
 * Use SetupWithOwnedFactory(): it takes a TUniquePtr<FactoryType>, stores it in OwnedFactory, then
 * calls Setup() with the raw pointer — guaranteeing the factory outlives the compute source.
 */
UCLASS(Transient)
class METAROADEDITOR_API URoadGraphBackgroundCompute : public UObject, public IRoadOpCompute
{
	GENERATED_BODY()

public:
	using OperatorType      = UE::Geometry::TGenericDataOperator<FRoadGraphScope>;
	using FactoryType       = UE::Geometry::IGenericDataOperatorFactory<FRoadGraphScope>;
	using ComputeSourceType = UE::Geometry::TBackgroundModelingComputeSource<OperatorType, FactoryType>;

	/** Low-level setup — caller must ensure OpGenerator outlives this object. Prefer SetupWithOwnedFactory(). */
	void Setup(IRoadMeshBuildHost* InHost, FactoryType* OpGenerator, TWeakPtr<MetaRoad::FRoadComputePipeline> InRoadComputePipeline)
	{
		check(InHost);
		check(OpGenerator != nullptr);
		check(InRoadComputePipeline != nullptr);

		Host = InHost;
		RoadComputePipeline = InRoadComputePipeline;
		BackgroundCompute = MakeUnique<ComputeSourceType>(OpGenerator);
		bResultValid = false;
	}

	/** Preferred setup path. Takes ownership of InFactory and keeps the raw pointer valid for the compute source. */
	void SetupWithOwnedFactory(IRoadMeshBuildHost* InHost, TUniquePtr<FactoryType>&& InFactory, TWeakPtr<MetaRoad::FRoadComputePipeline> InRoadComputePipeline)
	{
		OwnedFactory = MoveTemp(InFactory);
		Setup(InHost, OwnedFactory.Get(), InRoadComputePipeline);
	}

	virtual bool ShutdownAndGenerateAssets(AActor* TargetActor, const FTransform3d& ActorToWorld) override
	{
		BackgroundCompute->CancelActiveCompute();
		OnShutdownAndGenerateAssets.Broadcast(TargetActor, ActorToWorld, CurrentResult);
		return true; // road-graph build does not write standalone assets that can fail here
	}

	virtual void InvalidateResult() override
	{
		check(BackgroundCompute);
		if (BackgroundCompute)
		{
			BackgroundCompute->NotifyActiveComputeInvalidated();
		}
		bResultValid = false;
	}

	virtual void CancelCompute() override
	{
		if (BackgroundCompute)
		{
			BackgroundCompute->CancelActiveCompute();
		}
	}

	virtual void Tick(float DeltaTime) override
	{
		if (BackgroundCompute)
		{
			BackgroundCompute->Tick(DeltaTime);
		}
		UpdateResults();
	}

	// Synchronous build path (used by Bake / thumbnail): run the operator on the calling thread and store
	// the result directly, so ShutdownAndGenerateAssets has a valid FRoadGraphScope (the async Tick path
	// is otherwise never driven during a synchronous BuildAllSynchronous).
	virtual void ComputeSynchronous() override
	{
		if (!OwnedFactory)
		{
			return;
		}
		TUniquePtr<OperatorType> Op = OwnedFactory->MakeNewOperator();
		if (!Op)
		{
			return;
		}
		Op->CalculateResult(nullptr);
		CurrentResult = Op->ExtractResult();
		bResultValid = CurrentResult.IsValid();
		LastComputeStatus = UE::Geometry::EBackgroundComputeTaskStatus::ValidResultAvailable;
	}

	virtual void Cancel() override
	{
		BackgroundCompute->CancelActiveCompute();
	}

	virtual bool HaveValidNonEmptyResult() const override
	{
		return bResultValid;
	}

	virtual UE::Geometry::EBackgroundComputeTaskStatus GetLastComputeStatus() const override
	{
		return LastComputeStatus;
	}

	virtual void EnableWireframe(bool bEnable) override {}
	virtual void SetVisibility(bool bVisible) override {}
	virtual bool IsRoadAttribute() const override { return true; }
	virtual int GetNumVertices() const override { return 0; }
	virtual int GetNumTriangles() const override { return 0; }
	virtual UObject* AsObject() override { return this; }
	virtual TSet<FName>& GetRebuildTags() override { return RebuildTags; }
	virtual const TSet<FName>& GetRebuildTags() const override { return RebuildTags; }

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnShutdownAndGenerateAssets, AActor*, const FTransform3d&, TUniquePtr<FRoadGraphScope>&);
	FOnShutdownAndGenerateAssets OnShutdownAndGenerateAssets;

protected:
	/** Weak interface reference to the build host (weak by design — the host owns this compute). */
	TWeakInterfacePtr<IRoadMeshBuildHost> Host;
	TWeakPtr<MetaRoad::FRoadComputePipeline> RoadComputePipeline;

	// state flag, if true then we have valid result
	bool bResultValid = false;

	// current result value
	TUniquePtr<FRoadGraphScope> CurrentResult;

	TUniquePtr<FactoryType> OwnedFactory;
	TUniquePtr<ComputeSourceType> BackgroundCompute;

	UE::Geometry::EBackgroundComputeTaskStatus LastComputeStatus = UE::Geometry::EBackgroundComputeTaskStatus::NotComputing;

	TSet<FName> RebuildTags;

	// update CurrentResult if a new result is available from BackgroundCompute, and fire relevant signals
	void UpdateResults()
	{
		if (BackgroundCompute == nullptr)
		{
			LastComputeStatus = UE::Geometry::EBackgroundComputeTaskStatus::NotComputing;
			return;
		}

		LastComputeStatus = BackgroundCompute->CheckStatus().TaskStatus;
		if (LastComputeStatus == UE::Geometry::EBackgroundComputeTaskStatus::ValidResultAvailable)
		{
			TUniquePtr<OperatorType> ResultOp = BackgroundCompute->ExtractResult();
			CurrentResult = ResultOp->ExtractResult();
			bResultValid = true;

			OnResultUpdated();
		}
	}

	void OnResultUpdated()
	{
		if (Host.IsValid())
		{
			Host->NotifyMeshUpdated();
		}
	}
};
