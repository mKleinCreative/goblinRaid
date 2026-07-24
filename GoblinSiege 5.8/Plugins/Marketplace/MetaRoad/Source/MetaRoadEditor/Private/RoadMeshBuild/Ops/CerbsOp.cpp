/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "TriangulateRoadOp.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "RoadMeshBuild/RoadLanePolylineArrangement.h"
#include "Utils/OpUtils.h"
#include "Utils/CurveUtils.h"
#include "Utils/MeshUtils.h"
#include "DynamicMesh/MeshNormals.h"
#include "Assets/RoadCurbProfile.h"

#define LOCTEXT_NAMESPACE "FCurbsOp"

using namespace MetaRoad;

struct FCerbRoadPosition : public FRoadPosition
{
	int SectionIndex = 0; // For dynamic mesh GroupID
	double CurbsHeight;

};


struct FRoadLanePolylineCurb : public TRoadLanePolyline<FCerbRoadPosition, FRoadLanePolylineCurb>
{
	URoadCurbProfile* Profile = nullptr;
	UMaterialInterface* OverrideMaterial = nullptr;

	virtual bool CanAppend(const FRoadLanePolylineCurb& Other, EAppandMode AppandMode, double Tolerance) const override
	{

		if (Profile != Other.Profile)
		{
			return false;
		}

		if (OverrideMaterial != Other.OverrideMaterial)
		{
			return false;
		}

		return TRoadLanePolyline<FCerbRoadPosition, FRoadLanePolylineCurb>::CanAppend(Other, AppandMode, Tolerance);
	}
};

using FRoadCurbArrangemen = TRoadLanePolylineArrangement<FRoadLanePolylineCurb>;


static bool MakeCurb(const FRoadLanePolylineCurb& Polyline, const URoadCurbProfile* Profile, FDynamicMesh3& DynamicMesh, int MaterialID, double UV0Scale)
{
	check(Profile);

	if (Profile->CurbCurve.GetRichCurveConst()->GetNumKeys() < 2)
	{
		return false;
	}

	MeshUtils::EnableDefaultAttributes(DynamicMesh, true, false, true, true, 1);

	const int StartVertexIndex = DynamicMesh.MaxVertexID();
	auto* MaterialIDOverlay = DynamicMesh.Attributes()->GetMaterialID();
	auto* UV0Overlay = DynamicMesh.Attributes()->GetUVLayer(0);

	const float MaxSquareDistanceFromCurve = 0.01f;
	const float Tolerance = 0.01f;
	const int ReparamSteps = 200;

	TArray<float> Values;
	TArray<float> Times;
	if (!CurveUtils::CurveToPolyline(*Profile->CurbCurve.GetRichCurveConst(), 0.0, Profile->Width, MaxSquareDistanceFromCurve, Tolerance, ReparamSteps, Values, Times))
	{
		return false;
	}
	const int StepSize = Values.Num();
	
	TArray<float> AccumulatedValue;
	AccumulatedValue.SetNum(StepSize);
	for (int i = 1; i < StepSize; ++i)
	{
		AccumulatedValue[i] = AccumulatedValue[i - 1] + FMath::Sqrt(FMath::Square(Values[i] - Values[i - 1]) + FMath::Square(Times[i] - Times[i - 1]));
	}

	float MinValue, MaxValue;
	Profile->CurbCurve.GetRichCurveConst()->GetValueRange(MinValue, MaxValue);

	float MinTime, MaxTime;
	Profile->CurbCurve.GetRichCurveConst()->GetTimeRange(MinTime, MaxTime);

	float AccumulatedLength = 0;
	TArray<FVector2D> UV;
	UV.Reserve(Polyline.Num() * StepSize);
	for (int Step = 0; Step < Polyline.Num(); ++Step)
	{
		auto& RefPoint = Polyline[Step];

		FVector UpVector;
		FVector RightVector;
		FVector ForwardVector;
		double SinA;
		GetThreeVectors(Polyline.Vertices, Step, RightVector, UpVector, ForwardVector, SinA);

		for (int i = 0; i < StepSize; ++i)
		{
			FVector3d Vertex = RefPoint.Location - RightVector * (Times[i] - (MaxTime - MinTime) * 0.5) / SinA + UpVector * (Values[i] - MaxValue + RefPoint.CurbsHeight);
			DynamicMesh.AppendVertex(Vertex);

			UV.Add(FVector2D(AccumulatedLength, (AccumulatedValue.Last() - AccumulatedValue[i])) * UV0Scale);
		}

		if (Step != Polyline.Num() - 1)
		{
			AccumulatedLength += (Polyline[Step].Location - Polyline[Step + 1].Location).Length();
		}
	}

	for (int Step = 0; Step < Polyline.Num() - 1; ++Step)
	{
		for (int i = 0; i < StepSize - 1; ++i)
		{
			const FIndex3i T1 = {
				(Step + 0) * StepSize + 0 + i,
				(Step + 1) * StepSize + 0 + i,
				(Step + 0) * StepSize + 1 + i,
			};
			const FIndex3i T2 = {
				(Step + 1) * StepSize + 0 + i,
				(Step + 1) * StepSize + 1 + i,
				(Step + 0) * StepSize + 1 + i,
			};

			const int TID1 = DynamicMesh.AppendTriangle(T1);
			const int TID2 = DynamicMesh.AppendTriangle(T2);

			const FVector2D& UV1A = UV[T1.A];
			const FVector2D& UV1B = UV[T1.B];
			const FVector2D& UV1C = UV[T1.C];

			const FVector2D& UV2A = UV[T2.A];
			const FVector2D& UV2B = UV[T2.B];
			const FVector2D& UV2C = UV[T2.C];

			const int MinU1 = (int)(FMath::Min3(UV1A.X, UV1B.X, UV1C.X));
			const int MinU2 = (int)(FMath::Min3(UV2A.X, UV2B.X, UV2C.X));

			UV0Overlay->SetTriangle(TID1, FIndex3i{
				UV0Overlay->AppendElement(FVector2f(UV1A.X - MinU1, UV1A.Y)),
				UV0Overlay->AppendElement(FVector2f(UV1B.X - MinU1, UV1B.Y)),
				UV0Overlay->AppendElement(FVector2f(UV1C.X - MinU1, UV1C.Y)),
			});

			UV0Overlay->SetTriangle(TID2, FIndex3i{
				UV0Overlay->AppendElement(FVector2f(UV2A.X - MinU2, UV2A.Y)),
				UV0Overlay->AppendElement(FVector2f(UV2B.X - MinU2, UV2B.Y)),
				UV0Overlay->AppendElement(FVector2f(UV2C.X - MinU2, UV2C.Y)),
			});

			MaterialIDOverlay->SetValue(TID1, MaterialID);
			MaterialIDOverlay->SetValue(TID2, MaterialID);

			DynamicMesh.SetTriangleGroup(TID1, Polyline[Step].SectionIndex);
			DynamicMesh.SetTriangleGroup(TID2, Polyline[Step].SectionIndex);
		}
	}

	return true;
}

static TArray<FCerbRoadPosition> MakePolylineCerb(const TArray<FArrangementVertex3d>& Vertexes, const TArray<int>& VerticesIDs, const FProceduralPolygon* PolyFilter)
{
	TArray<FRoadPosition> Tmp = RoadPolygonUtils::MakePolyline(Vertexes, VerticesIDs, PolyFilter);
	TArray<FCerbRoadPosition> Ret;
	Ret.SetNumZeroed(Tmp.Num());
	for (int i = 0; i < Tmp.Num(); ++i)
	{
		*static_cast<FRoadPosition*>(&Ret[i]) = Tmp[i];
	}

	if (PolyFilter && PolyFilter->GetType() == ERoadPolygonType::RoadLane)
	{
		auto* LanePoly = static_cast<const FProceduralPolygon_RoadLane*>(PolyFilter);
		for (auto& It : Ret)
		{
			It.SectionIndex = LanePoly->GetSectionIndex();
			It.CurbsHeight = 0.5;// LanePoly->GetRoadZone().Get<FRoadZoneSidewalk>().DefaultHeight + 0.5; // Add 0.5cm so that the curb is slightly higher than the sidewalk
		}
	}
	return Ret;
}


void FCurbsOp::PreloadProfiles()
{
	// Runs on the game thread during op setup (see RoadComputeFactoryRegistry "RoadCurbs" factory).
	// FRoadZoneSidewalk::CurbProfile is a TSoftObjectPtr; resolve it here so CalculateResult (worker
	// thread) can read it via .Get() -- LoadSynchronous is unsafe off the game thread. BaseData is the
	// same shared instance the compute reads, so the assets become resident before the build runs.
	if (!BaseData)
	{
		return;
	}
	for (const auto& Poly : BaseData->Polygons)
	{
		if (!Poly)
		{
			continue;
		}
		if (const FRoadZoneSidewalk* Sidewalk = Poly->GetRoadZone().GetPtr<FRoadZoneSidewalk>())
		{
			Sidewalk->CurbProfile.LoadSynchronous(); // const; makes the curb profile asset resident
		}
	}
}

void FCurbsOp::CalculateResult(FProgressCancel* Progress)
{
	ResultInfo.Result = EGeometryResultType::InProgress;

	if (!BaseData || BaseData->ResultInfo.HasFailed())
	{
		ResultInfo.SetFailed(LOCTEXT("CalculateResultFail_Input", "FCurbsOp: input data is faild"));
		return;
	}

	FRoadCurbArrangemen Arrangemen;

	// Add road lanes mark attributes to Arrangemen
	for (const auto& Poly : BaseData->Polygons)
	{
		if (auto* RoadLaneSidewalk = Poly->GetRoadZone().GetPtr<FRoadZoneSidewalk>())
		{
			auto AddToArrangemen = [this, &Poly, RoadLaneSidewalk, &Arrangemen](const TArray<int>& VIDs, bool bReverse)
			{
				FRoadLanePolylineCurb Polyline;
				Polyline.Vertices = MakePolylineCerb(BaseData->Vertices3d, VIDs, Poly.Get());
				if (bReverse)
				{
					Algo::Reverse(Polyline.Vertices);
				}
				Polyline.Profile = RoadLaneSidewalk->CurbProfile.Get();
				Polyline.OverrideMaterial = RoadLaneSidewalk->bOverrideCurbMaterial ? RoadLaneSidewalk->OverrideCurbMaterial : nullptr;
				if (Polyline.Vertices.Num() > 1)
				{
					const double ArrangemenTolerance = 1.0;
					Arrangemen.Insert(MoveTemp(Polyline), ArrangemenTolerance);
				}
			};

			if (Poly->GetType() == ERoadPolygonType::RoadLane)
			{
				auto *LanePoly = static_cast<FProceduralPolygon_RoadLane*>(Poly.Get());
				const bool bIsRight = LanePoly->GetLaneIndex() >= 0;
				if (RoadLaneSidewalk->bBeginCurb && !LanePoly->IsLoop())
				{
					AddToArrangemen(LanePoly->BeginCapVertices, bIsRight);
				}
				if (RoadLaneSidewalk->bEndCurb && !LanePoly->IsLoop())
				{
					AddToArrangemen(LanePoly->EndCapVertices, !bIsRight);
				}
				if (RoadLaneSidewalk->bInsideCurb)
				{
					AddToArrangemen(LanePoly->InsideLineVertices, !bIsRight);
				}
				if (RoadLaneSidewalk->bOutsideCurb)
				{
					AddToArrangemen(LanePoly->OutsideLineVertices, bIsRight);
				}
			}
			else if (Poly->GetType() == ERoadPolygonType::SplineLoop)
			{
				auto* LoopPoly = static_cast<FProceduralPolygon_RoadLoop*>(Poly.Get());
				if (RoadLaneSidewalk->bInsideCurb || RoadLaneSidewalk->bOutsideCurb)
				{
					AddToArrangemen(LoopPoly->LineVertices, true);
				}
			}
		}
	}

	MeshUtils::EnableDefaultAttributes(*ResultMesh, true, true, true, true, 1);

	OpUtils::FMaterialSlotMap MaterialSlotMap{};

	for (const auto& It : Arrangemen.Polylines)
	{
		if (It.Profile)
		{
			TObjectPtr<UMaterialInterface> Material = OverrideMaterial ? OverrideMaterial : It.Profile->DefaultMaterial;
			if (It.OverrideMaterial)
			{
				Material = It.OverrideMaterial;
			}

			const int MaterialID = MaterialSlotMap.AddMaterial(Material);

			const double VScaleFactor = 0.001;
			FDynamicMesh3 DynamicMesh;
			if (!MakeCurb(It, It.Profile,  DynamicMesh, MaterialID, UV0Scale))
			{
				ResultInfo.AddWarning({ 0, LOCTEXT("CalculateResultWarning_MarkStruct", "Mark: Can't build curb mesh") });
			}

			if (DynamicMesh.VertexCount() > 0 && DynamicMesh.TriangleCount() > 0)
			{
				MeshUtils::AppendMesh(*ResultMesh, DynamicMesh);
			}

		}
	}

	// Resolve materials slot name
	ResultMaterialSlots = MaterialSlotMap.GetSlots();

	// ========================== Compute Normals ==========================
	FMeshNormals::QuickComputeVertexNormals(*ResultMesh);
	FMeshNormals::InitializeOverlayToPerVertexNormals(ResultMesh->Attributes()->PrimaryNormals(), true);
	FMeshNormals::QuickRecomputeOverlayNormals(*ResultMesh);

	ResultInfo.SetSuccess();
}



#undef LOCTEXT_NAMESPACE
