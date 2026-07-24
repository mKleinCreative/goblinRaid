/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "MetaRoadTypes.h"
#include "RoadHitProxies.h"

class AMetaRoad;


namespace RoadUtils
{

	METAROADEDITOR_API void  DrawTriangle(class FPrimitiveDrawInterface* PDI, const FVector& A, const FVector& B, const FVector& C, const FColor& VertexColor, uint8 DepthPriorityGroup);
	METAROADEDITOR_API void  DrawRoadLaneConnection(float Size, bool bIsSuccessor, bool bReverse, const FTransform& InTransform, const FColor& VertexColor, class FPrimitiveDrawInterface* PDI, const FSceneView* View, uint8 DepthPriorityGroup);


	struct FViewCameraState
	{
		const FMatrix& ViewToProj;
		const FIntRect& ViewRect;
		const FVector& ViewPosition;
		bool bIsOrthographic;
		float OrthoWorldCoordinateWidth;
	};

	METAROADEDITOR_API TSet<TWeakObjectPtr<const ULaneConnection>> CaptureConnections(
		const URoadConnection* SrcConnection, 
		const FViewCameraState& CameraState, 
		double MaxViewDistance, 
		double MaxOrthoWidth, 
		TFunction<bool(const ULaneConnection*)> IsConnectionAllowed = [](const ULaneConnection*) { return true; }
	);

	inline int GetRightOf(int LaneIndex, int Num)
	{
		check(Num >= 0);

		int NewIndex = LaneIndex + Num;
		if (LaneIndex < 0)
		{
			++NewIndex;
			if (NewIndex >= 0)
			{
				++NewIndex;
			}
		}
		return NewIndex;
	}

	inline int GetLeftOf(int LaneIndex, int Num)
	{
		check(Num >= 0);

		int NewIndex = LaneIndex - Num;
		if (LaneIndex > 0)
		{
			--NewIndex;
			if (NewIndex <= 0)
			{
				--NewIndex;
			}
		}
		return NewIndex;
	}

	METAROADEDITOR_API void FitLanesWidthToEndConnection(URoadSplineComponent* TargetSpline, const ULaneConnection* EndLaneConnection);
	METAROADEDITOR_API void FitLanesWidthToBeginConnection(URoadSplineComponent* TargetSpline, const ULaneConnection* EndLaneConnection);

	METAROADEDITOR_API AMetaRoad* SpawnRoadActor(UWorld* World, const FTransform& Tranform);

	/**
	 * Creates a URoadSplineComponent and attaches it to Actor. Low-level helper kept generic on purpose:
	 * besides AMetaRoad it is also used on the tools' transient preview/Blueprint actors while drawing.
	 * bTransact=true — marks Actor and component RF_Transactional (required for undo/redo).
	 * bSetAsRoot=true — sets the new component as root instead of attaching it.
	 */
	METAROADEDITOR_API URoadSplineComponent* CreateSplineInActor(
		AActor* Actor, bool bTransact = false, bool bSetAsRoot = false);

	/**
	 * Copies all points, tangents, up-vectors, point types, and the closed-loop flag
	 * from Source to Destination in world coordinates.
	 * bTransact=true — calls Destination.Modify() before writing.
	 */
	METAROADEDITOR_API void CopySplineToSpline(
		const URoadSplineComponent& Source,
		URoadSplineComponent& Destination,
		bool bTransact = false);

} // RoadUtils