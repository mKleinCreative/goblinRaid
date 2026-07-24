/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadMeshBuild/RoadComputeFactoryRegistry.h"
#include "RoadMeshBuild/IRoadMeshBuildHost.h"
#include "RoadMeshBuild/IRoadOpCompute.h"
#include "RoadMeshBuild/MetaRoadBuildSettings.h"
#include "RoadMeshBuild/RoadComputePipeline.h"
#include "RoadMeshBuild/RoadMeshOpHelper.h"
#include "RoadMeshBuild/SplineMeshOpHelpers.h"
#include "RoadMeshBuild/GenericDataBackgroundCompute.h"
#include "RoadMeshBuild/RoadGraphBackgroundCompute.h"
#include "RoadMeshBuild/ToolPropertySets.h"
#include "RoadMeshBuild/Ops/TriangulateRoadOp.h"
#include "Utils/AssetUtils.h"
#include "GameFramework/Actor.h"

using namespace UE::Geometry;

FRoadComputeFactoryRegistry& FRoadComputeFactoryRegistry::Get()
{
	static FRoadComputeFactoryRegistry Instance;
	return Instance;
}

void FRoadComputeFactoryRegistry::Register(FName FactoryName, FRoadComputeFactory&& Factory)
{
	Factories.Add(FactoryName, MoveTemp(Factory));
}

void FRoadComputeFactoryRegistry::Unregister(FName FactoryName)
{
	Factories.Remove(FactoryName);
}

namespace
{
	class FDynamicMeshOperatorDummy : public UE::Geometry::FDynamicMeshOperator
	{
		virtual void CalculateResult(FProgressCancel* Progress) override { SetResultInfo(FGeometryResult::Cancelled()); }
	};

	class FSplineMeshOperatorDummy : public MetaRoad::FSplineMeshOperator
	{
		virtual void CalculateResult(FProgressCancel* Progress) override { SetResultInfo(FGeometryResult::Cancelled()); }
	};

	class FRoadGraphOperatorDummy : public UE::Geometry::TGenericDataOperator<FRoadGraphScope>
	{
		virtual void CalculateResult(FProgressCancel* Progress) override { SetResult({}); }
	};

	URoadMeshOpPreviewWithBackgroundCompute* MakeMeshCompute(
		IRoadMeshBuildHost* Host,
		TWeakPtr<MetaRoad::FRoadComputePipeline> ScopeWeak,
		UMetaRoadBuildSettingsBase* Props,
		const TCHAR* AssetBaseName,
		TSet<FName> Tags,
		TFunction<TUniquePtr<UE::Geometry::FDynamicMeshOperator>()> MakeOp)
	{
		auto* Compute = NewObject<URoadMeshOpPreviewWithBackgroundCompute>(Host->GetHostObject());
		FString AssetName = AssetBaseName;
		if (auto Pin = ScopeWeak.Pin(); Pin && !Pin->GetSubGroup().IsNone())
			AssetName += TEXT("_") + Pin->GetSubGroup().ToString();
		Compute->BaseAssetName = AssetName;
		Compute->PropertySet = Props;
		Compute->RebuildTags = MoveTemp(Tags);
		Compute->Setup(Host, ScopeWeak, MoveTemp(MakeOp));
		return Compute;
	}

	USplineMeshOpPreviewWithBackgroundCompute* MakeSplineMeshCompute(
		IRoadMeshBuildHost* Host,
		TWeakPtr<MetaRoad::FRoadComputePipeline> ScopeWeak,
		UMetaRoadBuildSettingsBase* Props,
		TSet<FName> Tags,
		TFunction<TUniquePtr<MetaRoad::FSplineMeshOperator>()> MakeOp)
	{
		auto* Compute = NewObject<USplineMeshOpPreviewWithBackgroundCompute>(Host->GetHostObject());
		Compute->PropertySet = Props;
		Compute->GetRebuildTags().Append(Tags);
		Compute->Setup(Host, ScopeWeak, MoveTemp(MakeOp));
		return Compute;
	}

	// Resolve the layer property set for a factory from the scope's target actor's build settings holder,
	// obtained via the host (UMetaRoadPreviewManager returns the Preset working copy when editing; every
	// host ensures its actors have a holder on the game thread before the build). Runs at pipeline-init
	// time on the game thread, so creating the set here is safe.
	template<typename PropsType>
	PropsType* GetActorPropertySet(IRoadMeshBuildHost* Host, const TWeakPtr<MetaRoad::FRoadComputePipeline>& ScopeWeak)
	{
		if (TSharedPtr<MetaRoad::FRoadComputePipeline> Pin = ScopeWeak.Pin())
		{
			if (UMetaRoadBuildSettings* Settings = Host->GetBuildSettingsForActor(Pin->GetTargetActor()))
			{
				return Cast<PropsType>(Settings->FindOrCreatePropertySet(PropsType::StaticClass()));
			}
		}
		// Safety net — holders are normally ensured before build; CDO defaults keep the op non-null.
		return GetMutableDefault<PropsType>();
	}
}

void FRoadComputeFactoryRegistry::RegisterBuiltinFactories()
{
	Factories.Add("RoadSurface", FRoadComputeFactory::CreateLambda(
		[](IRoadMeshBuildHost* Host, TWeakPtr<MetaRoad::FRoadComputePipeline> Scope) -> IRoadOpCompute*
		{
			auto* Props = GetActorPropertySet<URoadSurfaceToolProperties>(Host, Scope);
			return MakeMeshCompute(Host, Scope, Props, TEXT("RoadSurface"), { "RebuildDriveSurface" },
				[Scope, WProps = TWeakObjectPtr<URoadSurfaceToolProperties>(Props)]() -> TUniquePtr<UE::Geometry::FDynamicMeshOperator>
				{
					check(WProps.IsValid());
					if (!WProps->bBuild) return MakeUnique<FDynamicMeshOperatorDummy>();
					auto Op = MakeUnique<MetaRoad::FDriveSurfaceOp>();
					Op->BaseData = Scope.Pin()->GetTriangulationResult();
					Op->DriveSurfaceIslandMaterial = WProps->DriveSurfaceIslandMaterial;
					Op->bComputVertexColor = WProps->bComputVertexColor;
					Op->VertexColorSmoothRadius = WProps->VertexColorSmoothRadius;
					Op->DefaultVertexColor = WProps->DefaultVertexColor;
					Op->EdgeVertexColor = WProps->EdgeVertexColor;
					Op->bSplitBySections = Scope.Pin()->GetTriangulationResult()->Params.bSplitBySections;
					Op->MergeSectionsAreaThreshold = Scope.Pin()->GetTriangulationResult()->Params.MergeSectionsAreaThreshold;
					return Op;
				});
		}
	));
	Factories.Add("RoadDecals", FRoadComputeFactory::CreateLambda(
		[](IRoadMeshBuildHost* Host, TWeakPtr<MetaRoad::FRoadComputePipeline> Scope) -> IRoadOpCompute*
		{
			auto* Props = GetActorPropertySet<URoadDecalToolProperties>(Host, Scope);
			return MakeMeshCompute(Host, Scope, Props, TEXT("RoadDecals"), { "RebuildDecals" },
				[Scope, WProps = TWeakObjectPtr<URoadDecalToolProperties>(Props)]() -> TUniquePtr<UE::Geometry::FDynamicMeshOperator>
				{
					check(WProps.IsValid());
					if (!WProps->bBuild) return MakeUnique<FDynamicMeshOperatorDummy>();
					auto Op = MakeUnique<MetaRoad::FDecalsOp>();
					Op->BaseData = Scope.Pin()->GetTriangulationResult();
					WProps->OverrideMaterials.GetKeys(Op->OverridesMaterials);
					Op->DecalOffset = WProps->DecalOffset;
					Op->bSplitBySections = Scope.Pin()->GetTriangulationResult()->Params.bSplitBySections;
					Op->MergeSectionsAreaThreshold = Scope.Pin()->GetTriangulationResult()->Params.MergeSectionsAreaThreshold;
					return Op;
				});
		}
	));
	Factories.Add("RoadSidewalks", FRoadComputeFactory::CreateLambda(
		[](IRoadMeshBuildHost* Host, TWeakPtr<MetaRoad::FRoadComputePipeline> Scope) -> IRoadOpCompute*
		{
			auto* Props = GetActorPropertySet<URoadSidewalkToolProperties>(Host, Scope);
			return MakeMeshCompute(Host, Scope, Props, TEXT("RoadSidewalks"), { "RebuildSidewalks" },
				[Scope, WProps = TWeakObjectPtr<URoadSidewalkToolProperties>(Props)]() -> TUniquePtr<UE::Geometry::FDynamicMeshOperator>
				{
					check(WProps.IsValid());
					if (!WProps->bBuild) return MakeUnique<FDynamicMeshOperatorDummy>();
					auto Op = MakeUnique<MetaRoad::FSidewalksOp>();
					Op->BaseData = Scope.Pin()->GetTriangulationResult();
					Op->bSplitBySections = Scope.Pin()->GetTriangulationResult()->Params.bSplitBySections;
					Op->MergeSectionsAreaThreshold = Scope.Pin()->GetTriangulationResult()->Params.MergeSectionsAreaThreshold;
					return Op;
				});
		}
	));
	Factories.Add("RoadCurbs", FRoadComputeFactory::CreateLambda(
		[](IRoadMeshBuildHost* Host, TWeakPtr<MetaRoad::FRoadComputePipeline> Scope) -> IRoadOpCompute*
		{
			auto* Props = GetActorPropertySet<URoadCertbToolProperties>(Host, Scope);
			return MakeMeshCompute(Host, Scope, Props, TEXT("RoadCurbs"), { "RebuildCurbs" },
				[Scope, WProps = TWeakObjectPtr<URoadCertbToolProperties>(Props)]() -> TUniquePtr<UE::Geometry::FDynamicMeshOperator>
				{
					check(WProps.IsValid());
					if (!WProps->bBuild) return MakeUnique<FDynamicMeshOperatorDummy>();
					auto Op = MakeUnique<MetaRoad::FCurbsOp>();
					Op->BaseData = Scope.Pin()->GetTriangulationResult();
					Op->UV0Scale = WProps->CurbsUV0Scale;
					Op->OverrideMaterial = WProps->OverrideMaterial;
					// Game-thread preload of soft curb profiles so CalculateResult (worker) can .Get() them.
					Op->PreloadProfiles();
					return Op;
				});
		}
	));
	Factories.Add("RoadMarks", FRoadComputeFactory::CreateLambda(
		[](IRoadMeshBuildHost* Host, TWeakPtr<MetaRoad::FRoadComputePipeline> Scope) -> IRoadOpCompute*
		{
			auto* Props = GetActorPropertySet<URoadMarkToolProperties>(Host, Scope);
			return MakeMeshCompute(Host, Scope, Props, TEXT("RoadMarks"), { "RebuildMarks" },
				[Scope, WProps = TWeakObjectPtr<URoadMarkToolProperties>(Props)]() -> TUniquePtr<UE::Geometry::FDynamicMeshOperator>
				{
					check(WProps.IsValid());
					if (!WProps->bBuild) return MakeUnique<FDynamicMeshOperatorDummy>();
					auto Op = MakeUnique<MetaRoad::FMarksOp>();
					Op->BaseData = Scope.Pin()->GetTriangulationResult();
					Op->MarkOffset = WProps->MarkOffset;
					Op->OverrideMaterial = WProps->OverrideMaterial;
					// Game-thread preload of soft mark profiles so CalculateResult (worker) can .Get() them.
					Op->PreloadProfiles();
					return Op;
				});
		}
	));
#if METAROAD_PRO
	Factories.Add("RoadLofting", FRoadComputeFactory::CreateLambda(
		[](IRoadMeshBuildHost* Host, TWeakPtr<MetaRoad::FRoadComputePipeline> Scope) -> IRoadOpCompute*
		{
			auto* Props = GetActorPropertySet<ULoftingToolProperties>(Host, Scope);
			return MakeMeshCompute(Host, Scope, Props, TEXT("RoadLofting"), { "RebuildLofting" },
				[Scope, WProps = TWeakObjectPtr<ULoftingToolProperties>(Props)]() -> TUniquePtr<UE::Geometry::FDynamicMeshOperator>
				{
					check(WProps.IsValid());
					if (!WProps->bBuild) return MakeUnique<FDynamicMeshOperatorDummy>();
					auto Op = MakeUnique<MetaRoad::FLoftingOp>();
					Op->BaseData = Scope.Pin()->GetTriangulationResult();
					Op->OverrideMaterial = WProps->OverrideMaterial;
					Op->ChordToleranceSq = Scope.Pin()->GetTriangulationResult()->Params.ChordToleranceSq;
					Op->MinSegmentLength = Scope.Pin()->GetTriangulationResult()->Params.MinSegmentLength;
					return Op;
				});
		}
	));
#endif
	Factories.Add("RoadSplineMeshes", FRoadComputeFactory::CreateLambda(
		[](IRoadMeshBuildHost* Host, TWeakPtr<MetaRoad::FRoadComputePipeline> Scope) -> IRoadOpCompute*
		{
			auto* Props = GetActorPropertySet<URoadAttributesToolProperties>(Host, Scope);
			return MakeSplineMeshCompute(Host, Scope, Props, { "RebuildAttributes" },
				[Scope, WProps = TWeakObjectPtr<URoadAttributesToolProperties>(Props)]() -> TUniquePtr<MetaRoad::FSplineMeshOperator>
				{
					check(WProps.IsValid());
					if (!WProps->bBuild) return MakeUnique<FSplineMeshOperatorDummy>();
					auto Op = MakeUnique<MetaRoad::FSplineMeshOp>();
					Op->BaseData = Scope.Pin()->GetTriangulationResult();
					Op->bDrawRefSplines = WProps->bDrawRefSplines;
					Op->ChordToleranceSq = Scope.Pin()->GetTriangulationResult()->Params.ChordToleranceSq;
					Op->MinSegmentLength = Scope.Pin()->GetTriangulationResult()->Params.MinSegmentLength;
					return Op;
				});
		}
	));
	Factories.Add("RoadGraph", FRoadComputeFactory::CreateLambda(
		[](IRoadMeshBuildHost* Host, TWeakPtr<MetaRoad::FRoadComputePipeline> Scope) -> IRoadOpCompute*
		{
			auto* RoadCompute = NewObject<URoadGraphBackgroundCompute>(Host->GetHostObject());
			auto* Props = GetActorPropertySet<URoadGraphToolProperties>(Host, Scope);
			auto Factory = MakeUnique<MetaRoad::TLambdaGenericDataFactory<FRoadGraphScope>>(
				[Scope, WProps = TWeakObjectPtr<URoadGraphToolProperties>(Props)]() -> TUniquePtr<UE::Geometry::TGenericDataOperator<FRoadGraphScope>>
				{
					check(WProps.IsValid());
					if (!WProps->bBuild) return MakeUnique<FRoadGraphOperatorDummy>();
					auto Op = MakeUnique<MetaRoad::FGraphOp>();
					Op->BaseData = Scope.Pin()->GetTriangulationResult();
					Op->MaxSquareDistanceFromSpline = WProps->MaxSquareDistanceFromSpline;
					Op->MinSegmentLength = WProps->MinSegmentLength;
					Op->ZOffset = WProps->ZOffset;
					return Op;
				});
			RoadCompute->SetupWithOwnedFactory(Host, MoveTemp(Factory), Scope);
			RoadCompute->GetRebuildTags().Add("RebuildAttributes");
			RoadCompute->OnShutdownAndGenerateAssets.AddLambda([](AActor* TargetActor, const FTransform3d& Transform, TUniquePtr<FRoadGraphScope>& Result)
			{
				// No graph result (e.g. bBuild disabled, or the synchronous path produced nothing) — skip.
				if (!Result)
				{
					return;
				}
				URoadGraphDataComponent* NewComponent = NewObject<URoadGraphDataComponent>(TargetActor, *AssetUtils::GenerateValidComponentName(TEXT("RoadGraphData"), TargetActor), RF_Transactional);
				TargetActor->AddInstanceComponent(NewComponent);
				NewComponent->OnComponentCreated();
				NewComponent->RegisterComponent();
				NewComponent->RoadGraph = *Result;
			});
			return RoadCompute;
		}
	));

#if !UE_BUILD_SHIPPING
	// Drift guard: every build property-set registry entry (except triangulation) must have a matching
	// compute factory key here, so the single schema in UMetaRoadBuildSettings stays in sync with the factories.
	for (const FMetaRoadBuildPropertySetInfo& Info : UMetaRoadBuildSettings::GetBuildPropertySetInfos())
	{
		ensureMsgf(Info.Key.IsNone() || Factories.Contains(Info.Key),
			TEXT("MetaRoad build property-set '%s' has no matching RoadComputeFactories entry"), *Info.Key.ToString());
	}
#endif
}
