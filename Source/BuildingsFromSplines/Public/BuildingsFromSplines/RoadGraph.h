#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "Templates/Function.h"

class USplineMeshComponent;

struct FChainSample
{
	int32 ChainId = INDEX_NONE;
	double ArcLength = 0.0;
};

struct FRoadNode
{
	FVector Location = FVector::ZeroVector;
	TArray<int32> EdgeIndices;
	bool bValid = true;

	// Position(s) along the original spline chain(s) this node came from.
	// Usually one entry; can pick up more via MergeNodeInto when chains join.
	TArray<FChainSample> ChainSamples;
};

struct FRoadEdge
{
	int32 StartNodeIndex = INDEX_NONE;
	int32 EndNodeIndex = INDEX_NONE;
	FVector StartTangentWorld = FVector::ZeroVector;
	FVector EndTangentWorld = FVector::ZeroVector;
	bool bValid = true;
};

struct FRoadGraph
{
	TArray<FRoadNode> Nodes;
	TArray<FRoadEdge> Edges;

	const int32 SpatialHashCellSize = 200;
	TMap<FIntVector, TArray<int32>> NodeSpatialHash;
	TMap<FIntVector, TArray<int32>> EdgeSpatialHash;

	double JoinDistance = 200.0;
	double IntersectionClearRadius = 400.0;
	int32 NextChainId = 0;

	void Reset();
	bool IsJunctionNode(int32 NodeIndex) const;
	FIntVector GetSpatialHashCell(const FVector& Location) const;

	int32 AddNode(const FVector& Location);
	int32 AddEdge(int32 StartNodeIndex, int32 EndNodeIndex, const FVector& StartTangent, const FVector& EndTangent);
	void DeleteEdge(int32 EdgeIndex);
	void RemoveEdgeFromNode(int32 NodeIndex, int32 EdgeIndex);
	void AddEdgeToSpatialHash(int32 EdgeIndex);
	void RemoveEdgeFromSpatialHash(int32 EdgeIndex);

	TArray<int32> QueryNodesNear(const FVector& Location, double Radius) const;
	TArray<int32> QueryNodesInBounds(const FVector& Min, const FVector& Max) const;
	TArray<int32> QueryEdgesNear(const FVector& Location, double Radius) const;
	TArray<int32> QueryEdgesNearSegment(const FVector& A, const FVector& B, double Radius) const;
	TArray<int32> QueryEdgesInBounds(const FVector& Min, const FVector& Max) const;

	using FOnEdgeSplit = TFunctionRef<void(int32 NewEdgeIndex, int32 SourceEdgeIndex)>;
	using FOnEdgeRemoved = TFunctionRef<void(int32 RemovedEdgeIndex)>;

	TArray<int32> AddNodedChain(
		const TArray<FVector>& Points,
		const TArray<FVector>& Tangents,
		bool bClosedLoop,
		TArray<int32>& OutTouchedNodeIndices,
		FOnEdgeSplit OnEdgeSplit,
		FOnEdgeRemoved OnEdgeRemoved);

protected:
	void InvalidateNode(int32 NodeIndex);
	void MergeNodeInto(int32 NodeToRemove, int32 NodeToKeep, FOnEdgeRemoved OnEdgeRemoved);
	void ResolveCrossings(TSet<int32>& ChainSet, TArray<int32>& OutTouchedNodeIndices, FOnEdgeSplit OnEdgeSplit, FOnEdgeRemoved OnEdgeRemoved);
	void SnapDanglingNodes(const TArray<int32>& CandidateNodes, TArray<int32>& OutTouchedNodeIndices, FOnEdgeSplit OnEdgeSplit, FOnEdgeRemoved OnEdgeRemoved, TSet<int32>* ChainSet);
	bool TrySnapToNode(int32 NodeIndex, TArray<int32>& OutTouchedNodeIndices, FOnEdgeRemoved OnEdgeRemoved);
	bool TrySnapToSegment(int32 NodeIndex, TArray<int32>& OutTouchedNodeIndices, FOnEdgeSplit OnEdgeSplit, FOnEdgeRemoved OnEdgeRemoved, TSet<int32>* ChainSet);
	TArray<int32> ClusterNearbyJunctions(const TArray<int32>& Seeds, FOnEdgeRemoved OnEdgeRemoved);

	void SplitEdgeAt(int32 EdgeIndex, int32 AtNode, int32& OutLeft, int32& OutRight, FOnEdgeSplit OnEdgeSplit, TSet<int32>* ChainSet);
	bool EdgesShareNode(int32 EdgeA, int32 EdgeB) const;
	bool IsSameChainWithinDistance(int32 NodeA, int32 NodeB, double MaxDistance) const;
	bool FindNearestCrossing(int32 EdgeA, int32& OutEdgeB, FVector& OutLocation) const;

	bool AbsorbDanglingStubs(const TArray<int32>& JunctionSeeds, FOnEdgeRemoved OnEdgeRemoved);
};
