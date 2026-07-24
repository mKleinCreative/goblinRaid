/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "TriangulateRoadOp.h"
//#include "Operations/ExtrudeMesh.h"
//#include "Operations/OffsetMeshRegion.h"
#include "DynamicMesh/MeshNormals.h"
#include "Utils/MeshUtils.h"
#include "Utils/MeshUtils.h"

#define LOCTEXT_NAMESPACE "SidewalksOp"

using namespace MetaRoad;

void FSidewalksOp::CalculateResult(FProgressCancel* Progress)
{

#define CHECK_CANCLE() if (Progress && Progress->Cancelled()) { ResultInfo.Result = EGeometryResultType::Cancelled; return; }

	ResultInfo.Result = EGeometryResultType::InProgress;

	if (!BaseData || BaseData->ResultInfo.HasFailed())
	{
		ResultInfo.SetFailed(LOCTEXT("CalculateResultFail_Input", "FSidewalksOp: input data is faild"));
		return;
	}

	/*
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
	*/

	auto& Graph = BaseData->Arrangement->Graph;
	MeshUtils::EnableDefaultAttributes(*ResultMesh, true, true, true, true, 3);


	// ========================== Copy all vertexes from Graph to DynamicMesh ==========================
	for (int VID = 0; VID < Graph.VertexCount(); ++VID)
	{
		const auto Pt = Graph.GetVertex(VID);
		const auto& Verticex3d = BaseData->Vertices3d[VID];
		//const auto LocalNormal = FVector3f(Info[0].Pos.Quat.GetUpVector());

		double HeightOffset = 0;
		for (auto& [Poly, Info] : Verticex3d.Infos)
		{
			HeightOffset = FMath::Max(HeightOffset, Info.HeightOffset);
		}

		int32 NewVID = ResultMesh->AppendVertex(ResultTransform.InverseTransformPosition(Verticex3d.Vertex + Verticex3d.Normal * HeightOffset));
		check(NewVID == VID);
	}

	CHECK_CANCLE();

	// ========================== Create Mesh ==========================

	// Create sorted LanesPoly by MaterialPriority
	TArray<const FProceduralPolygon*> LanesPolySorted;
	for (auto& Poly : BaseData->Polygons)
	{
		if (Poly->GetRoadZone().GetPtr<FRoadZoneSidewalk>() && !Poly->IsPolyline())
		{
			LanesPolySorted.Add(Poly.Get());
		}
	}
	LanesPolySorted.Sort([](const auto& A, const auto& B)
	{
		return A.GetMaterialPriority() > B.GetMaterialPriority();
	});

	auto* MaterialID = ResultMesh->Attributes()->GetMaterialID();
	auto* ColorOverlay = ResultMesh->Attributes()->PrimaryColors();

	OpUtils::FMaterialSlotMap MaterialSlotMap{};

	for (auto& Poly : LanesPolySorted)
	{
			
		for (auto& TID : Poly->TrianglesIDs)
		{
			if (ResultMesh->IsTriangle(TID) && OpUtils::IsTriangleValid(ResultMesh->GetTriangleRef(TID)))
			{
				continue;
			}

			auto& T = BaseData->Triangles[TID];
			EMeshResult Res = ResultMesh->InsertTriangle(TID, T);
			check(Res == EMeshResult::Ok);

			MaterialID->SetValue(TID, MaterialSlotMap.AddMaterial(Poly->GetRoadZone()));

			Poly->SetUVLayers(*ResultMesh, TID, BaseData->Params.UV0ScaleFactor, BaseData->Params.UV1ScaleFactor, BaseData->Params.UV2ScaleFactor, BaseData->Params.UVMaxSize);
				
			ColorOverlay->SetTriangle(TID, FIndex3i{ 
				ColorOverlay->AppendElement(FVector4f(1.0, 1.0, 1.0, 1.0)),
				ColorOverlay->AppendElement(FVector4f(1.0, 1.0, 1.0, 1.0)),
				ColorOverlay->AppendElement(FVector4f(1.0, 1.0, 1.0, 1.0)) 
			});

			if (bSplitBySections && BaseData->Splines.Num() == 1 && Poly->GetType() == ERoadPolygonType::RoadLane)
			{
				const auto* LanePoly = static_cast<const FProceduralPolygon_RoadLane*>(Poly);
				ResultMesh->SetTriangleGroup(TID, LanePoly->GetSectionIndex());
			}
		}
		CHECK_CANCLE();
	}

	// ========================== Split groups by mesh sections ==========================
	if (bSplitBySections)
	{
		MeshUtils::SplitMeshGroupsBySections(*ResultMesh);
		CHECK_CANCLE();
	}

	// ========================== Merge groups by arias ==========================
	if (bSplitBySections && MergeSectionsAreaThreshold > 0)
	{
		// TODO: instead of grouping "by area" make a grouping under the length of the common line
		MeshUtils::MergeGroupByArea(*ResultMesh, MergeSectionsAreaThreshold);
		CHECK_CANCLE();
	}

	// ========================== Resolve materials slot name ==========================
	ResultMaterialSlots = MaterialSlotMap.GetSlots();
	CHECK_CANCLE();

	// ========================== Compact mesh ==========================
	ResultMesh->CompactInPlace();


	CHECK_CANCLE();

	// ========================== Compute Normals ==========================
	FMeshNormals::QuickComputeVertexNormals(*ResultMesh);
	FMeshNormals::InitializeOverlayToPerVertexNormals(ResultMesh->Attributes()->PrimaryNormals(), true);
	FMeshNormals::QuickRecomputeOverlayNormals(*ResultMesh);


	ResultInfo.SetSuccess();

#undef CHECK_CANCLE
}

#undef LOCTEXT_NAMESPACE
