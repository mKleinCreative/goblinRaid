/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "RoadMeshBuild/RoadSplineEditorComponent.h"
#include "Polygon2.h"
#include "CompGeom/Delaunay2.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Util/ProgressCancel.h"
#include "Utils/GeomUtils.h"

using namespace UE::Geometry;

namespace MetaRoad 
{
	struct FProceduralPolygon;
	struct FRoadTriangulationData;

	/**
	 * FRoadLanePolyline
	 */
	namespace GUIFlags
	{
		static uint8 DrivingSurface = 1 << 0;
		static uint8 SidewalksSoft  = 1 << 1;
		static uint8 SidewalksHard  = 1 << 2;
		static uint8 CenterLine     = 1 << 3;

		static uint8 Other          = 1 << 7;
	}

	/**
	 * ERoadVertexInfoFlags
	 */
	enum class ERoadVertexInfoFlags: int32
	{
		Inside = (1U << 0),
		Outside = (1U << 1),
		EndCap = (1U << 2),
		BeginCap = (1U << 3),
		LoopSeam = (1U << 4),
		OverlapPoly = (1U << 5),
		LoopPoly = (1U << 6),
	};
	ENUM_CLASS_FLAGS(ERoadVertexInfoFlags)

	/**
	 * FRoadVertexInfo
	 */
	struct FRoadVertexInfo
	{
		const FProceduralPolygon * Poly;
		FRoadPosition Pos;
		double Location;
		double Alpha0;
		double Alpha1;
		double Alpha2;
		int VID;
		ERoadVertexInfoFlags Flags;
		double HeightOffset;
	};

	/**
	 * FArrangementVertex3d
	 */
	struct METAROADEDITOR_API FArrangementVertex3d
	{
		TMap<const FProceduralPolygon*, FRoadVertexInfo> Infos;
		FVector Vertex;
		FVector Normal; 
	};

	/**
	 * ERoadPolygonType
	 */
	enum ERoadPolygonType
	{
		RoadLane,
		SplineLoop,
		Simple
		//Polyline
	};

	/**
	 * FLineInfo 
	 * Notes:
	 * If VID_B == -1 means the line contain only one point (VID_A)
	 * If VID_A == VID_B means the line is loop
	 * if VID_A == -1 means line is not valid
	 */
	struct FLineInfo
	{
		int PID = -1;
		int VID_A = -1;
		int VID_B = -1;

		inline bool IsValid() const { return VID_A != -1; }
		inline bool IsLoop() const { return IsValid() && VID_A == VID_B; }
	};

	/**
	 * FTriInfo
	 */
	struct FTriInfo
	{
		FRoadVertexInfo* A;
		FRoadVertexInfo* B;
		FRoadVertexInfo* C;

		inline bool IsValid() const { return A && B && C; }
		inline FRoadVertexInfo& GetVertexInfo(int VID) const
		{
			check(IsValid());
			auto& Ret = A->VID == VID ? *A : (B->VID == VID ? *B : *C);
			check(Ret.VID == VID);
			return Ret;
		}
	};

	/**
	 * FProceduralPolygon
	 */
	struct METAROADEDITOR_API FProceduralPolygon
	{


		FProceduralPolygon(FRoadTriangulationData& Owner)
			: Owner(Owner)
		{}

		virtual ~FProceduralPolygon(){}

		virtual ERoadPolygonType GetType() const = 0;
		virtual bool OnCompleteArrangement() = 0;
		virtual void AddVertexInfo(int VID, ERoadVertexInfoFlags Flags) const = 0;
		virtual const TInstancedStruct<FRoadZone>& GetRoadZone() const = 0;
		virtual FText GetDescription() const = 0;
		virtual bool SetUVLayers(FDynamicMesh3& Mesh, int TID, double UV0ScaleFactor, double UV1ScaleFactor, double UV2ScaleFactor, int UVMaxSize) const = 0;
		virtual double GetMaterialPriority() const = 0;
		virtual int GetObjectIndex() const = 0;

		const UE::Geometry::FGeometryResult& GetResult() const { return ResultInfo; }

		FTriInfo FindTri(int TID) const;
		inline bool IsPolyline() const { return Boundary.Num() && Boundary[0].A != Boundary.Last().B; }
		FLineInfo AddToArrangement(const TArray<FVector2D>& Points, int GID);

		FRoadTriangulationData& Owner;

		TArray<FIndex2i> Boundary; // ClockWise
		TArray<TArray<FIndex2i>> Holes; // ClockWise
		//TArray<FIndex2i> InnerIndexes; // TODO
		TArray<int> TrianglesIDs;
		TArray<int> LineVertices;        // ordered boundary vertex list (loop / simple polygons)
		UE::Geometry::FPolygon2d Poly2d; // 2D polygon, built on arrangement completion
		UE::Geometry::FGeometryResult ResultInfo;



	};

	/**
	 * FProceduralPolygon_RoadBase 
	 */
	struct METAROADEDITOR_API FProceduralPolygon_RoadBase: public FProceduralPolygon
	{
		FProceduralPolygon_RoadBase(FRoadTriangulationData& Owner, int SplineIndex)
			: FProceduralPolygon(Owner)
			, SplineIndex(SplineIndex)
		{
		}
		virtual ~FProceduralPolygon_RoadBase()
		{
		}

		virtual double GetMaterialPriority() const override;
		virtual int GetObjectIndex() const override { return SplineIndex; }

		const URoadSplineEditorComponent& GetRoadSpline() const;
		URoadSplineEditorComponent& GetRoadSpline();

		const UE::Geometry::FAxisAlignedBox2d& GetSplineBounds() const;
		UE::Geometry::FAxisAlignedBox2d& GetSplineBounds();

		int GetSplineIndex() const { return SplineIndex; }

	private:
		int SplineIndex;
	};

	/**
	 * FProceduralPolygon_RoadLane
	 */
	struct METAROADEDITOR_API FProceduralPolygon_RoadLane: public FProceduralPolygon_RoadBase
	{
		FProceduralPolygon_RoadLane(FRoadTriangulationData& Owner, int SplineIndex, int SectionIndex, int LaneIndex, double MaxSquareDistanceFromSpline, double MaxSquareDistanceFromCap, double MinSegmentLength);

		virtual bool OnCompleteArrangement() override;

		virtual ERoadPolygonType GetType() const override { return ERoadPolygonType::RoadLane; }
		virtual void AddVertexInfo(int VID, ERoadVertexInfoFlags Flags) const override;
		virtual const TInstancedStruct<FRoadZone>& GetRoadZone() const override;
		virtual FText GetDescription() const override;
		virtual bool SetUVLayers(FDynamicMesh3& Mesh, int TID, double UV0ScaleFactor, double UV1ScaleFactor, double UV2ScaleFactor, int UVMaxSize) const override;
		
		const FRoadLaneSection& GetSection() const;
		const FRoadLane& GetLane() const;
		const TMap<TSoftClassPtr<URoadLaneAttributeDescriptor>, FRoadLaneAttribute>& GetLaneAttributes() const;
		double GetStartOffset() const;
		double GetEndOffset() const;

		bool IsLoop() const { return bIsLoop; }

		enum class ERoadLaneSide
		{
			Inner,
			Outer
		};

		TArray<FRoadPosition> GetPolyline(int LaneIndex, double Alpha) const;
		TArray<FRoadPosition> GetCenterPolyline(double S0, double S1) const;
		TArray<FRoadPosition> GetPolyline(FProceduralPolygon_RoadLane::ERoadLaneSide Side) const;

		int GetSectionIndex() const { return SectionIndex; }
		int GetLaneIndex() const { return LaneIndex; }

		TArray<int> InsideLineVertices;
		TArray<int> EndCapVertices;
		TArray<int> OutsideLineVertices;
		TArray<int> BeginCapVertices;

		FAxisAlignedBox2d Bounds;
		FAxisAlignedBox2d LocalSplineBounds; // X - SOffset, Y - ROffset


	private:
		void AddVertexInfo(int VID, ERoadVertexInfoFlags Flags, bool bWithUV1) const;
		bool ProcessPolyline(const FLineInfo& LineInfo, TArray<int>& VIDs, ERoadVertexInfoFlags Flags);

	private:
		int SectionIndex;
		int LaneIndex;
		double MaxSquareDistanceFromSpline;
		double MinSegmentLength;
		FLineInfo InsideLineInfo;
		FLineInfo EndCapInfo;
		FLineInfo OutsideLineInfo;
		FLineInfo BeginCapInfo;
		bool bIsLoop;
	};

	/**
	 * FProceduralPolygon_RoadLoop
	 */
	struct METAROADEDITOR_API FProceduralPolygon_RoadLoop : public FProceduralPolygon_RoadBase
	{
		FProceduralPolygon_RoadLoop(FRoadTriangulationData& Owner, int SplineIndex, double MaxSquareDistanceFromSpline, double MinSegmentLength);

		virtual ERoadPolygonType GetType() const override { return ERoadPolygonType::SplineLoop; }
		virtual bool OnCompleteArrangement() override;
		virtual void AddVertexInfo(int VID, ERoadVertexInfoFlags Flags) const override;
		virtual const TInstancedStruct<FRoadZone>& GetRoadZone() const override;
		virtual FText GetDescription() const override;
		virtual bool SetUVLayers(FDynamicMesh3& Mesh, int TID, double UV0ScaleFactor, double UV1ScaleFactor, double UV2ScaleFactor, int UVMaxSize) const override;

	private:
		FLineInfo LineInfo;
	};

	/**
	 * FProceduralPolygon_Simple
	 */
	struct METAROADEDITOR_API FProceduralPolygon_Simple : public FProceduralPolygon
	{
		FProceduralPolygon_Simple(FRoadTriangulationData& Owner, int SimplePolygonIndex);

		virtual ERoadPolygonType GetType() const override  { return ERoadPolygonType::Simple; }
		virtual bool OnCompleteArrangement() override;
		virtual void AddVertexInfo(int VID, ERoadVertexInfoFlags Flags) const override;
		virtual const TInstancedStruct<FRoadZone>& GetRoadZone() const override;
		virtual FText GetDescription() const override;
		virtual bool SetUVLayers(FDynamicMesh3& Mesh, int TID, double UV0ScaleFactor, double UV1ScaleFactor, double UV2ScaleFactor, int UVMaxSize) const override;
		virtual double GetMaterialPriority() const override;
		virtual int GetObjectIndex() const override;

	protected:
		FLineInfo LineInfo;
		int SimplePolygonIndex;
		GeomUtils::FPolygonIntersectApproximation IntersectApproximation;
		FVector WorldCenter;
	};

	namespace RoadPolygonUtils
	{
		TArray<FRoadPosition> MakePolyline(const TArray<FArrangementVertex3d>& Vertexes, const TArray<int>& VerticesIDs, const FProceduralPolygon* PolyFilter);
	}



} // MetaRoad
