/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "TriangulateRoadOp.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Assets/RoadMarkProfile.h"
#include "Assets/RoadLaneAttributeMark.h"
#include "RoadMeshBuild/RoadLanePolylineArrangement.h"
#include "Utils/RoadUtils.h"
#include "Utils/GeomUtils.h"
#include "Utils/MeshUtils.h"

#define LOCTEXT_NAMESPACE "MarksOp"

using namespace MetaRoad;

struct FMarkRoadPosition: public FRoadPosition
{
	int SectionIndex = 0; // For dynamic mesh GroupID
};

struct FRoadLanePolylineMark : public TRoadLanePolyline<FMarkRoadPosition, FRoadLanePolylineMark>
{
	FRoadLanePolylineMark() = default;
	FRoadLanePolylineMark(const TArray<FMarkRoadPosition>& Vertices)
		: TRoadLanePolyline<FMarkRoadPosition, FRoadLanePolylineMark>(Vertices)
	{
	}

	FRoadLanePolylineMark(TArray<FMarkRoadPosition>&& Vertices)
		: TRoadLanePolyline<FMarkRoadPosition, FRoadLanePolylineMark>(MoveTemp(Vertices))
	{
	}

	TInstancedStruct<FRoadLaneMarkProfile> Profile; //URoadMarkProfile* Profile = nullptr;
	UMaterialInterface* OverrideMaterial = nullptr;

	virtual bool CanAppend(const FRoadLanePolylineMark& Other, EAppandMode AppandMode, double Tolerance) const override
	{
		if (Profile != Other.Profile)
		{
			return false;
		}

		if (OverrideMaterial != Other.OverrideMaterial)
		{
			return false;
		}

		return TRoadLanePolyline<FMarkRoadPosition, FRoadLanePolylineMark>::CanAppend(Other, AppandMode, Tolerance);
	}
};

using FRoadMarkArrangemen = TRoadLanePolylineArrangement<FRoadLanePolylineMark>;


static bool MakeMarkMesh(const TArray<FMarkRoadPosition>& InVertices, double S0, double S1, double ZOffset, double ROffset, double Width, double VScaleFactor, const FColor & VertectColor, uint8 MaterialID, FDynamicMesh3& DynamicMesh)
{
	FRoadLanePolylineMark SubLine;
	const TArray<FMarkRoadPosition>* Vertices;
	if (S0 < 0 || S1 < 0)
	{
		Vertices = &InVertices;

	}
	else
	{
		SubLine.Vertices = GeomUtils::GetSubPolyline(InVertices, S0, S1);
		Vertices = &SubLine.Vertices;
	}

	if (Vertices->Num() < 2)
	{
		return false;
	}

	MeshUtils::EnableDefaultAttributes(DynamicMesh, true, true, true, true, 1);

	const int StartVertexIndex = DynamicMesh.MaxVertexID();
	const FLinearColor LinearColor(VertectColor);
	const FVector4f FloatColor(LinearColor.R, LinearColor.G, LinearColor.B, LinearColor.A);
	auto* ColorOverlay = DynamicMesh.Attributes()->PrimaryColors();
	auto* MaterialIDOverlay = DynamicMesh.Attributes()->GetMaterialID();
	auto* UV0Overlay = DynamicMesh.Attributes()->GetUVLayer(0);

	for (int i = 0; i < Vertices->Num(); ++i)
	{
		const auto& It = (*Vertices)[i];

		FVector UpVector;
		FVector RightVector;
		FVector ForwardVector;
		double SinA;
		GetThreeVectors(*Vertices, i, RightVector, UpVector, ForwardVector, SinA);

		FVertexInfo VertexA;
		VertexA.bHaveN = true;
		//VertexA.bHaveUV = true;
		VertexA.bHaveC = true;
		VertexA.Position = It.Location + RightVector * ((ROffset - Width * 0.5) / SinA) + UpVector * ZOffset; 
		VertexA.Normal = FVector3f(UpVector);
		//VertexA.UV = FVector2f(0.0, S0 * VScaleFactor);
		//VertexA.Color = 

		FVertexInfo VertexB;
		VertexB.bHaveN = true;
		//VertexB.bHaveUV = true;
		VertexB.Position = It.Location + RightVector * ((ROffset + Width * 0.5) / SinA) + UpVector * ZOffset;
		VertexB.Normal = FVector3f(UpVector);
		//VertexB.UV = FVector2f(1.0, S0 * VScaleFactor);
		//VertexB.Color = 

		DynamicMesh.AppendVertex(VertexA);
		DynamicMesh.AppendVertex(VertexB);

		UV0Overlay->AppendElement(FVector2f(0.0, It.SOffset * VScaleFactor));
		UV0Overlay->AppendElement(FVector2f(1.0, It.SOffset * VScaleFactor));

		ColorOverlay->AppendElement(FloatColor);
		ColorOverlay->AppendElement(FloatColor);
	}

	for (int i = 0; i < Vertices->Num() - 1; ++i)
	{
		FIndex3i T1 = FIndex3i{ StartVertexIndex + i * 2 + 0, StartVertexIndex + i * 2 + 1, StartVertexIndex + i * 2 + 2 };
		FIndex3i T2 = FIndex3i{ StartVertexIndex + i * 2 + 1, StartVertexIndex + i * 2 + 3, StartVertexIndex + i * 2 + 2 };

		int TID1 = DynamicMesh.AppendTriangle(T1);
		int TID2 = DynamicMesh.AppendTriangle(T2);

		UV0Overlay->SetTriangle(TID1, T1);
		UV0Overlay->SetTriangle(TID2, T2);

		ColorOverlay->SetTriangle(TID1, T1);
		ColorOverlay->SetTriangle(TID2, T2);

		MaterialIDOverlay->SetValue(TID1, (int32)MaterialID);
		MaterialIDOverlay->SetValue(TID2, (int32)MaterialID);

		DynamicMesh.SetTriangleGroup(TID1, (*Vertices)[i].SectionIndex);
		DynamicMesh.SetTriangleGroup(TID2, (*Vertices)[i].SectionIndex);
	}

	return true;
};


static bool MakeMarkMeshBroken(const TArray<FMarkRoadPosition>& Vertices, double ZOffset, double ROffset, double Width, double Long, double Gap, double VScaleFactor, const FColor& VertectColor, uint8 MaterialID, FDynamicMesh3& DynamicMesh)
{
	if (Vertices.Num() < 2)
	{
		return false;
	}

	double Length = 0; 

	for (int i = 0; i < Vertices.Num() - 1; ++i)
	{
		Length += (Vertices[i].Location - Vertices[i + 1].Location).Length();
	}
	
	if (Length < Long)
	{
		const double S0 = Vertices[0].SOffset;
		const double S1 = Vertices.Last().SOffset;
		return MakeMarkMesh(Vertices, S0, S1, ZOffset, ROffset, Width, VScaleFactor, VertectColor, MaterialID, DynamicMesh);
	}

	const double SectionRation = Long / (Long + Gap);
	const int NumSections = FMath::RoundToInt(Length / (Long + Gap));
	const double SectionLength = Length / NumSections;
	const double AlignedLong = SectionLength * SectionRation;
	//const double AlignedGap = SectionLength * (1.0 - SectionRation);

	for (int i = 0; i < NumSections; ++i)
	{

		const double L0 = i * SectionLength;
		const double L1 = i * SectionLength + AlignedLong;
		MakeMarkMesh(Vertices, L0 , L1, ZOffset, ROffset, Width, VScaleFactor, VertectColor, MaterialID, DynamicMesh);
	}

	return true;
}


static bool MakeMarkMeshFromProfile(const TArray<FMarkRoadPosition>& Vertices, double ZOffset, double ROffset, const TInstancedStruct<FRoadLaneMarkProfile>& Profile, double VScaleFactor, TObjectPtr<UMaterialInterface> OverrideGlobalMaterial, TObjectPtr<UMaterialInterface> OverrideLocalMaterial, FDynamicMesh3& DynamicMesh, OpUtils::FMaterialSlotMap& MaterialSlotMap)
{
	if (auto* SolidProfile = Profile.GetPtr<FRoadLaneMarkProfileSolid>())
	{
		TObjectPtr<UMaterialInterface> Material = OverrideGlobalMaterial ? OverrideGlobalMaterial : SolidProfile->DefaultMaterial;
		if (OverrideLocalMaterial)
		{
			Material = OverrideLocalMaterial;
		}
		const int MaterialID = MaterialSlotMap.AddMaterial(Material);
		return MakeMarkMesh(Vertices, -1, -1, ZOffset, ROffset, SolidProfile->Width, VScaleFactor, SolidProfile->VertexColor, MaterialID, DynamicMesh);
	}
	else if (auto* BrokedProfile = Profile.GetPtr<FRoadLaneMarkProfileBroked>())
	{
		TObjectPtr<UMaterialInterface> Material = OverrideGlobalMaterial ? OverrideGlobalMaterial : BrokedProfile->DefaultMaterial;
		if (OverrideLocalMaterial)
		{
			Material = OverrideLocalMaterial;
		}
		const int MaterialID = MaterialSlotMap.AddMaterial(Material);
		return MakeMarkMeshBroken(Vertices, ZOffset, ROffset, BrokedProfile->Width, BrokedProfile->Long, BrokedProfile->Gap, VScaleFactor, BrokedProfile->VertexColor, MaterialID, DynamicMesh);
	}
	else if (auto* DoubleProfile = Profile.GetPtr<FRoadLaneMarkProfileDouble>())
	{
		MakeMarkMeshFromProfile(Vertices, ZOffset, ROffset - DoubleProfile->Gap * 0.5, DoubleProfile->Left, VScaleFactor, OverrideGlobalMaterial, OverrideLocalMaterial, DynamicMesh, MaterialSlotMap);
		MakeMarkMeshFromProfile(Vertices, ZOffset, ROffset + DoubleProfile->Gap * 0.5, DoubleProfile->Right, VScaleFactor, OverrideGlobalMaterial, OverrideLocalMaterial, DynamicMesh, MaterialSlotMap);
		return true;
	}
	return false;
}

static int FindMinIndex(const TArray<FMarkRoadPosition>& Polyline) 
{
	if (Polyline.IsEmpty()) 
	{
		return INDEX_NONE;
	}

	const FMarkRoadPosition* MinVal = &Polyline[0];
	int MinIndex = 0;

	for (size_t i = 1; i < Polyline.Num(); ++i) 
	{
		if (Polyline[i].SOffset < MinVal->SOffset) 
		{
			MinVal = &Polyline[i];
			MinIndex = i;
		}
	}
	return MinIndex;
}

static void NormalizPolylineBySOffset(TArray<FMarkRoadPosition>& Polyline)
{
	if (Polyline.Num() == 0)
	{
		return;
	}

	int MinInd = FindMinIndex(Polyline);
	if (MinInd > 0)
	{
		Algo::Rotate(Polyline, MinInd);
	}

	if (Polyline.Num() > 2 && Polyline[1].SOffset > Polyline[2].SOffset)
	{
		Algo::Reverse(Polyline);
		MinInd = FindMinIndex(Polyline);
		if (MinInd > 0)
		{
			Algo::Rotate(Polyline, MinInd);
		}
	}
}

static TArray<FMarkRoadPosition> MakePolylineMark(const TArray<FArrangementVertex3d>& Vertexes, const TArray<int>& VerticesIDs, const FProceduralPolygon* PolyFilter)
{
	TArray<FRoadPosition> Tmp = RoadPolygonUtils::MakePolyline(Vertexes, VerticesIDs, PolyFilter);
	TArray<FMarkRoadPosition> Ret;
	Ret.SetNumZeroed(Tmp.Num());
	for (int i = 0; i < Tmp.Num(); ++i)
	{
		*static_cast<FRoadPosition*>(&Ret[i]) = Tmp[i];
	}

	if (PolyFilter && PolyFilter->GetType() == ERoadPolygonType::RoadLane)
	{
		auto* LanePoly = static_cast<const FProceduralPolygon_RoadLane*>(PolyFilter);
		for (auto& It: Ret)
		{
			It.SectionIndex = LanePoly->GetSectionIndex();
		}
		// For closed loop splines the last vertex is the seam repeated with SOffset≈0.
		// Assign SplineLength so the marking closes the loop instead of being discarded by NormalizePolyline.
		if (LanePoly->IsLoop() && Ret.Num() > 1)
		{
			Ret.Last().SOffset = LanePoly->GetEndOffset();
		}
	}

	NormalizPolylineBySOffset(Ret);
	return Ret;
}


// ---------------------------------------------------------------------------------------------------------------------------------

void FMarksOp::PreloadProfiles()
{
	// Runs on the game thread during op setup (see RoadComputeFactoryRegistry "RoadMarks" factory).
	// FRoadLaneMark::Profile is a TSoftObjectPtr; resolve it here so CalculateResult (worker thread)
	// can read it via .Get() -- LoadSynchronous is unsafe off the game thread. BaseData is the same
	// shared instance the compute reads, so the assets become resident before the build runs.
	if (!BaseData)
	{
		return;
	}
	for (const auto& Poly : BaseData->Polygons)
	{
		if (!Poly || Poly->GetType() != ERoadPolygonType::RoadLane)
		{
			continue;
		}
		const auto* LanePoly = static_cast<const FProceduralPolygon_RoadLane*>(Poly.Get());
		if (const FRoadLaneAttribute* MarkAttr = LanePoly->GetLaneAttributes().Find(URoadLaneAttributeMarkDescriptor::StaticClass()))
		{
			for (const FRoadLaneAttributeKey& Key : MarkAttr->Keys)
			{
				Key.GetValue<FRoadLaneMark>().Profile.LoadSynchronous(); // const; makes the profile asset resident
			}
		}
	}
}

void FMarksOp::CalculateResult(FProgressCancel* Progress)
{
#define CHECK_CANCLE() if (Progress && Progress->Cancelled()) { ResultInfo.Result = EGeometryResultType::Cancelled; return; }

	ResultInfo.Result = EGeometryResultType::InProgress;

	if (!BaseData || BaseData->ResultInfo.HasFailed())
	{
		ResultInfo.SetFailed(LOCTEXT("CalculateResultFail_Input", "FMarksOp: input data is faild"));
		return;
	}

	if (Progress && Progress->Cancelled())
	{
		ResultInfo.Result = EGeometryResultType::Cancelled;
		return;
	}

	FRoadMarkArrangemen Arrangemen;
	const double ArrangemenTolerance = 1.0;

	// ========================== Add road lanes mark attributes to Arrangemen ==========================
	for (const auto& Poly : BaseData->Polygons)
	{
		if (Poly->GetType() == ERoadPolygonType::RoadLane)
		{
			auto* LanePoly = static_cast<FProceduralPolygon_RoadLane*>(Poly.Get());
			const auto& Section = LanePoly->GetSection();
			if (const auto* FoundAttribute = LanePoly->GetLaneAttributes().Find(URoadLaneAttributeMarkDescriptor::StaticClass()))
			{
				for (int AttributeIndex = 0; AttributeIndex < FoundAttribute->Keys.Num(); ++AttributeIndex)
				{
					const auto& MarkKey = FoundAttribute->Keys[AttributeIndex];
					const auto& MarkValue = MarkKey.GetValue<FRoadLaneMark>();
					if (MarkValue.Profile.Get())
					{
						const double SOffsetStart = MarkKey.SOffset + Section.SOffset;
						const double SOffsetEnd = (AttributeIndex < FoundAttribute->Keys.Num() - 1) ? FoundAttribute->Keys[AttributeIndex + 1].SOffset + Section.SOffset : LanePoly->GetEndOffset();

						auto& LineVertices = LanePoly->GetLaneIndex() == 0 ? LanePoly->InsideLineVertices : LanePoly->OutsideLineVertices;
						if (ensure(LineVertices.Num()))
						{
							FRoadLanePolylineMark LineMark;
							LineMark.Vertices = GeomUtils::GetSubPolyline(::MakePolylineMark(BaseData->Vertices3d, LineVertices, LanePoly), SOffsetStart, SOffsetEnd);
							LineMark.Profile = MarkValue.GetProfile();
							if (LineMark.Vertices.Num() > 1)
							{
								Arrangemen.Insert(MoveTemp(LineMark), ArrangemenTolerance);
							}
						}
					}
				}
			}
		}


		CHECK_CANCLE();
	}

	// ========================== Convert polylines S-offset to L-offset ==========================
	for (auto& It : Arrangemen.Polylines)
	{
		if (It.Num() > 0)
		{
			double Length = 0;
			It[0].SOffset = Length;
			for (int i = 1; i < It.Num(); ++i)
			{
				const auto& PtA = It[i];
				const auto& PtB = It[i - 1];
				Length += (PtB.Location - PtA.Location).Length();
				It[i].SOffset = Length;
			}
		}
	}

	// ========================== Create mesh ==========================
	MeshUtils::EnableDefaultAttributes(*ResultMesh, true, true, true, true, 1);

	OpUtils::FMaterialSlotMap MaterialSlotMap{};

	for (const auto& It : Arrangemen.Polylines)
	{
		if (It.Profile.IsValid())
		{
				const double VScaleFactor = 0.001;
				FDynamicMesh3 DynamicMesh;
				if (!MakeMarkMeshFromProfile(It.Vertices, MarkOffset, 0.0, It.Profile, VScaleFactor, OverrideMaterial, It.OverrideMaterial, DynamicMesh, MaterialSlotMap))
				{
					ResultInfo.AddWarning({ 0, FText::Format(LOCTEXT("CalculateResultWarning_MarkStruct", "Mark: Can't build mark for unqnown FRoadLaneMarkProfile struct: {0}"), FText::FromString(It.Profile.GetScriptStruct()->GetName())) });
				}

				if (DynamicMesh.VertexCount() > 0 && DynamicMesh.TriangleCount() > 0)
				{
					MeshUtils::AppendMesh(*ResultMesh, DynamicMesh);
				}
		}
		 CHECK_CANCLE();
	} 
	 
	// ========================== Resolve materials slot name ==========================
	ResultMaterialSlots = MaterialSlotMap. GetSlots();

	ResultInfo.SetSuccess();

#undef CHECK_CANCLE
}

#undef LOCTEXT_NAMESPACE
