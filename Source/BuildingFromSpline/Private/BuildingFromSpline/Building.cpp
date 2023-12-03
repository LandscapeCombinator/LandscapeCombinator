// Copyright 2023 LandscapeCombinator. All Rights Reserved.


#include "BuildingFromSpline/Building.h"
#include "BuildingFromSpline/LogBuildingFromSpline.h"
#include "OSMUserData/OSMUserData.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshTransformFunctions.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshMaterialFunctions.h"
#include "GeometryScript/MeshPolygroupFunctions.h"
#include "GeometryScript/MeshQueryFunctions.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshSimplifyFunctions.h"
#include "GeometryScript/MeshUVFunctions.h"
#include "GeometryScript/CreateNewAssetUtilityFunctions.h"
#include "GeometryScript/PolyPathFunctions.h"
#include "Logging/StructuredLog.h"
#include "Polygon2.h"
#include "Algo/Reverse.h"
#include "Stats/Stats.h"
#include "Kismet/KismetSystemLibrary.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(Building)

#define LOCTEXT_NAMESPACE "FBuildingFromSplineModule"
#define MILLIMETER_TOLERANCE 0.1

using namespace UE::Geometry;

ABuilding::ABuilding() : ADynamicMeshActor()
{
	PrimaryActorTick.bCanEverTick = false;

	DynamicMeshComponent->SetMobility(EComponentMobility::Static);
	DynamicMeshComponent->SetNumMaterials(0);
	
	InstancedDoorsComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("InstancedDoorsComponent"));
	InstancedWindowsComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("InstancedWindowsComponent"));

	SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
	SplineComponent->SetupAttachment(RootComponent);
	SplineComponent->SetClosedLoop(true);
	SplineComponent->ClearSplinePoints();
	SplineComponent->AddSplinePoint(FVector(0, 0, 0), ESplineCoordinateSpace::Local, false);
	SplineComponent->AddSplinePoint(FVector(1000, 0, 0), ESplineCoordinateSpace::Local, false);
	SplineComponent->AddSplinePoint(FVector(1000, 1000, 0), ESplineCoordinateSpace::Local, false);
	SplineComponent->AddSplinePoint(FVector(0, 1000, 0), ESplineCoordinateSpace::Local, false);
	SplineComponent->SetSplinePointType(0, ESplinePointType::Linear, false);
	SplineComponent->SetSplinePointType(1, ESplinePointType::Linear, false);
	SplineComponent->SetSplinePointType(2, ESplinePointType::Linear, false);
	SplineComponent->SetSplinePointType(3, ESplinePointType::Linear, false);
	SplineComponent->UpdateSpline();
	SplineComponent->bSplineHasBeenEdited = true;
	SplineComponent->SetMobility(EComponentMobility::Static);

	BaseClockwiseSplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("BaseClockwiseSplineComponent"));
	BaseClockwiseSplineComponent->SetupAttachment(RootComponent);
	BaseClockwiseSplineComponent->SetClosedLoop(true);
	BaseClockwiseSplineComponent->ClearSplinePoints();
	BaseClockwiseSplineComponent->SetMobility(EComponentMobility::Static);

	BuildingConfiguration = CreateDefaultSubobject<UBuildingConfiguration>(TEXT("Building Configuration"));

	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	StaticMeshComponent->SetupAttachment(RootComponent);
	StaticMeshComponent->SetMobility(EComponentMobility::Static);
	StaticMeshComponent->SetMaterial(0, BuildingConfiguration->FloorMaterial);
	StaticMeshComponent->SetMaterial(1, BuildingConfiguration->CeilingMaterial);
	StaticMeshComponent->SetMaterial(2, BuildingConfiguration->ExteriorMaterial);
	StaticMeshComponent->SetMaterial(3, BuildingConfiguration->InteriorMaterial);
	StaticMeshComponent->SetMaterial(4, BuildingConfiguration->RoofMaterial);
}

void ABuilding::Destroyed()
{
	DeleteBuilding();
	Super::Destroyed();
}

void ABuilding::ComputeBaseVertices()
{
	// static FTotalTimeAndCount ComputeBaseVerticesTime;
	// SCOPE_LOG_TIME_FUNC_WITH_GLOBAL(&ComputeBaseVerticesTime);

	int NumPoints = SplineComponent->GetNumberOfSplinePoints();
	if (NumPoints == 0) return;

	BaseVertices2D.Empty();
	SplineIndexToBaseSplineIndex.Empty();
	SplineIndexToBaseSplineIndex.SetNum(NumPoints + 1);

	/* Compute the projection of the spline (with subdivisions) in BaseVertices2D */

	for (int i = 0; i < NumPoints; i++)
	{
		SplineIndexToBaseSplineIndex[i] = BaseVertices2D.Num();

		FTransform Transform = SplineComponent->GetRelativeTransform();
		FVector Location = Transform.TransformPosition(SplineComponent->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local));
		BaseVertices2D.Add({ Location[0], Location[1] });
		float Distance1 = SplineComponent->GetDistanceAlongSplineAtSplinePoint(i);
		float Distance2 = SplineComponent->GetDistanceAlongSplineAtSplinePoint(i + 1); // i + 1 = NumPoints seems to be OK here
		float Length = Distance2 - Distance1;

		if (Length == 0) continue;
		for (int j = 0; j < BuildingConfiguration->WallSubdivisions; j++)
		{
			FVector SubLocation =
				Transform.TransformPosition(
					SplineComponent->GetLocationAtDistanceAlongSpline(
						Distance1 + (j + 1) * Length / (BuildingConfiguration->WallSubdivisions + 1),
						ESplineCoordinateSpace::Local
					)
				);
			BaseVertices2D.Add({ SubLocation[0], SubLocation[1] });
		}
	}

	// we also add an entry for `NumPoints` to avoid an edge case when querying the distance along spline
	SplineIndexToBaseSplineIndex[NumPoints] = BaseVertices2D.Num();

	
	/* If BaseVertices2D is counter-clockwise, flip the points (the first point remains the first) */

	TPolygon2<double> BasePolygon(BaseVertices2D);

	int NumSubPoints = BaseVertices2D.Num();

	TArray<FVector2D> ClockwiseBaseVertices2D;
	ClockwiseBaseVertices2D.SetNum(NumSubPoints);

	// clockwise for TPolygon2 is counter-clockwise in game (inverted Y axis)
	if (BasePolygon.IsClockwise())
	{
		ClockwiseBaseVertices2D[0] = BaseVertices2D[0];
		for (int i = 1; i < NumSubPoints; i++)
		{
			ClockwiseBaseVertices2D[i] = BaseVertices2D[NumSubPoints - i];
		}
		BaseVertices2D = ClockwiseBaseVertices2D;
	}
	
	/* Initialize the new spline corresponding to these base vertices */
	
	BaseClockwiseSplineComponent->ClearSplinePoints();
	for (int i = 0; i < NumSubPoints; i++)
	{
		FVector2D Point2D = BaseVertices2D[i];
		BaseClockwiseSplineComponent->AddSplinePoint(FVector(Point2D[0], Point2D[1], MinHeightLocal), ESplineCoordinateSpace::Local, false);
		BaseClockwiseSplineComponent->SetSplinePointType(i, ESplinePointType::Linear, false);
	}

	BaseClockwiseSplineComponent->UpdateSpline();

	
	/* Sample transform splines from the new spline */

	FGeometryScriptSplineSamplingOptions SamplingOptions;
	SamplingOptions.SampleSpacing = EGeometryScriptSampleSpacing::UniformTime;
	SamplingOptions.NumSamples = BaseClockwiseSplineComponent->GetNumberOfSplinePoints();

	UGeometryScriptLibrary_PolyPathFunctions::SampleSplineToTransforms(
		BaseClockwiseSplineComponent,
		BaseClockwiseFrames,
		BaseClockwiseFramesTimes,
		SamplingOptions,
		FTransform()
	);
}

void ABuilding::ComputeOffsetPolygons()
{
	// static FTotalTimeAndCount ComputeOffsetPolygonsTime;
	// SCOPE_LOG_TIME_FUNC_WITH_GLOBAL(&ComputeOffsetPolygonsTime);

	/* Initialization */

	int NumFrames = BaseClockwiseFrames.Num();

	if (NumFrames == 0) return;

	/* External Wall Polygon */
	
	ExternalWallPolygon.Empty();
	ExternalWallPolygon.Reserve(2 * NumFrames);

	for (int i = 0; i < NumFrames; i++)
	{
		ExternalWallPolygon.Add(GetShiftedPoint(BaseClockwiseFrames, i, - BuildingConfiguration->ExternalWallThickness, true));
	}

	
	/* Internal Wall Polygon */
	
	InternalWallPolygon.Reserve(2 * NumFrames);
	DeflateFrames(
		BaseClockwiseFrames,
		InternalWallPolygon,
		IndexToInternalIndex,
		BuildingConfiguration->InternalWallThickness
	);
}

FVector2D To2D(FVector Vector)
{
	return FVector2D(Vector[0], Vector[1]);
}

FVector To3D(FVector2D Vector)
{
	return FVector(Vector[0], Vector[1], 0);
}

FVector2D ABuilding::GetShiftedPoint(TArray<FTransform> Frames, int Index, double Offset, bool bIsLoop)
{
	// static FTotalTimeAndCount GetShiftedPointTime;
	// SCOPE_LOG_TIME_FUNC_WITH_GLOBAL(&GetShiftedPointTime);

	int NumFrames = Frames.Num();
	check(0 <= Index);
	check(Index < NumFrames);

	int PrevIndex = (Index + NumFrames - 1) % NumFrames;
	int NextIndex = (Index + 1) % NumFrames;
	FVector2D PrevPoint = To2D(Frames[PrevIndex].GetLocation());
	FVector2D Point = To2D(Frames[Index].GetLocation());
	FVector2D NextPoint = To2D(Frames[NextIndex].GetLocation());

	if (NumFrames == 1)
	{
		return Point;
	}

	if (Offset == 0)
	{
		return Point;
	}

	FVector2D WallDirection2 = (NextPoint - Point).GetSafeNormal();
	FVector2D InsideDirection2 = WallDirection2.GetRotated(90);
	FVector2D WallDirection1 = (Point - PrevPoint).GetSafeNormal();
	FVector2D InsideDirection1 = WallDirection1.GetRotated(90);


	FVector2D PointShift1 = Point + Offset * InsideDirection1;
	FVector2D PointShift2 = Point + Offset * InsideDirection2;

	if (Index == 0 && !bIsLoop)
	{
		return PointShift2;
	}

	if (Index == NumFrames - 1 && !bIsLoop)
	{
		return PointShift1;
	}

	return GetIntersection(PointShift1, WallDirection1, PointShift2, WallDirection2);
}

FVector2D ABuilding::GetIntersection(FVector2D Point1, FVector2D Direction1, FVector2D Point2, FVector2D Direction2)
{
	// static FTotalTimeAndCount GetIntersectionTime;
	// SCOPE_LOG_TIME_FUNC_WITH_GLOBAL(&GetIntersectionTime);

	// Point1.X + t * Direction1.X = Point2.X + t' * Direction2.X
	// Point1.Y + t * Direction1.Y = Point2.Y + t' * Direction2.Y

	// t * Direction1.X - t' * Direction2.X = Point2.X - Point1.X
	// t * Direction1.Y - t' * Direction2.Y = Point2.Y - Point1.Y

	double Det = -Direction1.X * Direction2.Y + Direction1.Y * Direction2.X;

	FVector2D Intersection;

	if (FMath::IsNearlyEqual(Det, 0, MILLIMETER_TOLERANCE))
	{
		Intersection = Point2;
	}
	else
	{
		double Num = -(Point2.X - Point1.X) * Direction2.Y + (Point2.Y - Point1.Y) * Direction2.X;
		double t = Num / Det;
		Intersection = Point1 + t * Direction1;
	}

	return Intersection;
}

void ABuilding::DeflateFrames(TArray<FTransform> Frames, TArray<FVector2D>& OutOffsetPolygon, TArray<int>& OutIndexToOffsetIndex, double Offset)
{
	// static FTotalTimeAndCount DeflateFramesTime;
	// SCOPE_LOG_TIME_FUNC_WITH_GLOBAL(&DeflateFramesTime);

	int NumFrames = Frames.Num();
	int NumVertices = BaseVertices2D.Num();

	OutOffsetPolygon.Empty();
	OutIndexToOffsetIndex.Empty();
	OutIndexToOffsetIndex.SetNum(NumFrames);

	for (int i = 0; i < NumFrames; i++)
	{
		FVector2D Point = GetShiftedPoint(Frames, i, Offset, true);

		bool bIsTooClose = false;
		
		// When deflating a polygon, some points might be too close to an edge, we don't add those
		// TODO: Remove expensive check and implement a proper polygon deflate algorithm
		// TPolygon2::PolyOffset doesn't help as it has the same artefacts
		if (Offset > 0)
		{
			for (int j = 0; j < NumVertices; j++)
			{
				double Distance = TSegment2<double>::FastDistanceSquared(BaseVertices2D[j], BaseVertices2D[(j + 1) % NumVertices], Point);
				if (Distance < Offset * Offset - MILLIMETER_TOLERANCE)
				{
					bIsTooClose = true;
					break;
				}
			}
		}
		
		if (!bIsTooClose)
		{
			OutOffsetPolygon.Add(Point);
		}

		if (OutOffsetPolygon.IsEmpty())
		{
			OutIndexToOffsetIndex[i] = 0;
		}
		else
		{
			OutIndexToOffsetIndex[i] = OutOffsetPolygon.Num() - 1;
		}
	}

	return;
}

void ABuilding::AppendAlongSpline(UDynamicMesh* TargetMesh, bool bInternalWall, double BeginDistance, double Length, double Height, double ZOffset, int MaterialID)
{
	// static FTotalTimeAndCount AppendAlongSplineTime1;
	// SCOPE_LOG_TIME_FUNC_WITH_GLOBAL(&AppendAlongSplineTime1);
	
	TArray<FVector2D> Polygon = MakePolygon(bInternalWall, BeginDistance, Length);

	/* Allocate and build WallMesh */

	UDynamicMesh *WallMesh = AllocateComputeMesh();

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSimpleExtrudePolygon(
		WallMesh,
		FGeometryScriptPrimitiveOptions(),
		FTransform(FVector(0, 0, ZOffset)),
		Polygon,
		Height
	);

	
	/* Remap the material ID */

	UGeometryScriptLibrary_MeshMaterialFunctions::RemapMaterialIDs(WallMesh, 0, MaterialID);


	/* Add the WallMesh to our TargetMesh */

	UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(TargetMesh, WallMesh, FTransform(), true);

}


bool ABuilding::AppendFloors(UDynamicMesh* TargetMesh)
{
	// static FTotalTimeAndCount AppendFloorsTime;
	// SCOPE_LOG_TIME_FUNC_WITH_GLOBAL(&AppendFloorsTime);

	/* Create one floor tile in FloorMesh */

	UDynamicMesh *FloorMesh = AllocateComputeMesh();

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSimpleExtrudePolygon(
		FloorMesh,
		FGeometryScriptPrimitiveOptions(),
		FTransform(FVector(0, 0, - BuildingConfiguration->FloorThickness)),
		BaseVertices2D,
		BuildingConfiguration->FloorThickness
	);


	/* Set the Polygroup ID of ceiling to CeilingMaterialID in FloorMesh */

	FGeometryScriptIndexList PolygroupIDs0;
	UGeometryScriptLibrary_MeshPolygroupFunctions::GetPolygroupIDsInMesh(
		FloorMesh,
		FGeometryScriptGroupLayer(),
		PolygroupIDs0
	);

	TArray<int> PolygroupIDs = *PolygroupIDs0.List;

	if (PolygroupIDs.Num() < 3)
	{
		UE_LOGFMT(LogBuildingFromSpline, Error, "Internal error: something went wrong with the floor tile materials");
		return false;
	}

	bool bIsValidPolygroupID;
	UGeometryScriptLibrary_MeshMaterialFunctions::SetPolygroupMaterialID(
		FloorMesh,
		FGeometryScriptGroupLayer(),
		PolygroupIDs[2], // TODO: polygroup ID of the ceiling, is there a way to ensure it?
		CeilingMaterialID, // new material ID
		bIsValidPolygroupID,
		false,
		nullptr
	);


	/* Add several copies of the FloorMesh at every floor */

	if (BuildingConfiguration->bBuildFloorTiles)
	{
		for (int k = 0; k < BuildingConfiguration->NumFloors; k++)
		{
			UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
				TargetMesh, FloorMesh,
				FTransform(FVector(0, 0, MinHeightLocal + BuildingConfiguration->ExtraWallBottom + k * BuildingConfiguration->FloorHeight)),
				true
			);
		}
	}
	

	/* Add the last floor with roof material for flat roof kind */

	if (BuildingConfiguration->RoofKind == ERoofKind::Flat)
	{
		UGeometryScriptLibrary_MeshMaterialFunctions::RemapMaterialIDs(FloorMesh, 0, RoofMaterialID);
	
		UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
			TargetMesh, FloorMesh,
			FTransform(FVector(0, 0, MinHeightLocal + BuildingConfiguration->ExtraWallBottom + BuildingConfiguration->NumFloors * BuildingConfiguration->FloorHeight)),
			true
		);
	}

	return true;
}

bool ABuilding::AppendSimpleBuilding(UDynamicMesh* TargetMesh)
{
	// static FTotalTimeAndCount AppendSimpleBuildingTime;
	// SCOPE_LOG_TIME_FUNC_WITH_GLOBAL(&AppendSimpleBuildingTime);

	UDynamicMesh *SimpleBuildingMesh = AllocateComputeMesh();

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSimpleExtrudePolygon(
		SimpleBuildingMesh,
		FGeometryScriptPrimitiveOptions(),
		FTransform(FVector(0, 0, 0)),
		BaseVertices2D,
		BuildingConfiguration->ExtraWallBottom +
		BuildingConfiguration->NumFloors * BuildingConfiguration->FloorHeight +
		BuildingConfiguration->ExtraWallTop
	);


	/* Set the Polygroup ID of ceiling to CeilingMaterialID in SimpleBuildingMesh */

	FGeometryScriptIndexList PolygroupIDs0;
	UGeometryScriptLibrary_MeshPolygroupFunctions::GetPolygroupIDsInMesh(
		SimpleBuildingMesh,
		FGeometryScriptGroupLayer(),
		PolygroupIDs0
	);

	TArray<int> PolygroupIDs = *PolygroupIDs0.List;

	if (PolygroupIDs.Num() < 4)
	{
		UE_LOG(LogBuildingFromSpline, Error, TEXT("Internal error: something went wrong with the simple building materials"));
		return false;
	}

	bool bIsValidPolygroupID;
	UGeometryScriptLibrary_MeshMaterialFunctions::SetPolygroupMaterialID(
		SimpleBuildingMesh,
		FGeometryScriptGroupLayer(),
		PolygroupIDs[0], // TODO: polygroup ID of the sides of the polygon, is there a way to ensure it?
		0, // new material ID
		bIsValidPolygroupID,
		false,
		nullptr
	);
	UGeometryScriptLibrary_MeshMaterialFunctions::SetPolygroupMaterialID(
		SimpleBuildingMesh,
		FGeometryScriptGroupLayer(),
		PolygroupIDs[3], // TODO: polygroup ID of the top of the polygon, is there a way to ensure it?
		1, // new material ID
		bIsValidPolygroupID,
		false,
		nullptr
	);

	UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
		TargetMesh, SimpleBuildingMesh,
		FTransform(FVector(0, 0, MinHeightLocal)),
		true
	);

	if (BuildingConfiguration->DoorMesh)
	{
		FBox BoundingBox = BuildingConfiguration->DoorMesh->GetBoundingBox();
		InstancedDoorsComponent->SetStaticMesh(BuildingConfiguration->DoorMesh);
		TArray<float> HolePositions;
		AppendWallsWithHoles(nullptr, false, 0,
			BuildingConfiguration->MinDistanceDoorToCorner, BuildingConfiguration->MinDistanceDoorToDoor,
			BuildingConfiguration->DoorsWidth, BuildingConfiguration->DoorsHeight, BuildingConfiguration->DoorsDistanceToFloor,
			0, false, HolePositions, -1
		);
	
		for (auto& HolePosition : HolePositions)
		{
			FVector HoleLocation = BaseClockwiseSplineComponent->GetLocationAtDistanceAlongSpline(HolePosition, ESplineCoordinateSpace::World);
			FVector HoleTangent = BaseClockwiseSplineComponent->GetTangentAtDistanceAlongSpline(HolePosition, ESplineCoordinateSpace::World);
			HoleLocation.Z += BuildingConfiguration->ExtraWallBottom + BuildingConfiguration->DoorsDistanceToFloor;
				
			FVector Scale(
				BuildingConfiguration->DoorsWidth / BoundingBox.GetExtent()[0] / 2,
				1,
				BuildingConfiguration->DoorsHeight / BoundingBox.GetExtent()[2] / 2
			);

			FTransform Transform;
			Transform.SetLocation(HoleLocation);
			Transform.SetRotation(FQuat::FindBetweenVectors(FVector(1, 0, 0), HoleTangent));
			Transform.SetScale3D(Scale);
			InstancedDoorsComponent->AddInstance(Transform, true);
		}
	}

	if (BuildingConfiguration->WindowMesh)
	{
		FBox BoundingBox = BuildingConfiguration->WindowMesh->GetBoundingBox();
		InstancedWindowsComponent->SetStaticMesh(BuildingConfiguration->WindowMesh);
		TArray<float> HolePositions;
		AppendWallsWithHoles(nullptr, false, 0,
			BuildingConfiguration->MinDistanceWindowToCorner, BuildingConfiguration->MinDistanceWindowToWindow,
			BuildingConfiguration->WindowsWidth, BuildingConfiguration->WindowsHeight, BuildingConfiguration->WindowsDistanceToFloor,
			0, false, HolePositions, -1
		);

		for (int i = 1; i < BuildingConfiguration->NumFloors; i++)
		{			
			for (auto& HolePosition : HolePositions)
			{
				FVector HoleLocation = BaseClockwiseSplineComponent->GetLocationAtDistanceAlongSpline(HolePosition, ESplineCoordinateSpace::World);
				FVector HoleTangent = BaseClockwiseSplineComponent->GetTangentAtDistanceAlongSpline(HolePosition, ESplineCoordinateSpace::World);
				HoleLocation.Z += BuildingConfiguration->ExtraWallBottom + i * BuildingConfiguration->FloorHeight + BuildingConfiguration->WindowsDistanceToFloor;
				
				FVector Scale(
					BuildingConfiguration->WindowsWidth / BoundingBox.GetExtent()[0] / 2,
					1,
					BuildingConfiguration->WindowsHeight / BoundingBox.GetExtent()[2] / 2
				);

				FTransform Transform;
				Transform.SetLocation(HoleLocation);
				Transform.SetRotation(FQuat::FindBetweenVectors(FVector(1, 0, 0), HoleTangent));
				Transform.SetScale3D(Scale);
				InstancedWindowsComponent->AddInstance(Transform, true);
			}
		}
	}

	return true;
}

TArray<FVector2D> ABuilding::MakePolygon(bool bInternalWall, double BeginDistance, double Length)
{
	// static FTotalTimeAndCount MakePolygonTime;
	// SCOPE_LOG_TIME_FUNC_WITH_GLOBAL(&MakePolygonTime);

	TArray<FVector2D> Result;

	FTransform FirstTransform = BaseClockwiseSplineComponent->GetTransformAtDistanceAlongSpline(BeginDistance, ESplineCoordinateSpace::Local);
	FTransform LastTransform = BaseClockwiseSplineComponent->GetTransformAtDistanceAlongSpline(BeginDistance + Length, ESplineCoordinateSpace::Local);

	FVector2D FirstLocation = To2D(FirstTransform.GetLocation());
	FVector2D LastLocation = To2D(LastTransform.GetLocation());

	double FirstTime = BaseClockwiseSplineComponent->GetTimeAtDistanceAlongSpline(BeginDistance);
	double LastTime = BaseClockwiseSplineComponent->GetTimeAtDistanceAlongSpline(BeginDistance + Length);

	/* Add to Result all the frames whose frame time is between FirstTime and LastTime */

	int NumFrames = BaseClockwiseFrames.Num();
	int i = 0;
	
	// TODO: make this a binary search
	while (i < NumFrames && BaseClockwiseFramesTimes[i] < FirstTime) i++;

	// from here, all frames with an index higher or equal than `i` have frame times larger or equal to `FirstTime`


	// we only add a frame if its location is different from the previous one
	auto Add = [this, &Result](FVector2D Location)
	{
		if (Result.IsEmpty() || !(Result.Last() - Location).IsNearlyZero(MILLIMETER_TOLERANCE))
		{
			Result.Add(Location);
		}
	};
	
	Add(FirstLocation);


	const int BeginIndex = i;

	bool bAddedPoints = false;

	while (i < NumFrames && BaseClockwiseFramesTimes[i] <= LastTime)
	{
		Add(To2D(BaseClockwiseFrames[i].GetLocation()));
		i++;
		bAddedPoints = true;
	}
	int LastIndex = i - 1;

	if (LastTime == 1)
	{
		Add(To2D(BaseClockwiseFrames[0].GetLocation()));
		LastIndex = NumFrames;
	}

	Add(LastLocation);

	// if nothing was added in the loop, or if the LastLocation is distinct from what was added
	if (!bAddedPoints || !(To2D(BaseClockwiseFrames[LastIndex % NumFrames].GetLocation()) - LastLocation).IsNearlyZero(MILLIMETER_TOLERANCE))
	{
		// add a shifted location corresponding to LastLocation, on the internal or external wall
		FVector2D PrevPoint = Result.Last(1);
		FVector2D WallDirection = (LastLocation - PrevPoint).GetSafeNormal();
		FVector2D InsideDirection = WallDirection.GetRotated(90);
		if (bInternalWall)
		{
			Add(LastLocation + BuildingConfiguration->InternalWallThickness * InsideDirection);
		}
		else
		{
			Add(LastLocation - BuildingConfiguration->ExternalWallThickness * InsideDirection);
		}
	}

	for (i = LastIndex; i >= BeginIndex; i--)
	{
		if (bInternalWall && !InternalWallPolygon.IsEmpty())
		{
			Add(InternalWallPolygon[IndexToInternalIndex[i % NumFrames]]);
		}
		else
		{
			Add(ExternalWallPolygon[i % NumFrames]);
		}

	}
	
	// If the first point is not at the beginning of the spline, and
	// If nothing was added in the loop, or if the FirstLocation is distinct from what was added
	if (!bAddedPoints || !(To2D(BaseClockwiseFrames[BeginIndex].GetLocation()) - FirstLocation).IsNearlyZero(MILLIMETER_TOLERANCE))
	{
		// add a shifted location corresponding to LastLocation, on the internal or external wall
		FVector2D NextPoint = Result[1];
		FVector2D WallDirection = (NextPoint - FirstLocation).GetSafeNormal();
		FVector2D InsideDirection = WallDirection.GetRotated(90);

		if (bInternalWall)
		{
			Add(FirstLocation + BuildingConfiguration->InternalWallThickness * InsideDirection);
		}
		else
		{
			Add(FirstLocation - BuildingConfiguration->ExternalWallThickness * InsideDirection);
		}
	}

	if (!bInternalWall)
	{
		Algo::Reverse(Result);
	}

	return Result;
}

void ABuilding::AddSplineMesh(UStaticMesh* StaticMesh, double BeginDistance, double Length, double Height, double ZOffset)
{
	// static FTotalTimeAndCount AddSplineMeshTime;
	// SCOPE_LOG_TIME_FUNC_WITH_GLOBAL(&AddSplineMeshTime);

	if (!StaticMesh) return;

	USplineMeshComponent *SplineMeshComponent = NewObject<USplineMeshComponent>(RootComponent);
	SplineMeshComponents.Add(SplineMeshComponent);
	SplineMeshComponent->SetStaticMesh(StaticMesh);
	SplineMeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	SplineMeshComponent->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	SplineMeshComponent->RegisterComponent();
	FVector StartPos = BaseClockwiseSplineComponent->GetLocationAtDistanceAlongSpline(BeginDistance, ESplineCoordinateSpace::Local);
	FVector StartTangent = BaseClockwiseSplineComponent->GetTangentAtDistanceAlongSpline(BeginDistance, ESplineCoordinateSpace::Local);
	FVector EndPos = BaseClockwiseSplineComponent->GetLocationAtDistanceAlongSpline(BeginDistance + Length, ESplineCoordinateSpace::Local);
	FVector EndTangent = BaseClockwiseSplineComponent->GetTangentAtDistanceAlongSpline(BeginDistance + Length, ESplineCoordinateSpace::Local);

	StartTangent = StartTangent.GetSafeNormal() * Length;
	EndTangent = EndTangent.GetSafeNormal() * Length;

	FBox BoundingBox = StaticMesh->GetBoundingBox();
	double NewScale = Height / BoundingBox.GetExtent()[2] / 2;
	SplineMeshComponent->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent, false);
	SplineMeshComponent->SetStartScale( { 1, NewScale }, false);
	SplineMeshComponent->SetEndScale( { 1, NewScale }, false);

	SplineMeshComponent->SetRelativeLocation(FVector(0, 0, ZOffset - MinHeightLocal));
	this->AddInstanceComponent(SplineMeshComponent);
}

void ABuilding::AppendWallsWithHoles(
	UDynamicMesh* TargetMesh, bool bInternalWall, double WallHeight,
	double MinDistanceHoleToCorner, double MinDistanceHoleToHole, double HolesWidth, double HolesHeight, double HoleDistanceToFloor, double ZOffset,
	bool bBuildWalls, TArray<float> &OutHolePositions,
	int MaterialID
)
{
	// static FTotalTimeAndCount AppendWallsWithHolesTime1;
	// SCOPE_LOG_TIME_FUNC_WITH_GLOBAL(&AppendWallsWithHolesTime1);

	for (int i = 0; i < SplineComponent->GetNumberOfSplinePoints(); i++)
	{
		float Distance1 = BaseClockwiseSplineComponent->GetDistanceAlongSplineAtSplinePoint(SplineIndexToBaseSplineIndex[i]);
		float Distance2 = BaseClockwiseSplineComponent->GetDistanceAlongSplineAtSplinePoint(SplineIndexToBaseSplineIndex[i + 1]);

		float TotalDistance = Distance2 - Distance1;
		float AvailableDistance = TotalDistance - 2 * MinDistanceHoleToCorner;

		if (AvailableDistance >= HolesWidth)
		{
			int NumHoles = 1 + (AvailableDistance - HolesWidth) / (HolesWidth + MinDistanceHoleToHole); // >= 1
			int NumSpaces = NumHoles - 1; // >= 0

			float BeginOffset = (AvailableDistance - NumHoles * HolesWidth - NumSpaces * MinDistanceHoleToHole) / 2; // >= 0
			float BeginHoleDistance = Distance1 + MinDistanceHoleToCorner + BeginOffset; // >= Distance1 + MinDistanceHoleToCorner
			float BeginSpaceDistance = BeginHoleDistance + HolesWidth;

			if (bBuildWalls)
			{
				// before the first hole
				AppendAlongSpline(TargetMesh, bInternalWall, Distance1, MinDistanceHoleToCorner + BeginOffset, WallHeight, ZOffset, MaterialID);

				// in-between holes
				for (int j = 0; j < NumSpaces; j++)
				{
					AppendAlongSpline(TargetMesh, bInternalWall, BeginSpaceDistance + j * (HolesWidth + MinDistanceHoleToHole), MinDistanceHoleToHole, WallHeight, ZOffset, MaterialID);
				}
			
				// after the last hole
				AppendAlongSpline(
					TargetMesh, bInternalWall, BeginSpaceDistance + NumSpaces * (HolesWidth + MinDistanceHoleToHole),
					BeginOffset + MinDistanceHoleToCorner, WallHeight, ZOffset, MaterialID
				);
				
				// below the holes
				for (int j = 0; j < NumHoles; j++)
				{
					AppendAlongSpline(
						TargetMesh, bInternalWall, BeginHoleDistance + j * (HolesWidth + MinDistanceHoleToHole),
						HolesWidth, HoleDistanceToFloor, ZOffset, MaterialID
					);
				}
				
				// above the holes
				const double RemainingHeight = WallHeight - HoleDistanceToFloor - HolesHeight;
				if (RemainingHeight > 0)
				{
					for (int j = 0; j < NumHoles; j++)
					{
						AppendAlongSpline(
							TargetMesh, bInternalWall, BeginHoleDistance + j * (HolesWidth + MinDistanceHoleToHole),
							HolesWidth, RemainingHeight, ZOffset + HoleDistanceToFloor + HolesHeight, MaterialID
						);
					}
				}
			}

			// add holes positions
			for (int j = 0; j < NumHoles; j++)
			{
				OutHolePositions.Add(BeginHoleDistance + j * (HolesWidth + MinDistanceHoleToHole));
			}
		}
		else if (bBuildWalls)
		{
			AppendAlongSpline(TargetMesh, bInternalWall, Distance1, TotalDistance, WallHeight, ZOffset, MaterialID);
		}
	}
}

void ABuilding::AppendWallsWithHoles(UDynamicMesh* TargetMesh)
{
	// static FTotalTimeAndCount AppendWallsWithHolesTime2;
	// SCOPE_LOG_TIME_FUNC_WITH_GLOBAL(&AppendWallsWithHolesTime2);

	double InternalWallHeight = BuildingConfiguration->FloorHeight;
	if (BuildingConfiguration->bBuildFloorTiles) InternalWallHeight -= BuildingConfiguration->FloorThickness;

	// Inside ExtraWallBottom

	if (BuildingConfiguration->bBuildInternalWalls && BuildingConfiguration->ExtraWallBottom > 0) {
		AppendAlongSpline(
			TargetMesh, true, 0, BaseClockwiseSplineComponent->GetSplineLength(),
			BuildingConfiguration->ExtraWallBottom, MinHeightLocal, InteriorMaterialID
		);
	}

	// Inside Floor 0

	if (BuildingConfiguration->NumFloors > 0)
	{
		TArray<float> HolePositions;
		AppendWallsWithHoles(
			TargetMesh, true, InternalWallHeight,
			BuildingConfiguration->MinDistanceDoorToCorner, BuildingConfiguration->MinDistanceDoorToDoor,
			BuildingConfiguration->DoorsWidth, BuildingConfiguration->DoorsHeight, BuildingConfiguration->DoorsDistanceToFloor,
			MinHeightLocal + BuildingConfiguration->ExtraWallBottom,
			BuildingConfiguration->bBuildInternalWalls, HolePositions,
			InteriorMaterialID
		);
		for (auto& HolePosition : HolePositions)
		{
			AddSplineMesh(
				BuildingConfiguration->DoorMesh, HolePosition, BuildingConfiguration->DoorsWidth, BuildingConfiguration->DoorsHeight,
				MinHeightLocal + BuildingConfiguration->ExtraWallBottom + BuildingConfiguration->DoorsDistanceToFloor
			);
		}
	}


	// Inside Other Floors

	if (BuildingConfiguration->NumFloors > 1)
	{
		UDynamicMesh* WholeFloorMesh = AllocateComputeMesh();
		
		TArray<float> HolePositions;
		AppendWallsWithHoles(
			WholeFloorMesh, true, InternalWallHeight,
			BuildingConfiguration->MinDistanceWindowToCorner, BuildingConfiguration->MinDistanceWindowToWindow,
			BuildingConfiguration->WindowsWidth, BuildingConfiguration->WindowsHeight, BuildingConfiguration->WindowsDistanceToFloor,
			0,
			BuildingConfiguration->bBuildInternalWalls, HolePositions,
			InteriorMaterialID
		);

		for (int i = 1; i < BuildingConfiguration->NumFloors; i++)
		{
			UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
				TargetMesh, WholeFloorMesh,
				FTransform(FVector(0, 0, MinHeightLocal + BuildingConfiguration->ExtraWallBottom + i * BuildingConfiguration->FloorHeight)),
				true
			);
			for (auto& HolePosition : HolePositions)
			{
				AddSplineMesh(
					BuildingConfiguration->WindowMesh, HolePosition, BuildingConfiguration->WindowsWidth, BuildingConfiguration->WindowsHeight,
					MinHeightLocal + BuildingConfiguration->ExtraWallBottom + i * BuildingConfiguration->FloorHeight + BuildingConfiguration->WindowsDistanceToFloor
				);
			}
		}
	}
	
	// Inside ExtraWallTop

	if (BuildingConfiguration->bBuildInternalWalls && BuildingConfiguration->ExtraWallTop > 0)
	{
		AppendAlongSpline(
			TargetMesh, true, 0, BaseClockwiseSplineComponent->GetSplineLength(),
			BuildingConfiguration->ExtraWallTop,
			MinHeightLocal + BuildingConfiguration->ExtraWallBottom + BuildingConfiguration->NumFloors * BuildingConfiguration->FloorHeight,
			InteriorMaterialID
		);
	}



	// Outside ExtraWallBottom

	if (BuildingConfiguration->bBuildExternalWalls && BuildingConfiguration->ExtraWallBottom > 0)
	{
		AppendAlongSpline(
			TargetMesh, false, 0, BaseClockwiseSplineComponent->GetSplineLength(),
			BuildingConfiguration->ExtraWallBottom, MinHeightLocal, ExteriorMaterialID
		);
	}

	// Outside Floor 0

	if (BuildingConfiguration->NumFloors > 0)
	{
		TArray<float> HolePositions;
		AppendWallsWithHoles(
			TargetMesh, false, BuildingConfiguration->FloorHeight,
			BuildingConfiguration->MinDistanceDoorToCorner, BuildingConfiguration->MinDistanceDoorToDoor,
			BuildingConfiguration->DoorsWidth, BuildingConfiguration->DoorsHeight, BuildingConfiguration->DoorsDistanceToFloor,
			MinHeightLocal + BuildingConfiguration->ExtraWallBottom,
			BuildingConfiguration->bBuildExternalWalls, HolePositions, ExteriorMaterialID
		);
	}

	// Outside Other Floors

	if (BuildingConfiguration->NumFloors > 1)
	{
		UDynamicMesh* WholeFloorMesh = AllocateComputeMesh();
		
		TArray<float> HolePositions;
		AppendWallsWithHoles(
			WholeFloorMesh, false, BuildingConfiguration->FloorHeight,
			BuildingConfiguration->MinDistanceWindowToCorner, BuildingConfiguration->MinDistanceWindowToWindow,
			BuildingConfiguration->WindowsWidth, BuildingConfiguration->WindowsHeight, BuildingConfiguration->WindowsDistanceToFloor,
			0,
			BuildingConfiguration->bBuildExternalWalls, HolePositions, ExteriorMaterialID
		);

		for (int i = 1; i < BuildingConfiguration->NumFloors; i++)
		{
			UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
				TargetMesh, WholeFloorMesh,
				FTransform(FVector(0, 0, MinHeightLocal + BuildingConfiguration->ExtraWallBottom + i * BuildingConfiguration->FloorHeight)),
				true
			);
		}
	}

	// Outside ExtraWallTop
	
	if (BuildingConfiguration->bBuildExternalWalls && BuildingConfiguration->ExtraWallTop > 0)
	{
		AppendAlongSpline(
			TargetMesh, false, 0, BaseClockwiseSplineComponent->GetSplineLength(),
			BuildingConfiguration->ExtraWallTop, MinHeightLocal + BuildingConfiguration->ExtraWallBottom +
			BuildingConfiguration->NumFloors * BuildingConfiguration->FloorHeight, ExteriorMaterialID
		);
	}
}

void ABuilding::AppendRoof(UDynamicMesh* TargetMesh)
{
	///	// static FTotalTimeAndCount*/ AppendRoofTime;
	// S// E_LOG_TIME_FUNC_WITH_GLOBAL(&AppendRoofTime);

	/* Allocate RoofMesh */

	UDynamicMesh *RoofMesh = AllocateComputeMesh();

	
	/* Top of the roof */

	int NumFrames = BaseClockwiseFrames.Num();
	if (NumFrames == 0) return;

	const double WallTopHeight = MinHeightLocal + BuildingConfiguration->ExtraWallBottom + BuildingConfiguration->NumFloors * BuildingConfiguration->FloorHeight + BuildingConfiguration->ExtraWallTop;
	const double RoofTopHeight = WallTopHeight + BuildingConfiguration->RoofHeight;

	TArray<FTransform> Frames;
	Frames.Append(BaseClockwiseFrames);

	TArray<FVector2D> RoofPolygon;
	TArray<int> IndexToRoofIndex;
	DeflateFrames(Frames, RoofPolygon, IndexToRoofIndex, BuildingConfiguration->InnerRoofDistance);

	if (RoofPolygon.IsEmpty() || BuildingConfiguration->RoofKind == ERoofKind::Point)
	{
		double MiddleX = 0;
		double MiddleY = 0;
		int n = BaseVertices2D.Num();
		for (int i = 0; i < n; i++)
		{
			MiddleX += BaseVertices2D[i].X / n;
			MiddleY += BaseVertices2D[i].Y / n;
		}

		RoofPolygon = TArray<FVector2D>({ { MiddleX, MiddleY } });
		for (int i = 0; i < NumFrames; i++)
		{
			IndexToRoofIndex[i] = 0;
		}
	}
	else
	{
		UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSimpleExtrudePolygon(
			RoofMesh,
			FGeometryScriptPrimitiveOptions(),
			FTransform(FVector(0, 0, RoofTopHeight)),
			RoofPolygon,
			BuildingConfiguration->RoofThickness
		);

		UGeometryScriptLibrary_MeshMaterialFunctions::RemapMaterialIDs(RoofMesh, 0, RoofMaterialID);

		/* Set the Polygroup ID of ceiling to CeilingMaterialID */

		FGeometryScriptIndexList PolygroupIDs0;
		UGeometryScriptLibrary_MeshPolygroupFunctions::GetPolygroupIDsInMesh(
			RoofMesh,
			FGeometryScriptGroupLayer(),
			PolygroupIDs0
		);

		TArray<int> PolygroupIDs = *PolygroupIDs0.List;

		if (PolygroupIDs.Num() < 3)
		{
			UE_LOGFMT(LogBuildingFromSpline, Error, "Internal error: something went wrong with the floor tile materials");
			return;
		}

		bool bIsValidPolygroupID;
		UGeometryScriptLibrary_MeshMaterialFunctions::SetPolygroupMaterialID(
			RoofMesh,
			FGeometryScriptGroupLayer(),
			PolygroupIDs[2], // TODO: polygroup ID of the ceiling, is there a way to ensure it?
			CeilingMaterialID, // new material ID
			bIsValidPolygroupID,
			false,
			nullptr
		);
	}


	/* Connection from the walls to the roof, outside */

	FTransform BuildingTransform = this->GetTransform();

	TArray<FTransform> SweepPath;
	for (int i = 0; i < NumFrames; i++)
	{
		FVector2D WallPoint = ExternalWallPolygon[i];
		FVector2D RoofPoint = RoofPolygon[IndexToRoofIndex[i]];

		FVector Source(WallPoint[0], WallPoint[1], WallTopHeight);
		FVector Target(RoofPoint[0], RoofPoint[1], RoofTopHeight + BuildingConfiguration->RoofThickness);

		const FVector UnitDirection = (Target - Source).GetSafeNormal();
		Source -= UnitDirection * BuildingConfiguration->OuterRoofDistance;

		FTransform NewTransform;
		NewTransform.SetLocation(Source);
		NewTransform.SetRotation(FQuat::FindBetweenVectors(FVector(0, 0, 1), UnitDirection));
		NewTransform.SetScale3D(FVector(1, 1, FVector::Distance(Source, Target)));

		SweepPath.Add(NewTransform);
	}

	FGeometryScriptPrimitiveOptions Options;
	Options.bFlipOrientation = true;
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolyline(
		RoofMesh, Options,
		FTransform(), { {0, 0}, {0, 1} },
		SweepPath, {}, {}, true
	);
	UGeometryScriptLibrary_MeshMaterialFunctions::RemapMaterialIDs(RoofMesh, 0, RoofMaterialID);


	/* Connection from the walls to the roof, inside */

	if (!InternalWallPolygon.IsEmpty())
	{
		SweepPath.Empty();
		for (int i = 0; i < NumFrames; i++)
		{
			FVector2D WallPoint = InternalWallPolygon[IndexToInternalIndex[i]];
			FVector2D RoofPoint = RoofPolygon[IndexToRoofIndex[i]];

			FVector Source(WallPoint[0], WallPoint[1], WallTopHeight);
			const FVector Target(RoofPoint[0], RoofPoint[1], RoofTopHeight);

			const FVector UnitDirection = (Target - Source).GetSafeNormal();
			Source -= UnitDirection * BuildingConfiguration->OuterRoofDistance;

			FTransform NewTransform;
			NewTransform.SetLocation(Source);
			NewTransform.SetRotation(FQuat::FindBetweenVectors(FVector(0, 0, 1), UnitDirection));
			NewTransform.SetScale3D(FVector(1, 1, FVector::Distance(Source, Target)));

			SweepPath.Add(NewTransform);
		}

		Options.bFlipOrientation = false;
		UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolyline(
			RoofMesh, Options,
			FTransform(), { {0, 0}, {0, 1} },
			SweepPath, {}, {}, true
		);
		UGeometryScriptLibrary_MeshMaterialFunctions::RemapMaterialIDs(RoofMesh, 0, InteriorMaterialID);
	}
	

	/* Add the WallMesh to our TargetMesh */

	UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(TargetMesh, RoofMesh, FTransform(), true);
}

void ABuilding::ComputeMinMaxHeight()
{
	// static FTotalTimeAndCount ComputeMinMaxHeightTime;
	// SCOPE_LOG_TIME_FUNC_WITH_GLOBAL(&ComputeMinMaxHeightTime);
	int NumPoints = SplineComponent->GetNumberOfSplinePoints();
	MinHeightLocal = MAX_dbl;
	MinHeightWorld = MAX_dbl;
	MaxHeightLocal = -MAX_dbl;

	for (int i = 0; i < NumPoints; i++)
	{
		double HeightLocal = SplineComponent->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local)[2];
		double HeightWorld = SplineComponent->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World)[2];
		if (HeightLocal < MinHeightLocal) MinHeightLocal = HeightLocal;
		if (HeightWorld < MinHeightWorld) MinHeightWorld = HeightWorld;
		if (HeightLocal > MaxHeightLocal) MaxHeightLocal = HeightLocal;
	}
}

void ABuilding::SetReceivesDecals(bool bReceivesDecal)
{
	StaticMeshComponent->bReceivesDecals = bReceivesDecal; 
	GetDynamicMeshComponent()->bReceivesDecals = bReceivesDecal;
	InstancedWindowsComponent->bReceivesDecals = bReceivesDecal;
	InstancedDoorsComponent->bReceivesDecals = bReceivesDecal;
}

void ABuilding::DeleteBuilding()
{
	ClearSplineMeshComponents();

	if (IsValid(InstancedDoorsComponent))
	{
		InstancedDoorsComponent->ClearInstances();
	}

	if (IsValid(InstancedWindowsComponent))
	{
		InstancedWindowsComponent->ClearInstances();
	}
	
	if (IsValid(StaticMeshComponent))
	{
		StaticMeshComponent->SetStaticMesh(nullptr);
	}
	
	if (IsValid(DynamicMeshComponent))
	{
		DynamicMeshComponent->SetNumMaterials(0);
		DynamicMeshComponent->GetDynamicMesh()->Reset();
	}

	if (IsValid(Volume))
	{
		Volume->Destroy();
	}
}

void ABuilding::GenerateBuilding()
{
	// static FTotalTimeAndCount GenerateBuildingTime;
	// SCOPE_LOG_TIME_FUNC_WITH_GLOBAL(&GenerateBuildingTime);

	DeleteBuilding();

	if (BuildingConfiguration->bUseRandomNumFloors)
	{
		BuildingConfiguration->NumFloors = UKismetMathLibrary::RandomIntegerInRange(BuildingConfiguration->MinNumFloors, BuildingConfiguration->MaxNumFloors);
	}

	AppendBuilding(DynamicMeshComponent->GetDynamicMesh());
}

void ABuilding::ClearSplineMeshComponents()
{
	// static FTotalTimeAndCount ClearSplineMeshComponentsTime;
	// SCOPE_LOG_TIME_FUNC_WITH_GLOBAL(&ClearSplineMeshComponentsTime);

	for (auto& SplineMeshComponent : SplineMeshComponents)
	{
		if (IsValid(SplineMeshComponent))
		{
			SplineMeshComponent->DestroyComponent();
		}
	}
}

void ABuilding::AppendBuilding(UDynamicMesh* TargetMesh)
{
	// static FTotalTimeAndCount AppendBuildingTime, NormalsTime, UVsTime;
	// SCOPE_LOG_TIME_FUNC_WITH_GLOBAL(&AppendBuildingTime);

	ON_SCOPE_EXIT {
		ReleaseAllComputeMeshes();
		BaseClockwiseSplineComponent->ClearSplinePoints();
	};
	
	ComputeMinMaxHeight();
	ComputeBaseVertices();

	if (BuildingConfiguration->BuildingKind == EBuildingKind::Custom)
	{
		ComputeOffsetPolygons();

		AppendWallsWithHoles(TargetMesh);

		if (BuildingConfiguration->bBuildFloorTiles || BuildingConfiguration->RoofKind == ERoofKind::Flat)
		{
			if (!AppendFloors(TargetMesh)) return;
		}

		if (
			BuildingConfiguration->RoofKind == ERoofKind::Point ||
			BuildingConfiguration->RoofKind == ERoofKind::InnerSpline
		)
		{
			AppendRoof(TargetMesh);
		}
	
		DynamicMeshComponent->SetMaterial(0, BuildingConfiguration->FloorMaterial);
		DynamicMeshComponent->SetMaterial(1, BuildingConfiguration->CeilingMaterial);
		DynamicMeshComponent->SetMaterial(2, BuildingConfiguration->ExteriorMaterial);
		DynamicMeshComponent->SetMaterial(3, BuildingConfiguration->InteriorMaterial);
		DynamicMeshComponent->SetMaterial(4, BuildingConfiguration->RoofMaterial);
	}
	else
	{
		AppendSimpleBuilding(TargetMesh);

		DynamicMeshComponent->SetMaterial(0, BuildingConfiguration->ExteriorMaterial);
		DynamicMeshComponent->SetMaterial(1, BuildingConfiguration->RoofMaterial);
	}

	//{
	//	// SCOPE_LOG_TIME_IN_SECONDS(TEXT("Normals"), &NormalsTime);

	//	UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(TargetMesh, FGeometryScriptSplitNormalsOptions(), FGeometryScriptCalculateNormalsOptions());
	//}

	if (BuildingConfiguration->bConvertToStaticMesh)
	{
		GenerateStaticMesh();
	}

	if (BuildingConfiguration->bConvertToVolume)
	{
		GenerateVolume();
	}

	if (BuildingConfiguration->bConvertToStaticMesh || BuildingConfiguration->bConvertToVolume)
	{
		DynamicMeshComponent->GetDynamicMesh()->Reset();
	}

	{
		// SCOPE_LOG_TIME_IN_SECONDS(TEXT("UVs"), &UVsTime);

		if (BuildingConfiguration->bAutoGenerateXAtlasMeshUVs)
		{
			UGeometryScriptLibrary_MeshUVFunctions::AutoGenerateXAtlasMeshUVs(TargetMesh, 0, FGeometryScriptXAtlasOptions());
		}

	}

	GEditor->NoteSelectionChange();
}

void ABuilding::ConvertToStaticMesh()
{
	GenerateStaticMesh();
	DynamicMeshComponent->GetDynamicMesh()->Reset();
}

void ABuilding::GenerateStaticMesh()
{
	// static FTotalTimeAndCount GenerateStaticMeshTime, NormalsTime, UVsTime;
	// SCOPE_LOG_TIME_FUNC_WITH_GLOBAL(&GenerateStaticMeshTime);
	EGeometryScriptOutcomePins Outcome;

	FGeometryScriptCreateNewStaticMeshAssetOptions Options;
	FMeshNaniteSettings NaniteSettings;
	NaniteSettings.bEnabled = true;
	NaniteSettings.bPreserveArea = true;

	FString Path;
	FString Unused;

	UGeometryScriptLibrary_CreateNewAssetFunctions::CreateUniqueNewAssetPathName(FString("/Game"), FString("SM_Building"), Path, Unused, FGeometryScriptUniqueAssetNameOptions(), Outcome);

	if (Outcome != EGeometryScriptOutcomePins::Success)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("StaticMeshError", "Internal error while creating unique asset path name.")
		);
		return;
	}

	UStaticMesh *StaticMesh = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(
		DynamicMeshComponent->GetDynamicMesh(), Path,
		Options, Outcome
	);

	if (Outcome != EGeometryScriptOutcomePins::Success || !StaticMesh)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("StaticMeshError", "Internal error while creating static mesh for building. You may want to regenerate the building.")
		);
		return;
	}

	StaticMesh->NaniteSettings = NaniteSettings;

	int NumMaterials = DynamicMeshComponent->GetNumMaterials();
	TArray<FStaticMaterial> Materials;

	for (int i = 0; i < NumMaterials; i++)
	{
		Materials.Add(DynamicMeshComponent->GetMaterial(i));
		StaticMeshComponent->SetMaterial(i, DynamicMeshComponent->GetMaterial(i));
	}
	StaticMesh->SetStaticMaterials(Materials);

	StaticMeshComponent->SetStaticMesh(StaticMesh);
}

void ABuilding::ConvertToVolume()
{
	GenerateVolume();
	DynamicMeshComponent->GetDynamicMesh()->Reset();
}

void ABuilding::GenerateVolume()
{
	// static FTotalTimeAndCount GenerateVolumeTime, NormalsTime, UVsTime;
	// SCOPE_LOG_TIME_FUNC_WITH_GLOBAL(&GenerateVolumeTime);

	if (IsValid(Volume))
	{
		Volume->Destroy();
	}

	FGeometryScriptCreateNewVolumeFromMeshOptions Options;
	EGeometryScriptOutcomePins Outcome;
	Volume = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewVolumeFromMesh(
		DynamicMeshComponent->GetDynamicMesh(),
		this->GetWorld(),
		this->GetTransform(),
		this->GetActorLabel() + "Volume",
		Options, Outcome
	);
	
	Volume->SetFolderPath(FName(this->GetFolderPath().ToString() + "Volume"));

	UOSMUserData *BuildingOSMUserData = Cast<UOSMUserData>(this->GetRootComponent()->GetAssetUserDataOfClass(UOSMUserData::StaticClass()));
	UOSMUserData *VolumeOSMUserData = NewObject<UOSMUserData>(Volume->GetRootComponent());
	VolumeOSMUserData->Fields = BuildingOSMUserData->Fields;
	Volume->GetRootComponent()->AddAssetUserData(VolumeOSMUserData);

	if (Outcome != EGeometryScriptOutcomePins::Success || !Volume)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("StaticMeshError", "Internal error while creating volume.")
		);
		return;
	}
}

void ABuilding::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (BuildingConfiguration->bGenerateWhenModified)
	{
		GenerateBuilding();
	}
}

#undef LOCTEXT_NAMESPACE
