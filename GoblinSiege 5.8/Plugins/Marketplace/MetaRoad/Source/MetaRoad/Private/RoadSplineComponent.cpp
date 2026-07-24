/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadSplineComponent.h"
#include "Engine/Engine.h"
#include "SceneManagement.h"
#include "DynamicMeshBuilder.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "MetaRoadSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MetaRoadModule.h"
#include "Landscape.h"
#include "MetaRoadSubsystem.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

#define LOCTEXT_NAMESPACE "URoadSplineComponent"

/*
 * Fancy math to recalculate tangents to turn into circle
 */
static double CalcTangentMultiplier(const float InRadius, const float InRotInc)
{
	static constexpr double A = .5f;
	static constexpr double A2 = A * A;
	static constexpr double A3 = A2 * A;

	// Use first and second keys added as a sample calculation
	const FVector T0 = FVector::ForwardVector;
	const FVector T1 = T0.RotateAngleAxis(InRotInc, FVector::UpVector);
	const FVector P0 = FVector::RightVector * InRadius;
	const FVector P1 = P0.RotateAngleAxis(InRotInc, FVector::UpVector);

	// Calculate the difference between the actual interpolated midpoint and expected interpolated midpoint
	const FVector ActualVal = FMath::CubicInterp(P0, T0, P1, T1, A);
	const FVector ExpectedVal = P0.RotateAngleAxis(InRotInc * A, FVector::UpVector);
	const double Diff = (ActualVal.X - ExpectedVal.X);

	// Do a partial calculation of the cubic interpolation equation
	static constexpr double C1 = (A3 - (2 * A2) + A), C2 = (A3 - A2);
	const double PartialInterp = -1.f * ((C1 * T0.X) + (C2 * T1.X));

	// Calculate the final multiplier to multiply to all normalized tangents
	return FMath::IsNearlyZero(PartialInterp) ? 1.f : ((Diff / PartialInterp) + 1.f);
}

static void RemoveOutsideRange(TArray<double>& Array, double S0, double S1)
{
	TArray<double> FilteredArray;
	for (double Value : Array)
	{
		if (Value >= S0 && Value <= S1)
		{
			FilteredArray.Add(Value);
		}
	}
	Array = FilteredArray;
}

static void RemoveEmptySegments(TArray<double>& Array, double Tolerance = KINDA_SMALL_NUMBER)
{
	for (auto It = Array.CreateIterator() + 1; It; ++It)
	{
		if (*It - *(It - 1) < Tolerance)
		{
			It.RemoveCurrent();
		}
	}
}

static bool IsNearlyEqual(const FRoadPosition& A, const FRoadPosition& B, double Tol = UE_SMALL_NUMBER)
{
	return FMath::IsNearlyZero(FVector::Distance(A.Location, B.Location), Tol) && FMath::IsNearlyEqual(A.SOffset, B.SOffset, Tol);
}

URoadSplineMetadata::URoadSplineMetadata(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URoadSplineMetadata::InsertPoint(int32 Index, float t, bool bClosedLoop)
{
	check(Spline.IsValid());
	check(PointTypes.Num() == Spline->GetNumberOfSplinePoints() - 1);
	check(Index >= 0);

	const int PrevIndex = Spline->GetPrevPoint(Index);

	if (PrevIndex >= 0)
	{
		PointTypes.Insert(ERoadSplinePointTypeOverride::Inherited, Index);

		if (PointTypes[PrevIndex] == ERoadSplinePointTypeOverride::Arc)
		{
			const int32 EndPointIndex = PrevIndex;
			const int32 StartPointIndex = Spline->GetPrevPoint(EndPointIndex);

			if (EndPointIndex >= 0 && StartPointIndex >= 0)
			{
				PointTypes[StartPointIndex] = ERoadSplinePointTypeOverride::Inherited;
				PointTypes[EndPointIndex] = ERoadSplinePointTypeOverride::Inherited;
			}
		}
	}


}

void URoadSplineMetadata::UpdatePoint(int32 Index, float t, bool bClosedLoop) {}

void URoadSplineMetadata::AddPoint(float InputKey)
{
	PointTypes.Add(ERoadSplinePointTypeOverride::Inherited);
}

void URoadSplineMetadata::RemovePoint(int32 Index)
{
	if (PointTypes.IsValidIndex(Index))
	{
		PointTypes.RemoveAt(Index);
	}
}

void URoadSplineMetadata::DuplicatePoint(int32 Index)
{
	if (PointTypes.IsValidIndex(Index))
	{
		PointTypes.Insert(PointTypes[Index], Index);
	}
}

void URoadSplineMetadata::CopyPoint(const USplineMetadata* FromSplineMetadata, int32 FromIndex, int32 ToIndex)
{
	if (const URoadSplineMetadata* FromRoadMetadata = Cast<URoadSplineMetadata>(FromSplineMetadata))
	{
		if (FromRoadMetadata->PointTypes.IsValidIndex(FromIndex) && PointTypes.IsValidIndex(ToIndex))
		{
			PointTypes[ToIndex] = FromRoadMetadata->PointTypes[FromIndex];
		}
	}
}

void URoadSplineMetadata::Reset(int32 NumPoints)
{
	PointTypes.Reset(NumPoints);
	PointTypes.SetNum(NumPoints);
}

void URoadSplineMetadata::Fixup(int32 NumPoints, USplineComponent* SplineComp)
{
	// Preserve existing entries; only grow/shrink to match the point count.
	if (PointTypes.Num() != NumPoints)
	{
		PointTypes.SetNum(NumPoints);
	}
}


//===========================================================================================================


URoadSplineComponent::URoadSplineComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//bUseEditorCompositing = true;
	//DepthPriorityGroup = SDPG_Foreground;

	bHiddenInGame = true;

	SplineMetadata = CreateDefaultSubobject<URoadSplineMetadata>(TEXT("DriveSplineMetadataMetadata"));
	SplineMetadata->Spline = this;

	PredecessorConnection = CreateDefaultSubobject<URoadConnection>(TEXT("PredecessorConnection"));
	SuccessorConnection = CreateDefaultSubobject<URoadConnection>(TEXT("SuccessorConnection"));


	SplineCurves.Position.Points[1].OutVal = FVector(1000, 0, 0);
	UpdateSpline();

	//UE_LOG(LogMetaRoad, Warning, TEXT("URoadSplineComponent::URoadSplineComponent(%s)"), *GetName());
}

URoadSplineComponent::~URoadSplineComponent()
{
	//UE_LOG(LogMetaRoad, Warning, TEXT("URoadSplineComponent::~URoadSplineComponent(%s)"), *GetName());
}

void URoadSplineComponent::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		FixUpSegments();
		ValidateConnections();
	}

	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		FixUpSegments();
		ValidateConnections();
	}
	auto Version = FMetaRoadModule::GetVersion();

	uint8 MajorVer = (uint8)Version.Get<0>();
	uint8 MinorVer = (uint8)Version.Get<1>();
	uint8 PatchVer = (uint8)Version.Get<2>();
	uint8 Reserved = 0;

	

	Ar << MajorVer;
	Ar << MinorVer;
	Ar << PatchVer;
	Ar << Reserved;
}

void URoadSplineComponent::ValidateConnections()
{
	TSet<ULaneConnection*> ConnectionSet;
	auto ValidateUnique = [&ConnectionSet](TObjectPtr< ULaneConnection>& Connection)
	{
		if(!Connection)
		{
			return;
		}
		if (ConnectionSet.Contains(Connection))
		{
			UE_LOG(LogMetaRoad, Error, TEXT("URoadSplineComponent::ValidateConnections(); Found not unque connection for %s"), *Connection->GetPathName());
			Connection = nullptr;
		}
		else
		{
			ConnectionSet.Add(Connection);
		}
	};
	for (auto& Section : GetRoadLayout().Sections)
	{
		for (auto& It : Section.Left)
		{
			ValidateUnique(It.PredecessorConnection);
			ValidateUnique(It.SuccessorConnection);
		}
		for (auto& It : Section.Right)
		{
			ValidateUnique(It.PredecessorConnection);
			ValidateUnique(It.SuccessorConnection);
		}
	}

	UpdateRoadLayout();
}

void URoadSplineComponent::UpdateSpline()
{	
	//SetClosedLoop(false, false);
	FixUpSegments();
	UpdateAutoTangents();
	Super::UpdateSpline();
	UpdateLaneSectionBounds();
	/*
	for (auto& Section : Sections)
	{
		for (auto& Lane : Section.Left)
		{
			Lane.Width.AutoSetTangents();
		}
		for (auto& Lane : Section.Right)
		{
			Lane.Width.AutoSetTangents();
		}
	}
	*/

	MarkRoadStateDirty();
}

void URoadSplineComponent::UpdateSpline(int EditingPointIndex)
{
	//SetClosedLoop(false, false);
	FixUpSegments();
	UpdateAutoTangents(EditingPointIndex);
	Super::UpdateSpline();
	UpdateLaneSectionBounds();
	/*
	for (auto& Section : Sections)
	{
		for (auto& Lane : Section.Left)
		{
			Lane.Width.AutoSetTangents();
		}
		for (auto& Lane : Section.Right)
		{
			Lane.Width.AutoSetTangents();
		}
	}
	*/

	MarkRoadStateDirty();
}

ERoadSplinePointType URoadSplineComponent::GetRoadSplinePointType(int32 PointIndex) const
{
	if ((PointIndex >= 0) && (PointIndex < SplineCurves.Position.Points.Num()))
	{
		if (SplineMetadata->PointTypes.IsValidIndex(PointIndex))
		{
			if (SplineMetadata->PointTypes[PointIndex] == ERoadSplinePointTypeOverride::Arc)
			{
				return ERoadSplinePointType::Arc;
			}
			if (SplineMetadata->PointTypes[PointIndex] == ERoadSplinePointTypeOverride::Broken)
			{
				return ERoadSplinePointType::Broken;
			}
		}
		return static_cast<ERoadSplinePointType>(GetSplinePointType(PointIndex));
	}

	return ERoadSplinePointType::Constant;
}

void URoadSplineComponent::SetRoadSplinePointType(int32 PointIndex, ERoadSplinePointType Mode, bool bUpdateSpline)
{
	FixUpSegments();

	if ((PointIndex >= 0) && (PointIndex < SplineCurves.Position.Points.Num()))
	{
		const bool bWasBroken = SplineMetadata->PointTypes.IsValidIndex(PointIndex)
			&& SplineMetadata->PointTypes[PointIndex] == ERoadSplinePointTypeOverride::Broken;

		if (Mode == ERoadSplinePointType::Arc)
		{
			SplineMetadata->PointTypes[PointIndex] = ERoadSplinePointTypeOverride::Arc;
			SplineCurves.Position.Points[PointIndex].InterpMode = EInterpCurveMode::CIM_CurveUser;
			const int NextPoint = GetNextPoint(PointIndex);
			if (NextPoint >= 0)
			{
				SplineCurves.Position.Points[NextPoint].InterpMode = EInterpCurveMode::CIM_CurveUser;
			}
		}
		else if (Mode == ERoadSplinePointType::Broken)
		{
			// Broken: arrive/leave tangents are edited independently. Force user tangents so the
			// corner is preserved (this also clears any Arc override).
			SplineMetadata->PointTypes[PointIndex] = ERoadSplinePointTypeOverride::Broken;
			SplineCurves.Position.Points[PointIndex].InterpMode = EInterpCurveMode::CIM_CurveUser;
		}
		else
		{
			SplineMetadata->PointTypes[PointIndex] = ERoadSplinePointTypeOverride::Inherited;
			SplineCurves.Position.Points[PointIndex].InterpMode = ConvertSplinePointTypeToInterpCurveMode(static_cast<ESplinePointType::Type>(Mode));

			// Welding a broken point back to a manual curve: average the two tangent directions
			// (each keeps its magnitude). Other types recompute tangents anyway, so skip them.
			if (bWasBroken && Mode == ERoadSplinePointType::CurveCustomTangent)
			{
				FInterpCurvePoint<FVector>& Point = SplineCurves.Position.Points[PointIndex];
				const FVector LeaveN = Point.LeaveTangent.GetSafeNormal();
				const FVector ArriveN = Point.ArriveTangent.GetSafeNormal();
				FVector Dir = (LeaveN + ArriveN).GetSafeNormal();
				if (Dir.IsNearlyZero())
				{
					// Tangents are anti-parallel; fall back to the leave direction.
					Dir = LeaveN;
				}
				Point.LeaveTangent = Dir * Point.LeaveTangent.Size();
				Point.ArriveTangent = Dir * Point.ArriveTangent.Size();
			}
		}
		if (bUpdateSpline)
		{
			UpdateSpline();
		}
	}
}

bool URoadSplineComponent::IsRoadSplinePointBroken(int32 PointIndex) const
{
	return SplineMetadata->PointTypes.IsValidIndex(PointIndex)
		&& SplineMetadata->PointTypes[PointIndex] == ERoadSplinePointTypeOverride::Broken;
}

void URoadSplineComponent::FixUpSegments()
{
	// Delegate per-point array sizing to the metadata, which preserves existing entries
	// instead of resetting them.
	SplineMetadata->Fixup(GetNumberOfSplinePoints(), this);
}

int URoadSplineComponent::GetNextPoint(int PointIndex) const
{
	if (IsClosedLoop())
	{
		return (PointIndex + 1) % GetNumberOfSplinePoints();
	}
	else if (PointIndex < (GetNumberOfSplinePoints() - 2))
	{
		return PointIndex + 1;
	}
	else
	{
		return -1;
	}
};

int URoadSplineComponent::GetPrevPoint(int PointIndex) const
{
	if (IsClosedLoop())
	{
		return (PointIndex == 0) ? GetNumberOfSplinePoints() - 1 : PointIndex - 1;
	}
	else if (PointIndex > 0)
	{
		return PointIndex - 1;
	}
	else
	{
		return -1;
	}
};

void URoadSplineComponent::UpdateAutoTangents(int EditingPointIndex)
{
	if (GetNumberOfSplineSegments() <= 0)
	{
		return; 
	}

	FixUpSegments();

	// Set CIM_CurveUser for all arc segments
	for (int PointIndex = 0; PointIndex < GetNumberOfSplineSegments(); ++PointIndex)
	{
		if (SplineMetadata->PointTypes[PointIndex] == ERoadSplinePointTypeOverride::Arc)
		{
			SplineCurves.Position.Points[PointIndex].InterpMode = CIM_CurveUser;
			const int NextPointIndex = GetNextPoint(PointIndex);
			if (NextPointIndex >= 0)
			{
				SplineCurves.Position.Points[NextPointIndex].InterpMode = CIM_CurveUser;
			}
		}
	}

	// Normalize all Linear segments
	/*
	for (int PointIndex = 0; PointIndex < GetNumberOfSplineSegments(); ++PointIndex)
	{
		if (GetRoadSplinePointType(PointIndex) == ERoadSplinePointType::Linear)
		{
			AdjustLineSegment(PointIndex);
		}
	}
	*/
	
	// Normalize all Arc segments
	if (EditingPointIndex == INDEX_NONE)
	{
		for (int PointIndex = 0; PointIndex < GetNumberOfSplineSegments(); ++PointIndex)
		{
			if (GetRoadSplinePointType(PointIndex) == ERoadSplinePointType::Arc)
			{
				const int NextPointIndex = GetNextPoint(PointIndex);
				const int PrePointIndex = GetPrevPoint(PointIndex);

				const bool bPinStartTangent = PrePointIndex >= 0 && GetRoadSplinePointType(PrePointIndex) == ERoadSplinePointType::Linear;
				const bool bPinEndTangent = NextPointIndex >= 0 && GetRoadSplinePointType(NextPointIndex) == ERoadSplinePointType::Linear;

				if (bPinStartTangent && bPinEndTangent)
				{
					AdjustArcSegment(PointIndex, EComputeArcMode::AdjEndTangent);
				}
				else if (bPinStartTangent)
				{
					AdjustArcSegment(PointIndex, EComputeArcMode::AdjEndTangent);
				}
				else if (bPinEndTangent)
				{
					AdjustArcSegment(PointIndex, EComputeArcMode::AdjStartTangent);
				}
				else // !bPinEndTangent && !bPinEndTangent
				{
					AdjustArcSegment(PointIndex, EComputeArcMode::AdjEndTangent);
				}
			}
		}
	}
	else // EditingPointIndex != INDEX_NONE
	{
		// Update the second part of spline
		int NumSegments = IsClosedLoop() ? GetNumberOfSplineSegments() - 1 : GetNumberOfSplineSegments() - EditingPointIndex;
		for (int i = 0; i < NumSegments; ++i)
		{
			const int iSegment = (i + EditingPointIndex) % GetNumberOfSplineSegments();
			if (GetRoadSplinePointType(iSegment) == ERoadSplinePointType::Arc)
			{
				const int iNextSegment = GetNextPoint(iSegment);
				const int iPreSegment = GetPrevPoint(iSegment);

				const bool bPinStartTangent = iPreSegment >= 0 && GetRoadSplinePointType(iPreSegment) == ERoadSplinePointType::Linear;
				const bool bPinEndTangent = iNextSegment >= 0 && GetRoadSplinePointType(iNextSegment) == ERoadSplinePointType::Linear;

				if (bPinStartTangent && bPinEndTangent)
				{
					AdjustArcSegment(iSegment, EComputeArcMode::AdjEndPos);
					break;
				}
				else if (bPinStartTangent)
				{
					AdjustArcSegment(iSegment, EComputeArcMode::AdjEndTangent);
				}
				else if (bPinEndTangent)
				{
					AdjustArcSegment(iSegment, EComputeArcMode::AdjEndPos);
					break;
				}
				else // !bPinEndTangent && !bPinEndTangent
				{
					AdjustArcSegment(iSegment, EComputeArcMode::AdjEndTangent);
				}
			}
			else
			{
				break;
			}
		}

		// Update the first part of the spline
		NumSegments = IsClosedLoop() ? GetNumberOfSplineSegments() - 1 : EditingPointIndex;
		for (int i = 0; i < NumSegments; ++i)
		{
			int iSegment = (EditingPointIndex - 1 - i);
			if (iSegment < 0) iSegment += GetNumberOfSplineSegments();
			
			if (GetRoadSplinePointType(iSegment) == ERoadSplinePointType::Arc)
			{
				const int iNextSegment = GetNextPoint(iSegment);
				const int iPreSegment = GetPrevPoint(iSegment);

				const bool bPinStartTangent = iPreSegment >= 0 && GetRoadSplinePointType(iPreSegment) == ERoadSplinePointType::Linear;
				const bool bPinEndTangent = iNextSegment >= 0 && GetRoadSplinePointType(iNextSegment) == ERoadSplinePointType::Linear;

				if (bPinStartTangent && bPinEndTangent)
				{
					AdjustArcSegment(iSegment, EComputeArcMode::AdjStartPos);
					break;
				}
				else if (bPinStartTangent)
				{
					AdjustArcSegment(iSegment, EComputeArcMode::AdjStartPos);
					break;
				}
				else if (bPinEndTangent)
				{
					AdjustArcSegment(iSegment, EComputeArcMode::AdjStartTangent);
				}
				else // !bPinEndTangent && !bPinEndTangent
				{
					AdjustArcSegment(iSegment, EComputeArcMode::AdjStartTangent);
				}
			}
			else
			{
				break;
			}
		}
	}

	// Check segments
	/*
	for (int iSegment = 0; iSegment < GetNumberOfSplineSegments(); ++iSegment)
	{
		if (GetRoadSplinePointType(iSegment) == ERoadSplinePointType::Arc)
		{
			Segments[iSegment].bIsBreaked = !CheckArc(iSegment);
		}
		else if (GetRoadSplinePointType(iSegment) == ERoadSplinePointType::Linear)
		{
			Segments[iSegment].bIsBreaked = false; // TODO: check line segmenta
		}
		else
		{
			Segments[iSegment].bIsBreaked = false;
		}
	}	
	*/
}


bool URoadSplineComponent::AdjustArcSegment(int PointIndex, EComputeArcMode Mode)
{
	check(PointIndex >= 0 && PointIndex < GetNumberOfSplineSegments());

	bool Result = true;

	const int StartPointIndex = PointIndex;
	const int EndPointIndex = (PointIndex + 1) % GetNumberOfSplinePoints();

	auto& StartPoint = SplineCurves.Position.Points[StartPointIndex];
	auto& EndPoint = SplineCurves.Position.Points[EndPointIndex];

	const FVector StartToEnd = EndPoint.OutVal - StartPoint.OutVal;
	const FVector StartToEndDir = StartToEnd.GetSafeNormal();;

	FVector StartDir;
	FVector EndDir;

	if (Mode == EComputeArcMode::AdjStartTangent || Mode == EComputeArcMode::AdjEndTangent)
	{
		if (Mode == EComputeArcMode::AdjStartTangent)
		{
			EndDir = -GetDirectionAtSplinePoint(EndPointIndex, ESplineCoordinateSpace::Local);
			StartDir = FMath::GetReflectionVector(EndDir, StartToEndDir);
		}
		else // EComputeArcMode::AdjEndTangent
		{
			StartDir = GetDirectionAtSplinePoint(StartPointIndex, ESplineCoordinateSpace::Local);
			EndDir = FMath::GetReflectionVector(StartDir, -StartToEndDir);
		}
	}
	else // ComputeArcMode::AdjStartPos || EComputeArcMode::AdjEndPos
	{
		// TODO: Make sure that the length of the previes and next linear spline is at least 0.1 meters

		StartDir = GetDirectionAtSplinePoint(StartPointIndex, ESplineCoordinateSpace::Local);
		EndDir = -GetDirectionAtSplinePoint(EndPointIndex, ESplineCoordinateSpace::Local);
		FVector Intersection;
		if (FMath::SegmentIntersection2D(StartPoint.OutVal, StartPoint.OutVal + StartDir * 1.0e+7f, EndPoint.OutVal, EndPoint.OutVal + EndDir * 1.0e+7f, Intersection))
		{
			const double A = (Intersection - StartPoint.OutVal).Size();
			const double B = (Intersection - EndPoint.OutVal).Size();
			if (Mode == EComputeArcMode::AdjStartPos)
			{
				StartPoint.OutVal += StartDir * (A - B);
			}
			else // EComputeArcMode::AdjEndPos
			{
				EndPoint.OutVal += EndDir * (B - A);
			}
		}
		else
		{
			// TODO: What need to do? - just don't adjust the position of the points
			Result = false;
		}
	}

	const FVector StartRightVector(FVector2D(StartDir.X, StartDir.Y).GetRotated(90), StartDir.Z);
	const double CosA = StartRightVector.CosineAngle2D(StartToEndDir);

	if (StartDir.CosineAngle2D(StartToEndDir) < 0) 
	{
		// Force to fit angel to 180deg
		/*
		double Sign = CosA >= 0 ? 1 : -1;
		CosA = (1 - UE_KINDA_SMALL_NUMBER) * Sign;
		FVector2D Dir = FVector2D(StartToEndDir).GetRotated(-90 * Sign);
		StartDir = FVector(Dir, StartDir.Z);
		EndDir = FVector(Dir, EndDir.Z);
		*/

		Result = false;
	}

	const double Radius = (StartToEnd.Size2D() / 2) / CosA;
	const double Ang = (180 - FMath::Acos(CosA) * 2 / PI * 180);
	const double TangentMult = CalcTangentMultiplier(Radius, -Ang);

	StartPoint.LeaveTangent = StartDir * TangentMult;
	StartPoint.ArriveTangent = StartDir * StartPoint.ArriveTangent.Size() * (TangentMult >= 0 ? 1: -1);

	EndPoint.LeaveTangent = -EndDir * EndPoint.LeaveTangent.Size() * (TangentMult >= 0 ? 1 : -1);
	EndPoint.ArriveTangent = -EndDir * TangentMult;

	//SplineCurves.Position.Points[PointIndex].InterpMode = CIM_CurveUser;

	return Result;
}

void URoadSplineComponent::AdjustLineSegment(int PointIndex)
{
	if (PointIndex < 0 || PointIndex >= GetNumberOfSplineSegments())
	{
		return;
	}

	const int StartPointIndex = PointIndex;
	const int EndPointIndex = (PointIndex + 1) % GetNumberOfSplinePoints();

	auto& StartPoint = SplineCurves.Position.Points[StartPointIndex];
	auto& EndPoint = SplineCurves.Position.Points[EndPointIndex];

	const FVector Tangent = (EndPoint.OutVal - StartPoint.OutVal).GetSafeNormal();
	const double TangentLen = (EndPoint.OutVal - StartPoint.OutVal).Size() * 0.5; //0.5 = 0.25 size of segment

	StartPoint.ArriveTangent = Tangent * StartPoint.ArriveTangent.Size();
	StartPoint.LeaveTangent = Tangent * TangentLen;

	EndPoint.ArriveTangent = Tangent * TangentLen;
	EndPoint.LeaveTangent = Tangent * EndPoint.LeaveTangent.Size();
}

bool URoadSplineComponent::CheckArc(int PointIndex) const
{
	if (PointIndex < 0 || PointIndex >= GetNumberOfSplineSegments())
	{
		return false;
	}

	const int StartPointIndex = PointIndex;
	const int EndPointIndex = (PointIndex + 1) % GetNumberOfSplinePoints();

	const auto& StartPoint = SplineCurves.Position.Points[StartPointIndex];
	const auto& EndPoint = SplineCurves.Position.Points[EndPointIndex];

	const FVector StartToEnd = EndPoint.OutVal - StartPoint.OutVal;
	const FVector StartToEndDir = StartToEnd.GetSafeNormal();
	const FVector StartDir = StartPoint.LeaveTangent;
	const FVector EndDir = -FMath::GetReflectionVector(StartDir, -StartToEndDir);

	if ((EndDir - EndPoint.ArriveTangent).Size() > UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector StartRightVector(FVector2D(StartDir.X, StartDir.Y).GetRotated(90), StartDir.Z);
	const double CosA = StartRightVector.CosineAngle2D(StartToEndDir);
	if (StartDir.CosineAngle2D(StartToEndDir) < 0)
	{
		return false;
	}

	const double Radius = (StartToEnd.Size2D() / 2) / CosA;
	const double Ang = (180 - FMath::Acos(CosA) * 2 / PI * 180);
	const double TangentMult = FMath::Abs(CalcTangentMultiplier(Radius, -Ang));

	if (FMath::Abs(StartPoint.LeaveTangent.Size() - TangentMult) / TangentMult > UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	if (FMath::Abs(EndPoint.ArriveTangent.Size() - TangentMult) / TangentMult > UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	return true;
}

void URoadSplineComponent::ApplyComponentInstanceData(struct FDriveSplineInstanceData* SplineInstanceData, const bool bPostUCS)
{
	check(SplineInstanceData);

	if (bPostUCS)
	{
		if (bInputSplinePointsToConstructionScript)
		{
			// Don't reapply the saved state after the UCS has run if we are inputting the points to it.
			// This allows the UCS to work on the edited points and make its own changes.
			return;
		}
		else
		{
			bModifiedByConstructionScript = (SplineInstanceData->SplineCurvesPreUCS != SplineCurves);

			// If we are restoring the saved state, unmark the SplineCurves property as 'modified'.
			// We don't want to consider that these changes have been made through the UCS.
			TArray<FProperty*> Properties;
			Properties.Emplace(FindFProperty<FProperty>(USplineComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(USplineComponent, SplineCurves)));
			RemoveUCSModifiedProperties(Properties);
		}
	}
	else
	{
		SplineInstanceData->SplineCurvesPreUCS = SplineCurves;
	}

	if (SplineInstanceData->bSplineHasBeenEdited)
	{
		SplineCurves = SplineInstanceData->SplineCurves;
		SplineMetadata->PointTypes = SplineInstanceData->PointTypes;
		bModifiedByConstructionScript = false;
	}

	bSplineHasBeenEdited = SplineInstanceData->bSplineHasBeenEdited;

	UpdateSpline();
}

TStructOnScope<FActorComponentInstanceData> URoadSplineComponent::GetComponentInstanceData() const
{
	TStructOnScope<FActorComponentInstanceData> InstanceData = MakeStructOnScope<FActorComponentInstanceData, FDriveSplineInstanceData>(this);
	FDriveSplineInstanceData* SplineInstanceData = InstanceData.Cast<FDriveSplineInstanceData>();

	if (bSplineHasBeenEdited)
	{
		SplineInstanceData->SplineCurves = SplineCurves;
		SplineInstanceData->PointTypes = SplineMetadata->PointTypes;
	}
	SplineInstanceData->bSplineHasBeenEdited = bSplineHasBeenEdited;

	return InstanceData;
}

#if WITH_EDITOR
void URoadSplineComponent::UpdateLandscape()
{
#if METAROAD_PRO
	if (IsValid(Landscape.Get()))
	{
		// Landscape editing lives in the editor module (UMetaRoadLandscapeSubsystem). Fire the decoupling
		// delegate instead of referencing the subsystem type from the runtime module.
		FMetaRoadDelegates::RequestLandscapeUpdateDelegate.ExecuteIfBound(this);
	}
#endif
}
#endif

void URoadSplineComponent::UpdateRoadLayout()
{
	RoadLayout.UpdateLayout(this);
	UpdateLaneSectionBounds();
}

void URoadSplineComponent::UpdateLaneSectionBounds()
{
	RoadLayout.UpdateBounds(GetSplineLength());
}


double URoadSplineComponent::EvalROffset(double S) const
{
	return RoadLayout.EvalROffset(S);
}

URoadSplineComponent::FRang URoadSplineComponent::GetLaneRang(int SectionIndex, int LaneIndex) const
{
	auto& Section = GetLaneSection(SectionIndex);

	double StartS, EndS;
	if (LaneIndex == MetaRoad::ZeroLaneIndex)
	{
		StartS = Section.SOffset;
		EndS = Section.SOffsetEnd_Cashed;
	}
	else
	{
		auto& Lane = Section.GetLaneByIndex(LaneIndex);
		StartS = Lane.GetStartOffset();
		EndS = Lane.GetEndOffset();
	}
	return { StartS, EndS };
}

void URoadSplineComponent::TrimLaneSections(double Tolerance)
{
	RoadLayout.TrimSections(GetSplineLength(), Tolerance, this);
	RoadLayout.UpdateLayout(this);
}

#if WITH_EDITOR
FPrimitiveSceneProxy* URoadSplineComponent::CreateSceneProxy()
{
	// The schematic scene proxy lives in the editor module (FRoadSplineSceneProxy). It is created through a
	// factory delegate the editor module binds, so the runtime module stays free of the editor render types.
	// Unbound (e.g. packaged game) -> no proxy, matching the previous editor-only behavior.
	return FMetaRoadDelegates::CreateRoadSplineSceneProxyDelegate.IsBound()
		? FMetaRoadDelegates::CreateRoadSplineSceneProxyDelegate.Execute(this)
		: nullptr;
}

void URoadSplineComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	// The preview materials live on UMetaRoadEditorSettings; the editor module fills the list to match the proxy.
	FMetaRoadDelegates::GetRoadSplineUsedMaterialsDelegate.ExecuteIfBound(this, OutMaterials);
}

void URoadSplineComponent::CreateRenderState_Concurrent(FRegisterComponentContext* Context)
{
	Super::CreateRenderState_Concurrent(Context);

	// Update ULaneConnection::CashedTransform

	auto UpdateConnection = [](ULaneConnection* ProbeConnection)
	{
		ProbeConnection->CashedTransform = ProbeConnection->EvalTransform(0.5, ESplineCoordinateSpace::World);
	};
	for (auto& Section : GetLaneSections())
	{
		for (auto& Lane : Section.Left)
		{
			UpdateConnection(Lane.PredecessorConnection);
			UpdateConnection(Lane.SuccessorConnection);
		}
		for (auto& Lane : Section.Right)
		{
			UpdateConnection(Lane.PredecessorConnection);
			UpdateConnection(Lane.SuccessorConnection);
		}
	}
}

#endif

void URoadSplineComponent::BuildOffsetCurves(double RightOffset, FSplineCurves& OutCurves) const
{
	OutCurves = SplineCurves;

	for (int i = 0; i < GetNumberOfSplinePoints(); ++i)
	{
		const FVector RightVector = GetRightVectorAtSplinePoint(i, ESplineCoordinateSpace::Local);
		OutCurves.Position.Points[i].OutVal = SplineCurves.Position.Points[i].OutVal + RightVector * RightOffset;
	}

	/*
	for (int SegmentIndex = 0; SegmentIndex < GetNumberOfSplineSegments(); ++SegmentIndex)
	{
		const int StartPointIndex = SegmentIndex;
		const int EndPointIndex = (SegmentIndex + 1) % GetNumberOfSplinePoints();

		auto& StartPoint1 = SplineCurves.Position.Points[StartPointIndex];
		auto& EndPoint1 = SplineCurves.Position.Points[EndPointIndex];

		auto& StartPoint2 = OutCurves.Position.Points[StartPointIndex];
		auto& EndPoint2 = OutCurves.Position.Points[EndPointIndex];

		const double Scale = (EndPoint2.OutVal - StartPoint2.OutVal).Size() / (EndPoint1.OutVal - StartPoint1.OutVal).Size();

		StartPoint2.LeaveTangent = StartPoint2.LeaveTangent * Scale;
		EndPoint2.ArriveTangent = EndPoint2.ArriveTangent * Scale;
	}
	*/
	

	OutCurves.UpdateSpline(IsClosedLoop(), bStationaryEndpoints, ReparamStepsPerSegment, false/*bLoopPositionOverride*/, 0.0f/*LoopPosition*/, GetComponentTransform().GetScale3D());

}

void URoadSplineComponent::BuildLinearApproximation(TArray<FSplinePositionLinearApproximation>& OutPoints, const TFunction<double(double)>& RightOffsetFunc, double S0, double S1, int InReparamStepsPerSegment, int MinNumSteps, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	OutPoints.Reset();
	const double SO_Param = GetInputKeyValueAtDistanceAlongSpline(S0);
	const double S1_Param = GetInputKeyValueAtDistanceAlongSpline(S1);

	int NumStep = int((S1_Param - SO_Param) * InReparamStepsPerSegment + 0.5);
	if (NumStep < MinNumSteps) NumStep = MinNumSteps;
	const double Step = (S1_Param - SO_Param) / NumStep;

	for (int i = 0; i <= NumStep; ++i)
	{
		const float Param = SO_Param + i * Step;
		const FVector RightVector = GetRightVectorAtSplineInputKey(Param, ESplineCoordinateSpace::Local);
		const double RightOffset = RightOffsetFunc(GetDistanceAlongSplineAtSplineInputKey(Param));
		FVector Point = SplineCurves.Position.Eval(Param, FVector::ZeroVector) + RightVector * RightOffset;

		if (CoordinateSpace == ESplineCoordinateSpace::World)
		{
			Point = GetComponentTransform().TransformPosition(Point);
		}

		OutPoints.Emplace(Point, Param); 
	}
}

/*
bool URoadSplineComponent::ConvertSplineSegmentToPolyLine(const double StartDist, const double EndDist, ESplineCoordinateSpace::Type CoordinateSpace, const float MaxSquareDistanceFromSpline, TArray<FVector>& OutPoints) const
{
	OutPoints.Empty();


	const int32 NumLines = 2; // Dichotomic subdivision of the spline segment
	double Dist = EndDist - StartDist;
	double SubstepSize = Dist / NumLines;
	if (SubstepSize == 0.0)
	{
		// There is no distance to cover, so handle the segment with a single point
		OutPoints.Add(GetLocationAtDistanceAlongSpline(EndDist, CoordinateSpace));
		return true;
	}

	double SubstepStartDist = StartDist;
	for (int32 i = 0; i < NumLines; ++i)
	{
		double SubstepEndDist = SubstepStartDist + SubstepSize;
		TArray<FVector> NewPoints;
		// Recursively sub-divide each segment until the requested precision is reached :
		if (DivideSplineIntoPolylineRecursiveHelper(SubstepStartDist, SubstepEndDist, CoordinateSpace, MaxSquareDistanceFromSpline, NewPoints))
		{
			if (OutPoints.Num() > 0)
			{
				check(OutPoints.Last() == NewPoints[0]); // our last point must be the same as the new segment's first
				OutPoints.RemoveAt(OutPoints.Num() - 1);
			}
			OutPoints.Append(NewPoints);
		}

		SubstepStartDist = SubstepEndDist;
	}

	return (OutPoints.Num() > 0);
}
*/
FVector URoadSplineComponent::EvalLanePoistion(int SectionIndex, int LaneIndex, double S, double Alpha, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const double SParam = GetInputKeyValueAtDistanceAlongSpline(S);
	const FVector Loc = GetLocationAtSplineInputKey(SParam, CoordinateSpace);

	double ROffset = EvalROffset(S);
	if (LaneIndex != MetaRoad::ZeroLaneIndex)
	{
		ROffset += RoadLayout.Sections[SectionIndex].EvalLaneROffset(LaneIndex, S, Alpha);
	}

	const FVector RVector = GetRightVectorAtSplineInputKey(SParam, CoordinateSpace);
	return Loc + RVector * ROffset;
}

void URoadSplineComponent::PushSelectionToProxy()
{
	Super::PushSelectionToProxy();

	if (!IsComponentIndividuallySelected())
	{
		SetSelectedLane(INDEX_NONE, MetaRoad::ZeroLaneIndex);
		ClearSelectedLanes();
	}

	//UE_LOG(LogMetaRoad, Warning, TEXT("URoadSplineComponent::PushSelectionToProxy() **** %s %i"), *GetName(), IsComponentIndividuallySelected());

}

bool URoadSplineComponent::ShouldRenderSelected() const
{
	/*
	if (OwnerIsJunction())
	{
		return false;
	}
	*/

	return Super::ShouldRenderSelected();
}

bool URoadSplineComponent::MoveComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit, EMoveComponentFlags MoveFlags, ETeleportType Teleport)
{
	if (Super::MoveComponentImpl(Delta, NewRotation, bSweep, OutHit, MoveFlags, Teleport))
	{
		UpdateMagicTransform();
		return true;
	}
	return false;
}

void URoadSplineComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);

#if WITH_EDITOR
	if (int32(UpdateTransformFlags & EUpdateTransformFlags::PropagateFromParent))
	{
		auto* Subsystem = GetWorld()->GetSubsystem<UMetaRoadSubsystem>();
		check(Subsystem);
		const TSet<TWeakObjectPtr<AActor>>& MovingActors = Subsystem->GetMovingActors();

		if (MovingActors.Num() == 1)
		{
			UpdateMagicTransform(ERoadSplineMagicTransformFilter::OuterOnly);
		}
	}
#endif

	MarkRoadStateDirty();
}

int URoadSplineComponent::FindRoadSectionOnSplineKey(float SplineKey) const
{
	int SectionIndex = INDEX_NONE;

	for (int i = 0; i < GetLaneSections().Num(); ++i)
	{
		auto& Section = GetLaneSection(i);
		const float StartKey = GetInputKeyValueAtDistanceAlongSpline(Section.SOffset);
		const float EndKey = GetInputKeyValueAtDistanceAlongSpline(Section.SOffsetEnd_Cashed);
		if (SplineKey >= StartKey && SplineKey <= EndKey)
		{
			SectionIndex = i;
			break;
		}
	}

	return SectionIndex;
}

int URoadSplineComponent::SplitSection(float SplineKey, ERoadLaneSectionSide Side)
{
	const int SectionIndex = FindRoadSectionOnSplineKey(SplineKey);

	if(SectionIndex == INDEX_NONE)
	{
		UE_LOG(LogMetaRoad, Error, TEXT("URoadSplineComponent::SplitSection() can't find section in key %f"), SplineKey);
		return INDEX_NONE;
	}

#if WITH_EDITOR
	const FScopedTransaction Transaction(LOCTEXT("SplitSection", "Split Section"), !GIsTransacting);
	Modify();
#endif

	const double SOffset = GetDistanceAlongSplineAtSplineInputKey(SplineKey);

	FRoadLaneSection SectionToAdd{};
	SectionToAdd.SOffset = SOffset;
	SectionToAdd.Side = Side;
	SectionToAdd.Attributes = RoadLayout.Sections[SectionIndex].Attributes;

	int LeftSectionIndex = INDEX_NONE;
	if (Side == ERoadLaneSectionSide::Both || Side == ERoadLaneSectionSide::Left)
	{
		LeftSectionIndex = RoadLayout.FindSideSection(SectionIndex, ERoadLaneSectionSide::Left);
		if (LeftSectionIndex != INDEX_NONE)
		{
			SectionToAdd.Left = RoadLayout.Sections[LeftSectionIndex].Left;
		}
	}

	int RightSectionIndex = INDEX_NONE;
	if (Side == ERoadLaneSectionSide::Both || Side == ERoadLaneSectionSide::Right)
	{
		RightSectionIndex = RoadLayout.FindSideSection(SectionIndex, ERoadLaneSectionSide::Right);
		if (RightSectionIndex != INDEX_NONE)
		{
			SectionToAdd.Right = RoadLayout.Sections[RightSectionIndex].Right;
		}
	}
	
	RoadLayout.Sections.Insert(MoveTemp(SectionToAdd), SectionIndex + 1);
	UpdateLaneSectionBounds();
	UpdateRoadLayout();

	static auto ShiftWidth = [](FRichCurve& Width, double SDelta)
	{
		for(auto & Key: Width.Keys)
		{
			Key.Time += SDelta;
		}
	};

	static auto ShiftAttribute = [](FRoadLaneAttribute & Attribute, double SDelta)
	{
		for(auto & Key: Attribute.Keys)
		{
			Key.SOffset += SDelta;
		}
	};

	static auto ShifLane = [](FRoadLane& OldLane, FRoadLane& NewLane, double SDelta)
	{
		ShiftWidth(NewLane.Width, SDelta);
		for (auto& [Key, Value] : NewLane.Attributes)
		{
			ShiftAttribute(Value, SDelta);
		}

		if (OldLane.IsForwardLane())
		{
			NewLane.SuccessorConnection = OldLane.SuccessorConnection;
			OldLane.SuccessorConnection = nullptr;
			NewLane.PredecessorConnection = nullptr;
		}
		else
		{
			NewLane.PredecessorConnection = OldLane.PredecessorConnection;
			OldLane.PredecessorConnection = nullptr;
			NewLane.SuccessorConnection = nullptr;
		}

	};

	auto& NewSection = RoadLayout.Sections[SectionIndex + 1];

	for (int LaneIndex = 0; LaneIndex < NewSection.Left.Num(); ++LaneIndex)
	{
		auto& OldSection = RoadLayout.Sections[LeftSectionIndex];
		auto& OldLane = OldSection.Left[LaneIndex];
		auto& NewLane = NewSection.Left[LaneIndex];
		ShifLane(OldLane, NewLane, OldSection.SOffset - SOffset);
	}
	
	for (int LaneIndex = 0; LaneIndex < NewSection.Right.Num(); ++LaneIndex)
	{
		auto& OldSection = RoadLayout.Sections[RightSectionIndex];
		auto& OldLane = OldSection.Right[LaneIndex];
		auto& NewLane = NewSection.Right[LaneIndex];
		ShifLane(OldLane, NewLane, OldSection.SOffset - SOffset);
	}

	const int CenterSectionIndex = FMath::Max(LeftSectionIndex, RightSectionIndex);
	if (CenterSectionIndex != INDEX_NONE)
	{
		auto& OldSection = RoadLayout.Sections[CenterSectionIndex];
		for (auto& [Key, Value] : NewSection.Attributes)
		{
			ShiftAttribute(Value, OldSection.SOffset - SOffset);
		}
	}

	if (LeftSectionIndex == RightSectionIndex)
	{
		if (LeftSectionIndex != INDEX_NONE)
		{
			RoadLayout.Sections[LeftSectionIndex].Trim(true);
		}
	}
	else
	{
		if (LeftSectionIndex != INDEX_NONE)
		{
			RoadLayout.Sections[LeftSectionIndex].Trim(true);
		}
		if (RightSectionIndex != INDEX_NONE)
		{
			RoadLayout.Sections[RightSectionIndex].Trim(true);
		}
	}

	NewSection.Trim(true);

	UpdateRoadLayout();
	MarkRenderStateDirty();

	return SectionIndex + 1;
}

void URoadSplineComponent::DisconnectAll()
{
#if WITH_EDITOR
	const FScopedTransaction Transaction(LOCTEXT("DisconnectAll", "Disconnect All"), !GIsTransacting);
#endif

	if (IsValid(PredecessorConnection)) PredecessorConnection->Disconnect();
	if (IsValid(SuccessorConnection)) SuccessorConnection->Disconnect();

	for (auto& Section : RoadLayout.Sections)
	{
		for (auto& Lane : Section.Left)
		{
			if (IsValid(Lane.PredecessorConnection)) Lane.PredecessorConnection->DisconnectAll();
			if (IsValid(Lane.SuccessorConnection)) Lane.SuccessorConnection->DisconnectAll();
		}
		for (auto& Lane : Section.Right)
		{
			if (IsValid(Lane.PredecessorConnection)) Lane.PredecessorConnection->DisconnectAll();
			if (IsValid(Lane.SuccessorConnection)) Lane.SuccessorConnection->DisconnectAll();
		}
	}
}

/*
bool URoadSplineComponent::Modify(bool bAlwaysMarkDirty)
{
	bool bWasModified = Super::Modify(bAlwaysMarkDirty);

	bWasModified |= PredecessorConnection->Modify();
	bWasModified |= SuccessorConnection->Modify();

	return bWasModified;
}\*/

#if WITH_EDITOR
void URoadSplineComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	FProperty* MemberProperty = PropertyChangedEvent.PropertyChain.GetHead()->GetValue();

	if (MemberProperty)
	{
		static const FName RoadLayoutName = GET_MEMBER_NAME_CHECKED(URoadSplineComponent, RoadLayout);
		static const FName ClosedLoopName = "bClosedLoop"; //GET_MEMBER_NAME_CHECKED(USplineComponent, bClosedLoop);

		const FName PropertyName(MemberProperty->GetFName());

		if (PropertyName == RoadLayoutName || PropertyName == ClosedLoopName)
		{
			UpdateRoadLayout();
			UpdateMagicTransform();
			UpdateBounds();
		}
	}

	MarkRoadStateDirty();
}
void URoadSplineComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	//if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet || PropertyChangedEvent.ChangeType == EPropertyChangeType::Unspecified)
	{
#if WITH_EDITOR && METAROAD_PRO
		UpdateLandscape();
#endif
	}
}
#endif

void URoadSplineComponent::UpdateMagicTransform(ERoadSplineMagicTransformFilter Filter)
{
	URoadConnection::GlobalTransformMagic = FMath::Rand();

	TFunction<bool(const URoadSplineComponent*)> FilterFunc;

	if (Filter == ERoadSplineMagicTransformFilter::InnrerOnly)
	{
		FilterFunc = [this](const URoadSplineComponent* TargetSpline) 
		{ 
			return TargetSpline && TargetSpline->GetOwner() == GetOwner();
		};
	}
	else if (Filter == ERoadSplineMagicTransformFilter::OuterOnly)
	{
		FilterFunc = [this](const URoadSplineComponent* TargetSpline)
		{
			return TargetSpline && TargetSpline->GetOwner() != GetOwner();
		};
	}
	else
	{
		FilterFunc = [](const URoadSplineComponent*) { return true; };
	}

	MagicUpdateTransformInner(FilterFunc);

	URoadConnection::GlobalTransformMagic = 0;
}

void URoadSplineComponent::MagicUpdateTransformInner(TFunction<bool(const URoadSplineComponent*)> Filter)
{
	Modify();

	TArray<URoadSplineComponent*> SplinesToUpdate;

	auto TransformRoadConnection = [&SplinesToUpdate, &Filter](URoadConnection * RoadConnection)
	{
		if (RoadConnection && RoadConnection->IsConnectionValid())
		{
			if (RoadConnection->IsConnected() && Filter(RoadConnection->GetOuterConnection()->GetOwnedRoadSpline()))
			{
				if (RoadConnection->CanTransform())
				{
					RoadConnection->OuterLaneConnection->GetOwnedRoadSpline()->Modify();
					bool bIsTransformed = RoadConnection->SetTransformToOuter();
					RoadConnection->TransformMagic = URoadConnection::GlobalTransformMagic;
					if (bIsTransformed)
					{
						SplinesToUpdate.Add(RoadConnection->OuterLaneConnection->GetOwnedRoadSplineChecked());
					}
				}
				else
				{
					RoadConnection->SetTransformFormOuter();
				}
			}
			RoadConnection->TransformMagic = URoadConnection::GlobalTransformMagic;
		}
	};

	auto TransformLaneConnection = [&SplinesToUpdate, &Filter](ULaneConnection * LaneConnection)
	{
		if (LaneConnection && LaneConnection->IsConnectionValid())
		{
			const FTransform Transform = LaneConnection->EvalTransform(0.0, ESplineCoordinateSpace::World);
			for (auto& RoadConnection : LaneConnection->OuterRoadConnections)
			{
				if (RoadConnection.IsValid() && Filter(RoadConnection->GetOwnedRoadSpline()))
				{
					RoadConnection->GetOwnedRoadSpline()->Modify();
					if (RoadConnection->SetTransform(Transform, true, ESplineCoordinateSpace::World))
					{
						SplinesToUpdate.Add(RoadConnection->GetOwnedRoadSplineChecked());
					}
				}
			}
		}
	};

	TransformRoadConnection(GetPredecessorConnection());
	TransformRoadConnection(GetSuccessorConnection());
	
	for (auto& Section : RoadLayout.Sections)
	{
		for (auto& Lane : Section.Left)
		{
			TransformLaneConnection(Lane.PredecessorConnection);
			TransformLaneConnection(Lane.SuccessorConnection);
		}
		for (auto& Lane : Section.Right)
		{
			TransformLaneConnection(Lane.PredecessorConnection);
			TransformLaneConnection(Lane.SuccessorConnection);
		}
	}

	for (auto& It : SplinesToUpdate)
	{
		It->MagicUpdateTransformInner(Filter);
	}
}


TArray<ULaneConnection*> URoadSplineComponent::FindAllSuccessors(int SectionIndex, int LaneIndex, bool bIncludesThisRoad) const
{
	if (LaneIndex == MetaRoad::ZeroLaneIndex || SectionIndex < 0 || SectionIndex >= RoadLayout.Sections.Num() - 1)
	{
		return {};
	}

	const auto& Section = RoadLayout.Sections[SectionIndex];
	if (!Section.CheckLaneIndex(LaneIndex))
	{
		return {};
	}

	TArray<ULaneConnection*> Ret;

	auto CheckLane = [&Ret](const FRoadLane & Lane, int DesierIndex)
	{
		if(IsValid(Lane.SuccessorConnection))
		{
			for (const auto& Outer : Lane.SuccessorConnection->OuterRoadConnections)
			{
				if (Outer.IsValid())
				{
					check(Outer->IsPredecessorConnection());
					const FRoadLaneSection& OuterSections = Outer->GetOwnedRoadSplineChecked()->RoadLayout.Sections[0];
					if (OuterSections.CheckLaneIndex(DesierIndex))
					{
						auto PredecessorConnection = OuterSections.GetLaneByIndex(DesierIndex).PredecessorConnection;
						if (IsValid(PredecessorConnection))
						{
							Ret.Add(PredecessorConnection);
						}
					}
				}
			}
		}
	};

	for (int i = 0; i < Section.Left.Num(); ++i)
	{
		int DesierIndex = LaneIndex - (-i - 1);
		if (DesierIndex >= 0) ++LaneIndex;
		CheckLane(Section.Left[i], DesierIndex);
	}

	for (int i = 0; i < Section.Right.Num(); ++i)
	{
		int DesierIndex = LaneIndex - (i + 1);
		if (DesierIndex <= 0) --LaneIndex;
		CheckLane(Section.Right[i], DesierIndex);
	}

	if (bIncludesThisRoad && RoadLayout.Sections.Num() > 1 && SectionIndex < RoadLayout.Sections.Num() - 2)
	{
		const auto& NextSection = RoadLayout.Sections[SectionIndex + 1];
		if (NextSection.CheckLaneIndex(LaneIndex))
		{
			const auto& NextLane = NextSection.GetLaneByIndex(LaneIndex);
			if (IsValid(NextLane.PredecessorConnection))
			{
				Ret.Add(NextLane.PredecessorConnection);
			}
		}
	}

	if (SectionIndex == RoadLayout.Sections.Num() - 1 && SuccessorConnection->IsConnected())
	{
		Ret.Add(SuccessorConnection->OuterLaneConnection.Get());
	}

	return Ret;
}


TArray<ULaneConnection*> URoadSplineComponent::FindAllPredecessors(int SectionIndex, int LaneIndex, bool bIncludesThisRoad) const
{
	if (LaneIndex == MetaRoad::ZeroLaneIndex || SectionIndex < 0 || SectionIndex >= RoadLayout.Sections.Num() - 1)
	{
		return {};
	}

	const auto& Section = RoadLayout.Sections[SectionIndex];
	if (!Section.CheckLaneIndex(LaneIndex))
	{
		return {};
	}

	TArray<ULaneConnection*> Ret;

	auto CheckLane = [&Ret](const FRoadLane& Lane, int DesierIndex)
	{
		if(IsValid(Lane.PredecessorConnection))
		{
			for (const auto& Outer : Lane.PredecessorConnection->OuterRoadConnections)
			{
				if (Outer.IsValid())
				{
					check(Outer->IsSuccessorConnection());
					const FRoadLaneSection& OuterSections = Outer->GetOwnedRoadSplineChecked()->RoadLayout.Sections[0];
					if (OuterSections.CheckLaneIndex(DesierIndex))
					{
						auto& SuccessorConnection = OuterSections.GetLaneByIndex(DesierIndex).SuccessorConnection;
						if (IsValid(SuccessorConnection))
						{
							Ret.Add(SuccessorConnection);
						}
					}
				}
			}
		}
	};

	for (int i = 0; i < Section.Left.Num(); ++i)
	{
		int DesierIndex = LaneIndex - (-i - 1);
		if (DesierIndex >= 0) ++LaneIndex;
		CheckLane(Section.Left[i], DesierIndex);
	}

	for (int i = 0; i < Section.Right.Num(); ++i)
	{
		int DesierIndex = LaneIndex - (i + 1);
		if (DesierIndex <= 0) --LaneIndex;
		CheckLane(Section.Right[i], DesierIndex);
	}

	if (bIncludesThisRoad && RoadLayout.Sections.Num() > 1 && SectionIndex > 0)
	{
		const auto& PrevSection = RoadLayout.Sections[SectionIndex - 1];
		if (PrevSection.CheckLaneIndex(LaneIndex))
		{
			const auto& PreLane = PrevSection.GetLaneByIndex(LaneIndex);
			if (IsValid(PreLane.SuccessorConnection))
			{
				Ret.Add(PreLane.SuccessorConnection);
			}
		}
	}

	if (SectionIndex == 0 && PredecessorConnection->IsConnected())
	{
		Ret.Add(PredecessorConnection->OuterLaneConnection.Get());
	}

	return Ret;
}

FQuat URoadSplineComponent::GetBackwardQuaternionAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	FQuat Quat = SplineCurves.Rotation.Eval(InKey, FQuat::Identity);
	Quat.Normalize();

	const FVector Direction = -SplineCurves.Position.EvalDerivative(InKey, FVector::ZeroVector).GetSafeNormal();
	const FVector UpVector = Quat.RotateVector(DefaultUpVector);

	FQuat Rot = (FRotationMatrix::MakeFromXZ(Direction, UpVector)).ToQuat();

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		Rot = GetComponentTransform().GetRotation() * Rot;
	}

	return Rot;
}

FTransform URoadSplineComponent::GetBackwardTransformAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseScale) const
{
	const FVector Location(GetLocationAtSplineInputKey(InKey, ESplineCoordinateSpace::Local));
	const FQuat Rotation(GetBackwardQuaternionAtSplineInputKey(InKey, ESplineCoordinateSpace::Local));
	const FVector Scale = bUseScale ? GetScaleAtSplineInputKey(InKey) : FVector(1.0f);

	FTransform Transform(Rotation, Location, Scale);

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		Transform = Transform * GetComponentTransform();
	}

	return Transform;
}


void URoadSplineComponent::PostLoad()
{
	Super::PostLoad();

	UpdateRoadLayout();
}

void URoadSplineComponent::PostInitProperties()
{
	Super::PostInitProperties();
}

void URoadSplineComponent::CaptureConnectionGuids()
{
	ConnectionGuidSnapshot.Reset();
	for (const auto& Section : RoadLayout.Sections)
	{
		for (const auto& Lane : Section.Left)
		{
			ConnectionGuidSnapshot.Add(IsValid(Lane.PredecessorConnection) ? Lane.PredecessorConnection->GetGuid() : FGuid{});
			ConnectionGuidSnapshot.Add(IsValid(Lane.SuccessorConnection)   ? Lane.SuccessorConnection->GetGuid()   : FGuid{});
		}
		for (const auto& Lane : Section.Right)
		{
			ConnectionGuidSnapshot.Add(IsValid(Lane.PredecessorConnection) ? Lane.PredecessorConnection->GetGuid() : FGuid{});
			ConnectionGuidSnapshot.Add(IsValid(Lane.SuccessorConnection)   ? Lane.SuccessorConnection->GetGuid()   : FGuid{});
		}
	}
}

void URoadSplineComponent::RestoreConnectionGuids()
{
	if (ConnectionGuidSnapshot.IsEmpty()) return;

	int32 Idx = 0;
	for (auto& Section : RoadLayout.Sections)
	{
		for (auto& Lane : Section.Left)
		{
			if (Idx < ConnectionGuidSnapshot.Num() && IsValid(Lane.PredecessorConnection) && ConnectionGuidSnapshot[Idx].IsValid())
				Lane.PredecessorConnection->Guid = ConnectionGuidSnapshot[Idx];
			++Idx;
			if (Idx < ConnectionGuidSnapshot.Num() && IsValid(Lane.SuccessorConnection) && ConnectionGuidSnapshot[Idx].IsValid())
				Lane.SuccessorConnection->Guid = ConnectionGuidSnapshot[Idx];
			++Idx;
		}
		for (auto& Lane : Section.Right)
		{
			if (Idx < ConnectionGuidSnapshot.Num() && IsValid(Lane.PredecessorConnection) && ConnectionGuidSnapshot[Idx].IsValid())
				Lane.PredecessorConnection->Guid = ConnectionGuidSnapshot[Idx];
			++Idx;
			if (Idx < ConnectionGuidSnapshot.Num() && IsValid(Lane.SuccessorConnection) && ConnectionGuidSnapshot[Idx].IsValid())
				Lane.SuccessorConnection->Guid = ConnectionGuidSnapshot[Idx];
			++Idx;
		}
	}
	ConnectionGuidSnapshot.Reset();
}

void URoadSplineComponent::PreDuplicate(FObjectDuplicationParameters& DupParams)
{
	Super::PreDuplicate(DupParams);

	// If component has no world (transient package — T3D paste temp being binary-dup'd to final),
	// skip Guid regeneration so T3D-imported Guids survive intact into the duplicated component.
	if (GetWorld())
	{
		RefreshConnectionGuids();
		CaptureConnectionGuids();
	}
}

void URoadSplineComponent::PostEditImport()
{
	Super::PostEditImport();

	UpdateRoadLayout();

	PredecessorConnection->OuterLaneConnection = nullptr;
	SuccessorConnection->OuterLaneConnection = nullptr;

	bWasImported = true;
}

void URoadSplineComponent::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	UpdateRoadLayout();
	RestoreConnectionGuids();

#if WITH_EDITOR
	if (!bDuplicateForPIE)
	{
		PredecessorConnection->OuterLaneConnection = nullptr;
		SuccessorConnection->OuterLaneConnection = nullptr;
		bWasImported = true;
	}
#endif
}

void URoadSplineComponent::DestroyComponent(bool bPromoteChildren)
{
	Super::DestroyComponent(bPromoteChildren);

#if WITH_EDITOR && METAROAD_PRO
	UpdateLandscape();
#endif
}

void URoadSplineComponent::OnRegister()
{
	Super::OnRegister();

	UpdateRoadLayout();

#if WITH_EDITOR
	if (bWasImported)
	{
		bWasImported = false;

		if (auto* World = GetWorld())
		{
			World->GetSubsystem<UMetaRoadSubsystem>()->OnSplinePostEditImport(this);
		}
		else
		{
			ensureMsgf(false, TEXT("World is null?"));
		}
	}
#endif

	GetPredecessorConnection()->InitConnection();
	GetSuccessorConnection()->InitConnection();

	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		FMetaRoadDelegates::OnRoadSplineRegistredDelegate.Broadcast(this);
	}
}

void URoadSplineComponent::OnUnregister()
{
	Super::OnUnregister();

	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		FMetaRoadDelegates::OnRoadSplineUnregistredDelegate.Broadcast(this);
	}
}

void URoadSplineComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);
	DisconnectAll();
}

void URoadSplineComponent::SetRotationAtSplinePoint_Fixed(int32 PointIndex, const FRotator& InRotation, ESplineCoordinateSpace::Type CoordinateSpace, bool bUpdateSpline /*= true*/)
{
	SetRotationAtSplinePoint_Fixed(this, PointIndex, InRotation, CoordinateSpace, bUpdateSpline);
}

void URoadSplineComponent::SetRotationAtSplinePoint_Fixed(USplineComponent* Spline, int32 PointIndex, const FRotator& InRotation, ESplineCoordinateSpace::Type CoordinateSpace, bool bUpdateSpline)
{
	if (Spline->SplineCurves.Rotation.Points.IsValidIndex(PointIndex))
	{
		const FQuat Quat = (CoordinateSpace == ESplineCoordinateSpace::World) ?
			Spline->GetComponentTransform().InverseTransformRotation(InRotation.Quaternion()) : InRotation.Quaternion();

		FVector UpVector = Quat.GetUpVector();
		Spline->SetUpVectorAtSplinePoint(PointIndex, UpVector, ESplineCoordinateSpace::Local, false); // Origin SetRotationAtSplinePoint() use "CoordinateSpace" insted of ESplineCoordinateSpace::Local here

		FVector Direction = Quat.GetForwardVector();
		Spline->SetTangentAtSplinePoint(PointIndex, Direction, ESplineCoordinateSpace::Local, false); // Origin SetRotationAtSplinePoint() use "CoordinateSpace" insted of ESplineCoordinateSpace::Local here

		if (bUpdateSpline)
		{
			Spline->UpdateSpline();
		}
	}
}


FBoxSphereBounds URoadSplineComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	float MaxWidth = 0;

	for (auto& Section : RoadLayout.Sections)
	{
		float MaxLeftWidth = 0;
		float MaxRightWidth = 0;

		for (auto & Lane: Section.Left)
		{
			float MinValue;
			float MaxValue;
			Lane.Width.GetValueRange(MinValue, MaxValue);
			MaxLeftWidth += FMath::Max(0.0f, MaxValue);
		}
		for (auto& Lane : Section.Right)
		{
			float MinValue;
			float MaxValue;
			Lane.Width.GetValueRange(MinValue, MaxValue);
			MaxRightWidth += FMath::Max(0.0f, MaxValue);
		}

		MaxWidth = FMath::Max3(MaxWidth, MaxLeftWidth, MaxRightWidth);
	}

	float MinOffst;
	float MaxOffset;
	RoadLayout.ROffset.GetValueRange(MinOffst, MaxOffset);

	MaxWidth += FMath::Max(FMath::Abs(MinOffst), FMath::Abs(MaxOffset));

	return Super::CalcBounds(LocalToWorld).ExpandBy(MaxWidth);
}



FRoadPosition URoadSplineComponent::GetRoadPosition(double SOffset, double ROffset, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const float Param = SplineCurves.ReparamTable.Eval(SOffset, 0.0f);
	const FVector RightVector = GetRightVectorAtSplineInputKey(Param, CoordinateSpace);

	FRoadPosition Pos;
	Pos.Location = GetLocationAtSplineInputKey(Param, CoordinateSpace) + RightVector * ROffset;
	Pos.Quat = GetQuaternionAtSplineInputKey(Param, CoordinateSpace);
	Pos.SOffset = SOffset;
	Pos.ROffset = ROffset;

	return Pos;
}

FRoadPosition URoadSplineComponent::GetRoadPosition(int SectionIndex, int LaneIndex, double Alpha, double SOffset, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const double ROffset = RoadLayout.Sections[SectionIndex].EvalLaneROffset(LaneIndex, SOffset, Alpha) + EvalROffset(SOffset);
	return GetRoadPosition(SOffset, ROffset, CoordinateSpace);
}

double URoadSplineComponent::EvalAttributeAnchorROffset(int SectionIndex, int LaneIndex, double SOffset, const FRoadLaneAttributeValue& Value) const
{
	double LaneR;
	if (LaneIndex == MetaRoad::ZeroLaneIndex)
	{
		// Centre lane: the anchor spans the road; the value type decides where (piecewise / midpoint / 0).
		const auto [LeftR, RightR] = GetLaneSection(SectionIndex).GetCenterLaneEdgeROffsets(SOffset);
		LaneR = Value.GetCenterLaneROffset(LeftR, RightR);
	}
	else
	{
		LaneR = GetLaneSection(SectionIndex).EvalLaneROffset(LaneIndex, SOffset, Value.GetKeyAlpha());
	}
	return LaneR + EvalROffset(SOffset);
}

FRoadPosition URoadSplineComponent::EvalAttributeAnchorPosition(int SectionIndex, int LaneIndex, double SOffset, const FRoadLaneAttributeValue& Value, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	return GetRoadPosition(SOffset, EvalAttributeAnchorROffset(SectionIndex, LaneIndex, SOffset, Value), CoordinateSpace);
}

const FRoadLane* URoadSplineComponent::GetRoadLane(int SectionIndex, int LaneIndex) const
{
	if (SectionIndex >= 0 && SectionIndex < GetLaneSectionsNum())
	{
		const auto& Sections = GetLaneSection(SectionIndex);
		if (LaneIndex > 0 && LaneIndex <= Sections.Right.Num())
		{
			return &Sections.Right[LaneIndex - 1];
		}
		else if (LaneIndex < 0 && -LaneIndex <= Sections.Left.Num())
		{
			return &Sections.Left[-LaneIndex - 1];
		}
	}
	return nullptr;
}

FRoadLane* URoadSplineComponent::GetRoadLane(int SectionIndex, int LaneIndex)
{
	return const_cast<FRoadLane*>(const_cast<const URoadSplineComponent*>(this)->GetRoadLane(SectionIndex, LaneIndex));
}

float URoadSplineComponent::ClosetsKeyToSegmant(float Key1, float Key2, const FVector& A1, const FVector& A2) const
{
	check(Key2 > Key1);

	FVector SubsegmentStart = GetLocationAtSplineInputKey(Key1, ESplineCoordinateSpace::World);
	double ClosestDistance = TNumericLimits<double>::Max();
	FVector OutBestLocation = SubsegmentStart;

	// Divide segment into subsegments and test each subsegment against ray representing click position and camera direction.
	// Closest encounter with the spline determines the spline position.
	const int32 NumSubdivisionsPerSegm = 16;

	int32 NumSubdivisions = NumSubdivisionsPerSegm * (Key2 - Key1) + .5f;
	if (NumSubdivisions <= 1) NumSubdivisions = 2;
	const float StepLen = (Key2 - Key1) / NumSubdivisions;

	for (int32 Step = 1; Step <= NumSubdivisions; Step++)
	{
		const float Key = Key1 + Step * StepLen;
		const FVector SubsegmentEnd = GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World);

		FVector SplineClosest;
		FVector RayClosest;
		FMath::SegmentDistToSegmentSafe(SubsegmentStart, SubsegmentEnd, A1, A2, SplineClosest, RayClosest);

		const double Distance = FVector::DistSquared(SplineClosest, RayClosest);
		if (Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			OutBestLocation = SplineClosest;
		}
		SubsegmentStart = SubsegmentEnd;
	}

	return FindInputKeyClosestToWorldLocation(OutBestLocation);
}

float URoadSplineComponent::ClosetsKeyToSegmant2(float S1, float S2, const FVector& A1, const FVector& A2) const
{
	float Key1 = SplineCurves.ReparamTable.Eval(S1, 0.0f);
	float Key2 = SplineCurves.ReparamTable.Eval(S2, 0.0f);
	return ClosetsKeyToSegmant(Key1, Key2, A1, A2);
}

float URoadSplineComponent::KeyAtRayHit(float S1, float S2, const FVector& RayStart, const FVector& RayEnd) const
{
	const float Key1 = SplineCurves.ReparamTable.Eval(S1, 0.0f);
	const float Key2 = SplineCurves.ReparamTable.Eval(S2, 0.0f);

	const float EstimateKey = ClosetsKeyToSegmant(Key1, Key2, RayStart, RayEnd);

	const FVector Origin = GetLocationAtSplineInputKey(EstimateKey, ESplineCoordinateSpace::World);
	const FVector UpVec  = GetUpVectorAtSplineInputKey(EstimateKey, ESplineCoordinateSpace::World);

	FVector HitPoint;
	if (!UpVec.IsNearlyZero() &&
		FMath::SegmentPlaneIntersection(RayStart, RayEnd, FPlane(Origin, UpVec), HitPoint))
	{
		const float HitKey = FindInputKeyClosestToWorldLocation(HitPoint);
		return FMath::Clamp(HitKey, Key1, Key2);
	}

	return EstimateKey;
}


bool URoadSplineComponent::ConvertSplineToPolyline(
	const FGetRoadPositionFunc& GetRoadPositionFunc,
	ESplineCoordinateSpace::Type CoordinateSpace,
	double InMaxSquareDistanceFromSpline, double InMinSegmentLength,
	double S0, double S1,
	const TArray<double>& AdditionalSegments,
	bool bAllowWrappingIfClosed,
	TArray<FRoadPosition>& OutPoints) const
{
	OutPoints.SetNum(0, EAllowShrinking::No);

	/*
	if (SectionIndex < 0 || SectionIndex >= RoadLayout.Sections.Num())
	{
		return false;
	}

	auto& Section = RoadLayout.Sections[SectionIndex];

	if (!Section.CheckLaneIndex(LaneIndex) && LaneIndex != MetaRoad::ZeroLaneIndex)
	{
		return false;
	}
	*/

	const int32 NumPoints = SplineCurves.Position.Points.Num();
	if (NumPoints == 0)
	{
		return false;
	}
	const int32 NumSegments = GetNumberOfSplineSegments();

	float SplineLength = SplineCurves.GetSplineLength();
	if (SplineLength <= 0)
	{
		OutPoints.Add(GetRoadPositionFunc(0.0, CoordinateSpace));
		return false;
	}

	// Sanitize the sampling tolerance
	const float MaxSquareDistanceFromSpline = FMath::Max(UE_SMALL_NUMBER, InMaxSquareDistanceFromSpline);
	const float MinSegmentLength = FMath::Max(UE_SMALL_NUMBER, InMinSegmentLength);

	// Sanitize range and mark whether the range wraps through 0
	bool bNeedsWrap = false;
	if (!IsClosedLoop() || !bAllowWrappingIfClosed)
	{
		S0 = FMath::Clamp(S0, 0, SplineLength);
		S1 = FMath::Clamp(S1, 0, SplineLength);
	}
	else if (S0 < 0 || S1 > SplineLength)
	{
		bNeedsWrap = true;
	}
	if (S0 > S1)
	{
		return false;
	}

	// expect at least 2 points per segment covered
	int32 EstimatedPoints = 2 * NumSegments * static_cast<int32>((S1 - S0) / SplineLength);
	OutPoints.Empty();
	OutPoints.Reserve(EstimatedPoints);

	if (S0 == S1)
	{
		OutPoints.Add(GetRoadPositionFunc(S0, CoordinateSpace));
		return true;
	}

	// If we need to wrap around, break the wrapped segments into non-wrapped parts and add each part separately
	if (bNeedsWrap)
	{
		float TotalRange = S1 - S0;
		auto WrapDistance = [SplineLength](float Distance, int32& LoopIdx) -> float
		{
			LoopIdx = FMath::FloorToInt32(Distance / SplineLength);
			float WrappedDistance = FMath::Fmod(Distance, SplineLength);
			if (WrappedDistance < 0)
			{
				WrappedDistance += SplineLength;
			}
			return WrappedDistance;
		};
		int32 StartLoopIdx, EndLoopIdx;
		float WrappedStart = WrapDistance(S0, StartLoopIdx);
		float WrappedEnd = WrapDistance(S1, EndLoopIdx);
		float WrappedLoc = WrappedStart;
		bool bHasAdded = false;
		for (int32 LoopIdx = StartLoopIdx; LoopIdx <= EndLoopIdx; ++LoopIdx)
		{
			if (bHasAdded && ensure(OutPoints.Num()))
			{
				OutPoints.RemoveAt(OutPoints.Num() - 1, EAllowShrinking::No);
			}
			float EndLoc = LoopIdx == EndLoopIdx ? WrappedEnd : SplineLength;

			TArray<FRoadPosition> Points;
			ConvertSplineToPolyline(GetRoadPositionFunc, CoordinateSpace, MaxSquareDistanceFromSpline, MinSegmentLength, WrappedLoc, EndLoc, AdditionalSegments, false, Points);
			OutPoints.Append(Points);

			bHasAdded = true;
			WrappedLoc = 0;
		}
		return bHasAdded;
	} // end of the wrap-around case, after this values will be in the normal range


	// Create Segments - All starting points (SOffset)
	TArray<double> Segments = AdditionalSegments;
	for (int PointIndex = 0; PointIndex < SplineCurves.Position.Points.Num() + (IsClosedLoop() ? 1 : 0); ++PointIndex)
	{
		Segments.Add(GetDistanceAlongSplineAtSplinePoint(PointIndex));
	}
	for (auto& Section : RoadLayout.Sections)
	{
		Segments.Add(Section.SOffset);
	}
	if (RoadLayout.Sections.Num())
	{
		Segments.Add(RoadLayout.Sections.Last().SOffsetEnd_Cashed);
	}
	for (auto& Key : RoadLayout.ROffset.Keys)
	{
		Segments.Add(Key.Time);
	}
	Segments.Append({ S0, S1 });
	Segments.Sort();
	RemoveOutsideRange(Segments, S0, S1);
	RemoveEmptySegments(Segments);

	TArray<FRoadPosition> NewPoints;
	for (int PointIndex = 1; PointIndex < Segments.Num(); ++PointIndex)
	{
		// Get the segment range as distances, clipped with the input range
		double StartDist = Segments[PointIndex - 1];
		double StopDist = Segments[PointIndex];
		bool bIsLast = (PointIndex == (Segments.Num() - 1));

		const int32 NumLines = 2; // Dichotomic subdivision of the spline segment
		double Dist = StopDist - StartDist;
		double SubstepSize = Dist / NumLines;
		if (SubstepSize == 0.0)
		{
			// There is no distance to cover, so handle the segment with a single point (or nothing, if this isn't the very last point)
			if (bIsLast)
			{
				OutPoints.Add(GetRoadPositionFunc(StopDist, CoordinateSpace));
			}
			continue;
		}

		double SubstepStartDist = StartDist;
		for (int32 i = 0; i < NumLines; ++i)
		{
			double SubstepEndDist = SubstepStartDist + SubstepSize;
			NewPoints.Reset();
			// Recursively sub-divide each segment until the requested precision is reached :
			if (DivideSplineIntoPolylineRecursiveWithDistancesHelper2(GetRoadPositionFunc, SubstepStartDist, SubstepEndDist, CoordinateSpace, MaxSquareDistanceFromSpline, MinSegmentLength, NewPoints))
			{
				if (OutPoints.Num() > 0)
				{
					check(IsNearlyEqual(OutPoints.Last(), NewPoints[0])); // our last point must be the same as the new segment's first
					OutPoints.RemoveAt(OutPoints.Num() - 1);
				}
				OutPoints.Append(NewPoints);
			}

			SubstepStartDist = SubstepEndDist;
		}
	}

	return !OutPoints.IsEmpty();
}

bool URoadSplineComponent::DivideSplineIntoPolylineRecursiveWithDistancesHelper2(const FGetRoadPositionFunc& GetRoadPositionFunc, double StartDistanceAlongSpline, double EndDistanceAlongSpline, ESplineCoordinateSpace::Type CoordinateSpace, double MaxSquareDistanceFromSpline, double MinSegmentLength, TArray<FRoadPosition>& OutPoints) const
{

	double Dist = EndDistanceAlongSpline - StartDistanceAlongSpline;
	if (Dist <= 0.0f)
	{
		return false;
	}
	double MiddlePointDistancAlongSpline = StartDistanceAlongSpline + Dist / 2.0f;
	FRoadPosition Samples[3];
	Samples[0] = GetRoadPositionFunc(StartDistanceAlongSpline, CoordinateSpace);
	Samples[1] = GetRoadPositionFunc(MiddlePointDistancAlongSpline, CoordinateSpace);
	Samples[2] = GetRoadPositionFunc(EndDistanceAlongSpline, CoordinateSpace);


	if (FMath::PointDistToSegmentSquared(Samples[1].Location, Samples[0].Location, Samples[2].Location) > MaxSquareDistanceFromSpline || FVector::Dist(Samples[0].Location, Samples[1].Location) > MinSegmentLength)
	{
		TArray<FRoadPosition> NewPoints[2];
		DivideSplineIntoPolylineRecursiveWithDistancesHelper2(GetRoadPositionFunc, StartDistanceAlongSpline, MiddlePointDistancAlongSpline, CoordinateSpace, MaxSquareDistanceFromSpline, MinSegmentLength, NewPoints[0]);
		DivideSplineIntoPolylineRecursiveWithDistancesHelper2(GetRoadPositionFunc, MiddlePointDistancAlongSpline, EndDistanceAlongSpline, CoordinateSpace, MaxSquareDistanceFromSpline, MinSegmentLength, NewPoints[1]);
		if ((NewPoints[0].Num() > 0) && (NewPoints[1].Num() > 0))
		{
			check(IsNearlyEqual(NewPoints[0].Last(), NewPoints[1][0]));
			NewPoints[0].RemoveAt(NewPoints[0].Num() - 1);
		}
		NewPoints[0].Append(NewPoints[1]);
		OutPoints.Append(NewPoints[0]);
	}
	else
	{
		// The middle point is close enough to the other 2 points, let's keep those and stop the recursion :
		OutPoints.Add(Samples[0]);
		// For a constant spline, the end can be the exact same as the start; in this case, just add the point once
		if (!IsNearlyEqual(Samples[0], Samples[2]))
		{
			OutPoints.Add(Samples[2]);
		}

	}

	return (OutPoints.Num() > 0);
}

bool URoadSplineComponent::LineTrace(const FVector& Start, const FVector& End, FRoadHitResult& OutHit) const
{
	const int32 NumPoints = GetNumberOfSplinePoints();
	if (NumPoints < 2 || RoadLayout.Sections.IsEmpty())
	{
		return false;
	}

	// 1. Find the spline key at the closest approach between spline and segment
	const float Key = ClosetsKeyToSegmant(0.f, static_cast<float>(NumPoints - 1), Start, End);

	// 2. Build the road surface plane at that key.
	//    Normal = road up vector (perpendicular to the road surface), NOT the tangent.
	//    Using the tangent as normal creates a cross-section plane that is nearly parallel
	//    to a vertical camera ray, which causes SegmentPlaneIntersection to produce a
	//    numerically wrong Z. The up vector gives a "floor" plane that a vertical ray
	//    always crosses at the correct road-surface height.
	const FVector Origin = GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World);
	const FVector UpVec  = GetUpVectorAtSplineInputKey(Key, ESplineCoordinateSpace::World);
	if (UpVec.IsNearlyZero())
	{
		return false;
	}

	// 3. Intersect the segment with the road surface plane
	FVector HitPoint;
	if (!FMath::SegmentPlaneIntersection(Start, End, FPlane(Origin, UpVec), HitPoint))
	{
		return false;
	}

	// 4. Convert hit point to road coordinates: S-offset (along spline) and R-offset (lateral, signed)
	const float   SOffset  = GetDistanceAlongSplineAtSplineInputKey(Key);
	const FVector RightVec = GetRightVectorAtDistanceAlongSpline(SOffset, ESplineCoordinateSpace::World);
	const double  ROffset  = FVector::DotProduct(HitPoint - Origin, RightVec);

	// 5. Find the section index at this S-offset, then resolve the correct Left/Right
	//    physical sections via FindSideSection.
	//
	//    A lane can span several FRoadLaneSections. Sections can have Side == Left,
	//    Right, or Both. FindSectionAtSOffset returns the first section whose
	//    SOffsetEnd_Cashed > SOffset — but that section may only define one side
	//    (e.g. Side == Left). FindSideSection walks backwards from that index to find
	//    the nearest section that actually contains Left or Right lane data.
	const int32 SectionIdx = RoadLayout.FindSectionAtSOffset(SOffset);
	if (!RoadLayout.Sections.IsValidIndex(SectionIdx))
	{
		return false;
	}

	const int32 LeftSectionIdx  = RoadLayout.FindSideSection(SectionIdx, ERoadLaneSectionSide::Left);
	const int32 RightSectionIdx = RoadLayout.FindSideSection(SectionIdx, ERoadLaneSectionSide::Right);

	const FRoadLaneSection& LeftSection  = RoadLayout.Sections[LeftSectionIdx];
	const FRoadLaneSection& RightSection = RoadLayout.Sections[RightSectionIdx];

	// Alpha is [0..1] within the physical section where the lane is defined.
	auto CalcAlpha = [SOffset](const FRoadLaneSection& Sec) -> double
	{
		const double Len = Sec.SOffsetEnd_Cashed - Sec.SOffset;
		return (Len > KINDA_SMALL_NUMBER) ? ((double)SOffset - Sec.SOffset) / Len : 0.0;
	};

	// 6. Find which lane contains the R-offset, using the correct side section.
	//    Returns false if the hit point falls outside all defined lanes.
	int32 OutSectionIdx = INDEX_NONE;
	int32 LaneIndex     = MetaRoad::ZeroLaneIndex;

	if (ROffset >= 0.0)
	{
		const double Alpha = CalcAlpha(RightSection);
		for (int32 i = 1; i <= RightSection.Right.Num(); ++i)
		{
			if (ROffset <= RightSection.EvalLaneROffset(i, SOffset, Alpha))
			{
				LaneIndex    = i;
				OutSectionIdx = RightSectionIdx;
				break;
			}
		}
	}
	else
	{
		const double Alpha = CalcAlpha(LeftSection);
		for (int32 i = -1; i >= -LeftSection.Left.Num(); --i)
		{
			if (ROffset >= LeftSection.EvalLaneROffset(i, SOffset, Alpha))
			{
				LaneIndex    = i;
				OutSectionIdx = LeftSectionIdx;
				break;
			}
		}
	}

	if (LaneIndex == MetaRoad::ZeroLaneIndex)
	{
		return false;
	}

	OutHit.HitPoint     = HitPoint;
	OutHit.SectionIndex = OutSectionIdx;
	OutHit.LaneIndex    = LaneIndex;
	return true;
}

void URoadSplineComponent::MarkRoadStateDirty()
{
	FMetaRoadDelegates::OnRoadComponentDirtyDelegate.Broadcast(this);
}

void URoadSplineComponent::RefreshConnectionGuids()
{
	for (auto& Section : RoadLayout.Sections)
	{
		for (auto& It : Section.Left)
		{
			if (IsValid(It.PredecessorConnection)) It.PredecessorConnection->GetAndUpdateGuid();
			if (IsValid(It.SuccessorConnection)) It.SuccessorConnection->GetAndUpdateGuid();
		}
		for (auto& It : Section.Right)
		{
			if (IsValid(It.PredecessorConnection)) It.PredecessorConnection->GetAndUpdateGuid();
			if (IsValid(It.SuccessorConnection)) It.SuccessorConnection->GetAndUpdateGuid();
		}
	}

	static auto UpdateRoadConnection = [](URoadConnection* Conn)
	{
		if (Conn->OuterLaneConnection.IsValid() && Conn->OuterLaneConnection->IsConnectionValid())
		{
			Conn->LaneConnectionGuid = Conn->OuterLaneConnection->GetAndUpdateGuid();
		}
		else
		{
			Conn->LaneConnectionGuid = {};
		}
	};

	UpdateRoadConnection(GetPredecessorConnection());
	UpdateRoadConnection(GetSuccessorConnection());
}

#undef LOCTEXT_NAMESPACE