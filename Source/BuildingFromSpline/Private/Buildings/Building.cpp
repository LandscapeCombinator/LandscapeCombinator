// Copyright 2023 LandscapeCombinator. All Rights Reserved.


#include "Buildings/Building.h"
#include "Utils/Logging.h"

#include "Kismet/GameplayStatics.h"
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

#include UE_INLINE_GENERATED_CPP_BY_NAME(Building)

#define LOCTEXT_NAMESPACE "FBuildingFromSplineModule"

using namespace UE::Geometry;

ABuilding::ABuilding() : ADynamicMeshActor()
{
	PrimaryActorTick.bCanEverTick = false;

	DynamicMeshComponent->SetMobility(EComponentMobility::Static);
	DynamicMeshComponent->SetNumMaterials(0);

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

void ABuilding::ComputeBaseVertices()
{
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

	
	/* If BaseVertices2D is counter-clockwise, flip the points (the first point remain the first) */

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
	/* Initialization */

	int NumFrames = BaseClockwiseFrames.Num();

	if (NumFrames == 0) return;

	TArray<FTransform> Frames;
	Frames.Add(BaseClockwiseFrames.Last()); // to avoid edge cases in `GetShiftedPoint`
	Frames.Append(BaseClockwiseFrames);


	/* External Wall Polygon */
	
	ExternalWallPolygon.Empty();
	ExternalWallPolygon.Reserve(2 * NumFrames);

	for (int i = 0; i < NumFrames; i++)
	{
		ExternalWallPolygon.Add(GetShiftedPoint(Frames, i, - BuildingConfiguration->ExternalWallThickness));
	}

	
	/* Internal Wall Polygon */
	
	InternalWallPolygon.Reserve(2 * NumFrames);
	DeflateFrames(Frames, InternalWallPolygon, IndexToInternalIndex, BuildingConfiguration->InternalWallThickness);
}

FVector2D To2D(FVector Vector)
{
	return FVector2D(Vector[0], Vector[1]);
}

FVector2D ABuilding::GetIntersection(FVector2D Point1, FVector2D Direction1, FVector2D Point2, FVector2D Direction2)
{	
	// Point1.X + t * Direction1.X = Point2.X + t' * Direction2.X
	// Point1.Y + t * Direction1.Y = Point2.Y + t' * Direction2.Y
	
	// t * Direction1.X - t' * Direction2.X = Point2.X - Point1.X
	// t * Direction1.Y - t' * Direction2.Y = Point2.Y - Point1.Y
	
	double Det = - Direction1.X * Direction2.Y + Direction1.Y * Direction2.X;

	FVector2D Intersection;

	if (FMath::IsNearlyEqual(Det, 0))
	{
		Intersection = Point2;
	}
	else
	{
		double Num = - (Point2.X - Point1.X) * Direction2.Y + (Point2.Y - Point1.Y) * Direction2.X;
		double t = Num / Det;
		Intersection = Point1 + t * Direction1;
	}

	return Intersection;
}

FVector2D ABuilding::GetShiftedPoint(TArray<FTransform> Frames, int Index, int Offset)
{
	FVector2D Point = To2D(Frames[Index].GetLocation());
	FVector2D NextPoint = To2D(Frames[Index + 1].GetLocation());

	if (Offset == 0)
	{
		return NextPoint;
	}

	FVector2D Direction1 = To2D(Frames[Index].GetUnitAxis(EAxis::X));
	FVector2D Direction2 = To2D(Frames[Index + 1].GetUnitAxis(EAxis::X));
	
	FVector2D Direction1Y = To2D(Frames[Index].GetUnitAxis(EAxis::Y));
	FVector2D Direction2Y = To2D(Frames[Index + 1].GetUnitAxis(EAxis::Y));

	FVector2D ShiftedPoint1 = Point + Offset * Direction1Y;
	FVector2D ShiftedPoint2 = NextPoint + Offset * Direction2Y;

	return GetIntersection(ShiftedPoint1, Direction1, ShiftedPoint2, Direction2);
}

void ABuilding::DeflateFrames(TArray<FTransform> Frames, TArray<FVector2D>& OutOffsetPolygon, TArray<int>& OutIndexToOffsetIndex, double Offset)
{
	int NumFrames = Frames.Num() - 1; // the first one is only there to help compute intersections
	int NumVertices = BaseVertices2D.Num();

	OutOffsetPolygon.Empty();
	OutIndexToOffsetIndex.Empty();
	OutIndexToOffsetIndex.SetNum(NumFrames);

	const double Tolerance = 0.00000001;

	for (int i = 0; i < NumFrames; i++)
	{
		FVector2D Point = GetShiftedPoint(Frames, i, Offset);

		bool bIsTooClose = false;
		
		// When deflating a polygon, some points might be too close to an edge, we don't add those
		// TODO: Remove expensive check and implement a proper polygon deflate algorithm
		// TPolygon2::PolyOffset doesn't help as it has the same artefacts
		if (Offset > 0)
		{
			for (int j = 0; j < NumVertices; j++)
			{
				double Distance = TSegment2<double>::FastDistanceSquared(BaseVertices2D[j], BaseVertices2D[(j + 1) % NumVertices], Point);
				if (Distance < Offset * Offset - Tolerance)
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

void ABuilding::AppendAlongSpline(UDynamicMesh* TargetMesh, double InternalWidth, double ExternalWidth, double BeginDistance, double Length, double Height, double ZOffset, int MaterialID)
{
	// TODO: reuse the internal and external walls polygons when the parameters match the walls thickness

	/* Construct a polygon that follows the base splines, and which extends inside by `InternalWidth` and outside by `ExternalWidth` */

	TArray<FTransform> Frames = FindFrames(BeginDistance, Length);
	int NumFrames = Frames.Num() - 1; // the first is only there to help compute intersections

	if (NumFrames == 0) return;
	TArray<FVector2D> Polygon;
	Polygon.Reserve(2 * NumFrames);

	for (int i = 0; i < NumFrames; i++)
	{
		Polygon.Add(GetShiftedPoint(Frames, i, - ExternalWidth));
	}
	
	TArray<FVector2D> OffsetPolygon;
	TArray<int> IndexToOffsetIndex;
	DeflateFrames(Frames, OffsetPolygon, IndexToOffsetIndex, InternalWidth);
	Algo::Reverse(OffsetPolygon);
	Polygon.Append(OffsetPolygon);


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

	for (int k = 0; k < BuildingConfiguration->NumFloors; k++)
	{
		UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
			TargetMesh,
			FloorMesh,
			FTransform(FVector(0, 0, MinHeightLocal + BuildingConfiguration->ExtraWallBottom + k * BuildingConfiguration->FloorHeight)),
			true
		);
	}
	

	/* Add the last floor with roof material if there is no custom roof */

	if (!BuildingConfiguration->bBuildRoof)
	{
		UGeometryScriptLibrary_MeshMaterialFunctions::RemapMaterialIDs(FloorMesh, 0, RoofMaterialID);
	
		UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
			TargetMesh,
			FloorMesh,
			FTransform(FVector(0, 0, MinHeightLocal + BuildingConfiguration->ExtraWallBottom + BuildingConfiguration->NumFloors * BuildingConfiguration->FloorHeight)),
			true
		);
	}

	return true;
}

TArray<FTransform> ABuilding::FindFrames(double BeginDistance, double Length)
{
	TArray<FTransform> Result;

	FTransform FirstTransform = BaseClockwiseSplineComponent->GetTransformAtDistanceAlongSpline(BeginDistance, ESplineCoordinateSpace::Local);
	FTransform LastTransform = BaseClockwiseSplineComponent->GetTransformAtDistanceAlongSpline(BeginDistance + Length, ESplineCoordinateSpace::Local);

	double FirstTime = BaseClockwiseSplineComponent->GetTimeAtDistanceAlongSpline(BeginDistance);
	double LastTime = BaseClockwiseSplineComponent->GetTimeAtDistanceAlongSpline(BeginDistance + Length);

	/* Add to Result all the frames whose frame time is between FirstTime and LastTime */

	int NumFrames = BaseClockwiseFrames.Num();
	int i = 0;
	
	// TODO: make this a binary search
	while (i < NumFrames && BaseClockwiseFramesTimes[i] < FirstTime) i++;

	// here, all frames with an index higher or equal than `i` have frame times larger or equal to `FirstTime`

	// we add one frame preceding `FirstTransform` to be able to compute intersections to widen or shrink the spline
	Result.Add(BaseClockwiseFrames[(i + NumFrames - 1) % NumFrames]);

	// we only add a frame if its location is different from the previous one
	auto Add = [&Result](FTransform Transform) {
		if (!(Result.Last().GetLocation() - Transform.GetLocation()).IsNearlyZero())
		{
			Result.Add(Transform);
		}
	};
	
	Add(FirstTransform);

	while (i < NumFrames && BaseClockwiseFramesTimes[i] < LastTime)
	{
		Add(BaseClockwiseFrames[i]);
		i++;
	}

	Add(LastTransform);

	return Result;
}

void ABuilding::AppendHoles(UDynamicMesh* TargetMesh, double MinDistanceHoleToCorner, double MinDistanceHoleToHole, double HolesWidth, double HolesHeight, double ZOffset)
{
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
			float BeginWindowDistance = Distance1 + MinDistanceHoleToCorner + BeginOffset; // >= Distance1 + MinDistanceHoleToCorner

			while (BeginWindowDistance + HolesWidth + MinDistanceHoleToCorner <= Distance2)
			{
				AppendAlongSpline(TargetMesh, BuildingConfiguration->InternalWallThickness + 20, BuildingConfiguration->ExternalWallThickness + 20, BeginWindowDistance, HolesWidth, HolesHeight, ZOffset, 2);
				BeginWindowDistance += HolesWidth + MinDistanceHoleToHole;
			}
		}
	}
}

void ABuilding::AddSplineMesh(UStaticMesh* StaticMesh, double BeginDistance, double Length, double Height, double ZOffset)
{
	USplineMeshComponent *SplineMeshComponent = NewObject<USplineMeshComponent>(RootComponent);
	SplineMeshComponents.Add(SplineMeshComponent);
	SplineMeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	SplineMeshComponent->RegisterComponent();
	SplineMeshComponent->SetStaticMesh(StaticMesh);
	FVector StartPos = BaseClockwiseSplineComponent->GetLocationAtDistanceAlongSpline(BeginDistance, ESplineCoordinateSpace::Local);
	FVector StartTangent = BaseClockwiseSplineComponent->GetTangentAtDistanceAlongSpline(BeginDistance, ESplineCoordinateSpace::Local);
	FVector EndPos = BaseClockwiseSplineComponent->GetLocationAtDistanceAlongSpline(BeginDistance + Length, ESplineCoordinateSpace::Local);
	FVector EndTangent = BaseClockwiseSplineComponent->GetTangentAtDistanceAlongSpline(BeginDistance + Length, ESplineCoordinateSpace::Local);

	StartTangent = StartTangent.GetSafeNormal() * Length;
	EndTangent = EndTangent.GetSafeNormal() * Length;

	FBox BoundingBox = StaticMesh->GetBoundingBox();
	double NewScale = Height / BoundingBox.GetExtent()[2] / 2;
	//SplineMeshComponent->SetWorldScale3D(FVector(1, 1, ));
	SplineMeshComponent->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent, false);
	SplineMeshComponent->SetStartScale( { 1, NewScale }, false);
	SplineMeshComponent->SetEndScale( { 1, NewScale }, false);

	SplineMeshComponent->SetRelativeLocation(FVector(0, 0, ZOffset - MinHeightLocal));
	this->AddInstanceComponent(SplineMeshComponent);
}

void ABuilding::AppendWallsWithHoles(
	UDynamicMesh* TargetMesh, double InternalWidth, double ExternalWidth, double WallHeight,
	double MinDistanceHoleToCorner, double MinDistanceHoleToHole, double HolesWidth, double HolesHeight, double HoleDistanceToFloor, double ZOffset,
	bool bBuildWalls, UStaticMesh *StaticMesh,
	int MaterialID
)
{
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
				AppendAlongSpline(TargetMesh, InternalWidth, ExternalWidth, Distance1, MinDistanceHoleToCorner + BeginOffset, WallHeight, ZOffset, MaterialID);

				// in-between holes
				for (int j = 0; j < NumSpaces; j++)
				{
					AppendAlongSpline(TargetMesh, InternalWidth, ExternalWidth, BeginSpaceDistance + j * (HolesWidth + MinDistanceHoleToHole), MinDistanceHoleToHole, WallHeight, ZOffset, MaterialID);
				}
			
				// after the last hole
				AppendAlongSpline(
					TargetMesh, InternalWidth, ExternalWidth, BeginSpaceDistance + NumSpaces * (HolesWidth + MinDistanceHoleToHole),
					BeginOffset + MinDistanceHoleToCorner, WallHeight, ZOffset, MaterialID
				);
				
				// below the holes
				for (int j = 0; j < NumHoles; j++)
				{
					AppendAlongSpline(
						TargetMesh, InternalWidth, ExternalWidth, BeginHoleDistance + j * (HolesWidth + MinDistanceHoleToHole),
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
							TargetMesh, InternalWidth, ExternalWidth, BeginHoleDistance + j * (HolesWidth + MinDistanceHoleToHole),
							HolesWidth, RemainingHeight, ZOffset + HoleDistanceToFloor + HolesHeight, MaterialID
						);
					}
				}
			}

			// adding the meshes in the holes
			if (StaticMesh)
			{
				for (int j = 0; j < NumHoles; j++)
				{
					AddSplineMesh(StaticMesh, BeginHoleDistance + j * (HolesWidth + MinDistanceHoleToHole), HolesWidth, HolesHeight, ZOffset + HoleDistanceToFloor);
				}
			}
		}
		else if (bBuildWalls)
		{
			AppendAlongSpline(TargetMesh, InternalWidth, ExternalWidth, Distance1, TotalDistance, WallHeight, ZOffset, MaterialID);
		}
	}
}

void ABuilding::AppendWallsWithHoles(UDynamicMesh* TargetMesh)
{	
	double InternalWallHeight = BuildingConfiguration->FloorHeight;
	if (BuildingConfiguration->bBuildFloorTiles) InternalWallHeight -= BuildingConfiguration->FloorThickness;

	if (BuildingConfiguration->bBuildInternalWalls)
	{
		if (BuildingConfiguration->ExtraWallBottom > 0)
		{
			AppendAlongSpline(
				TargetMesh, BuildingConfiguration->InternalWallThickness, 0, 0, BaseClockwiseSplineComponent->GetSplineLength(),
				BuildingConfiguration->ExtraWallBottom, MinHeightLocal, InteriorMaterialID
			);
		}
	}

	AppendWallsWithHoles(
		TargetMesh, BuildingConfiguration->InternalWallThickness, 0, InternalWallHeight,
		BuildingConfiguration->MinDistanceDoorToCorner, BuildingConfiguration->MinDistanceDoorToDoor,
		BuildingConfiguration->DoorsWidth, BuildingConfiguration->DoorsHeight, BuildingConfiguration->DoorsDistanceToFloor,
		MinHeightLocal + BuildingConfiguration->ExtraWallBottom,
		BuildingConfiguration->bBuildInternalWalls, BuildingConfiguration->DoorMesh,
		InteriorMaterialID
	);

	for (int i = 1; i < BuildingConfiguration->NumFloors; i++)
	{
		if (i == BuildingConfiguration->NumFloors - 1 && BuildingConfiguration->bBuildRoof)
		{
			InternalWallHeight = BuildingConfiguration->FloorHeight;
		}
		AppendWallsWithHoles(
			TargetMesh, BuildingConfiguration->InternalWallThickness, 0, InternalWallHeight,
			BuildingConfiguration->MinDistanceWindowToCorner, BuildingConfiguration->MinDistanceWindowToWindow,
			BuildingConfiguration->WindowsWidth, BuildingConfiguration->WindowsHeight, BuildingConfiguration->WindowsDistanceToFloor,
			MinHeightLocal + BuildingConfiguration->ExtraWallBottom + i * BuildingConfiguration->FloorHeight,
			BuildingConfiguration->bBuildInternalWalls, BuildingConfiguration->WindowMesh,
			InteriorMaterialID
		);
	}
	
	if (BuildingConfiguration->bBuildInternalWalls && BuildingConfiguration->ExtraWallTop > 0)
	{
		AppendAlongSpline(
			TargetMesh, BuildingConfiguration->InternalWallThickness, 0, 0, BaseClockwiseSplineComponent->GetSplineLength(),
			BuildingConfiguration->ExtraWallTop,
			MinHeightLocal + BuildingConfiguration->ExtraWallBottom + BuildingConfiguration->NumFloors * BuildingConfiguration->FloorHeight,
			InteriorMaterialID
		);
	}

	if (BuildingConfiguration->bBuildExternalWalls)
	{
		if (BuildingConfiguration->ExtraWallBottom > 0)
		{
			AppendAlongSpline(
				TargetMesh, 0, BuildingConfiguration->ExternalWallThickness, 0, BaseClockwiseSplineComponent->GetSplineLength(),
				BuildingConfiguration->ExtraWallBottom, MinHeightLocal, ExteriorMaterialID
			);
		}
	}

	AppendWallsWithHoles(
		TargetMesh, 0, BuildingConfiguration->ExternalWallThickness, BuildingConfiguration->FloorHeight,
		BuildingConfiguration->MinDistanceDoorToCorner, BuildingConfiguration->MinDistanceDoorToDoor,
		BuildingConfiguration->DoorsWidth, BuildingConfiguration->DoorsHeight, BuildingConfiguration->DoorsDistanceToFloor,
		MinHeightLocal + BuildingConfiguration->ExtraWallBottom,
		BuildingConfiguration->bBuildExternalWalls, nullptr, ExteriorMaterialID
	);

	for (int i = 1; i < BuildingConfiguration->NumFloors; i++)
	{
		AppendWallsWithHoles(
			TargetMesh, 0, BuildingConfiguration->ExternalWallThickness, BuildingConfiguration->FloorHeight,
			BuildingConfiguration->MinDistanceWindowToCorner, BuildingConfiguration->MinDistanceWindowToWindow,
			BuildingConfiguration->WindowsWidth, BuildingConfiguration->WindowsHeight, BuildingConfiguration->WindowsDistanceToFloor,
			MinHeightLocal + BuildingConfiguration->ExtraWallBottom + i * BuildingConfiguration->FloorHeight,
			BuildingConfiguration->bBuildExternalWalls, nullptr, ExteriorMaterialID
		);
	}
	
	if (BuildingConfiguration->bBuildExternalWalls && BuildingConfiguration->ExtraWallTop > 0)
	{
		AppendAlongSpline(
			TargetMesh, 0, BuildingConfiguration->ExternalWallThickness, 0, BaseClockwiseSplineComponent->GetSplineLength(),
			BuildingConfiguration->ExtraWallTop, MinHeightLocal + BuildingConfiguration->ExtraWallBottom +
			BuildingConfiguration->NumFloors * BuildingConfiguration->FloorHeight, ExteriorMaterialID
		);
	}
}

void ABuilding::AppendRoof(UDynamicMesh* TargetMesh)
{
	/* Allocate RoofMesh */

	UDynamicMesh *RoofMesh = AllocateComputeMesh();

	
	/* Top of the roof */

	int NumFrames = BaseClockwiseFrames.Num();
	if (NumFrames == 0) return;

	const double WallTopHeight = MinHeightLocal + BuildingConfiguration->ExtraWallBottom + BuildingConfiguration->NumFloors * BuildingConfiguration->FloorHeight + BuildingConfiguration->ExtraWallTop;
	const double RoofTopHeight = WallTopHeight + BuildingConfiguration->RoofHeight;

	TArray<FTransform> Frames;
	Frames.Add(BaseClockwiseFrames.Last()); // this is required by `DeflateFrames`
	Frames.Append(BaseClockwiseFrames);

	TArray<FVector2D> RoofPolygon;
	TArray<int> IndexToRoofIndex;
	DeflateFrames(Frames, RoofPolygon, IndexToRoofIndex, BuildingConfiguration->InnerRoofDistance);

	if (RoofPolygon.IsEmpty() || InternalWallPolygon.IsEmpty()) return;

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSimpleExtrudePolygon(
		RoofMesh,
		FGeometryScriptPrimitiveOptions(),
		FTransform(FVector(0, 0, RoofTopHeight)),
		RoofPolygon,
		BuildingConfiguration->RoofThickness
	);

	UGeometryScriptLibrary_MeshMaterialFunctions::RemapMaterialIDs(RoofMesh, 0, RoofMaterialID);

	/* Set the Polygroup ID of ceiling to CeilingMaterialID in FloorMesh */

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
		FTransform(), { {0, 0}, {0, 1 } },
		SweepPath, {}, {}, true
	);
	UGeometryScriptLibrary_MeshMaterialFunctions::RemapMaterialIDs(RoofMesh, 0, RoofMaterialID);


	/* Connection from the walls to the roof, inside */

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
		FTransform(), { {0, 0}, {0, 1 } },
		SweepPath, {}, {}, true
	);
	UGeometryScriptLibrary_MeshMaterialFunctions::RemapMaterialIDs(RoofMesh, 0, InteriorMaterialID);
	

	/* Add the WallMesh to our TargetMesh */

	UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(TargetMesh, RoofMesh, FTransform(), true);
}

void ABuilding::ComputeMinMaxHeight()
{
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

void ABuilding::GenerateBuilding()
{
	AppendBuilding(DynamicMeshComponent->GetDynamicMesh());
}

void ABuilding::ClearSplineMeshComponents()
{
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
	ON_SCOPE_EXIT {
		ReleaseAllComputeMeshes();
		BaseClockwiseSplineComponent->ClearSplinePoints();
	};
	
	TargetMesh->Reset();
	ClearSplineMeshComponents();
	ComputeMinMaxHeight();
	ComputeBaseVertices();
	ComputeOffsetPolygons();

	AppendWallsWithHoles(TargetMesh);

	if (BuildingConfiguration->bBuildFloorTiles)
	{
		if (!AppendFloors(TargetMesh)) return;
	}

	if (BuildingConfiguration->bBuildRoof)
	{
		AppendRoof(TargetMesh);
	}

	UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(TargetMesh, FGeometryScriptSplitNormalsOptions(), FGeometryScriptCalculateNormalsOptions());
	
	DynamicMeshComponent->SetMaterial(0, BuildingConfiguration->FloorMaterial);
	DynamicMeshComponent->SetMaterial(1, BuildingConfiguration->CeilingMaterial);
	DynamicMeshComponent->SetMaterial(2, BuildingConfiguration->ExteriorMaterial);
	DynamicMeshComponent->SetMaterial(3, BuildingConfiguration->InteriorMaterial);
	DynamicMeshComponent->SetMaterial(4, BuildingConfiguration->RoofMaterial);

	UGeometryScriptLibrary_MeshUVFunctions::AutoGenerateXAtlasMeshUVs(TargetMesh, 0, FGeometryScriptXAtlasOptions());

	GEditor->NoteSelectionChange();
}

void ABuilding::ConvertToStaticMesh()
{
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
	StaticMesh->SetStaticMaterials({
		DynamicMeshComponent->GetMaterial(0),
		DynamicMeshComponent->GetMaterial(1),
		DynamicMeshComponent->GetMaterial(2),
		DynamicMeshComponent->GetMaterial(3),
		DynamicMeshComponent->GetMaterial(4)
	});

	StaticMeshComponent->SetStaticMesh(StaticMesh);
	DynamicMeshComponent->GetDynamicMesh()->Reset();
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
