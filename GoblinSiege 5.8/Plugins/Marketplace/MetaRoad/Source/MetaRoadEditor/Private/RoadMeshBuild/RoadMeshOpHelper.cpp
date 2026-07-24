
/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Copyright Epic Games, Inc. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */


#include "RoadMeshBuild/RoadMeshOpHelper.h"
#include "Utils/AssetUtils.h"
#include "RoadMeshBuild/RoadComputePipeline.h"
#include "RoadMeshBuild/IRoadMeshBuildHost.h"
#include "RoadMeshBuild/MetaRoadBuildSettings.h"
#include "EditorMode/MetaRoadVisibilitySettings.h" // bShowWireframe
#include "Engine/CollisionProfile.h" // UCollisionProfile::CustomCollisionProfileName
#include "RoadMeshBuild/Ops/TriangulateRoadOp.h"
#include "DynamicMesh/MeshTransforms.h"
#include "ModelingObjectsCreationAPI.h"
#include "ToolSetupUtil.h"
#include "MetaRoadModule.h"
#include "DynamicSubmesh3.h"
#include "RoadMeshBuild/ToolPropertySets.h"
#include "Engine/StaticMesh.h"

using namespace UE::Geometry;

namespace
{
	class FLambdaOperatorFactory : public UE::Geometry::IDynamicMeshOperatorFactory
	{
		TFunction<TUniquePtr<UE::Geometry::FDynamicMeshOperator>()> Fn;
	public:
		FLambdaOperatorFactory(TFunction<TUniquePtr<UE::Geometry::FDynamicMeshOperator>()> InFn)
			: Fn(MoveTemp(InFn)) {}
		TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override { return Fn(); }
	};

	class UMeshOpPreviewWithBackgroundCompute_Private : public UMeshOpPreviewWithBackgroundCompute
	{
	public:
		UE::Geometry::EBackgroundComputeTaskStatus GetLastComputeStatus() const { return this->BackgroundCompute->CheckStatus().TaskStatus; }
		void OnlyInvalidateResult() { bResultValid = false; }
	};

	int32 FillComponentTriIndicesFromTriIDs(const FDynamicMesh3& SourceMesh, TFunctionRef<int32(int32)> TIDtoID, TArray<TArray<int32>>& ComponentTriIndices)
	{
		TMap<int32, int32> ComponentIDMap;
		for (int32 TID : SourceMesh.TriangleIndicesItr())
		{
			int32 CompID = TIDtoID(TID);
			int32* FoundIdx = ComponentIDMap.Find(CompID);
			int32 UseIdx = -1;
			if (FoundIdx)
			{
				UseIdx = *FoundIdx;
			}
			else
			{
				UseIdx = ComponentTriIndices.AddDefaulted();
				ComponentIDMap.Add(CompID, UseIdx);
			}
			ComponentTriIndices[UseIdx].Add(TID);
		}
		//int32 NumComponents = ComponentTriIndices.Num();
		return ComponentTriIndices.Num();
	};

	struct FSplitedComponentResult
	{
		UE::Geometry::FDynamicMesh3 Mesh;
		TArray<UMaterialInterface*> Materials;
		TArray<FName> MaterialSlots;
		FVector3d Origins;
		//int GroupID;
	};

	bool SplitMeshesByGroupID(
		const FDynamicMesh3& SourceMesh, 
		const TArray<TObjectPtr<UMaterialInterface>>& Materials,
		const TArray<FName>& MaterialSlots,
		bool bCenterPivots, 
		TArray<FSplitedComponentResult>& SplitInfo)
	{
		check(Materials.Num() == MaterialSlots.Num());

		TArray<TArray<int32>> ComponentTriIndices;
		int32 NumComponents = FillComponentTriIndicesFromTriIDs(SourceMesh, [SourceMesh](int32 TID) { return SourceMesh.GetTriangleGroup(TID); }, ComponentTriIndices);

		if (NumComponents < 2)
		{
			return false;
		}

		SplitInfo.SetNum(NumComponents);

		for (int32 k = 0; k < NumComponents; ++k)
		{
			FDynamicSubmesh3 SubmeshCalc;

			// if statement should always be true- components should always have been calculated & populated when there's no geometry selection
			if (ensure(!ComponentTriIndices.IsEmpty()))
			{
				SubmeshCalc = FDynamicSubmesh3(&SourceMesh, ComponentTriIndices[k]);
			}
			
			FDynamicMesh3& Submesh = SubmeshCalc.GetSubmesh();
			TArray<UMaterialInterface*> NewMaterials;
			TArray<FName> NewMaterialSlots;

			// remap materials
			FDynamicMeshMaterialAttribute* MaterialIDs = Submesh.HasAttributes() ? Submesh.Attributes()->GetMaterialID() : nullptr;
			if (MaterialIDs)
			{
				TArray<int32> UniqueIDs;
				for (int32 tid : Submesh.TriangleIndicesItr())
				{
					int32 MaterialID = MaterialIDs->GetValue(tid);
					int32 Index = UniqueIDs.IndexOfByKey(MaterialID);
					if (Index == INDEX_NONE)
					{
						int32 NewMaterialID = UniqueIDs.Num();
						UniqueIDs.Add(MaterialID);
						NewMaterials.Add(Materials[MaterialID]);
						NewMaterialSlots.Add(MaterialSlots[MaterialID]);
						MaterialIDs->SetValue(tid, NewMaterialID);
					}
					else
					{
						MaterialIDs->SetValue(tid, Index);
					}
				}
			}

			FVector3d Origin = FVector3d::ZeroVector;
			if (bCenterPivots)
			{
				// reposition mesh
				FAxisAlignedBox3d Bounds = Submesh.GetBounds();
				Origin = Bounds.Center();
				MeshTransforms::Translate(Submesh, -Origin);
			}

			SplitInfo[k].Mesh = MoveTemp(Submesh);
			SplitInfo[k].Materials = MoveTemp(NewMaterials);
			SplitInfo[k].MaterialSlots = MoveTemp(NewMaterialSlots);
			SplitInfo[k].Origins = Origin;
			//SplitInfo[k].GroupID = ;
		}

		return true;
	}
}

UE::Geometry::EBackgroundComputeTaskStatus URoadMeshOpPreviewWithBackgroundCompute::GetLastComputeStatus() const
{
	return static_cast<UMeshOpPreviewWithBackgroundCompute_Private*>(BackgroundCompute)->GetLastComputeStatus();
}

UTriangulateRoadToolProperties* URoadMeshOpPreviewWithBackgroundCompute::GetEffectiveTriangulationProperties() const
{
	if (Host.IsValid())
	{
		if (TSharedPtr<MetaRoad::FRoadComputePipeline> Pin = OwningPipeline.Pin())
		{
			if (UMetaRoadBuildSettings* Settings = Host->GetBuildSettingsForActor(Pin->GetTargetActor()))
			{
				if (UTriangulateRoadToolProperties* Props = Settings->Find<UTriangulateRoadToolProperties>())
				{
					return Props;
				}
			}
		}
	}
	// Safety net — holders are normally ensured on the game thread before build.
	return GetMutableDefault<UTriangulateRoadToolProperties>();
}

void URoadMeshOpPreviewWithBackgroundCompute::Setup(IRoadMeshBuildHost* InHost, TWeakPtr<MetaRoad::FRoadComputePipeline> RoadComputePipeline, TFunction<TUniquePtr<UE::Geometry::FDynamicMeshOperator>()> MakeOp)
{
	Host = InHost;
	OwningPipeline = RoadComputePipeline;

	OwnedFactory = MakeUnique<FLambdaOperatorFactory>(MoveTemp(MakeOp));
	BackgroundCompute = NewObject<UMeshOpPreviewWithBackgroundCompute>(this);
	BackgroundCompute->Setup(Host->GetTargetWorld(), OwnedFactory.Get());
	BackgroundCompute->PreviewMesh->EnableWireframe(UMetaRoadVisibilitySettings::Get()->bShowWireframe);
	BackgroundCompute->PreviewMesh->SetTangentsMode(EDynamicMeshComponentTangentsMode::AutoCalculated);
	ToolSetupUtil::ApplyRenderingConfigurationToPreview(BackgroundCompute->PreviewMesh, nullptr);

	BackgroundCompute->OnOpCompleted.AddLambda(
		[this](const FDynamicMeshOperator* Op)
		{
			ApplyOperatorResult(Op);
		}
	);
	BackgroundCompute->OnMeshUpdated.AddLambda(
		[this](const UMeshOpPreviewWithBackgroundCompute* UpdatedPreview)
		{
			if (Host.IsValid())
			{
				Host->NotifyMeshUpdated();
			}
		}
	);
}

void URoadMeshOpPreviewWithBackgroundCompute::ApplyOperatorResult(const FDynamicMeshOperator* Op)
{
	if (!Op)
	{
		return;
	}

	if (TSharedPtr<MetaRoad::FRoadComputePipeline> Pipeline = OwningPipeline.Pin())
	{
		Pipeline->AppendResultInfo(Op->GetResultInfo());
	}

	if (!Op->GetResultInfo().HasResult())
	{
		return;
	}

	if (auto* InteractiveToolPropertyMaterialInterface = Cast<IInteractiveToolPropertyMaterialInterface>(PropertySet))
	{
		const auto* OpWithMat = static_cast<const MetaRoad::FDynamicMeshWithMaterialsOperator*>(Op);
		TArray<TObjectPtr<UMaterialInterface>> Materials;
		Materials.SetNum(OpWithMat->ResultMaterialSlots.Num());
		for (int i = 0; i < OpWithMat->ResultMaterialSlots.Num(); ++i)
		{
			auto& Slot = OpWithMat->ResultMaterialSlots[i];
			Materials[i] = Slot.Value.Get();
			if (!Materials[i] || GetEffectiveTriangulationProperties()->bForceAssignMaterial)
			{
				if (auto* Mat = InteractiveToolPropertyMaterialInterface->GetMaterialsMap().Find(Slot.Key))
				{
					Materials[i] = *Mat;
				}
			}
		}
		ResultMaterialSlots = OpWithMat->ResultMaterialSlots;
		BackgroundCompute->ConfigureMaterials(Materials, Host->GetWorkingMaterial());
	}
}

void URoadMeshOpPreviewWithBackgroundCompute::ComputeSynchronous()
{
	bSyncResultValid = false;

	if (!OwnedFactory)
	{
		return;
	}

	// Run the operator on the calling thread (the MakeOp closure configures it exactly as the async path),
	// then apply materials and push the mesh straight into the preview — no background threading.
	TUniquePtr<FDynamicMeshOperator> Op = OwnedFactory->MakeNewOperator();
	if (!Op)
	{
		return;
	}
	Op->CalculateResult(nullptr);

	ApplyOperatorResult(Op.Get());

	if (!Op->GetResultInfo().HasResult())
	{
		return;
	}

	TUniquePtr<FDynamicMesh3> Mesh = Op->ExtractResult();
	// Mark the result valid for the synchronous (Bake) path — the inner UMeshOpPreviewWithBackgroundCompute
	// only sets its own flag from the async OnOpCompleted, which never fires here. ShutdownAndGenerateAssets
	// then extracts this mesh from the preview via Shutdown().
	bSyncResultValid = Mesh.IsValid() && Mesh->TriangleCount() > 0;
	BackgroundCompute->PreviewMesh->UpdatePreview(Mesh.Get());
	BackgroundCompute->PreviewMesh->SetTransform((FTransform)Op->GetResultTransform());
	BackgroundCompute->PreviewMesh->SetVisible(true);
}

bool URoadMeshOpPreviewWithBackgroundCompute::ShutdownAndGenerateAssets(AActor* TargetActor, const FTransform3d& ActorToWorld)
{
	check(Host.IsValid());

	if (!HaveValidNonEmptyResult())
	{
		// Nothing to write (disabled/empty layer) — not an error.
		Cancel();
		return true;
	}

	const FString BaseNameWithPrefix = (GetEffectiveTriangulationProperties()->ObjectType == ECreateRoadObjectType::StaticMesh ? TEXT("SM_") : TEXT("DM_")) + BaseAssetName;
	auto OpResult = BackgroundCompute->Shutdown();
	if (!OpResult.Mesh.Get())
	{
		UE_LOG(LogMetaRoad, Error, TEXT(" UTriangulateRoadTool::Shutdown(); Can't generate asset \"%s\" for actor \"%s\""), *BaseNameWithPrefix, *TargetActor->GetActorLabel());
		BackgroundCompute = nullptr;
		return false; // had a valid result but failed to produce a mesh
	}

	bool bAllWritesOk = true;

	MeshTransforms::ApplyTransform(*OpResult.Mesh, OpResult.Transform, true);
	MeshTransforms::ApplyTransformInverse(*OpResult.Mesh, ActorToWorld, true);

	TArray<FSplitedComponentResult> SplitInfo;
	TArray<FName> Slots;
	Algo::Transform(ResultMaterialSlots, Slots, [](const auto& In) { return In.Key; });
	if (OpResult.Mesh->HasTriangleGroups())
	{
		SplitMeshesByGroupID(*OpResult.Mesh, BackgroundCompute->StandardMaterials, Slots, true, SplitInfo);
	}
	if (SplitInfo.Num() == 0)
	{
		SplitInfo.Add({ *OpResult.Mesh.Get(), BackgroundCompute->StandardMaterials, Slots, FVector::ZeroVector});
	}

	for (int i = 0; i < SplitInfo.Num(); ++i)
	{
		const FString BaseNameWithPrefixID = BaseNameWithPrefix + TEXT("_") + FString::FromInt(i);
		FCreateMeshObjectParams NewMeshObjectParams;
		NewMeshObjectParams.TargetWorld = TargetActor->GetWorld();
		NewMeshObjectParams.Transform = FTransform(FRotator::ZeroRotator, SplitInfo[i].Origins);
		NewMeshObjectParams.BaseName = BaseAssetName;
		NewMeshObjectParams.Materials = SplitInfo[i].Materials;
		NewMeshObjectParams.SetMesh(&SplitInfo[i].Mesh);
		NewMeshObjectParams.bEnableCollision = true;
		NewMeshObjectParams.CollisionMode = ECollisionTraceFlag::CTF_UseComplexAsSimple;
		NewMeshObjectParams.TypeHint = GetEffectiveTriangulationProperties()->ObjectType == ECreateRoadObjectType::StaticMesh ? ECreateObjectTypeHint::StaticMesh : ECreateObjectTypeHint::DynamicMeshActor;

		const bool bTransientOutput = Host.IsValid() && Host->WantsTransientMeshOutput();
		FCreateMeshObjectResult Res = AssetUtils::CreateMeshObject(MoveTemp(NewMeshObjectParams), TargetActor->GetRootComponent(), BaseNameWithPrefixID, bTransientOutput);
		if (!Res.IsOK())
		{
			UE_LOG(LogMetaRoad, Error, TEXT(" UTriangulateRoadTool::Shutdown(); Can't generate asset \"%s\" for actor \"%s\" code:\"%i\""), *BaseNameWithPrefixID, *TargetActor->GetActorLabel(), Res.ResultCode);
			bAllWritesOk = false;
			continue;
		}

		// Stamp the generated component so the re-bake cleanup knows this mesh was CREATED by the bake and
		// may be disposed of. Attribute components (URoadLaneAttributeSplineMeshDescriptor, etc.) never get this tag,
		// so their user-referenced meshes are protected. See MetaRoad::GeneratedAssetComponentTagName.
		if (Res.NewComponent)
		{
			Res.NewComponent->ComponentTags.AddUnique(FName(MetaRoad::GeneratedAssetComponentTagName));
		}

		if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Res.NewAsset))
		{
			auto& StaticMaterials = StaticMesh->GetStaticMaterials();
			check(StaticMaterials.Num() == SplitInfo[i].MaterialSlots.Num());
			for (int j = 0; j < StaticMaterials.Num(); ++j)
			{
				StaticMaterials[j].MaterialSlotName = SplitInfo[i].MaterialSlots[j];
			}
			
			if (Res.NewComponent)
			{
				if (auto* PhysInterface = Cast<IInteractiveToolPropertyPhysInterface>(PropertySet))
				{
					auto& BodyInstance = PhysInterface->GetBodyInstance();
					if (BodyInstance.GetCollisionProfileName() == UCollisionProfile::CustomCollisionProfileName)
					{
						Res.NewComponent->SetCollisionEnabled(BodyInstance.GetCollisionEnabled());
						Res.NewComponent->SetCollisionObjectType(BodyInstance.GetObjectType());
						Res.NewComponent->SetCollisionResponseToChannels(BodyInstance.GetResponseToChannels());
					}
					else
					{
						Res.NewComponent->SetCollisionProfileName(BodyInstance.GetCollisionProfileName());
					}
				}
			}
		}
	}

	BackgroundCompute = nullptr;
	return bAllWritesOk;
}

int URoadMeshOpPreviewWithBackgroundCompute::GetNumVertices() const
{
	if (BackgroundCompute->HaveValidNonEmptyResult())
	{
		auto* DynamicMesh = BackgroundCompute->PreviewMesh->GetMesh();
		return DynamicMesh->VertexCount();
	}
	return 0;
}

int URoadMeshOpPreviewWithBackgroundCompute::GetNumTriangles() const
{
	if (BackgroundCompute->HaveValidNonEmptyResult())
	{
		auto* DynamicMesh = BackgroundCompute->PreviewMesh->GetMesh();
		return DynamicMesh->TriangleCount();
	}
	return 0;
}