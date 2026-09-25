// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "BuildingsFromSplines/RoadsFromSplines.h"

#include "LCCommon/LCBlueprintLibrary.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"
#include "LandscapeUtils/LandscapeUtils.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/DynamicMeshComponent.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshUVFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "Algo/Reverse.h"

#include "BuildingsFromSplines/Building.h"
#include "Components/SplineMeshComponent.h"
#include "Logging/StructuredLog.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Materials/MaterialLayersFunctions.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RoadsFromSplines)

#define LOCTEXT_NAMESPACE "FBuildingsFromSplinesModule"

namespace
{
	void GetSortedJunctionArms(const FRoadGraph& RoadGraph, const TMap<int32, FRoadEdgeSplineData>& EdgeSplineData,
		int32 JunctionNode, TArray<TPair<int32, FVector>>& OutArms, TArray<FVector>& OutTangents)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

		const FVector Center = RoadGraph.Nodes[JunctionNode].Location;
		TArray<TPair<int32, FVector>> Arms;
		TArray<FVector> Tangents;
		for (int32 EdgeIndex : RoadGraph.Nodes[JunctionNode].EdgeIndices)
		{
			if (!RoadGraph.Edges.IsValidIndex(EdgeIndex) || !RoadGraph.Edges[EdgeIndex].bValid) continue;
			const FRoadEdge& Edge = RoadGraph.Edges[EdgeIndex];
			const bool bStartIsJunction = (Edge.StartNodeIndex == JunctionNode);
			const int32 FarNode = bStartIsJunction ? Edge.EndNodeIndex : Edge.StartNodeIndex;
			const FVector Dir = (RoadGraph.Nodes[FarNode].Location - Center).GetSafeNormal();

			const FRoadEdgeSplineData* Data = EdgeSplineData.Find(EdgeIndex);

			const FVector FarTangentAway = bStartIsJunction
				? (Data ? Data->BlendedEndTangent.Get(Edge.EndTangentWorld) : Edge.EndTangentWorld)
				: -(Data ? Data->BlendedStartTangent.Get(Edge.StartTangentWorld) : Edge.StartTangentWorld);

			Arms.Add(TPair<int32, FVector>(EdgeIndex, Dir));
			Tangents.Add(FarTangentAway);
		}

		TArray<int32> Order;
		for (int32 i = 0; i < Arms.Num(); i++) Order.Add(i);
		Order.Sort([&](int32 A, int32 B) {
			return FMath::Atan2(Arms[A].Value.Y, Arms[A].Value.X) < FMath::Atan2(Arms[B].Value.Y, Arms[B].Value.X);
		});

		for (int32 i : Order) { OutArms.Add(Arms[i]); OutTangents.Add(Tangents[i]); }
	}
}

ARoadsFromSplines::ARoadsFromSplines()
{
	PrimaryActorTick.bCanEverTick = false;

	EmptySceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("EmptySceneComponent"));
	EmptySceneComponent->SetMobility(EComponentMobility::Static);
	RootComponent = EmptySceneComponent;

	Tags.AddUnique("can-push-buildings");
}

void ARoadsFromSplines::CreateStraightJunctionMeshes(int32 JunctionNode)
{
    if (!IsValid(IntersectionMesh)) return;
    if (!RoadGraph.Nodes.IsValidIndex(JunctionNode) || !RoadGraph.IsJunctionNode(JunctionNode)) return;

    const FVector Center = RoadGraph.Nodes[JunctionNode].Location;
    const FTransform& ActorTransform = GetActorTransform();

    TArray<int32> ValidEdgeIndices;
    for (int32 EdgeIndex : RoadGraph.Nodes[JunctionNode].EdgeIndices)
        if (RoadGraph.Edges.IsValidIndex(EdgeIndex) && RoadGraph.Edges[EdgeIndex].bValid)
            ValidEdgeIndices.Add(EdgeIndex);

    const int32 NumArms = ValidEdgeIndices.Num();
    TArray<FEdgeSplinePoints> ArmPoints;
    ArmPoints.Reserve(NumArms);

    for (int32 StubIndex = 0; StubIndex < NumArms; StubIndex++)
    {
        const int32 EdgeIndex = ValidEdgeIndices[StubIndex];
        const FRoadEdge& Edge = RoadGraph.Edges[EdgeIndex];
        const bool bStartIsJunction = (Edge.StartNodeIndex == JunctionNode);
        const int32 FarNode = bStartIsJunction ? Edge.EndNodeIndex : Edge.StartNodeIndex;

        FRoadEdgeSplineData* Data = EdgeSplineData.Find(EdgeIndex);
        const FVector StartTangent = Data ? Data->BlendedStartTangent.Get(Edge.StartTangentWorld) : Edge.StartTangentWorld;
        const FVector EndTangent   = Data ? Data->BlendedEndTangent.Get(Edge.EndTangentWorld) : Edge.EndTangentWorld;
        const FVector NearTangent = bStartIsJunction ? StartTangent : -EndTangent;
        const FVector FarTangent  = bStartIsJunction ? EndTangent : -StartTangent;
        const float CenterZ = ZOffset + (StubIndex - NumArms / 2.f) * IntersectionStaggerZ;

        FEdgeSplinePoints Points;
        Points.StartLocal = ActorTransform.InverseTransformPosition(Center) + FVector(0, 0, CenterZ);
        Points.EndLocal   = ActorTransform.InverseTransformPosition(RoadGraph.Nodes[FarNode].Location) + FVector(0, 0, ZOffset);
        Points.StartTangentLocal = ActorTransform.InverseTransformVector(NearTangent);
        Points.EndTangentLocal   = ActorTransform.InverseTransformVector(FarTangent);
        ArmPoints.Add(Points);
    }

    TWeakObjectPtr<ARoadsFromSplines> WeakThis(this);
    Concurrency::RunOnGameThreadThrottled([WeakThis, ArmPoints = MoveTemp(ArmPoints), JunctionNode]() {
        if (!WeakThis.IsValid() || !IsValid(WeakThis->GetWorld())) return;
        if (!WeakThis->RoadGraph.Nodes.IsValidIndex(JunctionNode) || !WeakThis->RoadGraph.Nodes[JunctionNode].bValid) return;

        WeakThis->DestroyJunctionMeshes(JunctionNode);

        for (const FEdgeSplinePoints& Points : ArmPoints)
        {
            USplineMeshComponent* Mesh = WeakThis->SpawnSplineMeshComponent(
                Points, WeakThis->IntersectionMesh, WeakThis->DefaultIntersectionMeshLength);
            if (!IsValid(Mesh)) continue;

            if (!WeakThis->RoadGraph.Nodes.IsValidIndex(JunctionNode) || !WeakThis->RoadGraph.Nodes[JunctionNode].bValid)
            {
                Mesh->DestroyComponent();
                continue;
            }
            WeakThis->CornerMeshComponents.Add(Mesh);
            WeakThis->JunctionMeshComponents.FindOrAdd(JunctionNode).Add(Mesh);
        }
    });
}

void ARoadsFromSplines::CreateCurvyJunctionMeshes(int32 JunctionNode)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	if (!IsValid(IntersectionMesh)) return;
	if (!RoadGraph.Nodes.IsValidIndex(JunctionNode) || !RoadGraph.IsJunctionNode(JunctionNode)) return;

	TArray<TPair<int32, FVector>> Arms;
	TArray<FVector> Tangents;
	GetSortedJunctionArms(RoadGraph, EdgeSplineData, JunctionNode, Arms, Tangents);
	if (Arms.Num() < 2) return;

	for (int32 i = 0; i < Arms.Num(); i++)
		UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("CreateCurvyJunctionMeshes: Junction %d Arm %d -> Edge %d, Dir=%s, Angle=%.2f deg"),
			JunctionNode, i, Arms[i].Key, *Arms[i].Value.ToString(), FMath::RadiansToDegrees(FMath::Atan2(Arms[i].Value.Y, Arms[i].Value.X)));

	const FVector Center = RoadGraph.Nodes[JunctionNode].Location;
	const FTransform& ActorTransform = GetActorTransform();
	const int32 NumArms = Arms.Num();

	auto ArmEndpoint = [&](int32 ArmIdx, FVector& OutTangentDir) -> FVector
	{
		const FRoadEdge& Edge = RoadGraph.Edges[Arms[ArmIdx].Key];
		const int32 FarNode = (Edge.StartNodeIndex == JunctionNode) ? Edge.EndNodeIndex : Edge.StartNodeIndex;
		const FVector FarLocation = RoadGraph.Nodes[FarNode].Location;

		if (RoadGraph.IsJunctionNode(FarNode))
		{
			OutTangentDir = (FarLocation - Center).GetSafeNormal();
			return FMath::Lerp(Center, FarLocation, 0.5);
		}

		OutTangentDir = Tangents[ArmIdx].GetSafeNormal();
		return FarLocation;
	};

	struct FCornerData
	{
		bool bSkipped = false;
		FEdgeSplinePoints Points;
		FVector StartWorld = FVector::ZeroVector;
		FVector EndWorld = FVector::ZeroVector;
	};
	TArray<FCornerData> Corners;

	for (int32 i = 0; i < NumArms; i++)
	{
		const int32 j = (i + 1) % NumArms;

		const double CosAngle = FVector::DotProduct(Arms[i].Value, Arms[j].Value);
		const double AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosAngle, -1.0, 1.0)));

		FCornerData Corner;

		if (AngleDegrees < MinCornerAngleDegrees)
		{
			UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("CreateCurvyJunctionMeshes: skipping fillet between arm %d and %d (angle %.1f < %.1f)"),
				i, j, AngleDegrees, MinCornerAngleDegrees);
			FVector UnusedA, UnusedB;
			Corner.bSkipped = true;

			const FVector RawStart = ArmEndpoint(i, UnusedA);
			const FVector RawEnd   = ArmEndpoint(j, UnusedB);

			auto OffsetTowards = [&](const FVector& Point, const FVector& Dir, const FVector& Other) -> FVector
			{
				const FVector Perp = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal() * (JunctionCurbOffset * WidthScale);
				const FVector CandidateA = Point + Perp;
				const FVector CandidateB = Point - Perp;
				return (FVector::DistSquared(CandidateA, Other) < FVector::DistSquared(CandidateB, Other)) ? CandidateA : CandidateB;
			};

			Corner.StartWorld = OffsetTowards(RawStart, Arms[i].Value, RawEnd);
			Corner.EndWorld = OffsetTowards(RawEnd, Arms[j].Value, RawStart);

			UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("CreateCurvyJunctionMeshes: skipping fillet %d->%d (angle %.1f < %.1f); curb-offset points StartWorld=%s EndWorld=%s"),
				i, j, AngleDegrees, MinCornerAngleDegrees, *Corner.StartWorld.ToString(), *Corner.EndWorld.ToString());

			Corners.Add(Corner);
			continue;
		}

		FVector StartDir, EndDir;
		const FVector StartWorld = ArmEndpoint(i, StartDir);
		const FVector EndWorld   = ArmEndpoint(j, EndDir);

		UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("CreateCurvyJunctionMeshes: corner %d->%d (angle %.1f) StartWorld=%s EndWorld=%s"),
			i, j, AngleDegrees, *StartWorld.ToString(), *EndWorld.ToString());

		const double ChordDist = FVector::Dist(StartWorld, EndWorld);
		const FVector StartTangentWorld = -StartDir * ChordDist * 0.5 * CornerStraightness;
		const FVector EndTangentWorld   =  EndDir   * ChordDist * 0.5 * CornerStraightness;

		const float StaggerZ = ZOffset + (i - NumArms / 2.f) * IntersectionStaggerZ;

		Corner.StartWorld = StartWorld;
		Corner.EndWorld = EndWorld;
		Corner.Points.StartLocal = ActorTransform.InverseTransformPosition(StartWorld) + FVector(0, 0, StaggerZ);
		Corner.Points.EndLocal   = ActorTransform.InverseTransformPosition(EndWorld)   + FVector(0, 0, StaggerZ);
		Corner.Points.StartTangentLocal = ActorTransform.InverseTransformVector(StartTangentWorld);
		Corner.Points.EndTangentLocal   = ActorTransform.InverseTransformVector(EndTangentWorld);
		Corners.Add(Corner);
	}

	TWeakObjectPtr<ARoadsFromSplines> WeakThis(this);
	Concurrency::RunOnGameThreadThrottled([WeakThis, Corners = MoveTemp(Corners), JunctionNode]() {
		if (!WeakThis.IsValid() || !IsValid(WeakThis->GetWorld())) return;
		if (!WeakThis->RoadGraph.Nodes.IsValidIndex(JunctionNode) || !WeakThis->RoadGraph.Nodes[JunctionNode].bValid) return;

		WeakThis->DestroyJunctionMeshes(JunctionNode);

		const FVector JunctionCenter = WeakThis->RoadGraph.Nodes[JunctionNode].Location;
		TArray<FVector> BoundaryWorld;

		for (const FCornerData& Corner : Corners)
		{
			if (Corner.bSkipped)
			{
				BoundaryWorld.Add(Corner.StartWorld);
				BoundaryWorld.Add(Corner.EndWorld);
				UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("FillJunctionInterior: Junction %d boundary += SKIPPED corner raw points %s, %s (now %d pts)"),
					JunctionNode, *Corner.StartWorld.ToString(), *Corner.EndWorld.ToString(), BoundaryWorld.Num());
				continue;
			}

			USplineMeshComponent* Mesh = WeakThis->SpawnSplineMeshComponent(
				Corner.Points, WeakThis->IntersectionMesh, WeakThis->DefaultIntersectionMeshLength);
			if (!IsValid(Mesh)) continue;

			if (!WeakThis->RoadGraph.Nodes.IsValidIndex(JunctionNode) || !WeakThis->RoadGraph.Nodes[JunctionNode].bValid)
			{
				Mesh->DestroyComponent();
				continue;
			}

			WeakThis->CornerMeshComponents.Add(Mesh);
			WeakThis->JunctionMeshComponents.FindOrAdd(JunctionNode).Add(Mesh);

			WeakThis->SampleFilletInnerEdge(Mesh, JunctionCenter, BoundaryWorld);
			UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("FillJunctionInterior: Junction %d boundary now has %d points after filled corner"), JunctionNode, BoundaryWorld.Num());
		}

		if (WeakThis->bFillJunctions)
			WeakThis->FillJunctionInterior(JunctionNode, MoveTemp(BoundaryWorld));
	});
}

void ARoadsFromSplines::SampleFilletInnerEdge(USplineMeshComponent* Mesh, const FVector& JunctionCenter, TArray<FVector>& OutPoints) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	if (!IsValid(Mesh) || !IsValid(IntersectionMesh)) return;

	const int32 FwdAxis   = (RoadMeshAxis == ESplineMeshAxis::X) ? 0 : (RoadMeshAxis == ESplineMeshAxis::Y) ? 1 : 2;
	const int32 WidthAxis = (RoadMeshAxis == ESplineMeshAxis::X) ? 1 : (RoadMeshAxis == ESplineMeshAxis::Y) ? 2 : 0;
	static const EAxis::Type AxisEnum[3] = { EAxis::X, EAxis::Y, EAxis::Z };

	const FBox Bounds = IntersectionMesh->GetBoundingBox();
	const double MinDist = Bounds.Min[FwdAxis];
	const double MaxDist = Bounds.Max[FwdAxis];
	const double MinSide = Bounds.Min[WidthAxis];
	const double MaxSide = Bounds.Max[WidthAxis];

	const FTransform FirstSlice = Mesh->CalcSliceTransform(MinDist) * Mesh->GetComponentTransform();
	const FVector Forward = FirstSlice.GetUnitAxis(AxisEnum[FwdAxis]);
	const FVector Left = FVector::UpVector ^ Forward;

	FVector LocalMinProbe = FVector::ZeroVector; LocalMinProbe[WidthAxis] = MinSide;

	const bool bMinIsInner = (RoadMeshAxis != ESplineMeshAxis::X);

	for (int32 s = 0; s <= InnerFillingNumSamples; s++)
	{
		const double Distance = FMath::Lerp(MinDist, MaxDist, (double)s / InnerFillingNumSamples);
		const FTransform Slice = Mesh->CalcSliceTransform(Distance) * Mesh->GetComponentTransform();

		FVector Local = FVector::ZeroVector;
		Local[WidthAxis] = bMinIsInner ? MinSide : MaxSide;
		OutPoints.Add(Slice.TransformPosition(Local));
	}
}

void ARoadsFromSplines::FillJunctionInterior(int32 JunctionNode, TArray<FVector> BoundaryWorld)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("FillJunctionInterior: Junction %d raw boundary (%d points):"), JunctionNode, BoundaryWorld.Num());
	for (int32 i = 0; i < BoundaryWorld.Num(); i++)
		UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("  [%d] %s"), i, *BoundaryWorld[i].ToString());

	for (int32 i = BoundaryWorld.Num() - 1; i > 0; i--)
		if (FVector::DistSquared(BoundaryWorld[i], BoundaryWorld[i - 1]) < 1.0)
		{
			UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("FillJunctionInterior: Junction %d dropping near-dup [%d]=%s (dist to [%d]=%.3f)"),
				JunctionNode, i, *BoundaryWorld[i].ToString(), i - 1, FVector::Dist(BoundaryWorld[i], BoundaryWorld[i - 1]));
			BoundaryWorld.RemoveAt(i);
		}
	if (BoundaryWorld.Num() >= 2 && FVector::DistSquared(BoundaryWorld[0], BoundaryWorld.Last()) < 1.0)
		BoundaryWorld.RemoveAt(BoundaryWorld.Num() - 1);

	UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("FillJunctionInterior: Junction %d boundary after dedup (%d points)"), JunctionNode, BoundaryWorld.Num());

	if (BoundaryWorld.Num() < 3)
	{
		UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("FillJunctionInterior: Junction %d ABORTING, fewer than 3 boundary points"), JunctionNode);
		return;
	}

	const FTransform& ActorTransform = GetActorTransform();

	TArray<FVector2D> Boundary2D;
	TMap<FVector2D, double> HeightOffsets;
	double MinZ = MAX_dbl;

	for (const FVector& World : BoundaryWorld)
		MinZ = FMath::Min(MinZ, ActorTransform.InverseTransformPosition(World).Z);

	for (const FVector& World : BoundaryWorld)
	{
		const FVector Local = ActorTransform.InverseTransformPosition(World);
		const FVector2D Point2D(Local.X, Local.Y);
		Boundary2D.Add(Point2D);
		HeightOffsets.Add(Point2D, Local.Z - MinZ);
	}

	double SignedArea = 0.0;
	for (int32 i = 0; i < Boundary2D.Num(); i++)
	{
		const FVector2D& A = Boundary2D[i];
		const FVector2D& B = Boundary2D[(i + 1) % Boundary2D.Num()];
		SignedArea += A.X * B.Y - B.X * A.Y;
	}
	UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("FillJunctionInterior: Junction %d SignedArea=%.2f (%s) MinZ=%.2f"),
		JunctionNode, SignedArea, SignedArea < 0 ? TEXT("REVERSING") : TEXT("kept as-is"), MinZ);
	if (SignedArea < 0) Algo::Reverse(Boundary2D);

	if (!JunctionFillMeshComponents.Contains(JunctionNode))
	{
		UDynamicMeshComponent* FillComponent = NewObject<UDynamicMeshComponent>(RootComponent);
		FillComponent->SetMobility(EComponentMobility::Static);
		FillComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		FillComponent->CreationMethod = EComponentCreationMethod::Instance;
		FillComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		FillComponent->SetCollisionProfileName(TEXT("BlockAll"));
		FillComponent->bEnableComplexCollision = true;
		FillComponent->SetComplexAsSimpleCollisionEnabled(true);
		FillComponent->RegisterComponent();
		AddInstanceComponent(FillComponent);
		JunctionFillMeshComponents.Add(JunctionNode, FillComponent);
	}

	UDynamicMeshComponent* FillComponent = JunctionFillMeshComponents[JunctionNode];
	UDynamicMesh* FillMesh = FillComponent->GetDynamicMesh();
	FillMesh->Reset();

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSimpleExtrudePolygon(
		FillMesh, FGeometryScriptPrimitiveOptions(), FTransform(FVector(0, 0, MinZ - JunctionFillHeight)), Boundary2D, JunctionFillHeight);

	FTransform ProjectionTransform = FTransform::Identity;
	ProjectionTransform.SetScale3D(FVector(DefaultRoadMeshLength * FillJunctionUVScale));
	UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromPlanarProjection(
		FillMesh, 0, ProjectionTransform, FGeometryScriptMeshSelection());

	UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(FillMesh, FGeometryScriptCalculateNormalsOptions());

	int32 NumMatched = 0, NumUnmatched = 0;
	for (int32 VID : FillMesh->GetMeshRef().VertexIndicesItr())
	{
		FVector V = FillMesh->GetMeshRef().GetVertex(VID);
		if (const double* Offset = HeightOffsets.Find(FVector2D(V.X, V.Y)))
		{
			V.Z += *Offset;
			FillMesh->GetMeshRef().SetVertex(VID, V);
			NumMatched++;
		}
		else
		{
			NumUnmatched++;
			UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("FillJunctionInterior: Junction %d vertex %d at (%.3f, %.3f) NO HeightOffset match -- left at MinZ"),
				JunctionNode, VID, V.X, V.Y);
		}
	}
	UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("FillJunctionInterior: Junction %d height pass: %d matched, %d unmatched of %d vertices"),
		JunctionNode, NumMatched, NumUnmatched, FillMesh->GetMeshRef().VertexCount());

	if (IsValid(JunctionFillMaterial))
		FillComponent->SetMaterial(0, JunctionFillMaterial);

	FillComponent->NotifyMeshUpdated();
	FillComponent->UpdateCollision(false);

	if (bShowDebugJunctionFill)
	{
		for (int32 i = 0; i < BoundaryWorld.Num(); i++)
			DrawDebugLine(GetWorld(), BoundaryWorld[i], BoundaryWorld[(i + 1) % BoundaryWorld.Num()], FColor::Orange, false, 20, 0, 6);
	}
}

void ARoadsFromSplines::CreateMeshesForJunctions(const TArray<int32>& TouchedNodeIndices)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	for (int32 NodeIndex : TouchedNodeIndices)
	{
		if (!RoadGraph.Nodes.IsValidIndex(NodeIndex) || !RoadGraph.Nodes[NodeIndex].bValid) continue;

		const bool bIsJunction = RoadGraph.IsJunctionNode(NodeIndex);
		UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("CreateMeshesForJunctions: Node %d IsJunction=%d Degree=%d"),
			NodeIndex, bIsJunction, RoadGraph.Nodes[NodeIndex].EdgeIndices.Num());

		if (!bIsJunction)
		{
			for (int32 EdgeIndex : RoadGraph.Nodes[NodeIndex].EdgeIndices)
			{
				if (EdgeTouchesJunction(EdgeIndex))
				{
					UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("  Node %d skip Edge %d (other end is a junction)"), NodeIndex, EdgeIndex);
					continue;
				}
				FRoadEdgeSplineData* Data = EdgeSplineData.Find(EdgeIndex);
				if (Data && !IsValid(Data->MeshComponent) && !Data->bMeshSpawnPending)
				{
					UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("  Node %d meshing plain Edge %d"), NodeIndex, EdgeIndex);
					CreateSplineMeshForEdge(EdgeIndex, ZOffset);
				}
			}
			continue;
		}

		if (!IsValid(IntersectionMesh))
		{
			UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("  Node %d is a junction, IntersectionMesh unset -> leaving unmeshed"), NodeIndex);
			continue;
		}

		if (JunctionFillMode == ERoadJunctionFillMode::CurvyCorners)
		{
			CreateCurvyJunctionMeshes(NodeIndex);
		}
		else
		{
			CreateStraightJunctionMeshes(NodeIndex);
		}
	}
}

bool ARoadsFromSplines::OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated)
{
	return GenerateRoads(bIsUserInitiated);
}

TArray<UObject*> ARoadsFromSplines::GetGeneratedObjects() const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);
	TArray<UObject*> GeneratedObjects;
	for (auto &SplineMeshComponent: SplineMeshComponents)
		if (IsValid(SplineMeshComponent)) GeneratedObjects.Add(SplineMeshComponent);
	for (auto &CornerMeshComponent: CornerMeshComponents)
		if (IsValid(CornerMeshComponent)) GeneratedObjects.Add(CornerMeshComponent);

	for (auto &[i, DynamicMeshComponent] : JunctionFillMeshComponents)
		GeneratedObjects.Add(DynamicMeshComponent);

	return GeneratedObjects;
}

bool ARoadsFromSplines::Cleanup_Implementation(bool bSkipPrompt)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	TWeakObjectPtr<ARoadsFromSplines> WeakThis(this);
	bool bDeleteFailed = false;

	bool bOk = Concurrency::RunOnGameThreadAndWait([&, WeakThis]() {
		if (!WeakThis.IsValid()) return false;

		WeakThis->Modify();

		if (!WeakThis->DeleteGeneratedObjects(bSkipPrompt))
		{
			bDeleteFailed = true;
			return false;
		}

		WeakThis->SplineMeshComponents.Empty();
		WeakThis->CornerMeshComponents.Empty();
		WeakThis->RoadGraph.Reset();
		WeakThis->SplineToEdgeIndices.Empty();
		WeakThis->EdgeSplineData.Empty();
		WeakThis->JunctionMeshComponents.Empty();
		WeakThis->JunctionFillMeshComponents.Empty();

		if (IsValid(WeakThis->GetWorld()))
			FlushPersistentDebugLines(WeakThis->GetWorld());

		return true;
	});

	if (!bOk && !bDeleteFailed)
		UE_LOG(LogBuildingsFromSplines, Warning, TEXT("Cleanup_Implementation: actor no longer valid before cleanup could run -- aborting"));

	return bOk;
}

void ARoadsFromSplines::Destroyed()
{
	Execute_Cleanup(this, true);
	Super::Destroyed();
}

void ARoadsFromSplines::ClearRoads()
{
	Execute_Cleanup(this, false);
}

bool ARoadsFromSplines::GenerateRoads(bool bIsUserInitiated)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	TWeakObjectPtr<ARoadsFromSplines> WeakThis(this);

	UWorld* World = GetWorld();
	TSet<TObjectPtr<USplineComponent>> FoundSplines;

	if (!Concurrency::RunOnGameThreadAndWait([&, WeakThis]() {
		if (!WeakThis.IsValid() || !IsValid(WeakThis->GetWorld())) return false;
		WeakThis->Modify();
		FoundSplines = ULCBlueprintLibrary::FindSplineComponents(World, bIsUserInitiated, SplinesTag, SplineComponentsTag);
		if (bAdaptSplineMeshRollToLandscape)
			return LandscapeUtils::CustomCollisionQueryParams(World, LandscapeSelection, LandscapeCollisionQueryParams);
		else
			return true;
	}))
	{
		return false;
	}

	if (bDeleteOldRoadsWhenCreatingRoads)
	{
		if (!Execute_Cleanup(this, !bIsUserInitiated)) return false;
	}

	RoadGraph.JoinDistance = JoinDistance;
	RoadGraph.IntersectionClearRadius = IntersectionSize;

	for (USplineComponent* SplineComponent : FoundSplines)
	{
		if (!IsValid(SplineComponent)) return false;
		if (SplineToEdgeIndices.Contains(SplineComponent)) continue;

		TArray<FVector> Points;
		TArray<FVector> Tangents;
		const bool bClosedLoop = SplineComponent->IsClosedLoop();

		if (bResampleSplines)
		{
			if (!BuildResampledPoints(SplineComponent, SplineMaxSpacing, Points, Tangents))
				continue;
		}
		else
		{
			const int32 NumPoints = SplineComponent->GetNumberOfSplinePoints();
			for (int32 i = 0; i < NumPoints; i++)
			{
				Points.Add(SplineComponent->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World));
				Tangents.Add(SplineComponent->GetTangentAtSplinePoint(i, ESplineCoordinateSpace::World));
			}
		}

		TArray<int32> NewEdgeIndices;
		const bool bBuilt = BuildEdgesForSpline(SplineComponent, Points, Tangents, bClosedLoop, NewEdgeIndices);

		if (bBuilt)
			CreateMeshesForNewEdges(NewEdgeIndices);
	}

	if (bShowDebugGraph) DebugDrawGraph();

	UE_LOG(LogBuildingsFromSplines, Log, TEXT("Road graph now has %d nodes and %d edges"), RoadGraph.Nodes.Num(), RoadGraph.Edges.Num());

	return true;
}

bool ARoadsFromSplines::EdgeTouchesJunction(int32 EdgeIndex) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);
	
	if (!RoadGraph.Edges.IsValidIndex(EdgeIndex)) return false;
	const FRoadEdge& Edge = RoadGraph.Edges[EdgeIndex];
	return RoadGraph.IsJunctionNode(Edge.StartNodeIndex) || RoadGraph.IsJunctionNode(Edge.EndNodeIndex);
}

void ARoadsFromSplines::CreateMeshesForNewEdges(const TArray<int32>& EdgeIndices)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	for (int32 EdgeIndex : EdgeIndices)
	{
		if (!RoadGraph.Edges.IsValidIndex(EdgeIndex) || !RoadGraph.Edges[EdgeIndex].bValid) continue;

		if (EdgeTouchesJunction(EdgeIndex))
		{
			UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("CreateMeshesForNewEdges: skip Edge %d (touches junction)"), EdgeIndex);
			continue;
		}

		FRoadEdgeSplineData* Data = EdgeSplineData.Find(EdgeIndex);
		if (Data && (IsValid(Data->MeshComponent) || Data->bMeshSpawnPending))
		{
			UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("CreateMeshesForNewEdges: Edge %d already meshed"), EdgeIndex);
			continue;
		}

		UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("CreateMeshesForNewEdges: meshing Edge %d"), EdgeIndex);
		CreateSplineMeshForEdge(EdgeIndex, ZOffset);
	}
}

USplineMeshComponent* ARoadsFromSplines::SpawnSplineMeshComponent(const FEdgeSplinePoints& Points, UStaticMesh* Mesh, const float DefaultMeshLength)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	if (!IsValid(RootComponent)) return nullptr;
	if (!IsValid(Mesh) || WidthScale == 0 || HeightScale == 0) return nullptr;

	USplineMeshComponent* MeshComponent = NewObject<USplineMeshComponent>(RootComponent);
	if (!IsValid(MeshComponent)) return nullptr;

	MeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	MeshComponent->SetStaticMesh(Mesh);
	MeshComponent->SetForwardAxis(RoadMeshAxis, false);
	MeshComponent->SetStartAndEnd(Points.StartLocal, Points.StartTangentLocal, Points.EndLocal, Points.EndTangentLocal, false);
	ApplyLandscapeRoll(MeshComponent, Points);
	MeshComponent->SetStartScale(FVector2D(WidthScale, HeightScale), false);
	MeshComponent->SetEndScale(FVector2D(WidthScale, HeightScale), false);
	MeshComponent->SetCollisionProfileName("BlockAll");
	MeshComponent->SetGenerateOverlapEvents(true);
	MeshComponent->MarkRenderStateDirty();
	MeshComponent->CreationMethod = EComponentCreationMethod::Instance;
	MeshComponent->RegisterComponent();
	AddInstanceComponent(MeshComponent);

	if (UWorld* World = MeshComponent->GetWorld())
	{
		TWeakObjectPtr<USplineMeshComponent> WeakMesh(MeshComponent);
		World->GetTimerManager().SetTimerForNextTick([WeakMesh]() {
			if (WeakMesh.IsValid()) WeakMesh->UpdateOverlaps();
		});
	}
	
	if (DefaultMeshLength > KINDA_SMALL_NUMBER)
	{
		const float EdgeLength = FVector::Dist(Points.StartLocal, Points.EndLocal);
		const float Tiling = EdgeLength / DefaultMeshLength;

		if (UMaterialInstanceDynamic* DynMaterial = MeshComponent->CreateAndSetMaterialInstanceDynamic(0))
		{
			auto SetEverywhere = [&](FName ParamName, float Value)
			{
				if (ParamName.IsNone()) return;

				DynMaterial->SetScalarParameterValue(ParamName, Value);

				// Also set the param value inside material layers
				FMaterialLayersFunctions MaterialLayers;
				if (DynMaterial->GetMaterialLayers(MaterialLayers))
				{
					for (int32 LayerIndex = 0; LayerIndex < MaterialLayers.Layers.Num(); LayerIndex++)
					{
						DynMaterial->SetScalarParameterValueByInfo(
							FMaterialParameterInfo(ParamName, EMaterialParameterAssociation::LayerParameter, LayerIndex),
							Value);
					}
				}
			};

			SetEverywhere(UVTilingParameterName, Tiling);
			SetEverywhere(TilingOffsetParameterName, Points.TilingOffset);
		}
	}

	if (bPushBuildingsOnSpawn)
	{
		TWeakObjectPtr<ARoadsFromSplines> WeakThis(this);
		TWeakObjectPtr<USplineMeshComponent> WeakMesh(MeshComponent);
		Concurrency::RunOnGameThreadThrottled([WeakThis, WeakMesh]() {
			if (WeakThis.IsValid() && WeakMesh.IsValid())
				WeakThis->PushOverlappingBuildings(WeakMesh.Get());
		});
	}

	return MeshComponent;
}

USplineMeshComponent* ARoadsFromSplines::DetachEdgeMesh(int32 EdgeIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	FRoadEdgeSplineData* Data = EdgeSplineData.Find(EdgeIndex);
	if (!Data || !IsValid(Data->MeshComponent)) return nullptr;

	USplineMeshComponent* Mesh = Data->MeshComponent;
	Data->MeshComponent = nullptr;

	SplineMeshComponents.Remove(Mesh);
	return Mesh;
}

void ARoadsFromSplines::DestroyEdgeMesh(int32 EdgeIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	if (USplineMeshComponent* Mesh = DetachEdgeMesh(EdgeIndex))
	{
		TWeakObjectPtr<ARoadsFromSplines> WeakThis(this);
		Concurrency::RunOnGameThreadThrottled([WeakThis, Mesh]() {
			if (!WeakThis.IsValid() || !IsValid(WeakThis->GetWorld())) return;
			if (!IsValid(Mesh)) return;
			Mesh->DestroyComponent();
		});
	}
}

bool ARoadsFromSplines::CreateSplineMeshForEdge(int32 EdgeIndex, float ZStagger)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);
	
	FRoadEdgeSplineData* Data = EdgeSplineData.Find(EdgeIndex);
	if (!Data || !IsValid(RoadMesh)) return false;
	if (!RoadGraph.Edges.IsValidIndex(EdgeIndex) || !RoadGraph.Edges[EdgeIndex].bValid) return false;

	const FRoadEdge& Edge = RoadGraph.Edges[EdgeIndex];
	const FVector StartWorld = RoadGraph.Nodes[Edge.StartNodeIndex].Location;
	const FVector EndWorld = RoadGraph.Nodes[Edge.EndNodeIndex].Location;
	const FVector StartTangent = Edge.StartTangentWorld;
	const FVector EndTangent = Edge.EndTangentWorld;

	const int32 CapturedStartNode = Edge.StartNodeIndex;
	const int32 CapturedEndNode = Edge.EndNodeIndex;
	
	UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("CreateSplineMeshForEdge: From %s to %s"), *StartWorld.ToString(), *EndWorld.ToString());

	const FTransform& ActorTransform = GetActorTransform();

	const double EdgeLength = FVector::Dist(StartWorld, EndWorld);
	FVector StartTangentWorld = Data->BlendedStartTangent.Get(StartTangent);
	FVector EndTangentWorld   = Data->BlendedEndTangent.Get(EndTangent);
	StartTangentWorld = StartTangentWorld.GetSafeNormal() * EdgeLength;
	EndTangentWorld   = EndTangentWorld.GetSafeNormal()   * EdgeLength;

	FEdgeSplinePoints Points;
	Points.StartLocal = ActorTransform.InverseTransformPosition(StartWorld) + FVector(0, 0, ZStagger);
	Points.EndLocal   = ActorTransform.InverseTransformPosition(EndWorld) + FVector(0, 0, ZStagger);
	Points.StartTangentLocal = ActorTransform.InverseTransformVector(StartTangentWorld);
	Points.EndTangentLocal   = ActorTransform.InverseTransformVector(EndTangentWorld);

	if (USplineComponent* OrigSpline = Data->OriginalSplineComponent.Get())
	{
		const float Key = OrigSpline->FindInputKeyClosestToWorldLocation(StartWorld);
		const float DistAlong = OrigSpline->GetDistanceAlongSplineAtSplineInputKey(Key);
		Points.TilingOffset = (DefaultRoadMeshLength > KINDA_SMALL_NUMBER) ? FMath::Fmod(DistAlong / DefaultRoadMeshLength, 1.0) : 0.0;
	}

	// claim synchronously so a second caller doesn't re-queue before this runs
	Data->bMeshSpawnPending = true; 

	TWeakObjectPtr<ARoadsFromSplines> WeakThis(this);
	Concurrency::RunOnGameThreadThrottled([WeakThis, EdgeIndex, Points = MoveTemp(Points), CapturedStartNode, CapturedEndNode]() {
		if (!WeakThis.IsValid() || !IsValid(WeakThis->GetWorld())) return;

		FRoadEdgeSplineData* Data = WeakThis->EdgeSplineData.Find(EdgeIndex);
		if (Data) Data->bMeshSpawnPending = false;

		USplineMeshComponent* Mesh = WeakThis->SpawnSplineMeshComponent(Points, WeakThis->RoadMesh, WeakThis->DefaultRoadMeshLength);
		if (!IsValid(Mesh)) return;

		const bool bEdgeStillLive = WeakThis->RoadGraph.Edges.IsValidIndex(EdgeIndex) && WeakThis->RoadGraph.Edges[EdgeIndex].bValid;
		const bool bNowTouchesJunction = bEdgeStillLive && WeakThis->EdgeTouchesJunction(EdgeIndex);
		const bool bEndpointsMoved = bEdgeStillLive &&
			(WeakThis->RoadGraph.Edges[EdgeIndex].StartNodeIndex != CapturedStartNode ||
			 WeakThis->RoadGraph.Edges[EdgeIndex].EndNodeIndex   != CapturedEndNode);

		bool bTangentsChanged = false;
		if (Data && bEdgeStillLive && !bEndpointsMoved)
		{
			const FRoadEdge& CurEdge = WeakThis->RoadGraph.Edges[EdgeIndex];
			const double CurLength = FVector::Dist(
				WeakThis->RoadGraph.Nodes[CurEdge.StartNodeIndex].Location,
				WeakThis->RoadGraph.Nodes[CurEdge.EndNodeIndex].Location);
			const FVector CurStartTangentWorld = Data->BlendedStartTangent.Get(CurEdge.StartTangentWorld).GetSafeNormal() * CurLength;
			const FVector CurEndTangentWorld   = Data->BlendedEndTangent.Get(CurEdge.EndTangentWorld).GetSafeNormal() * CurLength;
			const FTransform& CurActorTransform = WeakThis->GetActorTransform();

			bTangentsChanged =
				!CurActorTransform.InverseTransformVector(CurStartTangentWorld).Equals(Points.StartTangentLocal, 0.1) ||
				!CurActorTransform.InverseTransformVector(CurEndTangentWorld).Equals(Points.EndTangentLocal, 0.1);

			if (bTangentsChanged)
			{
				UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("CreateSplineMeshForEdge: Edge %d built with stale tangents (blend changed while spawn was queued), re-issuing"), EdgeIndex);
			}
		}

		const bool bStale = bEndpointsMoved || bTangentsChanged;

		if (!Data || !bEdgeStillLive || bNowTouchesJunction || bStale)
		{
			Mesh->DestroyComponent();
			if (Data && bEdgeStillLive && bStale && !bNowTouchesJunction)
				WeakThis->CreateSplineMeshForEdge(EdgeIndex, WeakThis->ZOffset);

			return;
		}

		Data->MeshComponent = Mesh;
		WeakThis->SplineMeshComponents.Add(Mesh);
		WeakThis->OnSplineMeshCreated(Data->OriginalSplineComponent.Get(), Mesh);
	});
	return true;
}

bool ARoadsFromSplines::ComputeLandscapeRoll(const FVector& LocationWorld, const FVector& TangentWorld, float& OutRoll) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	UWorld* World = GetWorld();
	if (!IsValid(World)) return false;

	const FVector TraceStart(LocationWorld.X, LocationWorld.Y, HALF_WORLD_MAX);
	const FVector TraceEnd(LocationWorld.X, LocationWorld.Y, -HALF_WORLD_MAX);

	FHitResult HitResult;
	if (!World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECollisionChannel::ECC_Visibility, LandscapeCollisionQueryParams))
		return false;

	if (HitResult.Normal.IsZero()) return false;

	const float MaxRoll = FMath::Acos(HitResult.Normal.Z);
	const FVector TiltAxis = FVector::CrossProduct(HitResult.Normal, FVector::UpVector).GetSafeNormal();
	const float Factor = FVector::DotProduct(TangentWorld.GetSafeNormal(), TiltAxis);
	OutRoll = AdaptSplineMeshRollAlpha * MaxRoll * Factor;
	OutRoll = FMath::Clamp(OutRoll, -FMath::DegreesToRadians(AdaptSplineMeshMaxAngle), FMath::DegreesToRadians(AdaptSplineMeshMaxAngle));
	return true;
}

void ARoadsFromSplines::ApplyLandscapeRoll(USplineMeshComponent* MeshComponent, const FEdgeSplinePoints& Points) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	if (!bAdaptSplineMeshRollToLandscape || !IsValid(MeshComponent)) return;

	const FTransform& ParentTransform = GetActorTransform();
	const FVector StartWorld = ParentTransform.TransformPosition(Points.StartLocal);
	const FVector EndWorld   = ParentTransform.TransformPosition(Points.EndLocal);
	const FVector StartTangentWorld = ParentTransform.TransformVector(Points.StartTangentLocal);
	const FVector EndTangentWorld   = ParentTransform.TransformVector(Points.EndTangentLocal);

	float StartRoll;
	if (ComputeLandscapeRoll(StartWorld, StartTangentWorld, StartRoll))
		MeshComponent->SetStartRoll(StartRoll, false);

	float EndRoll;
	if (ComputeLandscapeRoll(EndWorld, EndTangentWorld, EndRoll))
		MeshComponent->SetEndRoll(EndRoll, false);
}

bool ARoadsFromSplines::BuildResampledPoints(USplineComponent* Source, float MaxSpacing, TArray<FVector>& OutPoints, TArray<FVector>& OutTangents) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	if (!IsValid(Source)) return false;

	const bool bClosedLoop = Source->IsClosedLoop();
	const int32 NumPoints = Source->GetNumberOfSplinePoints();
	if (NumPoints < 2) return false;

	const int32 NumSegments = bClosedLoop ? NumPoints : NumPoints - 1;
	const float SplineLength = Source->GetSplineLength();

	OutPoints.Add(Source->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World));
	OutTangents.Add(Source->GetTangentAtSplinePoint(0, ESplineCoordinateSpace::World));

	for (int32 i = 0; i < NumSegments; i++)
	{
		const float DistStart = Source->GetDistanceAlongSplineAtSplinePoint(i);
		const float DistEnd = (i + 1 < NumPoints) ? Source->GetDistanceAlongSplineAtSplinePoint(i + 1) : SplineLength;

		const int32 NumSubSamples = FMath::Clamp(FMath::CeilToInt((DistEnd - DistStart) / MaxSpacing), 1, 64);

		for (int32 s = 1; s < NumSubSamples; s++)
		{
			const float Distance = FMath::Lerp(DistStart, DistEnd, (float)s / NumSubSamples);
			OutPoints.Add(Source->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World));
			OutTangents.Add(Source->GetTangentAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World));
		}

		const int32 NextPointIndex = (i + 1) % NumPoints;
		OutPoints.Add(Source->GetLocationAtSplinePoint(NextPointIndex, ESplineCoordinateSpace::World));
		OutTangents.Add(Source->GetTangentAtSplinePoint(NextPointIndex, ESplineCoordinateSpace::World));
	}

	return true;
}

bool ARoadsFromSplines::BuildEdgesForSpline(USplineComponent* OriginalSpline, const TArray<FVector>& Points, const TArray<FVector>& Tangents, bool bClosedLoop, TArray<int32>& OutNewEdgeIndices)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	if (Points.Num() < 2) return false;

	auto OnEdgeSplit = [&](int32 NewEdgeIndex, int32 SourceEdgeIndex) {
		DestroyEdgeMesh(SourceEdgeIndex);

		if (FRoadEdgeSplineData* SourceData = EdgeSplineData.Find(SourceEdgeIndex))
		{
			FRoadEdgeSplineData& NewData = EdgeSplineData.Add(NewEdgeIndex);
			NewData.OriginalSplineComponent = SourceData->OriginalSplineComponent;
		}
		else
		{
			EdgeSplineData.Add(NewEdgeIndex).OriginalSplineComponent = OriginalSpline;
		}
		SplineToEdgeIndices.FindOrAdd(OriginalSpline).AddUnique(NewEdgeIndex);
	};

	auto OnEdgeRemoved = [&](int32 RemovedEdgeIndex) {
		DestroyEdgeMesh(RemovedEdgeIndex);
		EdgeSplineData.Remove(RemovedEdgeIndex);
		if (TArray<int32>* Indices = SplineToEdgeIndices.Find(OriginalSpline))
			Indices->Remove(RemovedEdgeIndex);
	};

	TArray<int32> TouchedNodeIndices;
	TArray<int32> NewEdgeIndices = RoadGraph.AddNodedChain(Points, Tangents, bClosedLoop, TouchedNodeIndices, OnEdgeSplit, OnEdgeRemoved);

	if (NewEdgeIndices.Num() == 0) return false;

	for (int32 EdgeIndex : NewEdgeIndices)
	{
		if (!EdgeSplineData.Contains(EdgeIndex))
			EdgeSplineData.Add(EdgeIndex).OriginalSplineComponent = OriginalSpline;
	}

	for (int32 NodeIndex : TouchedNodeIndices)
		UpdateBlendedTangentsAtNode(NodeIndex);

	DestroyMeshesAtJunctionNodes(TouchedNodeIndices);
	CreateMeshesForJunctions(TouchedNodeIndices);
	SplineToEdgeIndices.FindOrAdd(OriginalSpline).Append(NewEdgeIndices);
	OutNewEdgeIndices.Append(NewEdgeIndices);

	return true;
}

void ARoadsFromSplines::DestroyJunctionMeshes(int32 JunctionNode)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	TArray<TObjectPtr<USplineMeshComponent>>* Meshes = JunctionMeshComponents.Find(JunctionNode);
	UE_LOG(LogBuildingsFromSplines, Verbose, TEXT("DestroyJunctionMeshes: Junction %d destroying %d meshes"), JunctionNode, Meshes ? Meshes->Num() : 0);
	if (Meshes)
	{
		for (USplineMeshComponent* Mesh : *Meshes)
		{
			if (!IsValid(Mesh)) continue;
			CornerMeshComponents.Remove(Mesh);
			TWeakObjectPtr<ARoadsFromSplines> WeakThis(this);
			Concurrency::RunOnGameThreadThrottled([WeakThis, Mesh]() {
				if (!WeakThis.IsValid() || !IsValid(WeakThis->GetWorld())) return;
				if (!IsValid(Mesh)) return;
				Mesh->DestroyComponent();
			});
		}
		JunctionMeshComponents.Remove(JunctionNode);
	}

	if (TObjectPtr<UDynamicMeshComponent>* FillComponent = JunctionFillMeshComponents.Find(JunctionNode))
	{
		if (IsValid(*FillComponent))
		{
			TWeakObjectPtr<UDynamicMeshComponent> WeakFill(*FillComponent);
			Concurrency::RunOnGameThreadThrottled([WeakFill]() {
				if (WeakFill.IsValid()) WeakFill->DestroyComponent();
			});
		}
		JunctionFillMeshComponents.Remove(JunctionNode);
	}
}

void ARoadsFromSplines::DestroyMeshesAtJunctionNodes(const TArray<int32>& TouchedNodeIndices)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);
	
	for (int32 NodeIndex : TouchedNodeIndices)
	{
		DestroyJunctionMeshes(NodeIndex);

		if (!RoadGraph.Nodes.IsValidIndex(NodeIndex)) continue;
		for (int32 EdgeIndex : RoadGraph.Nodes[NodeIndex].EdgeIndices)
			DestroyEdgeMesh(EdgeIndex);
	}
}

void ARoadsFromSplines::DebugDrawGraph()
{
	UWorld* World = GetWorld();
	if (!IsValid(World)) return;

	TWeakObjectPtr<ARoadsFromSplines> WeakThis(this);
	Concurrency::RunOnGameThreadAndWait([&, WeakThis]() {
		if (!WeakThis.IsValid() || !IsValid(WeakThis->GetWorld())) return false;
		FlushPersistentDebugLines(World);

		for (const FRoadEdge& Edge : RoadGraph.Edges)
		{
			if (!Edge.bValid) continue;
			if (!RoadGraph.Nodes.IsValidIndex(Edge.StartNodeIndex) || !RoadGraph.Nodes.IsValidIndex(Edge.EndNodeIndex)) continue;

			DrawDebugLine(World,
				RoadGraph.Nodes[Edge.StartNodeIndex].Location,
				RoadGraph.Nodes[Edge.EndNodeIndex].Location,
				FColor::Cyan, false, 20, 0, 4);
		}

		for (const FRoadNode& Node : RoadGraph.Nodes)
		{
			if (!Node.bValid) continue;
			const int32 Degree = Node.EdgeIndices.Num();
			const float Size = 10.f + 10.f * Degree;
			FColor Color =
				  Degree <= 1 ? FColor::Cyan
				: Degree == 2 ? FColor::Green
				: Degree == 3 ? FColor::Yellow
				: Degree == 4 ? FColor::Orange
				: Degree == 5 ? FColor::Red
				: Degree == 6 ? FColor::Magenta
				: Degree == 7 ? FColor::Purple
				: FColor::Black;
			DrawDebugPoint(World, Node.Location, Size, Color, false, 20, 0);
		}
		return true;
	});
}

void ARoadsFromSplines::UpdateBlendedTangentsAtNode(int32 NodeIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	if (!RoadGraph.Nodes.IsValidIndex(NodeIndex)) return;

	TArray<int32> ValidEdges;
	for (int32 EdgeIndex : RoadGraph.Nodes[NodeIndex].EdgeIndices)
		if (RoadGraph.Edges.IsValidIndex(EdgeIndex) && RoadGraph.Edges[EdgeIndex].bValid)
			ValidEdges.Add(EdgeIndex);

	if (ValidEdges.Num() != 2)
	{
		for (int32 EdgeIndex : ValidEdges)
			if (FRoadEdgeSplineData* Data = EdgeSplineData.Find(EdgeIndex))
			{
				Data->BlendedStartTangent.Reset();
				Data->BlendedEndTangent.Reset();
			}
		return;
	}

	auto OutwardDir = [&](int32 EdgeIndex, bool& bIsStart) -> FVector
	{
		FRoadEdge& Edge = RoadGraph.Edges[EdgeIndex];
		bIsStart = (Edge.StartNodeIndex == NodeIndex);
		return bIsStart ? Edge.StartTangentWorld.GetSafeNormal() : -Edge.EndTangentWorld.GetSafeNormal();
	};

	const int32 EdgeA = ValidEdges[0];
	const int32 EdgeB = ValidEdges[1];
	bool bAIsStart, bBIsStart;
	const FVector OutA = OutwardDir(EdgeA, bAIsStart);
	const FVector OutB = OutwardDir(EdgeB, bBIsStart);

	const FVector Blended = (OutA - OutB).GetSafeNormal();
	if (Blended.IsNearlyZero()) return;

	FRoadEdge& A = RoadGraph.Edges[EdgeA];
	FRoadEdge& B = RoadGraph.Edges[EdgeB];

	if (FRoadEdgeSplineData* DataA = EdgeSplineData.Find(EdgeA))
	{
		if (bAIsStart) DataA->BlendedStartTangent = Blended * A.StartTangentWorld.Size();
		else DataA->BlendedEndTangent = -Blended * A.EndTangentWorld.Size();
	}
	if (FRoadEdgeSplineData* DataB = EdgeSplineData.Find(EdgeB))
	{
		if (bBIsStart) DataB->BlendedStartTangent = -Blended * B.StartTangentWorld.Size();
		else DataB->BlendedEndTangent = Blended * B.EndTangentWorld.Size();
	}
}

void ARoadsFromSplines::PushOverlappingBuildings(UPrimitiveComponent* MeshComponent) const
{
	UWorld* World = IsValid(MeshComponent) ? MeshComponent->GetWorld() : nullptr;
	if (!IsValid(World)) return;

	FComponentQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	TArray<FOverlapResult> Overlaps;
	MeshComponent->ComponentOverlapMulti(Overlaps, World, MeshComponent->GetComponentLocation(), MeshComponent->GetComponentQuat(), ECC_Visibility, QueryParams);

	TSet<ABuilding*> Buildings;
	for (const FOverlapResult& Overlap : Overlaps)
		if (ABuilding* Building = Cast<ABuilding>(Overlap.GetActor()))
			Buildings.Add(Building);

	for (ABuilding* Building : Buildings)
		Building->TryPushOutOfCollision();
}

#if WITH_EDITOR

AActor *ARoadsFromSplines::Duplicate(FName FromName, FName ToName)
{
	if (ARoadsFromSplines *NewRoadsFromSplines =
		Cast<ARoadsFromSplines>(GEditor->GetEditorSubsystem<UEditorActorSubsystem>()->DuplicateActor(this)))
	{
		NewRoadsFromSplines->SplinesTag = ULCBlueprintLibrary::ReplaceName(SplinesTag, FromName, ToName);
		NewRoadsFromSplines->SplineComponentsTag = ULCBlueprintLibrary::ReplaceName(SplineComponentsTag, FromName, ToName);

		return NewRoadsFromSplines;
	}
	else
	{
		LCReporter::ShowError(LOCTEXT("ARoadsFromSplines::DuplicateActor", "Failed to duplicate actor."));
		return nullptr;
	}
}

#endif
