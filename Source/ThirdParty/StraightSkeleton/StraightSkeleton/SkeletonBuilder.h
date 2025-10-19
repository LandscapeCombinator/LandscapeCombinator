#pragma once
#include <vector>
#include <unordered_set>
#include <queue>
#include <memory>
#include <algorithm>
#include <limits>
#include <map>
#include <list>
#include "Skeleton.h"
#include "Vertex.h"
#include "Edge.h"
#include "CircularList.h"
#include "MultiEdgeEvent.h"
#include "PickEvent.h"
#include "MultiSplitEvent.h"
#include "SplitEvent.h"
#include "FaceQueueUtil.h"
#include "PrimitiveUtils.h"
#include "VertexSplitEvent.h"
#include "SplitChain.h"
#include "EChainType.h"
#include "PickEvent.h"
#include "MultiEdgeEvent.h"
#include "MultiSplitEvent.h"
#include "LavUtil.h"
#include "SingleEdgeChain.h"

/// <summary> 
///     Straight skeleton algorithm implementation. Base on highly modified Petr
///     Felkel and Stepan Obdrzalek algorithm. 
/// </summary>
/// <remarks> 
///     This is a C++ adopted port of java implementation from kendzi-straight-skeleton library.
/// </remarks>
class STRAIGHTSKELETON_API SkeletonBuilder
{	
	
protected:
	/// <summary>
	///     Check if given point is on one of edge bisectors. If so this is vertex
	///     split event. This event need two opposite edges to process but second
	///     (next) edge can be take from edges list and it is next edge on list.
	/// </summary>
	/// <param name="point">Point of event.</param>
	/// <param name="edge">candidate for opposite edge.</param>
	/// <returns>previous opposite edge if it is vertex split event.</returns>
	static std::shared_ptr<Edge> VertexOpositeEdge(std::shared_ptr<Vector2d> point, std::shared_ptr<Edge> edge);
private:	
	// Error epsilon.
	static const double SplitEpsilon;
	class SkeletonEventDistanseComparer
	{
	public:
		bool operator()(std::shared_ptr<SkeletonEvent> const& left, std::shared_ptr<SkeletonEvent> const& right) const
		{
			return left->Distance > right->Distance;
		}
	};
	class ChainComparer
	{
	private:
		Vector2d _center;

	public:
		ChainComparer(Vector2d center)
		{
			_center = center;
		}

		bool operator()(std::shared_ptr<IChain> x, std::shared_ptr<IChain> y)
		{
			if (x == y)
				return true;

			auto angle1 = Angle(_center, *y->PreviousEdge()->Begin);
			auto angle2 = Angle(_center, *x->PreviousEdge()->Begin);
			return angle1 > angle2;
		}

		double Angle(Vector2d p0, Vector2d p1)
		{
			auto dx = p1.X - p0.X;
			auto dy = p1.Y - p0.Y;
			return atan2(dy, dx);
		}
	};
	struct SplitCandidate
	{
	private:
		using spe = std::shared_ptr<Edge>;
		using spv2d = std::shared_ptr<Vector2d>;
	public:
		double Distance;
		spe OppositeEdge;
		spv2d OppositePoint;
		spv2d Point;

		SplitCandidate(spv2d point, double distance, spe oppositeEdge, spv2d oppositePoint)
		{
			Point = point;
			Distance = distance;
			OppositeEdge = oppositeEdge;
			OppositePoint = oppositePoint;
		}
	};	
	struct SplitCandidateComparer
	{
	public:
		bool operator()(SplitCandidate& left, SplitCandidate& right)
		{
			return left.Distance < right.Distance;
		}
	};
	template<typename T>
	using sp = std::shared_ptr<T>;
	template<typename T>
	using wp = std::weak_ptr<T>;
	using queueSkeletonEvent = std::priority_queue<sp<SkeletonEvent>, std::vector<sp<SkeletonEvent>>, SkeletonEventDistanseComparer>;
	using listVector2d = std::vector<Vector2d>;
	using nestedlistVector2d = std::vector<std::vector<Vector2d>>;
	using hashsetCircularList = std::unordered_set<sp<CircularList>, CircularList::HashFunction>;
	using listEdgeEvent = std::vector<sp<EdgeEvent>>;
	using listEdge = std::list<sp<Edge>>;
	using listIChain = std::vector<sp<IChain>>;
	using listSkeletonEvent = std::vector<sp<SkeletonEvent>>;
	using hashsetVertex = std::unordered_set<sp<Vertex>, Vertex::HashFunction>;
	using listFaceQueue = std::vector<sp<FaceQueue>>;
	using listVertex = std::vector<sp<Vertex>>;
	
	static listVector2d InitPolygon(listVector2d& polygon);
	static void ProcessTwoNodeLavs(sp<hashsetCircularList> sLav);
	static void RemoveEmptyLav(sp<hashsetCircularList> sLav);
	static void AddMultiBackFaces(sp<listEdgeEvent> edgeList, sp<Vertex> edgeVertex);
	//Renamed due to conflict with class name
	static void EmitMultiEdgeEvent(sp<MultiEdgeEvent> event, sp<queueSkeletonEvent> queue, sp<listEdge> edges);
	//Renamed due to conflict with class name
	static void EmitPickEvent(sp<PickEvent> event);
	//Renamed due to conflict with class name
	static void EmitMultiSplitEvent(sp<MultiSplitEvent> event, sp<hashsetCircularList> sLav, sp<queueSkeletonEvent> queue, sp<listEdge> edges);
	static void CorrectBisectorDirection(sp<LineParametric2d> bisector, sp<Vertex> beginNextVertex, sp<Vertex> endPreviousVertex, sp<Edge> beginEdge, sp<Edge> endEdge);
	static std::shared_ptr<FaceNode> AddSplitFaces(sp<FaceNode> lastFaceNode, sp<IChain> chainBegin, sp<IChain> chainEnd, sp<Vertex> newVertex);
	static std::shared_ptr<Vertex> CreateOppositeEdgeVertex(sp<Vertex> newVertex);
	static void CreateOppositeEdgeChains(sp<hashsetCircularList> sLav, sp<listIChain> chains, sp<Vector2d> center);
	static std::shared_ptr<Vertex> CreateMultiSplitVertex(sp<Edge> nextEdge, sp<Edge> previousEdge, sp<Vector2d> center, double distance);
	/// <summary>
	///     Create chains of events from cluster. Cluster is set of events which meet
	///     in the same result point. Try to connect all event which share the same
	///     vertex into chain. events in chain are sorted. If events don't share
	///     vertex, returned chains contains only one event.
	/// </summary>
	/// <param name="cluster">Set of event which meet in the same result point</param>
	/// <returns>chains of events</returns>
	static sp<listIChain> CreateChains(sp<listSkeletonEvent> cluster);
	static bool IsInEdgeChain(sp<SplitEvent> split, sp<EdgeChain> chain);
	static sp<listEdgeEvent> CreateEdgeChain(sp<listEdgeEvent> edgeCluster);
	static void RemoveEventsUnderHeight(sp<queueSkeletonEvent> queue, double levelHeight);
	static sp<listSkeletonEvent> LoadAndGroupLevelEvents(sp<queueSkeletonEvent> queue);
	static sp<listSkeletonEvent> GroupLevelEvents(sp<listSkeletonEvent> levelEvents);
	static bool IsEventInGroup(sp<hashsetVertex> parentGroup, sp<SkeletonEvent> event);
	static void AddEventToGroup(sp<hashsetVertex> parentGroup, sp<SkeletonEvent> event);
	static std::shared_ptr<SkeletonEvent> CreateLevelEvent(sp<Vector2d> eventCenter, double distance, sp<listSkeletonEvent> eventCluster);
	/// <summary> Loads all not obsolete event which are on one level. As level heigh is taken epsilon. </summary>
	static sp<listSkeletonEvent> LoadLevelEvents(sp<queueSkeletonEvent> queue);
	static int AssertMaxNumberOfInteraction(int& count);
	static nestedlistVector2d MakeClockwise(nestedlistVector2d& holes);
	static listVector2d MakeCounterClockwise(listVector2d& polygon);
	static void InitSlav(listVector2d& polygon, sp<hashsetCircularList> sLav, sp<listEdge> edges, sp<listFaceQueue> faces);
	static Skeleton AddFacesToOutput(sp<listFaceQueue> faces);
	static void InitEvents(sp<hashsetCircularList> sLav, sp<queueSkeletonEvent> queue, sp<listEdge> edges);
	static void ComputeSplitEvents(sp<Vertex> vertex, sp<listEdge> edges, sp<queueSkeletonEvent> queue, double distanceSquared);
	static void ComputeEvents(sp<Vertex> vertex, sp<queueSkeletonEvent> queue, sp<listEdge> edges);
	/// <summary>
	///     Calculate two new edge events for given vertex. events are generated
	///     using current, previous and next vertex in current lav. When two edge
	///     events are generated distance from source is check. To queue is added
	///     only closer event or both if they have the same distance.
	/// </summary>
	static double ComputeCloserEdgeEvent(sp<Vertex> vertex, sp<queueSkeletonEvent> queue);
	static std::shared_ptr<SkeletonEvent> CreateEdgeEvent(sp<Vector2d> point, sp<Vertex> previousVertex, sp<Vertex> nextVertex);
	static void ComputeEdgeEvents(sp<Vertex> previousVertex, sp<Vertex> nextVertex, sp<queueSkeletonEvent> queue);
	static std::shared_ptr<std::vector<SplitCandidate>> CalcOppositeEdges(sp<Vertex> vertex, sp<listEdge> edges);
	static bool EdgeBehindBisector(LineParametric2d bisector, LineLinear2d edge);
	static std::shared_ptr<SplitCandidate> CalcCandidatePointForSplit(sp<Vertex> vertex, sp<Edge> edge);
	static std::shared_ptr<Edge> ChoseLessParallelVertexEdge(sp<Vertex> vertex, sp<Edge> edge);
	static Vector2d ComputeIntersectionBisectors(sp<Vertex> vertexPrevious, sp<Vertex> vertexNext);
	static std::shared_ptr<Vertex> FindOppositeEdgeLav(sp<hashsetCircularList> sLav, sp<Edge> oppositeEdge, sp<Vector2d>center);
	static std::shared_ptr<Vertex> ChooseOppositeEdgeLav(sp<listVertex> edgeLavs, sp<Edge> oppositeEdge, sp<Vector2d> center);
	static sp<listVertex> FindEdgeLavs(sp<hashsetCircularList> sLav, sp<Edge> oppositeEdge, sp<CircularList> skippedLav);
	
	/// <summary>
		///     Take next lav vertex _AFTER_ given edge, find vertex is always on RIGHT
		///     site of edge.
		/// </summary>
	static std::shared_ptr<Vertex> GetEdgeInLav(sp<CircularList> lav, sp<Edge> oppositeEdge);
	static void AddFaceBack(sp<Vertex> newVertex, sp<Vertex> va, sp<Vertex> vb);
	static void AddFaceRight(sp<Vertex> newVertex, sp<Vertex> vb);
	static void AddFaceLeft(sp<Vertex> newVertex, sp<Vertex> va);
	static double CalcDistance(Vector2d intersect, Edge currentEdge);
	static Vector2d CalcVectorBisector(Vector2d norm1, Vector2d norm2);
	static sp<LineParametric2d> CalcBisector(sp<Vector2d> p, Edge e1, Edge e2);

public:
	/// <summary> Creates straight skeleton for given polygon. </summary>
	static Skeleton Build(listVector2d& polygon);
	/// <summary> Creates straight skeleton for given polygon with holes. </summary>
	static Skeleton Build(listVector2d& polygon, nestedlistVector2d& holes);
};
