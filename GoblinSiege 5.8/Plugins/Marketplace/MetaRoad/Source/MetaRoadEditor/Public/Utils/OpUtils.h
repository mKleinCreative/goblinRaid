/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "RoadSplineComponent.h"
#include "Geometry/DynamicGraph2.h"
#include "DynamicMesh/DynamicMesh3.h"


namespace OpUtils 
{
	using namespace UE::Geometry;

	using TGIDFilter = TFunction<bool(int)>;
	METAROADEDITOR_API bool FindBoundary(const MetaRoad::FDynamicGraph2d& Graph, const TArray<FIndex2i> & SkipEdges, TArray<FIndex2i>& Boundary, const TGIDFilter& GIDFilter);
	METAROADEDITOR_API int FindBoundaries(const MetaRoad::FDynamicGraph2d& Graph, const TArray<FIndex2i>& SkipEdges, TArray<TArray<FIndex2i>>& Boundaries, TGIDFilter GIDFilter);
	inline int FindBoundaries(const MetaRoad::FDynamicGraph2d& Graph, const TArray<FIndex2i>& SkipEdges, TArray<TArray<FIndex2i>>& Boundaries, int GID)
	{ 
		return FindBoundaries(Graph, SkipEdges, Boundaries, [AllowedGID = GID](int GID) { return AllowedGID == -1 || AllowedGID == GID; });
	}
	METAROADEDITOR_API TArray<FIndex2i> MergeBoundaries(const TArray<TArray<FIndex2i>>& Boundary);
	METAROADEDITOR_API TArray<FIndex2i> MergeBoundaries(const TArray<TArray<FIndex2i>>& Boundary, const TArray<TArray<FIndex2i>>& Holes);
	METAROADEDITOR_API bool IsSameBoundary(const TArray<FIndex2i>& BoundaryA, const TArray<FIndex2i>& BoundaryB);
	// Remove the probe boundaries from the target boundaries
	METAROADEDITOR_API void RemoveBoundaries(const TArray<TArray<FIndex2i>>& ProbeBoundaries, TArray<TArray<FIndex2i>>& TargetBoundaries);
	METAROADEDITOR_API TArray<FIndex2i> ReverseBoundary(const TArray<FIndex2i>& Boundary);

	METAROADEDITOR_API void RemoveTriangles(const TArray<FIndex3i>& Probes, TArray<FIndex3i>& Targets);

	inline bool IsTriangleValid(const FIndex3i& T)
	{
		return T.A != IndexConstants::InvalidID && T.B != IndexConstants::InvalidID && T.C != IndexConstants::InvalidID;
	}

	inline double AngleBetweenNormals(const FVector& V1, const FVector& V2)
	{
		double s = FVector::CrossProduct(V1, V2).Size();
		double c = FVector::DotProduct(V1, V2);
		return FMath::Atan2(s, c);
	}


	struct METAROADEDITOR_API FMaterialSlotMap
	{
		int MaterialIndex = 0;
		int CustomMaterialIndex = 0;

		struct FItem
		{
			int Index;
			TWeakObjectPtr<UMaterialInterface> Material;
		};

		TMap<FName, FItem> MaterialMap;

		// Return Material ID
		int AddMaterial(FName SlotName);

		int AddMaterial(UMaterialInterface* CustomMaterial);

		// Return Material ID
		int AddMaterial(FRoadZoneType ZoneType);

		// Return Material ID
		int AddMaterial(const TInstancedStruct<FRoadZone>& RoadZone);

		TArray<TPair<FName, TWeakObjectPtr<UMaterialInterface>>> GetSlots();
	};

} // OpUtils

// For debug only
/*
UCLASS()
class METAROADEDITOR_API UOpUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	static bool BakeCurve(const UCurveFloat* SrcCurve, UCurveFloat* TargetCurve, float TimeStart, float TimeEnd, float MaxSquareDistanceFromCurve, float Tolerance, int ReparamSteps = 200);
};
*/

