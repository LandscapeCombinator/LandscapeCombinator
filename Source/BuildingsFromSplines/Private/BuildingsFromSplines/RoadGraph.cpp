#include "BuildingsFromSplines/RoadGraph.h"

namespace
{
	double ProjectParam(const FVector& A, const FVector& B, const FVector& P)
	{
		const FVector AB = B - A;
		const double LenSq = AB.SizeSquared();
		return (LenSq > KINDA_SMALL_NUMBER) ? FMath::Clamp(FVector::DotProduct(P - A, AB) / LenSq, 0.0, 1.0) : 0.0;
	}

	FVector ClosestPointOnSegment(const FVector& P, const FVector& A, const FVector& B, double& OutT)
	{
		OutT = ProjectParam(A, B, P);
		return FMath::Lerp(A, B, OutT);
	}

	bool TryFindPlanCrossing(const FVector& A0, const FVector& A1, const FVector& B0, const FVector& B1, double JoinDistance, FVector& OutLocation)
	{
		const FVector2D D1 = FVector2D(A1) - FVector2D(A0);
		const FVector2D D2 = FVector2D(B1) - FVector2D(B0);

		const double Denom = D1.X * D2.Y - D1.Y * D2.X;
		const double Scale = D1.Size() * D2.Size();
		if (Scale < KINDA_SMALL_NUMBER || FMath::Abs(Denom) < 1e-6 * Scale)
			return false;

		const FVector2D R = FVector2D(B0) - FVector2D(A0);
		const double T1 = (R.X * D2.Y - R.Y * D2.X) / Denom;
		const double T2 = (R.X * D1.Y - R.Y * D1.X) / Denom;

		if (T1 < 0.0 || T1 > 1.0 || T2 < 0.0 || T2 > 1.0) return false;

		const FVector P1 = FMath::Lerp(A0, A1, T1);
		const FVector P2 = FMath::Lerp(B0, B1, T2);
		if (FVector::Dist(P1, P2) > JoinDistance) return false;

		OutLocation = FMath::Lerp(P1, P2, 0.5);
		return true;
	}
}

void FRoadGraph::Reset()
{
	Nodes.Empty();
	Edges.Empty();
	NodeSpatialHash.Empty();
	EdgeSpatialHash.Empty();
	NextChainId = 0;
}

bool FRoadGraph::IsJunctionNode(int32 NodeIndex) const
{
	return Nodes.IsValidIndex(NodeIndex) && Nodes[NodeIndex].EdgeIndices.Num() > 2;
}

FIntVector FRoadGraph::GetSpatialHashCell(const FVector& Location) const
{
	return FIntVector(
		FMath::FloorToInt(Location.X / SpatialHashCellSize),
		FMath::FloorToInt(Location.Y / SpatialHashCellSize),
		FMath::FloorToInt(Location.Z / SpatialHashCellSize));
}

int32 FRoadGraph::AddNode(const FVector& Location)
{
	FRoadNode NewNode;
	NewNode.Location = Location;
	const int32 NewIndex = Nodes.Add(NewNode);
	NodeSpatialHash.FindOrAdd(GetSpatialHashCell(Location)).Add(NewIndex);
	UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("AddNode: Created Node %d at %s"), NewIndex, *Location.ToString());
	return NewIndex;
}

void FRoadGraph::InvalidateNode(int32 NodeIndex)
{
	const FIntVector Cell = GetSpatialHashCell(Nodes[NodeIndex].Location);
	if (TArray<int32>* CellNodes = NodeSpatialHash.Find(Cell))
	{
		CellNodes->Remove(NodeIndex);
		if (CellNodes->Num() == 0) NodeSpatialHash.Remove(Cell);
	}
	Nodes[NodeIndex].bValid = false;
}

void FRoadGraph::AddEdgeToSpatialHash(int32 EdgeIndex)
{
	const FRoadEdge& Edge = Edges[EdgeIndex];
	const FVector& A = Nodes[Edge.StartNodeIndex].Location;
	const FVector& B = Nodes[Edge.EndNodeIndex].Location;
	const FIntVector CellMin = GetSpatialHashCell(A.ComponentMin(B)) - FIntVector(1);
	const FIntVector CellMax = GetSpatialHashCell(A.ComponentMax(B)) + FIntVector(1);
	for (int32 x = CellMin.X; x <= CellMax.X; x++)
	for (int32 y = CellMin.Y; y <= CellMax.Y; y++)
	for (int32 z = CellMin.Z; z <= CellMax.Z; z++)
		EdgeSpatialHash.FindOrAdd(FIntVector(x, y, z)).AddUnique(EdgeIndex);
}

void FRoadGraph::RemoveEdgeFromSpatialHash(int32 EdgeIndex)
{
	const FRoadEdge& Edge = Edges[EdgeIndex];
	const FVector& A = Nodes[Edge.StartNodeIndex].Location;
	const FVector& B = Nodes[Edge.EndNodeIndex].Location;
	const FIntVector CellMin = GetSpatialHashCell(A.ComponentMin(B)) - FIntVector(1);
	const FIntVector CellMax = GetSpatialHashCell(A.ComponentMax(B)) + FIntVector(1);
	for (int32 x = CellMin.X; x <= CellMax.X; x++)
	for (int32 y = CellMin.Y; y <= CellMax.Y; y++)
	for (int32 z = CellMin.Z; z <= CellMax.Z; z++)
		if (TArray<int32>* Cell = EdgeSpatialHash.Find(FIntVector(x, y, z)))
		{
			Cell->Remove(EdgeIndex);
			if (Cell->Num() == 0) EdgeSpatialHash.Remove(FIntVector(x, y, z));
		}
}

int32 FRoadGraph::AddEdge(int32 StartNodeIndex, int32 EndNodeIndex, const FVector& StartTangent, const FVector& EndTangent)
{
	if (StartNodeIndex == EndNodeIndex) return INDEX_NONE;
	if (!Nodes.IsValidIndex(StartNodeIndex) || !Nodes[StartNodeIndex].bValid) return INDEX_NONE;
	if (!Nodes.IsValidIndex(EndNodeIndex) || !Nodes[EndNodeIndex].bValid) return INDEX_NONE;

	FRoadEdge NewEdge;
	NewEdge.StartNodeIndex = StartNodeIndex;
	NewEdge.EndNodeIndex = EndNodeIndex;
	NewEdge.StartTangentWorld = StartTangent;
	NewEdge.EndTangentWorld = EndTangent;
	const int32 EdgeIndex = Edges.Add(NewEdge);
	Nodes[StartNodeIndex].EdgeIndices.Add(EdgeIndex);
	Nodes[EndNodeIndex].EdgeIndices.Add(EdgeIndex);
	AddEdgeToSpatialHash(EdgeIndex);
	UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("AddEdge: Created Edge %d connecting Node %d and Node %d"), EdgeIndex, StartNodeIndex, EndNodeIndex);
	return EdgeIndex;
}

void FRoadGraph::RemoveEdgeFromNode(int32 NodeIndex, int32 EdgeIndex)
{
	if (!Nodes.IsValidIndex(NodeIndex)) return;
	Nodes[NodeIndex].EdgeIndices.Remove(EdgeIndex);
	if (Nodes[NodeIndex].EdgeIndices.Num() == 0) InvalidateNode(NodeIndex);
}

void FRoadGraph::DeleteEdge(int32 EdgeIndex)
{
	if (!Edges.IsValidIndex(EdgeIndex)) return;
	FRoadEdge& Edge = Edges[EdgeIndex];
	if (!Edge.bValid) return;

	RemoveEdgeFromSpatialHash(EdgeIndex);
	RemoveEdgeFromNode(Edge.StartNodeIndex, EdgeIndex);
	RemoveEdgeFromNode(Edge.EndNodeIndex, EdgeIndex);
	Edge.bValid = false;
}

TArray<int32> FRoadGraph::QueryNodesNear(const FVector& Location, double Radius) const
{
	TArray<int32> Result;
	const FIntVector CellMin = GetSpatialHashCell(Location - FVector(Radius));
	const FIntVector CellMax = GetSpatialHashCell(Location + FVector(Radius));
	for (int32 x = CellMin.X; x <= CellMax.X; x++)
	for (int32 y = CellMin.Y; y <= CellMax.Y; y++)
	for (int32 z = CellMin.Z; z <= CellMax.Z; z++)
		if (const TArray<int32>* Cell = NodeSpatialHash.Find(FIntVector(x, y, z)))
			for (int32 NodeIndex : *Cell)
				Result.AddUnique(NodeIndex);
	return Result;
}

TArray<int32> FRoadGraph::QueryNodesInBounds(const FVector& Min, const FVector& Max) const
{
	TArray<int32> Result;
	const FIntVector CellMin = GetSpatialHashCell(Min);
	const FIntVector CellMax = GetSpatialHashCell(Max);
	for (int32 x = CellMin.X; x <= CellMax.X; x++)
	for (int32 y = CellMin.Y; y <= CellMax.Y; y++)
	for (int32 z = CellMin.Z; z <= CellMax.Z; z++)
		if (const TArray<int32>* Cell = NodeSpatialHash.Find(FIntVector(x, y, z)))
			for (int32 NodeIndex : *Cell)
				if (Nodes[NodeIndex].bValid) Result.AddUnique(NodeIndex);
	return Result;
}

TArray<int32> FRoadGraph::QueryEdgesInBounds(const FVector& Min, const FVector& Max) const
{
	TArray<int32> Result;
	const FIntVector CellMin = GetSpatialHashCell(Min);
	const FIntVector CellMax = GetSpatialHashCell(Max);
	for (int32 x = CellMin.X; x <= CellMax.X; x++)
	for (int32 y = CellMin.Y; y <= CellMax.Y; y++)
	for (int32 z = CellMin.Z; z <= CellMax.Z; z++)
		if (const TArray<int32>* Cell = EdgeSpatialHash.Find(FIntVector(x, y, z)))
			for (int32 EdgeIndex : *Cell)
				if (Edges[EdgeIndex].bValid) Result.AddUnique(EdgeIndex);
	return Result;
}

TArray<int32> FRoadGraph::QueryEdgesNear(const FVector& Location, double Radius) const
{
	return QueryEdgesInBounds(Location - FVector(Radius), Location + FVector(Radius));
}

TArray<int32> FRoadGraph::QueryEdgesNearSegment(const FVector& A, const FVector& B, double Radius) const
{
	return QueryEdgesInBounds(A.ComponentMin(B) - FVector(Radius), A.ComponentMax(B) + FVector(Radius));
}

bool FRoadGraph::EdgesShareNode(int32 EdgeA, int32 EdgeB) const
{
	const FRoadEdge& A = Edges[EdgeA];
	const FRoadEdge& B = Edges[EdgeB];
	return A.StartNodeIndex == B.StartNodeIndex || A.StartNodeIndex == B.EndNodeIndex
		|| A.EndNodeIndex == B.StartNodeIndex || A.EndNodeIndex == B.EndNodeIndex;
}

void FRoadGraph::SplitEdgeAt(int32 EdgeIndex, int32 AtNode, int32& OutLeft, int32& OutRight, FOnEdgeSplit OnEdgeSplit, TSet<int32>* ChainSet)
{
	const FRoadEdge Old = Edges[EdgeIndex];
	const double T = ProjectParam(Nodes[Old.StartNodeIndex].Location, Nodes[Old.EndNodeIndex].Location, Nodes[AtNode].Location);
	const FVector SplitTangent = FMath::Lerp(Old.StartTangentWorld, Old.EndTangentWorld, T);

	// Carry arc-length data across the split for any chain both endpoints agree on.
	for (const FChainSample& StartSample : Nodes[Old.StartNodeIndex].ChainSamples)
	for (const FChainSample& EndSample : Nodes[Old.EndNodeIndex].ChainSamples)
		if (StartSample.ChainId == EndSample.ChainId)
			Nodes[AtNode].ChainSamples.Add({ StartSample.ChainId, FMath::Lerp(StartSample.ArcLength, EndSample.ArcLength, T) });

	OutLeft  = AddEdge(Old.StartNodeIndex, AtNode, Old.StartTangentWorld, SplitTangent);
	OutRight = AddEdge(AtNode, Old.EndNodeIndex, SplitTangent, Old.EndTangentWorld);

	DeleteEdge(EdgeIndex);
	OnEdgeSplit(OutLeft, EdgeIndex);
	OnEdgeSplit(OutRight, EdgeIndex);

	if (ChainSet && ChainSet->Remove(EdgeIndex) > 0)
	{
		ChainSet->Add(OutLeft);
		ChainSet->Add(OutRight);
	}

	UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("SplitEdgeAt: Split Edge %d at Node %d. Created Left %d, Right %d"), EdgeIndex, AtNode, OutLeft, OutRight);
}

bool FRoadGraph::FindNearestCrossing(int32 EdgeA, int32& OutEdgeB, FVector& OutLocation) const
{
	const FVector& A0 = Nodes[Edges[EdgeA].StartNodeIndex].Location;
	const FVector& A1 = Nodes[Edges[EdgeA].EndNodeIndex].Location;

	OutEdgeB = INDEX_NONE;
	double BestDistSq = TNumericLimits<double>::Max();

	for (int32 EdgeB : QueryEdgesNearSegment(A0, A1, JoinDistance))
	{
		if (EdgeB == EdgeA || EdgesShareNode(EdgeA, EdgeB)) continue;

		const FVector& B0 = Nodes[Edges[EdgeB].StartNodeIndex].Location;
		const FVector& B1 = Nodes[Edges[EdgeB].EndNodeIndex].Location;

		FVector Location;
		if (!TryFindPlanCrossing(A0, A1, B0, B1, JoinDistance, Location)) continue;

		const double DistSq = FVector::DistSquared(A0, Location);
		if (DistSq < BestDistSq) { BestDistSq = DistSq; OutEdgeB = EdgeB; OutLocation = Location; }
	}
	return OutEdgeB != INDEX_NONE;
}

void FRoadGraph::MergeNodeInto(int32 NodeToRemove, int32 NodeToKeep, FOnEdgeRemoved OnEdgeRemoved)
{
	if (NodeToRemove == NodeToKeep) return;

	UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("MergeNodeInto DETAIL: NodeToKeep (%d, current edges: %d) receiving edges from NodeToRemove (%d, current edges: %d)"), 
		NodeToKeep, Nodes[NodeToKeep].EdgeIndices.Num(), NodeToRemove, Nodes[NodeToRemove].EdgeIndices.Num());

	for (int32 EdgeIndex : Nodes[NodeToRemove].EdgeIndices)
	{
		RemoveEdgeFromSpatialHash(EdgeIndex);
		FRoadEdge& Edge = Edges[EdgeIndex];

		UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("  * Processing edge %d (StartNode: %d, EndNode: %d, bValid: %d)"), 
			EdgeIndex, Edge.StartNodeIndex, Edge.EndNodeIndex, Edge.bValid);

		if (Edge.StartNodeIndex == NodeToRemove) Edge.StartNodeIndex = NodeToKeep;
		if (Edge.EndNodeIndex == NodeToRemove) Edge.EndNodeIndex = NodeToKeep;

		if (Edge.StartNodeIndex == Edge.EndNodeIndex) 
		{ 
			UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("    -> Edge %d became a degenerate self-loop and was invalidated."), EdgeIndex);
			Edge.bValid = false; 
			Nodes[NodeToKeep].EdgeIndices.Remove(EdgeIndex);
			OnEdgeRemoved(EdgeIndex);
			continue; 
		}

		AddEdgeToSpatialHash(EdgeIndex);
		Nodes[NodeToKeep].EdgeIndices.AddUnique(EdgeIndex);
	}

	UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("  -> Final edge count on NodeToKeep %d after merge: %d"), NodeToKeep, Nodes[NodeToKeep].EdgeIndices.Num());
	Nodes[NodeToKeep].ChainSamples.Append(Nodes[NodeToRemove].ChainSamples);
	Nodes[NodeToRemove].EdgeIndices.Empty();
	InvalidateNode(NodeToRemove);
}

// True if Candidate sits within MaxDistance of Self, measured along a chain they
// both belong to -- i.e. it's an upcoming/preceding sample of the same original
// curve, not a genuine gap to close. O(1): a handful of stored samples per node,
// no graph traversal.
bool FRoadGraph::IsSameChainWithinDistance(int32 NodeA, int32 NodeB, double MaxDistance) const
{
	for (const FChainSample& A : Nodes[NodeA].ChainSamples)
	for (const FChainSample& B : Nodes[NodeB].ChainSamples)
		if (A.ChainId == B.ChainId && FMath::Abs(A.ArcLength - B.ArcLength) <= MaxDistance)
			return true;
	return false;
}

void FRoadGraph::ResolveCrossings(TSet<int32>& ChainSet, TArray<int32>& OutTouchedNodeIndices, FOnEdgeSplit OnEdgeSplit, FOnEdgeRemoved OnEdgeRemoved)
{
	TArray<int32> Pending = ChainSet.Array();
	while (Pending.Num() > 0)
	{
		const int32 EdgeA = Pending.Pop();
		if (!Edges.IsValidIndex(EdgeA) || !Edges[EdgeA].bValid) continue;

		int32 EdgeB;
		FVector CrossingLocation;
		if (!FindNearestCrossing(EdgeA, EdgeB, CrossingLocation)) continue;

		int32 NewNode = INDEX_NONE;
		for (int32 Nearby : QueryNodesNear(CrossingLocation, JoinDistance))
		{
			if (Nodes.IsValidIndex(Nearby) && Nodes[Nearby].bValid && IsJunctionNode(Nearby))
			{				
				NewNode = Nearby;
				break;
			}
		}
		if (NewNode == INDEX_NONE)
		{
			NewNode = AddNode(CrossingLocation);
		}
		OutTouchedNodeIndices.AddUnique(NewNode);

		UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("ResolveCrossings: Edge %d crossed Edge %d. Created intersection Node %d at %s"), EdgeA, EdgeB, NewNode, *CrossingLocation.ToString());

		const int32 EdgePair[2] = { EdgeA, EdgeB };
		for (int32 Edge : EdgePair)
		{
			const int32 S = Edges[Edge].StartNodeIndex;
			const int32 E = Edges[Edge].EndNodeIndex;
			const double DistS = FVector::Dist(Nodes[S].Location, CrossingLocation);
			const double DistE = FVector::Dist(Nodes[E].Location, CrossingLocation);
			const int32 NearEndpoint = (DistS <= DistE)
				? (DistS <= JoinDistance ? S : INDEX_NONE)
				: (DistE <= JoinDistance ? E : INDEX_NONE);

			if (NearEndpoint != INDEX_NONE)
			{
				if (NearEndpoint != NewNode)
				{
					MergeNodeInto(NearEndpoint, NewNode, OnEdgeRemoved);

					// The merge moved this edge's endpoint, so it may now cross something
					if (Edges.IsValidIndex(Edge) && Edges[Edge].bValid)
					{
						Pending.AddUnique(Edge);
					}
				}
				continue;
			}

			int32 Left, Right;
			SplitEdgeAt(Edge, NewNode, Left, Right, OnEdgeSplit, &ChainSet);
			Pending.Add(Left);
			Pending.Add(Right);
		}
	}
}

bool FRoadGraph::TrySnapToNode(int32 NodeIndex, TArray<int32>& OutTouchedNodeIndices, FOnEdgeRemoved OnEdgeRemoved)
{
	const FVector Location = Nodes[NodeIndex].Location;

	int32 BestNode = INDEX_NONE;
	double BestDist = JoinDistance;
	for (int32 Other : QueryNodesNear(Location, JoinDistance))
	{
		if (Other == NodeIndex || !Nodes[Other].bValid) continue;
		if (IsSameChainWithinDistance(NodeIndex, Other, JoinDistance)) continue;

		const double Dist = FVector::Dist(Location, Nodes[Other].Location);
		if (Dist <= BestDist)
		{
			BestDist = Dist; BestNode = Other;
			UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("TrySnapToNode: Dangling Node %d at %s considering Node %d at %s (dist: %f)"), NodeIndex, *Location.ToString(), Other, *Nodes[Other].Location.ToString(), Dist);
		}
	}
	if (BestNode == INDEX_NONE) return false;

	UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("TrySnapToNode: Dangling Node %d snapping to existing Node %d"), NodeIndex, BestNode);
	MergeNodeInto(NodeIndex, BestNode, OnEdgeRemoved);
	OutTouchedNodeIndices.AddUnique(BestNode);
	return true;
}

bool FRoadGraph::TrySnapToSegment(int32 NodeIndex, TArray<int32>& OutTouchedNodeIndices, FOnEdgeSplit OnEdgeSplit, FOnEdgeRemoved OnEdgeRemoved, TSet<int32>* ChainSet)
{
	const FVector Location = Nodes[NodeIndex].Location;
	const int32 OwnEdge = Nodes[NodeIndex].EdgeIndices[0];

	int32 BestEdge = INDEX_NONE;
	FVector BestPoint;
	double BestDist = JoinDistance;
	for (int32 Other : QueryEdgesNear(Location, JoinDistance))
	{
		if (Other == OwnEdge) continue;
		if (Edges[Other].StartNodeIndex == NodeIndex || Edges[Other].EndNodeIndex == NodeIndex) continue;
		if (IsSameChainWithinDistance(NodeIndex, Edges[Other].StartNodeIndex, JoinDistance) ||
			IsSameChainWithinDistance(NodeIndex, Edges[Other].EndNodeIndex, JoinDistance)) continue;

		double T;
		const FVector P = ClosestPointOnSegment(Location,
			Nodes[Edges[Other].StartNodeIndex].Location, Nodes[Edges[Other].EndNodeIndex].Location, T);
		if (T <= 1e-3 || T >= 1.0 - 1e-3) continue; // near an existing endpoint: TrySnapToNode's job

		const double Dist = FVector::Dist(Location, P);
		if (Dist <= BestDist) { BestDist = Dist; BestEdge = Other; BestPoint = P; }
	}
	if (BestEdge == INDEX_NONE) return false;

	const int32 NewNode = AddNode(BestPoint);
	int32 Left, Right;
	SplitEdgeAt(BestEdge, NewNode, Left, Right, OnEdgeSplit, ChainSet);
	MergeNodeInto(NodeIndex, NewNode, OnEdgeRemoved);
	OutTouchedNodeIndices.AddUnique(NewNode);
	return true;
}

void FRoadGraph::SnapDanglingNodes(const TArray<int32>& CandidateNodes, TArray<int32>& OutTouchedNodeIndices, FOnEdgeSplit OnEdgeSplit, FOnEdgeRemoved OnEdgeRemoved, TSet<int32>* ChainSet)
{
	for (int32 NodeIndex : CandidateNodes)
	{
		if (!Nodes.IsValidIndex(NodeIndex) || !Nodes[NodeIndex].bValid) continue;
		if (Nodes[NodeIndex].EdgeIndices.Num() != 1) continue; // only true dangling ends

		if (TrySnapToNode(NodeIndex, OutTouchedNodeIndices, OnEdgeRemoved)) continue;
		TrySnapToSegment(NodeIndex, OutTouchedNodeIndices, OnEdgeSplit, OnEdgeRemoved, ChainSet);
	}
}

TArray<int32> FRoadGraph::ClusterNearbyJunctions(const TArray<int32>& Seeds, FOnEdgeRemoved OnEdgeRemoved)
{
	TArray<int32> Active = Seeds;
	bool bMerged = true;
	while (bMerged)
	{
		bMerged = false;
		for (int32 i = 0; i < Active.Num() && !bMerged; i++)
		{
			if (!Nodes.IsValidIndex(Active[i]) || !IsJunctionNode(Active[i])) continue;
			for (int32 Nearby : QueryNodesNear(Nodes[Active[i]].Location, JoinDistance))
			{
				UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("ClusterNearbyJunctions: Junction %d evaluating nearby Node %d (Degree %d, Valid %d, Dist %f)"), 
					Active[i], Nearby, Nodes[Nearby].EdgeIndices.Num(), Nodes[Nearby].bValid, FVector::Dist(Nodes[Active[i]].Location, Nodes[Nearby].Location));
				if (Nearby == Active[i]) continue;

				if (!IsJunctionNode(Nearby)) continue;
				if (FVector::Dist(Nodes[Active[i]].Location, Nodes[Nearby].Location) > JoinDistance) continue;
				UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("ClusterNearbyJunctions: Merging Junction Node %d (at %s) into Junction Node %d (at %s) because distance is within JoinDistance"), Nearby, *Nodes[Nearby].Location.ToString(), Active[i], *Nodes[Active[i]].Location.ToString());
				MergeNodeInto(Nearby, Active[i], OnEdgeRemoved);
				Active.Remove(Nearby);
				bMerged = true;
				break;
			}
		}
	}
	return Active;
}

bool FRoadGraph::AbsorbDanglingStubs(const TArray<int32>& JunctionSeeds, FOnEdgeRemoved OnEdgeRemoved)
{
	bool bAnyChanged = false;

	for (int32 JunctionNode : JunctionSeeds)
	{
		if (!Nodes.IsValidIndex(JunctionNode) || !IsJunctionNode(JunctionNode)) continue;
		const FVector Location = Nodes[JunctionNode].Location;

		bool bChanged = true;
		while (bChanged)
		{
			bChanged = false;
			for (int32 EdgeIndex : Nodes[JunctionNode].EdgeIndices)
			{
				if (!Edges.IsValidIndex(EdgeIndex) || !Edges[EdgeIndex].bValid) continue;
				const FRoadEdge& Edge = Edges[EdgeIndex];
				const int32 FarNode = (Edge.StartNodeIndex == JunctionNode) ? Edge.EndNodeIndex : Edge.StartNodeIndex;
				const double Dist = FVector::Dist(Location, Nodes[FarNode].Location);

				UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("AbsorbDanglingStubs: Junction %d arm Edge %d reaches Node %d (Degree %d, Dist %f)"),
					JunctionNode, EdgeIndex, FarNode, Nodes[FarNode].EdgeIndices.Num(), Dist);

				if (Dist >= IntersectionClearRadius) continue; // arm already clears the intersection

				if (Nodes[FarNode].EdgeIndices.Num() == 1)
				{
					UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("AbsorbDanglingStubs: Deleting stub Edge %d (Node %d dead-ends within IntersectionClearRadius)"), EdgeIndex, FarNode);
					OnEdgeRemoved(EdgeIndex);
					DeleteEdge(EdgeIndex);
					bChanged = true;
					bAnyChanged = true;
					break;
				}
				else if (Nodes[FarNode].EdgeIndices.Num() == 2)
				{
					UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("AbsorbDanglingStubs: Absorbing pass-through Node %d into Junction %d (within IntersectionClearRadius)"), FarNode, JunctionNode);
					MergeNodeInto(FarNode, JunctionNode, OnEdgeRemoved);
					bChanged = true;
					bAnyChanged = true;
					break;
				}
				// else FarNode is itself another junction
			}
		}
	}

	return bAnyChanged;
}

TArray<int32> FRoadGraph::AddNodedChain(
	const TArray<FVector>& Points, const TArray<FVector>& Tangents, bool bClosedLoop,
	TArray<int32>& OutTouchedNodeIndices, FOnEdgeSplit OnEdgeSplit, FOnEdgeRemoved OnEdgeRemoved)
{
	OutTouchedNodeIndices.Reset();
	if (Points.Num() < 2 || Points.Num() != Tangents.Num()) return {};

	const int32 ChainId = NextChainId++;
	TArray<int32> ChainNodes;
	double ArcLength = 0.0;
	for (int32 i = 0; i < Points.Num(); i++)
	{
		const int32 NodeIndex = AddNode(Points[i]);
		if (i > 0) ArcLength += FVector::Dist(Points[i - 1], Points[i]);
		Nodes[NodeIndex].ChainSamples.Add({ ChainId, ArcLength });
		ChainNodes.Add(NodeIndex);
	}

	TSet<int32> ChainSet;
	const int32 NumSegments = bClosedLoop ? Points.Num() : Points.Num() - 1;

	for (int32 i = 0; i < NumSegments; i++)
	{
		const int32 NextIdx = (i + 1) % Points.Num();
		const int32 EdgeIndex = AddEdge(ChainNodes[i], ChainNodes[NextIdx], Tangents[i], Tangents[NextIdx]);
		if (EdgeIndex != INDEX_NONE) ChainSet.Add(EdgeIndex);
	}

	OutTouchedNodeIndices.Append(ChainNodes);

	// Noding
	ResolveCrossings(ChainSet, OutTouchedNodeIndices, OnEdgeSplit, OnEdgeRemoved);

	// Endpoint snapping
	TArray<int32> DangleCandidates;
	if (!bClosedLoop && ChainNodes.Num() > 0)
	{
		DangleCandidates.Add(ChainNodes[0]);
		DangleCandidates.Add(ChainNodes.Last());
	}

	for (int32 NodeIndex : OutTouchedNodeIndices)
	{
		if (!Nodes.IsValidIndex(NodeIndex) || !Nodes[NodeIndex].bValid) continue;
		for (int32 Nearby : QueryNodesNear(Nodes[NodeIndex].Location, JoinDistance))
		{
			if (Nodes.IsValidIndex(Nearby) && Nodes[Nearby].bValid && Nodes[Nearby].EdgeIndices.Num() == 1)
			{
				DangleCandidates.AddUnique(Nearby);
			}
		}
	}

	// Collect pre-existing dangling ends lying near the segments of this chain, so that
	// a dead-end from an earlier chain that touches the middle of one of our edges is
	// considered for TrySnapToSegment.
	for (int32 EdgeIndex : ChainSet.Array())
	{
		if (!Edges.IsValidIndex(EdgeIndex) || !Edges[EdgeIndex].bValid) continue;
		const FVector& SegA = Nodes[Edges[EdgeIndex].StartNodeIndex].Location;
		const FVector& SegB = Nodes[Edges[EdgeIndex].EndNodeIndex].Location;
		const FVector Margin(JoinDistance);
		for (int32 Nearby : QueryNodesInBounds(SegA.ComponentMin(SegB) - Margin, SegA.ComponentMax(SegB) + Margin))
		{
			if (Nodes[Nearby].EdgeIndices.Num() == 1)
			{
				DangleCandidates.AddUnique(Nearby);
			}
		}
	}

	SnapDanglingNodes(DangleCandidates, OutTouchedNodeIndices, OnEdgeSplit, OnEdgeRemoved, &ChainSet);

	// Junction clustering
	OutTouchedNodeIndices = ClusterNearbyJunctions(OutTouchedNodeIndices, OnEdgeRemoved);

	// Dangle absorption
	TArray<int32> JunctionSeeds;
	for (int32 NodeIndex : OutTouchedNodeIndices)
		if (IsJunctionNode(NodeIndex)) JunctionSeeds.Add(NodeIndex);

	while (true)
	{
		const bool bChanged = AbsorbDanglingStubs(JunctionSeeds, OnEdgeRemoved);

		TArray<int32> Expanded = JunctionSeeds;
		for (int32 NodeIndex : JunctionSeeds)
		{
			if (!Nodes.IsValidIndex(NodeIndex) || !Nodes[NodeIndex].bValid) continue;
			for (int32 EdgeIndex : Nodes[NodeIndex].EdgeIndices)
			{
				if (!Edges.IsValidIndex(EdgeIndex) || !Edges[EdgeIndex].bValid) continue;
				const FRoadEdge& Edge = Edges[EdgeIndex];
				const int32 Other = (Edge.StartNodeIndex == NodeIndex) ? Edge.EndNodeIndex : Edge.StartNodeIndex;
				if (Nodes.IsValidIndex(Other) && Nodes[Other].bValid && IsJunctionNode(Other))
					Expanded.AddUnique(Other);
			}
		}

		const bool bGrew = Expanded.Num() != JunctionSeeds.Num();
		JunctionSeeds = MoveTemp(Expanded);

		if (!bChanged && !bGrew) break;
	}

	// AbsorbDanglingStubs can merge this chain's nodes into a different (untouched) junction
	for (int32 NodeIndex : JunctionSeeds)
		if (Nodes.IsValidIndex(NodeIndex) && Nodes[NodeIndex].bValid)
			OutTouchedNodeIndices.AddUnique(NodeIndex);

	// Any junction on the far side of an edge needs a remesh too
	TArray<int32> ExpandedTouched = OutTouchedNodeIndices;
	for (int32 NodeIndex : OutTouchedNodeIndices)
	{
		if (!Nodes.IsValidIndex(NodeIndex)) continue;
		for (int32 EdgeIndex : Nodes[NodeIndex].EdgeIndices)
		{
			if (!Edges.IsValidIndex(EdgeIndex) || !Edges[EdgeIndex].bValid) continue;
			const FRoadEdge& Edge = Edges[EdgeIndex];
			const int32 Other = (Edge.StartNodeIndex == NodeIndex) ? Edge.EndNodeIndex : Edge.StartNodeIndex;
			if (Nodes.IsValidIndex(Other) && Nodes[Other].bValid && IsJunctionNode(Other))
				ExpandedTouched.AddUnique(Other);
		}
	}
	OutTouchedNodeIndices = ExpandedTouched;

	return ChainSet.Array();
}
