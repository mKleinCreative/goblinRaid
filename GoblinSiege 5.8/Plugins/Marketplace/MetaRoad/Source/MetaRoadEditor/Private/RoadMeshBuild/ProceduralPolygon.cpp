/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadMeshBuild/ProceduralPolygon.h"
#include "MetaRoadModule.h"
#include "MetaRoadSettings.h"
#include "Assets/RoadLaneAttributePolygoneCustomization.h"
#include "Assets/RoadLaneAttributeSidewalkHeight.h"
#if METAROAD_PRO
#include "Assets/RoadLaneAttributePolygon.h"
#endif
#include "Utils/OpUtils.h"
#include "Utils/CurveUtils.h"
#include "RoadMeshBuild/Ops/TriangulateRoadOp.h"
#include "Algo/MaxElement.h"
#include "Algo/MinElement.h"
#include "Misc/Optional.h"
#include "Utils/RoadUtils.h"
#include "Utils/GeomUtils.h"
#include <queue>
#include <map>

#define LOCTEXT_NAMESPACE "FProceduralPolygon"

using namespace MetaRoad;

static void RemovePointsFromBegin(TArray<FVector2D>& Vertices, double Distance)
{
	if (Vertices.Num() && Distance >= 0)
	{
		double Dt = 0;
		for (int i = 1; i < Vertices.Num(); ++i)
		{
			Dt += (Vertices[i] - Vertices[i - 1]).Length();
			if (Dt >= Distance)
			{
				Vertices.RemoveAt(0, i);
				return;
			}
		}
		Vertices.Empty();
	}
}

static void RemovePointsFromEnd(TArray<FVector2D>& Vertices, double Distance)
{
	if (Vertices.Num() && Distance > 0)
	{
		double Dt = 0;
		for (int i = Vertices.Num() - 2; i >= 0; --i)
		{
			Dt += (Vertices[i] - Vertices[i + 1]).Length();
			if (Dt >= Distance)
			{
				Vertices.RemoveAt(i + 1, Vertices.Num() - i - 1);
				return;
			}
		}
		Vertices.Empty();
	}
}


// @param Adj - Assuming graph representation as adjacency list {neighbor, weight}. See https://en.wikipedia.org/wiki/Adjacency_list
static TArray<int> Dijkstra(const TMap<int, TArray<TPair<int, double>>>& Adj, int Source, int Destination)
{
	TMap<int, double> dist;
	TMap<int, int> parent; // Store parent for path reconstruction

	for (auto& [Key, Value] : Adj)
	{
		dist.Add(Key, std::numeric_limits<double>::max());
		parent.Add(Key, -1);
	}

	std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, std::greater<std::pair<double, int>>> pq;

	dist[Source] = 0.0;
	pq.push({ 0.0, Source });

	while (!pq.empty()) 
	{
		double d = pq.top().first;
		int u = pq.top().second;
		pq.pop();

		if (d > dist[u]) continue; // Already found a shorter path

		for (const auto& edge : Adj[u])
		{
			int v = edge.Key;
			double weight = edge.Value;

			if (dist[u] + weight < dist[v]) 
			{
				dist[v] = dist[u] + weight;
				parent[v] = u; // Update parent
				pq.push({ dist[v], v });
			}
		}
	}

	// Reconstruct the path
	TArray<int> Path;
	int currentNode = Destination;
	while (currentNode != -1 && currentNode != Source) // Stop if no path or reached source
	{ 
		Path.Insert(currentNode, 0); // Add to front for correct order
		currentNode = parent[currentNode];
	}
	if (currentNode == Source)  // If source was reached
	{
		Path.Insert(Source, 0);
	}
	else 
	{
		// No path found (destination unreachable from source)
		return {}; // Return empty path
	}

	return Path;
}

static bool FindPolyline(const MetaRoad::FDynamicGraph2d& Graph, const FLineInfo& LineInfo, TArray<int>& VIDs)
{
	if (LineInfo.VID_B == -1 && Graph.IsVertex(LineInfo.VID_A))
	{
		VIDs.Add(LineInfo.VID_A);
		return true;
	}

	TMap<int, TArray<TPair<int, double>>> Adj;
	for (int EID : Graph.EdgeIndices())
	{
		if (Graph.HasPolylineID(EID, LineInfo.PID))
		{
			const auto& Edge = Graph.GetEdgeRef(EID);
			double Len = Graph.GetEdgeSegment(EID).Length();
			Adj.FindOrAdd(Edge.A).Add({ Edge.B, Len });
			Adj.FindOrAdd(Edge.B).Add({ Edge.A, Len });
		}
	}

	if (!Adj.Contains(LineInfo.VID_A))
	{
		return false;
	}

	if (!Adj.Contains(LineInfo.VID_B))
	{
		return false;
	}

	if (LineInfo.IsLoop())
	{
		auto& NodeA = Adj[LineInfo.VID_A];
		for (auto& Edge: NodeA)
		{
			auto AdjCpy = Adj;
			AdjCpy[LineInfo.VID_A].RemoveAll([&Edge](const TPair<int, double>& It)
			{
				return It.Key == Edge.Key;
			});
			AdjCpy[Edge.Key].RemoveAll([&LineInfo](const TPair<int, double>& It)
			{
				return It.Key == LineInfo.VID_A;
			});
			VIDs = Dijkstra(AdjCpy, LineInfo.VID_A, Edge.Key);
			if (VIDs.Num() > 0)
			{
				VIDs.Add(LineInfo.VID_A);
				return true;
			}
		}
	}
	else
	{
		VIDs = Dijkstra(Adj, LineInfo.VID_A, LineInfo.VID_B);
	}

	return VIDs.Num() > 0;
}

// If the polygon is not looped, then it simply returns Info.Pos.SOffset,
// otherwise it is necessary to determine the SOffset on the seam (0 or SplineLength)
static double GetSOffset(const FTriInfo& TriInfo, int TID, int VID)
{
	auto& VertexInfo = TriInfo.GetVertexInfo(VID);

	if (int32(VertexInfo.Flags & ERoadVertexInfoFlags::LoopSeam))
	{
		auto* Poly = static_cast<const FProceduralPolygon_RoadLane*>(VertexInfo.Poly);

		double SplineLength = Poly->GetRoadSpline().SplineCurves.GetSplineLength();

		check(VertexInfo.Poly);
		auto& BasOpData = VertexInfo.Poly->Owner;

		const FVector ForwardVector = VertexInfo.Pos.Quat.GetForwardVector();
		auto& T = BasOpData.Triangles[TID];

		const FVector& V_A = BasOpData.Vertices3d[T.A].Vertex;
		const FVector& V_B = BasOpData.Vertices3d[T.B].Vertex;
		const FVector& V_C = BasOpData.Vertices3d[T.C].Vertex;

		bool bIsSeam_A = bool(TriInfo.A->Flags & ERoadVertexInfoFlags::LoopSeam);
		bool bIsSeam_B = bool(TriInfo.B->Flags & ERoadVertexInfoFlags::LoopSeam);
		bool bIsSeam_C = bool(TriInfo.C->Flags & ERoadVertexInfoFlags::LoopSeam);

		if (bIsSeam_A && bIsSeam_B && bIsSeam_C)
		{
			ensure(false);
			return VertexInfo.Pos.SOffset;
		}

		TOptional<FVector> V1;
		TOptional<FVector> V2;

		if (VertexInfo.VID == T.A)
		{
			if (!bIsSeam_B)
			{
				V1 = (V_B - V_A).GetSafeNormal();
			}
			if (!bIsSeam_C)
			{
				V2 = (V_C - V_A).GetSafeNormal();
			}
		}
		else if (VertexInfo.VID == T.B)
		{
			if (!bIsSeam_A)
			{
				V1 = (V_A - V_B).GetSafeNormal();
			}
			if (!bIsSeam_C)
			{
				V2 = (V_C - V_B).GetSafeNormal();
			}
		}
		else if (VertexInfo.VID == T.C)
		{
			if (!bIsSeam_A)
			{
				V1 = (V_A - V_C).GetSafeNormal();
			}
			if (!bIsSeam_B)
			{
				V2 = (V_B - V_C).GetSafeNormal();
			}
		}
		else
		{
			ensure(false);
			return VertexInfo.Pos.SOffset;
		}

		if (V1.IsSet() && V2.IsSet())
		{
			if (ForwardVector.Dot(*V1) < 0 && ForwardVector.Dot(*V2) < 0)
			{
				return SplineLength;
			}
			else
			{
				return VertexInfo.Pos.SOffset;
			}
		}
		else if (V1.IsSet())
		{
			if (ForwardVector.Dot(*V1) < 0)
			{
				return SplineLength;
			}
			else
			{
				return VertexInfo.Pos.SOffset;
			}
		}
		else if (V2.IsSet())
		{
			if (ForwardVector.Dot(*V2) < 0)
			{
				return SplineLength;
			}
			else
			{
				return VertexInfo.Pos.SOffset;
			}
		}
		else
		{
			return VertexInfo.Pos.SOffset;
		}
	}
	else
	{
		return VertexInfo.Pos.SOffset;
	}
}

static TArray<FVector2D> GetPolyline2D(const TArray<FRoadPosition>& InPoints)
{
	if (InPoints.Num() == 0)
	{
		return {};
	}

	TArray<FVector2D> Points2D;
	Points2D.Reserve(InPoints.Num());
	for (auto& It : InPoints)
	{
		Points2D.Add(FVector2D{ It.Location });
	}
	GeomUtils::RemovedPolylineSelfIntersection(Points2D);
	return Points2D;
}

// @param adj - Assuming graph representation as adjacency list {neighbor, weight}. See https://en.wikipedia.org/wiki/Adjacency_list
/*
static std::vector<int> Dijkstra(const std::map<int, std::vector<std::pair<int, double>>>& adj, int source, int destination)
{
	std::map<int, double> dist;
	std::map<int, int> parent; // Store parent for path reconstruction

	for (auto& it : adj)
	{
		dist[it.first] = std::numeric_limits<double>::max();
		parent[it.first] = -1;
	}

	std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, std::greater<std::pair<double, int>>> pq;

	dist[source] = 0.0;
	pq.push({ 0.0, source });

	while (!pq.empty())
	{
		double d = pq.top().first;
		int u = pq.top().second;
		pq.pop();

		if (d > dist[u]) continue; // Already found a shorter path

		for (const auto& edge : adj.at(u))
		{
			int v = edge.first;
			double weight = edge.second;

			if (dist[u] + weight < dist[v])
			{
				dist[v] = dist[u] + weight;
				parent[v] = u; // Update parent
				pq.push({ dist[v], v });
			}
		}
	}

	// Reconstruct the path
	std::vector<int> path;
	int currentNode = destination;
	while (currentNode != -1 && currentNode != source) // Stop if no path or reached source
	{
		path.insert(path.begin(), currentNode); // Add to front for correct order
		currentNode = parent[currentNode];
	}
	if (currentNode == source)  // If source was reached
	{
		path.insert(path.begin(), source);
	}
	else
	{
		// No path found (destination unreachable from source)
		return {}; // Return empty path
	}

	return path;
}

static bool FindPolyline(const MetaRoad::FDynamicGraph2d& Graph, const FLineInfo& LineInfo, TArray<int>& VIDs)
{
	if (LineInfo.VID_B == -1 && Graph.IsVertex(LineInfo.VID_A))
	{
		VIDs.Add(LineInfo.VID_A);
		return true;
	}

	std::map<int, std::vector<std::pair<int, double>>> Adj;
	for (int EID : Graph.EdgeIndices())
	{
		if (Graph.HasPolylineID(EID, LineInfo.PID))
		{
			const auto& Edge = Graph.GetEdgeRef(EID);
			double Len = Graph.GetEdgeSegment(EID).Length();
			Adj[Edge.A].push_back({ Edge.B, Len });
			Adj[Edge.B].push_back({ Edge.A, Len });
		}
	}

	if (!Adj.contains(LineInfo.VID_A))
	{
		return false;
	}

	if (!Adj.contains(LineInfo.VID_B))
	{
		return false;
	}

	std::vector<int> Path;
	if (LineInfo.IsLoop())
	{
		auto& NodeA = Adj[LineInfo.VID_A];
		for (auto& Edge: NodeA)
		{
			auto AdjCpy = Adj;
			std::erase_if(AdjCpy[LineInfo.VID_A], [&Edge](const std::pair<int, double>& It)
			{
				return It.first == Edge.first;
			});
			std::erase_if(AdjCpy[Edge.first],[&LineInfo](const std::pair<int, double>& It)
			{
				return It.first == LineInfo.VID_A;
			});
			Path = Dijkstra(AdjCpy, LineInfo.VID_A, Edge.first);
			if (Path.size() > 0)
			{
				break;
			}
		}
	}
	else
	{
		Path = Dijkstra(Adj, LineInfo.VID_A, LineInfo.VID_B);
	}

	if (Path.size())
	{
		VIDs = TArray<int>(&Path[0], Path.size());
		return true;
	}
	else
	{
		return false;
	}
}
*/


/*
static bool FindPolyline_2(const MetaRoad::FDynamicGraph2d& Graph, const FLineInfo& LineInfo, TArray<int>& VIDs)
{
	if (LineInfo.VID_B == -1 && Graph.IsVertex(LineInfo.VID_A))
	{
		VIDs.Add(LineInfo.VID_A);
		return true;
	}

	if (LineInfo.IsLoop())
	{
		return false; // Not supported
	}

	TMap<int, TSet<int>> Nodes;
	for (int EID : Graph.EdgeIndices())
	{
		if (Graph.HasPolylineID(EID, LineInfo.PID))
		{
			const auto& Edge = Graph.GetEdgeRef(EID);
			Nodes.FindOrAdd(Edge.A).Add(Edge.B);
			Nodes.FindOrAdd(Edge.B).Add(Edge.A);
		}
	}
		
	if (!Nodes.Contains(LineInfo.VID_A))
	{
		return false;
	}

	if (!Nodes.Contains(LineInfo.VID_B))
	{
		return false;
	}

	std::queue<TArray<int>> Queue;
	TMap<int, bool> Visited;
	for (auto& It : Nodes)
	{
		Visited.Add(It.Key, false);
	}

	Visited[LineInfo.VID_A] = true;
	Queue.push({ LineInfo.VID_A });
	VIDs.Empty();


	while (!Queue.empty())
	{
		TArray<int>& CurrPath = Queue.front();

		if (CurrPath.Last() == LineInfo.VID_B)
		{
			VIDs = CurrPath;
			return true;
		}

		auto& Node = Nodes[CurrPath.Last()];
		bool bWasPushed = false;
		if (Node.Num() == 1)
		{
			int VID = *Node.begin();
			if (!Visited[VID])
			{
				Visited[VID] = true;
				CurrPath.Add(VID);
				bWasPushed = true;
			}
		}
		else if(Node.Num() > 1)
		{
			for (int VID : Node)
			{
				if (!Visited[VID])
				{
					TArray<int> NewPath = CurrPath;
					NewPath.Add(VID);
					Visited[VID] = true;
					Queue.push(NewPath);
					bWasPushed = true;
				}
			}
		}
		if(!bWasPushed)
		{
			Queue.pop();
		}
	}

	return false;
}
*/

/*
static FRoadPosition FindNearestAtKay(const URoadSplineComponent* Spline, double Key, const FVector& TargetWorldLocation)
{
	const FTransform KeyTransform = Spline->GetTransformAtSplineInputKey(Key, ESplineCoordinateSpace::World);
	const FVector TargetLocalLocation = KeyTransform.InverseTransformPositionNoScale(TargetWorldLocation);

	FRoadPosition Ret;
	Ret.SOffset = Spline->GetDistanceAlongSplineAtSplineInputKey(Key);
	Ret.ROffset = TargetLocalLocation.Y;
	Ret.Quat = KeyTransform.GetRotation();
	Ret.Location = KeyTransform.TransformPosition(FVector(0, TargetLocalLocation.Y, 0.0));
	return Ret;
}

static FRoadPosition FindNearestAtDistance(const URoadSplineComponent* Spline, double SOffest, const FVector& TargetWorldLocation)
{
	const FTransform KeyTransform = Spline->GetTransformAtDistanceAlongSpline(SOffest, ESplineCoordinateSpace::World);
	const FVector TargetLocalLocation = KeyTransform.InverseTransformPositionNoScale(TargetWorldLocation);

	FRoadPosition Ret;
	Ret.SOffset = SOffest;
	Ret.ROffset = TargetLocalLocation.Y;
	Ret.Quat = KeyTransform.GetRotation();
	Ret.Location = KeyTransform.TransformPosition(FVector(0, TargetLocalLocation.Y, 0.0));
	return Ret;
}
*/


// ---------------------------------------------------------------------------------------------------------------------------------
FTriInfo FProceduralPolygon::FindTri(int TID) const
{
	auto& T = Owner.Triangles[TID];
	auto* InfoA = Owner.Vertices3d[T.A].Infos.Find(this);
	auto* InfoB = Owner.Vertices3d[T.B].Infos.Find(this);
	auto* InfoC = Owner.Vertices3d[T.C].Infos.Find(this);
	return { InfoA , InfoB, InfoC };
}

FLineInfo FProceduralPolygon::AddToArrangement(const TArray<FVector2D>& Points, int GID)
{
	if (Points.Num() <= 1)
	{
		return FLineInfo{};
	}

	FLineInfo Info;
	Info.PID = Owner.Arrangement->Graph.AllocateEdgePolylines();
	for (int i = 0; i < Points.Num() - 1; ++i)
	{
		Owner.Arrangement->Insert(Points[i], Points[i + 1], GID, Info.PID);
	}

	Info.VID_A = Owner.Arrangement->FindExistingVertex(Points[0]);
	Info.VID_B = Owner.Arrangement->FindExistingVertex(Points.Last());

	if (Info.VID_A == Info.VID_B && Points.Num() <= 2)
	{
		Info.VID_B = -1;
	}

	return Info;
};
// ---------------------------------------------------------------------------------------------------------------------------------

const URoadSplineEditorComponent& FProceduralPolygon_RoadBase::GetRoadSpline() const
{
	return *Owner.Splines[SplineIndex];
}

URoadSplineEditorComponent& FProceduralPolygon_RoadBase::GetRoadSpline()
{
	return *Owner.Splines[SplineIndex];
}

const UE::Geometry::FAxisAlignedBox2d& FProceduralPolygon_RoadBase::GetSplineBounds() const
{
	return Owner.Splines[SplineIndex]->GetSplineBounds();
}

UE::Geometry::FAxisAlignedBox2d& FProceduralPolygon_RoadBase::GetSplineBounds()
{
	return Owner.Splines[SplineIndex]->GetSplineBounds();
}

double FProceduralPolygon_RoadBase::GetMaterialPriority() const
{
	int ProfilePriority = 0;

	if (auto* RoadZone = GetRoadZone().GetPtr<FRoadZone>())
	{
		if (const FRoadZoneTypeDetails* Found = GetDefault<UMetaRoadSettings>()->RoadZoneTypes.Find(RoadZone->ZoneType.GetName()))
		{
			ProfilePriority = Found->MaterialPriority;
			if (RoadZone->bOverridePriority)
			{
				ProfilePriority = RoadZone->OverridePriority;
			}
		}
	}

	return ProfilePriority + double(GetRoadSpline().MaterialPriority) / 1000.0 + double(Owner.Splines.Num() - GetObjectIndex() - 1) / 1000000.0;
}



// ---------------------------------------------------------------------------------------------------------------------------------

FProceduralPolygon_RoadLane::FProceduralPolygon_RoadLane(FRoadTriangulationData& Owner, int SplineIndex, int SectionIndex, int LaneIndex, double MaxSquareDistanceFromSpline, double MaxSquareDistanceFromCap, double MinSegmentLength)
	: FProceduralPolygon_RoadBase(Owner, SplineIndex)
	, SectionIndex(SectionIndex)
	, LaneIndex(LaneIndex)
	, MaxSquareDistanceFromSpline(MaxSquareDistanceFromSpline)
	, MinSegmentLength(MinSegmentLength)
	, bIsLoop(false)
{

	ResultInfo = { EGeometryResultType::InProgress };

	const auto& RoadSplineCache = GetRoadSpline();
	const auto& Section = GetSection();
	const FRoadZoneSidewalk* AsSidewalk = GetRoadZone().GetPtr<FRoadZoneSidewalk>();


	TArray<FVector2D> InsideLineVertices2D = GetPolyline2D(GetPolyline(ERoadLaneSide::Inner));
	TArray<FVector2D> OutsideLineVertices2D;
	TArray<FVector2D> EndCapVertices2D;
	TArray<FVector2D> BeginCapVertices2D;

	if (LaneIndex != MetaRoad::ZeroLaneIndex)
	{
		OutsideLineVertices2D = GetPolyline2D(GetPolyline(ERoadLaneSide::Outer));
	}

	if (AsSidewalk && LaneIndex != MetaRoad::ZeroLaneIndex)
	{
		const FRoadLane& Lane = GetLane();

		if (AsSidewalk->bEndCurb && AsSidewalk->EndCapCurve.GetRichCurveConst()->GetNumKeys())
		{
			double LaneWidth = Lane.Width.Eval(Lane.GetEndOffset());

			TArray<float> Values;
			TArray<float> Times;
			CurveUtils::CurveToPolyline(*AsSidewalk->EndCapCurve.GetRichCurveConst(), 0.0, 1.0, MaxSquareDistanceFromCap / (LaneWidth * LaneWidth), 0.0001f, 200, Values, Times);
			double MaxValue = *Algo::MaxElement(Values);

			for (int i = 0; i < Values.Num(); ++i)
			{
				EndCapVertices2D.Add(FVector2D{ RoadSplineCache.GetRoadPosition(SectionIndex, LaneIndex, Times[i], Lane.GetEndOffset() - (MaxValue - Values[i]) * LaneWidth, ESplineCoordinateSpace::World).Location});
			}
			if (MaxValue > 0)
			{
				RemovePointsFromEnd(InsideLineVertices2D, MaxValue * LaneWidth);
				RemovePointsFromEnd(OutsideLineVertices2D, MaxValue * LaneWidth);

				InsideLineVertices2D.Add(EndCapVertices2D[0]);
				OutsideLineVertices2D.Add(EndCapVertices2D.Last());
			}
		}
		if (AsSidewalk->bBeginCurb && AsSidewalk->BeginCapCurve.GetRichCurveConst()->GetNumKeys())
		{
			double LaneWidth = Lane.Width.Eval(Section.SOffset);
	
			TArray<float> Values;
			TArray<float> Times;
			CurveUtils::CurveToPolyline(*AsSidewalk->BeginCapCurve.GetRichCurveConst(), 0.0, 1.0, MaxSquareDistanceFromCap / (LaneWidth * LaneWidth), 0.0001f, 200, Values, Times);
			double MaxValue = *Algo::MaxElement(Values);

			for (int i = 0; i < Values.Num(); ++i)
			{
				BeginCapVertices2D.Add(FVector2D{ RoadSplineCache.GetRoadPosition(SectionIndex, LaneIndex, Times[i], Section.SOffset + (MaxValue - Values[i]) * LaneWidth, ESplineCoordinateSpace::World).Location });
			}
			if (MaxValue > 0)
			{
				RemovePointsFromBegin(InsideLineVertices2D, MaxValue * LaneWidth);
				RemovePointsFromBegin(OutsideLineVertices2D, MaxValue * LaneWidth);

				InsideLineVertices2D.Insert(BeginCapVertices2D[0], 0);
				OutsideLineVertices2D.Insert(BeginCapVertices2D.Last(), 0);
			}
		}
	}

	if (LaneIndex != MetaRoad::ZeroLaneIndex)
	{
		if (BeginCapVertices2D.Num() < 2)
		{
			BeginCapVertices2D.Empty();
			BeginCapVertices2D.Add(InsideLineVertices2D[0]);
			BeginCapVertices2D.Add(OutsideLineVertices2D[0]);
		}

		if (EndCapVertices2D.Num() < 2)
		{
			EndCapVertices2D.Empty();
			EndCapVertices2D.Add(InsideLineVertices2D.Last());
			EndCapVertices2D.Add(OutsideLineVertices2D.Last());
		}
	}

	int GID = 0; 

	if (LaneIndex == 0)
	{
		GID = GUIFlags::CenterLine;
	}
	else if (GetRoadZone().GetPtr<FRoadZoneDriving>() != nullptr)
	{
		GID = GUIFlags::DrivingSurface;
	}
	else if (AsSidewalk)
	{
		if (AsSidewalk->bIsSoftBorder)
		{
			GID = GUIFlags::SidewalksSoft;
		}
		else
		{
			GID = GUIFlags::SidewalksHard;
		}
	}

	InsideLineInfo = AddToArrangement(InsideLineVertices2D, GID);
	if (!InsideLineInfo.IsValid())
	{
		ResultInfo.SetFailed(FText::Format(LOCTEXT("FProceduralPolygon_RoadLane_InsideLineFaild", "FProceduralPolygon_RoadLane: {0}: InsideLineInfo faild"), GetDescription()));
		return;
	}

	bIsLoop = InsideLineInfo.IsLoop();

	if (LaneIndex != MetaRoad::ZeroLaneIndex)
	{
		OutsideLineInfo = AddToArrangement(OutsideLineVertices2D, GID);
		if (!OutsideLineInfo.IsValid())
		{
			ResultInfo.SetFailed(FText::Format(LOCTEXT("FProceduralPolygon_RoadLane_OutsideLineFaild", "FProceduralPolygon_RoadLane: {0}: OutsideLineInfo faild "), GetDescription()));
			return;
		}

		if (InsideLineInfo.IsLoop() ^ OutsideLineInfo.IsLoop())
		{
			// There should not be situations when only one of the lines is a loop.
			ResultInfo.SetFailed(FText::Format(LOCTEXT("FProceduralPolygon_RoadLane_LoopFaild", "FProceduralPolygon_RoadLane: {0}: Wrong loop"), GetDescription()));
			return;
		}

		EndCapInfo = AddToArrangement(EndCapVertices2D, GID);
		BeginCapInfo = AddToArrangement(BeginCapVertices2D, GID);

		if (!BeginCapInfo.IsValid())
		{
			ResultInfo.SetFailed(FText::Format(LOCTEXT("FProceduralPolygon_RoadLane_BeginCapInfo", "FProceduralPolygon_RoadLane: {0}: BeginCapInfo line info faild "), GetDescription()));
			return;
		}

		if (!EndCapInfo.IsValid())
		{
			ResultInfo.SetFailed(FText::Format(LOCTEXT("FProceduralPolygon_RoadLane_EndCapInfo", "FProceduralPolygon_RoadLane: {0}: EndCapInfo line info faild "), GetDescription()));
			return;
		}

		for (const auto& [Desc, Attribute] : GetLane().Attributes)
		{
			if (Attribute.IsChildOf<FRoadLaneAttributePolygoneCustomizationValue>())
			{
				for (auto& Key : Attribute.Keys)
				{
					if (auto* Value = Key.GetValuePtr< FRoadLaneAttributePolygoneCustomizationValue>())
					{
						Value->OnPolygoneCreated(*this, Key.SOffset);
					}
				}
			}
		}
	}

#if METAROAD_PRO
	for (const auto& [Desc, Attribute] : GetLaneAttributes())
	{
		if (Attribute.IsChildOf<FRoadLaneAttributePolygonValue>())
		{
			for (auto& Key : Attribute.Keys)
			{
				if (auto* Value = Key.GetValuePtr< FRoadLaneAttributePolygonValue>())
				{
					const FRoadPosition RoadPose = GetRoadSpline().EvalAttributeAnchorPosition(SectionIndex, LaneIndex, GetStartOffset() + Key.SOffset, *Value, ESplineCoordinateSpace::World);
					const bool bIsForward = (LaneIndex == MetaRoad::ZeroLaneIndex) || GetRoadSpline().GetRoadLayout().Sections[SectionIndex].GetLaneByIndex(LaneIndex).IsForwardLane();
					const FTransform Transform(RoadPose.Quat * FRotator(0.0, bIsForward ? -90.0 : +90.0, 0.0).Quaternion(), RoadPose.Location, FVector(-1.0, 1.0, 1.0));
					Owner.SimplePolygones.Append(Value->BuildPolygones(Transform));
				}
			}
		}
	}
#endif
	
	ResultInfo.SetSuccess();
}


const FRoadLaneSection& FProceduralPolygon_RoadLane::GetSection() const
{
	return GetRoadSpline().RoadLayout.Sections[SectionIndex];
}

const FRoadLane& FProceduralPolygon_RoadLane::GetLane() const
{
	check(LaneIndex != MetaRoad::ZeroLaneIndex);
	if (LaneIndex > 0)
	{
		return GetSection().Right[LaneIndex - 1];
	}
	else
	{
		return GetSection().Left[-LaneIndex - 1];
	}
}

const TMap<TSoftClassPtr<URoadLaneAttributeDescriptor>, FRoadLaneAttribute>& FProceduralPolygon_RoadLane::GetLaneAttributes() const
{
	if (LaneIndex != MetaRoad::ZeroLaneIndex)
	{
		return GetLane().Attributes;
	}
	else
	{
		return GetSection().Attributes;
	}
}

double FProceduralPolygon_RoadLane::GetStartOffset() const
{
	if (LaneIndex != MetaRoad::ZeroLaneIndex)
	{
		return GetLane().GetStartOffset();
	}
	else
	{
		return GetSection().SOffset;
	}
}

double FProceduralPolygon_RoadLane::GetEndOffset() const
{
	if (LaneIndex != MetaRoad::ZeroLaneIndex)
	{
		return GetLane().GetEndOffset();
	}
	else
	{
		return GetSection().SOffsetEnd_Cashed;
	}
}

bool FProceduralPolygon_RoadLane::OnCompleteArrangement()
{
	if (ResultInfo.HasFailed())
	{
		return false;
	}

	if (!ProcessPolyline(InsideLineInfo, InsideLineVertices, ERoadVertexInfoFlags::Inside))
	{
		ResultInfo.SetFailed(FText::Format(LOCTEXT("FProceduralPolygon_RoadLane_InsideLineNotFound", "FProceduralPolygon_RoadLane: {0}: Inside line not found after arrangement"), GetDescription()));
		return false;
	}

	if (LaneIndex != MetaRoad::ZeroLaneIndex)
	{
		if (!ProcessPolyline(OutsideLineInfo, OutsideLineVertices, ERoadVertexInfoFlags::Outside))
		{
			ResultInfo.SetFailed(FText::Format(LOCTEXT("FProceduralPolygon_RoadLane_OutsideLineNotFound", "FProceduralPolygon_RoadLane: {0}: Outside line not found after arrangement"), GetDescription()));
			return false;
		}

		if (!bIsLoop)
		{
			if (!ProcessPolyline(BeginCapInfo, BeginCapVertices, ERoadVertexInfoFlags::BeginCap))
			{
				ResultInfo.SetFailed(FText::Format(LOCTEXT("FProceduralPolygon_RoadLane_BeginCapeNotFound", "FProceduralPolygon_RoadLane: {0}: BeginCap line not found after arrangement"), GetDescription()));
				return false;
			}

			if (!ProcessPolyline(EndCapInfo, EndCapVertices, ERoadVertexInfoFlags::EndCap))
			{
				ResultInfo.SetFailed(FText::Format(LOCTEXT("FProceduralPolygon_RoadLane_EndCapLineNotFound", "FProceduralPolygon_RoadLane: {0}: EndCap line not found after arrangement"), GetDescription()));
				return false;
			}
		}
		else
		{
			if (!ProcessPolyline(BeginCapInfo, BeginCapVertices, ERoadVertexInfoFlags::LoopSeam))
			{
				ResultInfo.SetFailed(FText::Format(LOCTEXT("FProceduralPolygon_RoadLane_LoopSeamNotFound", "FProceduralPolygon_RoadLoop: {0}: LoopSeam line not found after arrangement"), GetDescription()));
				return false;
			}
		}
	}

	if (LaneIndex == MetaRoad::ZeroLaneIndex)
	{
		for (int i = 0; i < InsideLineVertices.Num() - 1; ++i)
		{
			Boundary.Add({ InsideLineVertices[i], InsideLineVertices[i + 1] });
		}
	}
	else if (bIsLoop)
	{
		FAxisAlignedBox2d InsideBound;
		for (int i = 0; i < InsideLineVertices.Num() - 1; ++i)
		{
			const auto& It = Owner.Vertices3d[InsideLineVertices[i]].Infos[this];
			InsideBound.Contain(FVector2D{ It.Pos.Location });
		}
		FAxisAlignedBox2d OutsideBound;
		for (int i = 0; i < OutsideLineVertices.Num() - 1; ++i)
		{
			const auto& It = Owner.Vertices3d[OutsideLineVertices[i]].Infos[this];
			OutsideBound.Contain(FVector2D{ It.Pos.Location });
		}

		auto AddBoundaries = [this](const TArray<int> & InBoundary, const TArray<int>& InHole)
		{
			for (int i = 0; i < InBoundary.Num() - 1; ++i)
			{
				Boundary.Add({ InBoundary[i], InBoundary[i + 1] });
			}
			Holes.Add({});
			for (int i = 0; i < InHole.Num() - 1; ++i)
			{
				Holes[0].Add({ InHole[i], InHole[i + 1]});
			}
			if (InBoundary.Num())
			{
				LocalSplineBounds.Contain(FVector2D{
					GetRoadSpline().SplineCurves.GetSplineLength(),
					Owner.Vertices3d[InBoundary[0]].Infos[this].Pos.ROffset
				});
			}
		};

		if (InsideBound.Area() > OutsideBound.Area())
		{
			AddBoundaries(InsideLineVertices, OutsideLineVertices);
		}
		else 
		{
			AddBoundaries(OutsideLineVertices, InsideLineVertices);
		}
	}
	else
	{
		for (int i = 0; i < InsideLineVertices.Num() - 1; ++i)
		{
			Boundary.Add({ InsideLineVertices[i], InsideLineVertices[i + 1] });
		}
		for (int i = 0; i < EndCapVertices.Num() - 1; ++i)
		{
			Boundary.Add({ EndCapVertices[i], EndCapVertices[i + 1] });
		}
		for (int i = OutsideLineVertices.Num() - 1; i >= 1; --i)
		{
			Boundary.Add({ OutsideLineVertices[i], OutsideLineVertices[i - 1] });
		}
		for (int i = BeginCapVertices.Num() - 1; i >= 1; --i)
		{
			Boundary.Add({ BeginCapVertices[i], BeginCapVertices[i - 1] });
		}
	}

	auto MakePoly = [this](TArray<FIndex2i>& InBoundary)
	{
		TArray<FVector2d> Vertex2d;
		{
			const auto& FirstInfo = Owner.Vertices3d[InBoundary[0].A].Infos[this];
			Vertex2d.Add(FVector2D{ FirstInfo.Pos.Location });
			LocalSplineBounds.Contain(FVector2D{ FirstInfo.Pos.SOffset, FirstInfo.Pos.ROffset });
		}
		for (const auto& It : InBoundary)
		{
			const auto& Info = Owner.Vertices3d[It.B].Infos[this];
			Vertex2d.Add(FVector2D{ Info.Pos.Location });
			LocalSplineBounds.Contain(FVector2D{ Info.Pos.SOffset, Info.Pos.ROffset });
		}
		auto Poly = UE::Geometry::FPolygon2d(Vertex2d);

		if (!Poly.IsClockwise())
		{
			Algo::Reverse(InBoundary);
			for (auto& It : InBoundary)
			{
				It = FIndex2i{ It.B, It.A };
			}
		}

		return Poly;
	};


	Poly2d = MakePoly(Boundary);

	if (Holes.Num())
	{
		MakePoly(Holes[0]);
	}

	Bounds = FAxisAlignedBox2d(Poly2d.GetVertices());
	GetSplineBounds().Contain(LocalSplineBounds);

	return true;
}

bool FProceduralPolygon_RoadLane::ProcessPolyline(const FLineInfo& LineInfo, TArray<int>& VIDs, ERoadVertexInfoFlags Flags)
{
	if (!FindPolyline(Owner.Arrangement->Graph, LineInfo, VIDs))
	{
		return false;
	}
	
	for (int i = 0; i < VIDs.Num(); ++i)
	{
		AddVertexInfo(VIDs[i], Flags, false);
	}

	return true;
};

void FProceduralPolygon_RoadLane::AddVertexInfo(int VID, ERoadVertexInfoFlags Flags) const
{
	AddVertexInfo(VID, Flags, true);
}

void FProceduralPolygon_RoadLane::AddVertexInfo(int VID, ERoadVertexInfoFlags Flags, bool bWithUV1) const
{
	check(Owner.Arrangement->Graph.IsVertex(VID));

	const auto& SplineBounds = GetSplineBounds();

	if (auto* FoundVertex = Owner.Vertices3d[VID].Infos.Find(this))
	{
		// If found just update and return
		FoundVertex->Alpha1 = bWithUV1 ? (FoundVertex->Pos.ROffset - SplineBounds.Max.Y) / (SplineBounds.Max.Y - SplineBounds.Min.Y) : 0.0; // Alpha1 = [0..1]
		FoundVertex->Flags = FoundVertex->Flags | Flags;
		return;
	}

	const auto& RoadSplineCache = GetRoadSpline();

	const auto V2d = Owner.Arrangement->Graph.GetVertex(VID);
	const auto Pos = Owner.Splines[GetSplineIndex()]->UpRayIntersection(V2d);
	const double ROffset1 = GetSection().EvalLaneROffset( LaneIndex, Pos.SOffset, 0.0);
	const double ROffset2 = GetSection().EvalLaneROffset( LaneIndex, Pos.SOffset, 1.0);

	auto& VertexInfo = Owner.Vertices3d[VID].Infos.Add(this);
	VertexInfo.Poly = this;
	VertexInfo.Pos = Pos;
	VertexInfo.Pos.Location.X = V2d.X;
	VertexInfo.Pos.Location.Y = V2d.Y;
	VertexInfo.Alpha0 = (Pos.ROffset - ROffset1) / (ROffset2 - ROffset1);  // Alpha0 = [0..1]
	VertexInfo.Alpha1 = bWithUV1 ? (VertexInfo.Pos.ROffset - SplineBounds.Max.Y) / (SplineBounds.Max.Y - SplineBounds.Min.Y) : 0.0; // Alpha1 = [0..1]
	VertexInfo.Alpha2 = (Pos.ROffset - ROffset1); // Alpha0 = [0..width]
	VertexInfo.VID = VID;
	VertexInfo.Flags = Flags;
	VertexInfo.HeightOffset = 0;

	if (const FRoadZoneDriving* LaneDriving = GetRoadZone().GetPtr<FRoadZoneDriving>())
	{
		if (LaneDriving && LaneDriving->bInvertUV0)
		{
			VertexInfo.Alpha0 = 1.0 - VertexInfo.Alpha0;
		}
	}
	else if (const FRoadZoneSidewalk* LaneSidewalk = GetRoadZone().GetPtr<FRoadZoneSidewalk>())
	{
		VertexInfo.HeightOffset = LaneSidewalk->DefaultHeight;

		const FRoadLaneAttributeSidewalkHeightValue DefHeightValue(VertexInfo.HeightOffset);

		const auto& Lane = GetLane();

		for (const auto& [Desc, Attribute] : Lane.Attributes)
		{
			if (Attribute.IsChildOf<FRoadLaneAttributeSidewalkHeightValue>())
			{
				VertexInfo.HeightOffset = Attribute.Evaluate(VertexInfo.Pos.SOffset - GetStartOffset(), DefHeightValue).Height;
			}
		}

		for (const auto& [Desc, Attribute] : Lane.Attributes)
		{
			if (Attribute.IsChildOf<FRoadLaneAttributePolygoneCustomizationValue>())
			{
				for (auto& Key : Attribute.Keys)
				{
					if (auto* Value = Key.GetValuePtr< FRoadLaneAttributePolygoneCustomizationValue>())
					{
						VertexInfo.HeightOffset = Value->GetHeightOffset(*this, Key.SOffset, Pos.SOffset, VertexInfo.Alpha0, VertexInfo.HeightOffset);
					}
				}
			}
		}
	}
}

const TInstancedStruct<FRoadZone>& FProceduralPolygon_RoadLane::GetRoadZone() const
{ 
	if (LaneIndex != MetaRoad::ZeroLaneIndex)
	{
		return GetLane().RoadZone;
	}
	else
	{
		static const TInstancedStruct<FRoadZone> Dummy;
		return Dummy;
	}
}

FText FProceduralPolygon_RoadLane::GetDescription() const
{
	return FText::Format(LOCTEXT("FProceduralPolygon_RoadLane_GetDescription", "RoadLanePoly (ComponentName: \"{0}\", SectionIndex: {1}, LaneIndex: {2})"), 
		FText::FromString(GetRoadSpline().GetOriginSplineName()),
		FText::AsNumber(SectionIndex), 
		FText::AsNumber(LaneIndex)
	);
}

bool FProceduralPolygon_RoadLane::SetUVLayers(FDynamicMesh3& Mesh, int TID, double UV0ScaleFactor, double UV1ScaleFactor, double UV2ScaleFactor, int UVMaxSize) const
{
	check(Mesh.Attributes()->NumUVLayers() >= 2);

	double SplineLength = GetSplineBounds().Max.X - GetSplineBounds().Min.X;
	double UV0ScaleFactorAligned = double(FMath::RoundToInt(SplineLength * UV0ScaleFactor)) / SplineLength;
	double UV1ScaleFactorAligned = double(FMath::RoundToInt(SplineLength * UV1ScaleFactor)) / SplineLength;
	double UV2ScaleFactorAligned = double(FMath::RoundToInt(SplineLength * UV2ScaleFactor)) / SplineLength;

	const double Alpha1Mul = GetSplineBounds().Extents().Y * UV1ScaleFactorAligned * 2.0;

	auto TriInfo = FindTri(TID);

	if (ensure(TriInfo.IsValid()))
	{
		const double SOffsetA = GetSOffset(TriInfo, TID, TriInfo.A->VID);
		const double SOffsetB = GetSOffset(TriInfo, TID, TriInfo.B->VID);
		const double SOffsetC = GetSOffset(TriInfo, TID, TriInfo.C->VID);

		if (auto* UVLayer0 = Mesh.Attributes()->GetUVLayer(0))
		{
			int MinV = (int)(FMath::Min3(SOffsetA, SOffsetB, SOffsetC) * UV0ScaleFactorAligned);
			MinV =  MinV - MinV % UVMaxSize;

			UVLayer0->SetTriangle(TID, FIndex3i{
				UVLayer0->AppendElement(FVector2f(TriInfo.A->Alpha0, SOffsetA * UV0ScaleFactorAligned - MinV)),
				UVLayer0->AppendElement(FVector2f(TriInfo.B->Alpha0, SOffsetB * UV0ScaleFactorAligned - MinV)),
				UVLayer0->AppendElement(FVector2f(TriInfo.C->Alpha0, SOffsetC * UV0ScaleFactorAligned - MinV))
			});
		}

		if (auto* UVLayer1 = Mesh.Attributes()->GetUVLayer(1))
		{
			int MinV = (int)(FMath::Min3(SOffsetA, SOffsetB, SOffsetC) * UV1ScaleFactorAligned);
			MinV = MinV - MinV % UVMaxSize;

			UVLayer1->SetTriangle(TID, FIndex3i{
				UVLayer1->AppendElement(FVector2f((TriInfo.A->Alpha1 - 0.5) * Alpha1Mul + 0.5, SOffsetA * UV1ScaleFactorAligned - MinV)),
				UVLayer1->AppendElement(FVector2f((TriInfo.B->Alpha1 - 0.5) * Alpha1Mul + 0.5, SOffsetB * UV1ScaleFactorAligned - MinV)),
				UVLayer1->AppendElement(FVector2f((TriInfo.C->Alpha1 - 0.5) * Alpha1Mul + 0.5, SOffsetC * UV1ScaleFactorAligned - MinV))
			});
		}

		if (auto* UVLayer2 = Mesh.Attributes()->GetUVLayer(2))
		{
			int MinV = (int)(FMath::Min3(SOffsetA, SOffsetB, SOffsetC) * UV2ScaleFactorAligned);
			MinV = MinV - MinV % UVMaxSize;

			UVLayer2->SetTriangle(TID, FIndex3i{
				UVLayer2->AppendElement(FVector2f(TriInfo.A->Alpha2 * UV2ScaleFactor, SOffsetA * UV2ScaleFactorAligned - MinV)),
				UVLayer2->AppendElement(FVector2f(TriInfo.B->Alpha2 * UV2ScaleFactor, SOffsetB * UV2ScaleFactorAligned - MinV)),
				UVLayer2->AppendElement(FVector2f(TriInfo.C->Alpha2 * UV2ScaleFactor, SOffsetC * UV2ScaleFactorAligned - MinV))
			});
		}

		return true;
	}

	return false;
}


TArray<FRoadPosition> FProceduralPolygon_RoadLane::GetPolyline(int InLaneIndex, double Alpha) const
{
	auto& RoadSplineCache = GetRoadSpline();

	if (!RoadSplineCache.RoadLayout.Sections.IsValidIndex(SectionIndex))
	{
		return {};
	}

	const auto& Section = RoadSplineCache.RoadLayout.Sections[SectionIndex];

	if (InLaneIndex != MetaRoad::ZeroLaneIndex && !Section.CheckLaneIndex(InLaneIndex))
	{
		return {};
	}

	TArray<FRoadPosition> OutPoints;

	if (InLaneIndex != MetaRoad::ZeroLaneIndex)
	{
		const auto& Lane = Section.GetLaneByIndex(InLaneIndex);


		for (int i = Lane.GetStartSectionIndex(); i <= Lane.GetEndSectionIndex(); ++i)
		{
			TArray<FRoadPosition> TmpPoints;
			if (RoadSplineCache.ConvertSplineToPolyline(
				[this, InLaneIndex, &RoadSplineCache, Alpha](double SOffset, ESplineCoordinateSpace::Type CoordinateSpace)
				{
					return RoadSplineCache.GetRoadPosition(SectionIndex, InLaneIndex, Alpha, SOffset, CoordinateSpace);
				},
				ESplineCoordinateSpace::World, MaxSquareDistanceFromSpline, MinSegmentLength,
				RoadSplineCache.RoadLayout.Sections[i].SOffset, RoadSplineCache.RoadLayout.Sections[i].SOffsetEnd_Cashed,
				{}, true, TmpPoints))
			{
				if (OutPoints.Num())
				{
					OutPoints.RemoveAt(OutPoints.Num() - 1);
				}
				OutPoints.Append(MoveTemp(TmpPoints));
			}
		}

		if (Lane.RoadZone.GetPtr<FRoadZoneSidewalk>())
		{
			for (const auto& [Desc, Attribute] : Lane.Attributes)
			{
				if (Attribute.IsChildOf<FRoadLaneAttributePolygoneCustomizationValue>())
				{
					for (auto& Key : Attribute.Keys)
					{
						if (auto* Value = Key.GetValuePtr< FRoadLaneAttributePolygoneCustomizationValue>())
						{
							TArray<double> Segmants;
							Value->GetAdditionalSplinePoints(*this, Key.SOffset, Alpha, Segmants);

							TArray<FRoadPosition> AttributePoints;
							AttributePoints.Reserve(Segmants.Num());
							for (double SOffset : Segmants)
							{
								AttributePoints.Add(RoadSplineCache.GetRoadPosition(SectionIndex, InLaneIndex, Alpha, SOffset, ESplineCoordinateSpace::World));
							}
							GeomUtils::InsertPolyline(OutPoints, AttributePoints);
						}
					}
				}
			}
		}
	}
	else
	{
		RoadSplineCache.ConvertSplineToPolyline(
			[this, &RoadSplineCache](double SOffset, ESplineCoordinateSpace::Type CoordinateSpace)
			{
				return RoadSplineCache.GetRoadPosition(SectionIndex, MetaRoad::ZeroLaneIndex, 0.0, SOffset, CoordinateSpace);
			},
			ESplineCoordinateSpace::World, MaxSquareDistanceFromSpline, MinSegmentLength,
			Section.SOffset, Section.SOffsetEnd_Cashed,
			{}, true, OutPoints);
	}

	return OutPoints;
}

TArray<FRoadPosition> FProceduralPolygon_RoadLane::GetCenterPolyline(double S0, double S1) const
{
	TArray<FRoadPosition> OutPoints;

	auto& RoadSplineCache = GetRoadSpline();
	auto& Sections = RoadSplineCache.RoadLayout.Sections;

	for (auto& Section: Sections)
	{
		OutPoints.Append(GetPolyline(0, 0.0));

		if(Section.Left.Num())
		{
			OutPoints.Append(GetPolyline(-1, 0.0));
		}

		if (Section.Right.Num())
		{
			OutPoints.Append(GetPolyline(+1, 0.0));
		}
	}

	GeomUtils::NormalizePolyline(OutPoints);
	OutPoints = GeomUtils::GetSubPolyline(OutPoints, S0, S1);

	return OutPoints;
}

TArray<FRoadPosition> FProceduralPolygon_RoadLane::GetPolyline(ERoadLaneSide Side) const
{
	auto& RoadSplineCache = GetRoadSpline();

	const auto& Section = RoadSplineCache.RoadLayout.Sections[SectionIndex];

	if (LaneIndex == MetaRoad::ZeroLaneIndex)
	{
		return GetCenterPolyline(Section.SOffset, Section.SOffsetEnd_Cashed);
	}

	const auto& Lane = Section.GetLaneByIndex(LaneIndex);
	const double Alpha = Side == ERoadLaneSide::Inner ? 0.0 : 1.0;
	const double S0 = Lane.GetStartOffset();
	const double S1 = Lane.GetEndOffset();

	TArray<FRoadPosition> OutPoints = GetPolyline(LaneIndex, Alpha);

	if (Side == ERoadLaneSide::Inner)
	{
		if (LaneIndex == 1 || LaneIndex == -1)
		{
			OutPoints.Append(GetCenterPolyline(S0, S1));
		}
		else if(LaneIndex < 0)
		{
			OutPoints.Append(GetPolyline(LaneIndex + 1, 1.0));
		}
		else if (LaneIndex > 0)
		{
			OutPoints.Append(GetPolyline(LaneIndex - 1, 1.0));
		}
	}
	else // if (Side == ERoadLaneSide::Outer)
	{
		if (LaneIndex < 0)
		{
			OutPoints.Append(GetPolyline(LaneIndex - 1, 0.0));
		}
		else if (LaneIndex > 0)
		{
			OutPoints.Append(GetPolyline(LaneIndex + 1, 0.0));
		}
	}

	GeomUtils::NormalizePolyline(OutPoints);
	OutPoints = GeomUtils::GetSubPolyline(OutPoints, S0, S1);

	return OutPoints;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------
FProceduralPolygon_RoadLoop::FProceduralPolygon_RoadLoop(FRoadTriangulationData& Owner, int SplineIndex, double MaxSquareDistanceFromSpline, double MinSegmentLength)
	:FProceduralPolygon_RoadBase(Owner, SplineIndex)
{
	ResultInfo = { EGeometryResultType::InProgress };

	auto RoadPositionFunc = [this](double SOffset, ESplineCoordinateSpace::Type CoordinateSpace)
	{
		return GetRoadSpline().GetRoadPosition(0, MetaRoad::ZeroLaneIndex, 0.0, SOffset, CoordinateSpace);
	};

	TArray<FRoadPosition> RoadPoints;
	if (!GetRoadSpline().ConvertSplineToPolyline(RoadPositionFunc, ESplineCoordinateSpace::World, MaxSquareDistanceFromSpline, MinSegmentLength, 0.0, GetRoadSpline().SplineCurves.GetSplineLength(), {}, true, RoadPoints))
	{
		ResultInfo.SetFailed(FText::Format(LOCTEXT("FProceduralPolygon_RoadLoop_ConvertSplineToPolyline", "FProceduralPolygon_RoadLoop: {0}: Can't ConvertSplineToPolyline"), GetDescription()));
		return;
	}

	TArray<FVector2D> Points2D;
	Points2D.Reserve(RoadPoints.Num());
	for (auto& It : RoadPoints)
	{
		Points2D.Add(FVector2D{ It.Location });
	}
	GeomUtils::RemovedPolylineSelfIntersection(Points2D);

	int GID = 0;

	if (GetRoadZone().GetPtr<FRoadZoneDriving>() != nullptr)
	{
		GID = GUIFlags::DrivingSurface;
	}
	else if (GetRoadZone().GetPtr<FRoadZoneSidewalk>() != nullptr)
	{
		GID = GUIFlags::SidewalksSoft;
	}

	LineInfo.PID = this->Owner.Arrangement->Graph.AllocateEdgePolylines();
	for (int i = 0; i < Points2D.Num() - 1; ++i)
	{
		this->Owner.Arrangement->Insert(Points2D[i], Points2D[i + 1], GID, LineInfo.PID);
	}

	LineInfo.VID_A = LineInfo.VID_B = this->Owner.Arrangement->FindExistingVertex(Points2D[0]);

	if (!LineInfo.IsValid())
	{
		ResultInfo.SetFailed(FText::Format(LOCTEXT("FProceduralPolygon_RoadLoop_OutsideLineFaild", "FProceduralPolygon_RoadLoop: {0}: polygon faild "), GetDescription()));
		return;
	}

	ResultInfo.SetSuccess();
}

bool FProceduralPolygon_RoadLoop::OnCompleteArrangement()
{
	if (ResultInfo.HasFailed())
	{
		return false;
	}

	if (!FindPolyline(Owner.Arrangement->Graph, LineInfo, LineVertices))
	{
		ResultInfo.SetFailed(FText::Format(LOCTEXT("FProceduralPolygon_RoadLoop_InsideLineNotFound", "FProceduralPolygon_RoadLoop: {0}: line not found after arrangement"), GetDescription()));
		return false;
	}

	for (int i = 0; i < LineVertices.Num(); ++i)
	{
		AddVertexInfo(LineVertices[i], ERoadVertexInfoFlags::LoopPoly);
	}

	for (int i = 0; i < LineVertices.Num() - 1; ++i)
	{
		Boundary.Add({ LineVertices[i], LineVertices[i + 1] });
	}

	TArray<FVector2d> Vertex2d;
	{
		const auto& FirstInfo = Owner.Vertices3d[Boundary[0].A].Infos[this];
		Vertex2d.Add(FVector2D{ FirstInfo.Pos.Location });
	}
	for (const auto& It : Boundary)
	{
		const auto& Info = Owner.Vertices3d[It.B].Infos[this];
		Vertex2d.Add(FVector2D{ Info.Pos.Location });
	}
	Poly2d = UE::Geometry::FPolygon2d(Vertex2d);

	if (!Poly2d.IsClockwise())
	{
		Algo::Reverse(LineVertices);
		Algo::Reverse(Vertex2d);
		Poly2d = UE::Geometry::FPolygon2d(Vertex2d);
		Algo::Reverse(Boundary);
		for (auto& It : Boundary)
		{
			It = FIndex2i{ It.B, It.A };
		}
	}

	return true;
}

void FProceduralPolygon_RoadLoop::AddVertexInfo(int VID, ERoadVertexInfoFlags Flags) const
{
	check(Owner.Arrangement->Graph.IsVertex(VID));

	if (auto* FoundVertex = Owner.Vertices3d[VID].Infos.Find(this))
	{
		FoundVertex->Flags = FoundVertex->Flags | Flags;
		return;
	}

	const auto& RoadSplineCache = GetRoadSpline();
	const auto V2d = Owner.Arrangement->Graph.GetVertex(VID);
	const auto Pos = Owner.Splines[GetSplineIndex()]->UpRayIntersection(V2d);

	auto& VertexInfo = Owner.Vertices3d[VID].Infos.Add(this);
	VertexInfo.Poly = this;
	VertexInfo.Pos = Pos;
	VertexInfo.Pos.Location.X = V2d.X;
	VertexInfo.Pos.Location.Y = V2d.Y;
	VertexInfo.Alpha0 = 0.0;
	VertexInfo.Alpha1 = 0.0;
	VertexInfo.Alpha2 = 0.0;
	VertexInfo.VID = VID;
	VertexInfo.Flags = Flags;
	VertexInfo.HeightOffset = 0;

	if (const FRoadZoneSidewalk* LaneSidewalk = GetRoadZone().GetPtr<FRoadZoneSidewalk>())
	{
		VertexInfo.HeightOffset = LaneSidewalk->DefaultHeight;

		// TODO: Support FRoadLaneAttributeSidewalkHeightValue
		// TODO: Support FRoadLaneAttributePolygoneCustomizationValue
	}
}

const TInstancedStruct<FRoadZone>& FProceduralPolygon_RoadLoop::GetRoadZone() const
{
	return GetRoadSpline().RoadLayout.LoopedRoadZone;
}

FText FProceduralPolygon_RoadLoop::GetDescription() const
{
	return LOCTEXT("FProceduralPolygon_RoadLoop_GetDescription", "LoopPoly");
}

bool FProceduralPolygon_RoadLoop::SetUVLayers(FDynamicMesh3& Mesh, int TID, double /*UV0ScaleFactor*/, double /*UV1ScaleFactor*/, double /*UV2ScaleFactor*/, int /*UVMaxSize*/) const
{
	check(Mesh.Attributes()->NumUVLayers() >= 2);

	const auto& Layout = GetRoadSpline().RoadLayout;
	const FAxisAlignedBox2d Bounds = FAxisAlignedBox2d(Poly2d.GetVertices());
	const FMatrix2f UVRotate = FMatrix2f::RotationDeg(Layout.LoopedRoadZoneTexAngle);
	const double UVScale = 1.0 / (Bounds.Max - Bounds.Min).GetMax() * Layout.LoopedRoadZoneTexScale;
	const auto WorldLocation = GetRoadSpline().Bounds.GetBox().GetCenter();

	auto TriInfo = FindTri(TID);

	if (ensure(TriInfo.IsValid()))
	{
		const FVector2f UV_A = UVRotate * (FVector2f(TriInfo.A->Pos.Location.X - WorldLocation.X, TriInfo.A->Pos.Location.Y - WorldLocation.Y) * UVScale);
		const FVector2f UV_B = UVRotate * (FVector2f(TriInfo.B->Pos.Location.X - WorldLocation.X, TriInfo.B->Pos.Location.Y - WorldLocation.Y) * UVScale);
		const FVector2f UV_C = UVRotate * (FVector2f(TriInfo.C->Pos.Location.X - WorldLocation.X, TriInfo.C->Pos.Location.Y - WorldLocation.Y) * UVScale);

		if (auto* UVLayer0 = Mesh.Attributes()->GetUVLayer(0))
		{
			UVLayer0->SetTriangle(TID, FIndex3i{
				UVLayer0->AppendElement(UV_A),
				UVLayer0->AppendElement(UV_B),
				UVLayer0->AppendElement(UV_C)
				});
		}

		if (auto* UVLayer1 = Mesh.Attributes()->GetUVLayer(1))
		{
			UVLayer1->SetTriangle(TID, FIndex3i{
				UVLayer1->AppendElement(UV_A),
				UVLayer1->AppendElement(UV_B),
				UVLayer1->AppendElement(UV_C)
				});
		}

		if (auto* UVLayer2 = Mesh.Attributes()->GetUVLayer(2))
		{
			UVLayer2->SetTriangle(TID, FIndex3i{
				UVLayer2->AppendElement(UV_A),
				UVLayer2->AppendElement(UV_B),
				UVLayer2->AppendElement(UV_C)
				});
		}

		return true;
	}

	return false;
}
// --------------------------------------------------------------------------------------------------------------------------------------------------
FProceduralPolygon_Simple::FProceduralPolygon_Simple(FRoadTriangulationData& Owner, int SimplePolygonIndex)
	: FProceduralPolygon(Owner)
	, SimplePolygonIndex(SimplePolygonIndex)
{
	ResultInfo = { EGeometryResultType::InProgress };

	const TArray<FVector>& Vertices = Owner.SimplePolygones[SimplePolygonIndex].Vertices;


	TArray<FVector2D> Points2D;
	Points2D.Reserve(Vertices.Num());
	for (auto& It : Vertices)
	{
		Points2D.Add(FVector2D{ It });
	}

	// Collapse near-coincident vertices (exact closing dup, the loop seam, and any sub-tolerance
	// vertices) before triangulation — they otherwise produce degenerate sliver triangles / holes.
	GeomUtils::RemoveClosePolylineVertices(Points2D, GeomUtils::DefSnapDistance, /*bClosed*/ true);

	GeomUtils::RemovedPolylineSelfIntersection(Points2D);

	FVector2D IntersectionPoint;
	FIndex2i IntersectionIndex;
	if (GeomUtils::FindPolylineSelfIntersection(Points2D, IntersectionPoint, IntersectionIndex))
	{
		ResultInfo.SetFailed(FText::Format(LOCTEXT("FProceduralPolygon_Simple_SelfIntersectionFaild", "FProceduralPolygon_Simple: {0}: Self intersection found"), GetDescription()));
		return;
	}

	if (!IntersectApproximation.UpdateAABBTree(Vertices))
	{
		ResultInfo.SetFailed(FText::Format(LOCTEXT("FProceduralPolygon_Simple_UpdateAABBTreeFaild", "FProceduralPolygon_Simple: {0}: UpdateAABBTree() faild"), GetDescription()));
		return;
	}

	int GID = 0;

	if (GetRoadZone().GetPtr<FRoadZoneDriving>() != nullptr)
	{
		GID = GUIFlags::DrivingSurface;
	}
	else if (GetRoadZone().GetPtr<FRoadZoneSidewalk>() != nullptr)
	{
		GID = GUIFlags::SidewalksSoft;
	}

	LineInfo.PID = this->Owner.Arrangement->Graph.AllocateEdgePolylines();
	for (int i = 0; i < Points2D.Num(); ++i)
	{
		Owner.Arrangement->Insert(Points2D[i], Points2D[(i + 1) % Points2D.Num()], GID, LineInfo.PID);
	}

	LineInfo.VID_A = LineInfo.VID_B = this->Owner.Arrangement->FindExistingVertex(Points2D[0]);

	if (!LineInfo.IsValid())
	{
		ResultInfo.SetFailed(FText::Format(LOCTEXT("FProceduralPolygon_Simple_OutsideLineFaild", "FProceduralPolygon_Simple: {0}: polygon faild "), GetDescription()));
		return;
	}

	WorldCenter = FBox(Vertices).GetCenter();

	ResultInfo.SetSuccess();
}


bool FProceduralPolygon_Simple::OnCompleteArrangement()
{
	if (ResultInfo.HasFailed())
	{
		return false;
	}

	if (!FindPolyline(Owner.Arrangement->Graph, LineInfo, LineVertices))
	{
		ResultInfo.SetFailed(FText::Format(LOCTEXT("FProceduralPolygon_Simple_InsideLineNotFound", "FProceduralPolygon_Simple: {0}: line not found after arrangement"), GetDescription()));
		return false;
	}

	for (int i = 0; i < LineVertices.Num(); ++i)
	{
		AddVertexInfo(LineVertices[i], ERoadVertexInfoFlags::LoopPoly);
	}

	for (int i = 0; i < LineVertices.Num() - 1; ++i)
	{
		Boundary.Add({ LineVertices[i], LineVertices[i + 1] });
	}

	TArray<FVector2d> Vertex2d;
	{
		const auto& FirstInfo = Owner.Vertices3d[Boundary[0].A].Infos[this];
		Vertex2d.Add(FVector2D{ FirstInfo.Pos.Location });
	}
	for (const auto& It : Boundary)
	{
		const auto& Info = Owner.Vertices3d[It.B].Infos[this];
		Vertex2d.Add(FVector2D{ Info.Pos.Location });
	}
	Poly2d = UE::Geometry::FPolygon2d(Vertex2d);

	if (!Poly2d.IsClockwise())
	{
		Algo::Reverse(LineVertices);
		Algo::Reverse(Vertex2d);
		Poly2d = UE::Geometry::FPolygon2d(Vertex2d);
		Algo::Reverse(Boundary);
		for (auto& It : Boundary)
		{
			It = FIndex2i{ It.B, It.A };
		}
	}

	return true;
}

void FProceduralPolygon_Simple::AddVertexInfo(int VID, ERoadVertexInfoFlags Flags) const
{
	check(Owner.Arrangement->Graph.IsVertex(VID));

	if (auto* FoundVertex = Owner.Vertices3d[VID].Infos.Find(this))
	{
		FoundVertex->Flags = FoundVertex->Flags | Flags;
		return;
	}

	const auto V2d = Owner.Arrangement->Graph.GetVertex(VID);
	
	FVector Point;
	FVector Normal;
	IntersectApproximation.Intersect(V2d, Point, Normal);

	auto& VertexInfo = Owner.Vertices3d[VID].Infos.Add(this);
	VertexInfo.Poly = this;
	VertexInfo.Pos.Location.X = V2d.X;
	VertexInfo.Pos.Location.Y = V2d.Y;
	VertexInfo.Pos.Location.Z = Point.Z;
	VertexInfo.Pos.Quat = FQuat::FindBetweenNormals(FVector::UpVector, Normal);
	VertexInfo.Pos.ROffset = 0;
	VertexInfo.Pos.SOffset = 0;
	VertexInfo.Alpha0 = 0.0;
	VertexInfo.Alpha1 = 0.0;
	VertexInfo.Alpha2 = 0.0;
	VertexInfo.VID = VID;
	VertexInfo.Flags = Flags;
	VertexInfo.HeightOffset = 0;

	if (const FRoadZoneSidewalk* LaneSidewalk = GetRoadZone().GetPtr<FRoadZoneSidewalk>())
	{
		VertexInfo.HeightOffset = LaneSidewalk->DefaultHeight;
	}

}

const TInstancedStruct<FRoadZone>& FProceduralPolygon_Simple::GetRoadZone() const
{
	return Owner.SimplePolygones[SimplePolygonIndex].RoadZone;
}

FText FProceduralPolygon_Simple::GetDescription() const
{
	return LOCTEXT("FProceduralPolygon_Simple_GetDescription", "SimplePoly");
}

bool FProceduralPolygon_Simple::SetUVLayers(FDynamicMesh3& Mesh, int TID, double UV0ScaleFactor, double UV1ScaleFactor, double UV2ScaleFactor, int UVMaxSize) const
{

	check(Mesh.Attributes()->NumUVLayers() >= 2);

	const auto& SimplePolygone = Owner.SimplePolygones[SimplePolygonIndex];

	const FAxisAlignedBox2d Bounds = FAxisAlignedBox2d(Poly2d.GetVertices());
	const FMatrix2f UVRotate = FMatrix2f::RotationDeg(SimplePolygone.TextureAngle);
	const double UVScale = 1.0 / (Bounds.Max - Bounds.Min).GetMax() * SimplePolygone.TextureScale;

	auto TriInfo = FindTri(TID);

	if (ensure(TriInfo.IsValid()))
	{
		const FVector2f UV_A = UVRotate * (FVector2f(TriInfo.A->Pos.Location.X - WorldCenter.X, TriInfo.A->Pos.Location.Y - WorldCenter.Y) * UVScale);
		const FVector2f UV_B = UVRotate * (FVector2f(TriInfo.B->Pos.Location.X - WorldCenter.X, TriInfo.B->Pos.Location.Y - WorldCenter.Y) * UVScale);
		const FVector2f UV_C = UVRotate * (FVector2f(TriInfo.C->Pos.Location.X - WorldCenter.X, TriInfo.C->Pos.Location.Y - WorldCenter.Y) * UVScale);

		if (auto* UVLayer0 = Mesh.Attributes()->GetUVLayer(0))
		{
			UVLayer0->SetTriangle(TID, FIndex3i{
				UVLayer0->AppendElement(UV_A),
				UVLayer0->AppendElement(UV_B),
				UVLayer0->AppendElement(UV_C)
				});
		}

		if (auto* UVLayer1 = Mesh.Attributes()->GetUVLayer(1))
		{
			UVLayer1->SetTriangle(TID, FIndex3i{
				UVLayer1->AppendElement(UV_A),
				UVLayer1->AppendElement(UV_B),
				UVLayer1->AppendElement(UV_C)
				});
		}

		if (auto* UVLayer2 = Mesh.Attributes()->GetUVLayer(2))
		{
			UVLayer2->SetTriangle(TID, FIndex3i{
				UVLayer2->AppendElement(UV_A),
				UVLayer2->AppendElement(UV_B),
				UVLayer2->AppendElement(UV_C)
				});
		}

		return true;
	}
	
	return false;
}

double FProceduralPolygon_Simple::GetMaterialPriority() const
{
	int ProfilePriority = 0;

	if (auto* RoadZone = GetRoadZone().GetPtr<FRoadZone>())
	{
		if (const FRoadZoneTypeDetails* Found = GetDefault<UMetaRoadSettings>()->RoadZoneTypes.Find(RoadZone->ZoneType.GetName()))
		{
			ProfilePriority = Found->MaterialPriority;
			if (RoadZone->bOverridePriority)
			{
				ProfilePriority = RoadZone->OverridePriority;
			}
		}
	}

	const auto& SimplePolygone = Owner.SimplePolygones[SimplePolygonIndex];

	return ProfilePriority + /*double(SimplePolygone.MaterialPriority) / 1000.0*/ + double(Owner.Splines.Num() - GetObjectIndex() - 1) / 1000000.0;
}

int FProceduralPolygon_Simple::GetObjectIndex() const
{
	return 	Owner.Splines.Num() + SimplePolygonIndex;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------

TArray<FRoadPosition> RoadPolygonUtils::MakePolyline(const TArray<FArrangementVertex3d>& Vertexes, const TArray<int>& VerticesIDs, const FProceduralPolygon* PolyFilter)
{
	TArray<FRoadPosition> Vertices;
	Vertices.Reserve(VerticesIDs.Num());

	for (auto VID : VerticesIDs)
	{
		auto* Info = Vertexes[VID].Infos.Find(PolyFilter);
		check(Info);

		FRoadPosition Pos = Info->Pos;
		Pos.Location = Vertexes[VID].Vertex + Vertexes[VID].Normal * Info->HeightOffset;

		// Set normal from FRoadPosition to Quat
		auto Diff = FQuat::FindBetweenNormals(Pos.Quat.GetUpVector(), Vertexes[VID].Normal);
		Pos.Quat = Diff * Pos.Quat;

		Vertices.Add(Pos);
	}

	return Vertices;
}

#undef LOCTEXT_NAMESPACE