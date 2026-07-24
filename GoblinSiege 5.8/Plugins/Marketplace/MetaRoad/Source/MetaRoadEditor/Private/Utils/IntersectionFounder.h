/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "RoadSplineComponent.h"

namespace MetaRoad
{

struct FSplinePosition
{
	FVector Position;
	double SOffset;
};


class FIntersectionFounder
{
public:
	enum class ENodeLaneFlags : uint8
	{
		//Forward,
		TurnLeft = (1 << 0),
		TurnRight = (1 << 1),
		TurnAround = (1 << 2),
	};

	/*
	enum class ESplineIntersectionSide : uint8
	{
		None,
		Begin,
		End,
		Full
	};
	*/

	enum class ENodeDir : uint8
	{
		None,
		Forward,
		Backward
	};

	//enum class ESide : uint8
	//{
	//	Left,
	//	Right,
	//};


	//struct FNodeLane
	//{
	//	uint8 Flags; // Flags from ENodeLaneFlags
	//};

	struct FIntersection
	{
		int Index;
		double SOffset;

		//FSplinePosition Position;
		//FSplinePosition TargetPosition;
		//int TargetSplineIndex;
		//ESide TargetSide;
		//int TargetIntersectionIndex;

	};

	struct FNode: TSharedFromThis<FNode>
	{
		//int SplineIndex;
		double SOffset{};
		ENodeDir Dir{};
		int LeftIntersectionIndex = -1;
		int RightIntersectionIndex = -1;

		//TArray<FNodeLane> LeftLanes{};
		//TArray<FNodeLane> RightLanes{};

		TWeakPtr<FNode> ToLeft;
		TWeakPtr<FNode> ToRight;

	};

	struct FSplineData
	{
		TWeakObjectPtr<const URoadSplineComponent> Spline;

		TArray<FSplinePosition> PolyLeft;
		TArray<FSplinePosition> PolyRight;

		TArray<FIntersection> LeftIntersections;
		TArray<FIntersection> RightIntersections;

		//ESplineIntersectionSide Side{};
		TSharedPtr<FNode> NodeBegin;
		TSharedPtr<FNode> NodeEnd{};
	};

	FIntersectionFounder()
	{}

	bool Setup(TArray<TObjectPtr<const URoadSplineComponent>>& InputSplines);

	bool Solve();

	TArray<FSplineData>& GetSplinesData() { return SplinesData; }
	const TArray<FVector>& GetIntersections() const { return Intersections; }
	
protected:
	TArray<FSplineData> SplinesData;
	TArray<FVector> Intersections; // World Space

	

	bool SolveTIntersection();
	bool SolveXIntersection();
};

} // namespace MetaRoad