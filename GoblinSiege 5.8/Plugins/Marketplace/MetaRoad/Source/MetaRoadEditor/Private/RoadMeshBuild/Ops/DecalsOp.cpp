/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "TriangulateRoadOp.h"
#include "DynamicMesh/MeshNormals.h"
#include "MetaRoadSettings.h"
#include "Utils/MeshUtils.h"

#define LOCTEXT_NAMESPACE "DecalsOp"

using namespace MetaRoad;


void FDecalsOp::CalculateResult(FProgressCancel* Progress)
{
#define CHECK_CANCLE() if (Progress && Progress->Cancelled()) { ResultInfo.Result = EGeometryResultType::Cancelled; return; }

	ResultInfo.Result = EGeometryResultType::InProgress;

	if (!BaseData || BaseData->ResultInfo.HasFailed())
	{
		ResultInfo.SetFailed(LOCTEXT("CalculateResultFail_Input", "FDecalsOp: input data is faild"));
		return;
	}

	bool bSurfaceIsPresent = false;
	for (auto& Poly : BaseData->Polygons)
	{
		if (Poly->GetRoadZone().GetPtr<FRoadZoneDriving>())
		{
			bSurfaceIsPresent = true;
			break;
		}
	}

	if (!bSurfaceIsPresent)
	{
		ResultInfo.SetSuccess();
		return;
	}

	auto& Graph = BaseData->Arrangement->Graph;
	MeshUtils::EnableDefaultAttributes(*ResultMesh, true, true, true, true, 2);

	const TMap<FName, FRoadZoneTypeDetails>& RoadZoneTypes = GetDefault<UMetaRoadSettings>()->RoadZoneTypes;

	// ========================== Create Mesh ==========================

	OpUtils::FMaterialSlotMap MaterialSlotMap{};

	for (auto& Poly : BaseData->Polygons)
	{
		const auto* LaneDriving = Poly->GetRoadZone().GetPtr<FRoadZoneDriving>();
		if (!LaneDriving)
		{
			continue;
		}

		FRoadZoneType MaterialProfile{};

		if (LaneDriving->ZoneType.IsValid())
		{
			if (auto* Profile = RoadZoneTypes.Find(LaneDriving->ZoneType.GetName()))
			{
				if (IsValid(Profile->DecalMaterial))
				{
					MaterialProfile = LaneDriving->ZoneType;
				}
			}
		}

		if (OverridesMaterials.Contains(LaneDriving->ZoneType))
		{
			MaterialProfile = LaneDriving->ZoneType;
		}

		if (!MaterialProfile.IsValid())
		{
			continue;
		}

		FDynamicMesh3 DynamicMesh;
		MeshUtils::EnableDefaultAttributes(DynamicMesh, true, true, true, true, 2);

		// Copy all vertexes from Graph to DynamicMesh 
		for (int VID = 0; VID < Graph.VertexCount(); ++VID)
		{
			const auto Pt = Graph.GetVertex(VID);
			const auto& Verticex3d = BaseData->Vertices3d[VID];
			int32 NewVID = DynamicMesh.AppendVertex(ResultTransform.InverseTransformPosition(Verticex3d.Vertex + Verticex3d.Normal * DecalOffset));
			check(NewVID == VID);
		}

		CHECK_CANCLE();

		auto* MaterialID = DynamicMesh.Attributes()->GetMaterialID();
		auto* ColorOverlay = DynamicMesh.Attributes()->PrimaryColors();

		// Creat triangles
		for (auto& TID : Poly->TrianglesIDs)
		{
			auto& T = BaseData->Triangles[TID];
			EMeshResult Res = DynamicMesh.InsertTriangle(TID, T);
			check(Res == EMeshResult::Ok);

			MaterialID->SetValue(TID, MaterialSlotMap.AddMaterial(Poly->GetRoadZone()));

			Poly->SetUVLayers(DynamicMesh, TID, BaseData->Params.UV0ScaleFactor, BaseData->Params.UV1ScaleFactor, BaseData->Params.UV2ScaleFactor, BaseData->Params.UVMaxSize);

			ColorOverlay->SetTriangle(TID, FIndex3i{
				ColorOverlay->AppendElement(FVector4f(1.0, 1.0, 1.0, 1.0)),
				ColorOverlay->AppendElement(FVector4f(1.0, 1.0, 1.0, 1.0)),
				ColorOverlay->AppendElement(FVector4f(1.0, 1.0, 1.0, 1.0)) 
			});

			if (bSplitBySections && BaseData->Splines.Num() == 1 && Poly->GetType() == ERoadPolygonType::RoadLane)
			{
				auto* LanePoly = static_cast<FProceduralPolygon_RoadLane*>(Poly.Get());
				DynamicMesh.SetTriangleGroup(TID, LanePoly->GetSectionIndex());
			}
		}


		CHECK_CANCLE()

		// Compact mesh 
		DynamicMesh.CompactInPlace();

		CHECK_CANCLE()

		// Compute Normals
		FMeshNormals::QuickComputeVertexNormals(DynamicMesh);
		FMeshNormals::InitializeOverlayToPerVertexNormals(DynamicMesh.Attributes()->PrimaryNormals(), true);
		FMeshNormals::QuickRecomputeOverlayNormals(DynamicMesh);

		CHECK_CANCLE();

		// Append DynamicMesh to ResultMesh
		MeshUtils::AppendMesh(*ResultMesh, DynamicMesh);

		
	}

	// Resolve materials slot name
	ResultMaterialSlots = MaterialSlotMap.GetSlots();

	ResultInfo.SetSuccess();

#undef CHECK_CANCLE

}


#undef LOCTEXT_NAMESPACE
