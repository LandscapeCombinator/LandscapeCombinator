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
#include "GeometryScript/PolyPathFunctions.h"
#include "Logging/StructuredLog.h"
#include "Polygon2.h"
#include "Algo/Reverse.h"
#include "Stats/Stats.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/MessageDialog.h"


#if WITH_EDITOR

#include "GeometryScript/CreateNewAssetUtilityFunctions.h"
#include "Editor/EditorEngine.h"
extern UNREALED_API class UEditorEngine* GEditor;

#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(Building)

#define LOCTEXT_NAMESPACE "FBuildingFromSplineModule"
#define MILLIMETER_TOLERANCE 0.1

using namespace UE::Geometry;

ABuilding::ABuilding() : AActor()
{
	PrimaryActorTick.bCanEverTick = false;

	EmptySceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("EmptySceneComponent"));
	EmptySceneComponent->SetMobility(EComponentMobility::Static);
	RootComponent = EmptySceneComponent;
	
	DynamicMeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("DynamicMeshComponent"));
	DynamicMeshComponent->SetMobility(EComponentMobility::Static);
	DynamicMeshComponent->SetupAttachment(RootComponent);
	DynamicMeshComponent->SetNumMaterials(0);

	SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
	SplineComponent->SetMobility(EComponentMobility::Static);
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
	BaseClockwiseSplineComponent->SetMobility(EComponentMobility::Static);
	BaseClockwiseSplineComponent->SetupAttachment(RootComponent);
	BaseClockwiseSplineComponent->SetClosedLoop(true);
	BaseClockwiseSplineComponent->ClearSplinePoints();
	BaseClockwiseSplineComponent->SetMobility(EComponentMobility::Static);

	BuildingConfiguration = CreateDefaultSubobject<UBuildingConfiguration>(TEXT("Building Configuration"));
}

void ABuilding::DeleteBuilding()
{
	DestroySplineMeshComponents();
	DestroyInstancedStaticMeshComponents();
	
	if (IsValid(StaticMeshComponent))
	{
		RemoveInstanceComponent(StaticMeshComponent);
		StaticMeshComponent->DestroyComponent();
		StaticMeshComponent = nullptr;
	}
	
	if (IsValid(DynamicMeshComponent))
	{
		DynamicMeshComponent->SetNumMaterials(0);
		DynamicMeshComponent->GetDynamicMesh()->Reset();
	}

	if (Volume.IsValid())
	{
		Volume->Destroy();
		Volume = nullptr;
	}
}

void ABuilding::DestroySplineMeshComponents()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("DestroySplineMeshComponents");

	for (auto& SplineMeshComponent : SplineMeshComponents)
	{
		if (IsValid(SplineMeshComponent))
		{
			RemoveInstanceComponent(SplineMeshComponent);
			SplineMeshComponent->DestroyComponent();
		}
	}
	SplineMeshComponents.Empty();
}

void ABuilding::DestroyInstancedStaticMeshComponents()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("DestroyInstancedStaticMeshComponents");

	for (auto& InstancedStaticMeshComponent : InstancedStaticMeshComponents)
	{
		if (IsValid(InstancedStaticMeshComponent))
		{
			RemoveInstanceComponent(InstancedStaticMeshComponent);
			InstancedStaticMeshComponent->ClearInstances();
			InstancedStaticMeshComponent->DestroyComponent();
		}
	}
	InstancedStaticMeshComponents.Empty();
}

void ABuilding::Destroyed()
{
	DeleteBuilding();
	Super::Destroyed();
}

void ABuilding::ComputeBaseVertices()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("ComputeBaseVertices");

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
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("ComputeOffsetPolygons");

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
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("GetShiftedPoint");

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

void ABuilding::ComputeWindowsPositionsParameterizedByDistance(FWindowsSpecification &WindowsSpecification)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("ComputeWindowsPositionsParameterizedByDistance");

	if (WindowsSpecification.WindowsPlacement == EWindowsPlacement::ParameterizedByDistance)
	{
		WindowsSpecification.WindowsPositions.Empty();
		for (int i = 0; i < SplineComponent->GetNumberOfSplinePoints(); i++)
		{
			float Distance1 = BaseClockwiseSplineComponent->GetDistanceAlongSplineAtSplinePoint(SplineIndexToBaseSplineIndex[i]);
			float Distance2 = BaseClockwiseSplineComponent->GetDistanceAlongSplineAtSplinePoint(SplineIndexToBaseSplineIndex[i + 1]);

			float TotalDistance = Distance2 - Distance1;
			float AvailableDistance = TotalDistance - 2 * WindowsSpecification.MinDistanceWindowToCorner;

			if (AvailableDistance >= WindowsSpecification.WindowsWidth)
			{
				int NumHoles = 1 + (AvailableDistance - WindowsSpecification.WindowsWidth) / (WindowsSpecification.WindowsWidth + WindowsSpecification.MinDistanceWindowToWindow); // >= 1
				int NumSpaces = NumHoles - 1; // >= 0

				float BeginOffset = (AvailableDistance - NumHoles * WindowsSpecification.WindowsWidth - NumSpaces * WindowsSpecification.MinDistanceWindowToWindow) / 2; // >= 0
				float BeginHoleDistance = Distance1 + WindowsSpecification.MinDistanceWindowToCorner + BeginOffset; // >= Distance1 + MinDistanceWindowToCorner

				for (int j = 0; j < NumHoles; j++)
				{
					WindowsSpecification.WindowsPositions.Add(BeginHoleDistance + j * (WindowsSpecification.WindowsWidth + WindowsSpecification.MinDistanceWindowToWindow));
				}
			}
		}
	}
}

TArray<float> ABuilding::GetSafeWindowsPositions(const TArray<float> &WindowsPositions, float WindowsWidth)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("GetSafeWindowsPositions");

	const int NumSplinePoints = SplineComponent->GetNumberOfSplinePoints();
	if (NumSplinePoints == 0)
	{
		UE_LOG(LogBuildingFromSpline, Error, TEXT("Attempting to create a building with an empty spline"))
		return TArray<float>();
	}

	TArray<float> Result;
	float LastWindowPosition = -WindowsWidth;

	// point which is right after the next hole position
	int NextSplinePoint = 0;

	for (auto& WindowPosition : WindowsPositions)
	{
		// Move NextSplinePoint to the next spline point after this hole position
		while (NextSplinePoint < NumSplinePoints && BaseClockwiseSplineComponent->GetDistanceAlongSplineAtSplinePoint(SplineIndexToBaseSplineIndex[NextSplinePoint]) < WindowPosition)
		{
			NextSplinePoint++;
		}

		const float DistanceAtNextSplinePoint = BaseClockwiseSplineComponent->GetDistanceAlongSplineAtSplinePoint(SplineIndexToBaseSplineIndex[NextSplinePoint]);

		if (LastWindowPosition + WindowsWidth > WindowPosition)
		{
			UE_LOG(LogBuildingFromSpline, Warning, TEXT("Skipping hole position %f, which is too close to the previous hole position %f"), WindowPosition, LastWindowPosition);
			continue;
		}

		if (WindowPosition < 0 || WindowPosition > DistanceAtNextSplinePoint - WindowsWidth)
		{
			UE_LOG(LogBuildingFromSpline, Warning, TEXT("Skipping invalid hole position %f, which should be between 0 and %f"), WindowPosition, DistanceAtNextSplinePoint - WindowsWidth);
			continue;
		}

		Result.Add(WindowPosition);
		LastWindowPosition = WindowPosition;
	}

	return Result;
}

FVector2D ABuilding::GetIntersection(FVector2D Point1, FVector2D Direction1, FVector2D Point2, FVector2D Direction2)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("GetIntersection");

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
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("DeflateFrames");

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
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("AppendAlongSpline");

	if (Length <= 0) return;
	
	TArray<FVector2D> Polygon = MakePolygon(bInternalWall, BeginDistance, Length);

	/* Allocate and build WallMesh */

	TObjectPtr<UDynamicMesh> WallMesh = NewObject<UDynamicMesh>(this);

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

	WallMesh->MarkAsGarbage();
}

bool ABuilding::AppendFloors(UDynamicMesh* TargetMesh)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("AppendFloors");

	/* Create one floor tile in FloorMesh */

	TObjectPtr<UDynamicMesh> FloorMesh = NewObject<UDynamicMesh>(this);

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
		GetCeilingMaterialID(), // new material ID
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
		UGeometryScriptLibrary_MeshMaterialFunctions::RemapMaterialIDs(FloorMesh, 0, GetRoofMaterialID());
	
		UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
			TargetMesh, FloorMesh,
			FTransform(FVector(0, 0, MinHeightLocal + BuildingConfiguration->ExtraWallBottom + BuildingConfiguration->NumFloors * BuildingConfiguration->FloorHeight)),
			true
		);
	}

	FloorMesh->MarkAsGarbage();

	return true;
}

bool ABuilding::AppendBuildingWithoutInside(UDynamicMesh* TargetMesh)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("AppendBuildingWithoutInside");

	TObjectPtr<UDynamicMesh> SimpleBuildingMesh = NewObject<UDynamicMesh>(this);

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSimpleExtrudePolygon(
		SimpleBuildingMesh,
		FGeometryScriptPrimitiveOptions(),
		FTransform(FVector(0, 0, 0)),
		BaseVertices2D,
		BuildingConfiguration->ExtraWallBottom +
		BuildingConfiguration->NumFloors * BuildingConfiguration->FloorHeight +
		BuildingConfiguration->ExtraWallTop
	);


	/* Set the Polygroup ID of roof in SimpleBuildingMesh */

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
		SimpleBuildingMesh->MarkAsGarbage();
		return false;
	}

	bool bIsValidPolygroupID;
	UGeometryScriptLibrary_MeshMaterialFunctions::SetPolygroupMaterialID(
		SimpleBuildingMesh,
		FGeometryScriptGroupLayer(),
		PolygroupIDs[0], // TODO: polygroup ID of the sides of the polygon, is there a way to ensure it?
		GetExteriorMaterialID(),
		bIsValidPolygroupID,
		false,
		nullptr
	);

	if (BuildingConfiguration->RoofKind == ERoofKind::None || BuildingConfiguration->RoofKind == ERoofKind::Flat)
	{
		UGeometryScriptLibrary_MeshMaterialFunctions::SetPolygroupMaterialID(
			SimpleBuildingMesh,
			FGeometryScriptGroupLayer(),
			PolygroupIDs[3], // TODO: polygroup ID of the top of the polygon, is there a way to ensure it?
			GetRoofMaterialID(),
			bIsValidPolygroupID,
			false,
			nullptr
		);
	}

	UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
		TargetMesh, SimpleBuildingMesh,
		FTransform(FVector(0, 0, MinHeightLocal)),
		true
	);
	SimpleBuildingMesh->MarkAsGarbage();
	
	return true;
}

TArray<FVector2D> ABuilding::MakePolygon(bool bInternalWall, double BeginDistance, double Length)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("MakePolygon");

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
	if (Result.Num() >= 2 && (!bAddedPoints || !(To2D(BaseClockwiseFrames[LastIndex % NumFrames].GetLocation()) - LastLocation).IsNearlyZero(MILLIMETER_TOLERANCE)))
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
	if (Result.Num() >= 2 && (!bAddedPoints || !(To2D(BaseClockwiseFrames[BeginIndex].GetLocation()) - FirstLocation).IsNearlyZero(MILLIMETER_TOLERANCE)))
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
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("AddSplineMesh");

	if (!StaticMesh) return;

	USplineMeshComponent *SplineMeshComponent = NewObject<USplineMeshComponent>(RootComponent);
	SplineMeshComponents.Add(SplineMeshComponent);
	SplineMeshComponent->SetStaticMesh(StaticMesh);
	SplineMeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	SplineMeshComponent->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	SplineMeshComponent->RegisterComponent();
	AddInstanceComponent(SplineMeshComponent);

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
}

void ABuilding::AppendWallsWithHoles(
	UDynamicMesh* TargetMesh, bool bInternalWall, double WallHeight, double ZOffset,
	FWindowsSpecification &WindowsSpecification, int MaterialID
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("AppendWallsWithHoles1");

	if (WindowsSpecification.bMakeWindowsHoles)
	{
		TArray<float> SafeWindowsPositions = GetSafeWindowsPositions(WindowsSpecification.WindowsPositions, WindowsSpecification.WindowsWidth);

		return AppendWallsWithHoles(
			TargetMesh, bInternalWall, WallHeight,
			WindowsSpecification.WindowsWidth, WindowsSpecification.WindowsHeight,
			WindowsSpecification.WindowsDistanceToFloor, ZOffset,
			SafeWindowsPositions, MaterialID
		);
	}
	else
	{
		return AppendWallsWithHoles(
			TargetMesh, bInternalWall, WallHeight,
			WindowsSpecification.WindowsWidth, WindowsSpecification.WindowsHeight,
			WindowsSpecification.WindowsDistanceToFloor, ZOffset,
			TArray<float>(), MaterialID
		);
	}
}

void ABuilding::AppendWallsWithHoles(
	UDynamicMesh* TargetMesh, bool bInternalWall, double WallHeight,
	double WindowsWidth, double HolesHeight, double HoleDistanceToFloor, double ZOffset,
	const TArray<float> &WindowsPositions,
	int MaterialID
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("AppendWallsWithHoles2");

	const int NumSplinePoints = SplineComponent->GetNumberOfSplinePoints();
	if (NumSplinePoints == 0)
	{
		UE_LOG(LogBuildingFromSpline, Error, TEXT("Attempting to create a building with an empty spline"))
		return;
	}

	if (WindowsWidth < 0)
	{
		UE_LOG(LogBuildingFromSpline, Error, TEXT("Attempting to create a building with negative Windows width"))
		return;
	}

	if (HolesHeight < 0)
	{
		UE_LOG(LogBuildingFromSpline, Error, TEXT("Attempting to create a building with negative Windows height"))
		return;
	}

	if (HoleDistanceToFloor < 0)
	{
		UE_LOG(LogBuildingFromSpline, Error, TEXT("Attempting to create a building with negative Windows height"))
		return;
	}

	if (WallHeight < 0)
	{
		UE_LOG(LogBuildingFromSpline, Error, TEXT("Attempting to create a building with negative Windows height"))
		return;
	}
	
	// before this spline point, all the walls are constructed
	int PreviousSplinePoint = 0;

	// how much distance along the spline has already been built
	float CompletedWall = 0;

	// point which is right after the next hole position
	int NextSplinePoint = 0;
	
	const float SplineLength = SplineComponent->GetSplineLength();
	for (auto& WindowPosition : WindowsPositions)
	{
		// Move NextSplinePoint to the next spline point after this hole position
		while (NextSplinePoint < NumSplinePoints && BaseClockwiseSplineComponent->GetDistanceAlongSplineAtSplinePoint(SplineIndexToBaseSplineIndex[NextSplinePoint]) < WindowPosition)
		{
			NextSplinePoint++;
		}
		
		check(NextSplinePoint > PreviousSplinePoint)

		// build missing walls until NextSplinePoint - 1
		while (PreviousSplinePoint < NextSplinePoint - 1)
		{
			PreviousSplinePoint++;
			const float DistanceAtSplinePoint = BaseClockwiseSplineComponent->GetDistanceAlongSplineAtSplinePoint(SplineIndexToBaseSplineIndex[PreviousSplinePoint]);
			AppendAlongSpline(TargetMesh, bInternalWall, CompletedWall, DistanceAtSplinePoint - CompletedWall, WallHeight, ZOffset, MaterialID);
			CompletedWall = DistanceAtSplinePoint;
		}

		check(NextSplinePoint - 1 == PreviousSplinePoint);

		// build the missing wall section to the hole position
		AppendAlongSpline(TargetMesh, bInternalWall, CompletedWall, WindowPosition - CompletedWall, WallHeight, ZOffset, MaterialID);

		CompletedWall = WindowPosition;

		// if there is enough space for a hole, we make a hole
		const float DistanceAlongSplineNextSplinePoint = BaseClockwiseSplineComponent->GetDistanceAlongSplineAtSplinePoint(SplineIndexToBaseSplineIndex[NextSplinePoint]);
		if (DistanceAlongSplineNextSplinePoint - WindowPosition >= WindowsWidth)
		{
			// below the hole
			AppendAlongSpline(
				TargetMesh, bInternalWall, WindowPosition,
				WindowsWidth, HoleDistanceToFloor, ZOffset, MaterialID
			);

			// above the hole
			const double RemainingHeight = WallHeight - HoleDistanceToFloor - HolesHeight;
			if (RemainingHeight > 0)
			{
				AppendAlongSpline(
					TargetMesh, bInternalWall, WindowPosition,
					WindowsWidth, RemainingHeight, ZOffset + HoleDistanceToFloor + HolesHeight, MaterialID
				);
			}

			CompletedWall += WindowsWidth;
		}
	}

	// build wall after the last window position
	while (PreviousSplinePoint < NumSplinePoints)
	{
		PreviousSplinePoint++;
		const float DistanceAtSplinePoint = SplineComponent->GetDistanceAlongSplineAtSplinePoint(PreviousSplinePoint);
		AppendAlongSpline(TargetMesh, bInternalWall, CompletedWall, DistanceAtSplinePoint - CompletedWall, WallHeight, ZOffset, MaterialID);
		CompletedWall = DistanceAtSplinePoint;
	}

	// build the missing wall section to the hole position
	AppendAlongSpline(TargetMesh, bInternalWall, CompletedWall, SplineLength - CompletedWall, WallHeight, ZOffset, MaterialID);

	// CompletedWall = SplineLength; we're done!
}

void ABuilding::AppendWallsWithHoles(UDynamicMesh* TargetMesh)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("AppendWallsWithHoles3");

	// ExtraWallBottom (inside wall)

	if (BuildingConfiguration->bBuildInternalWalls && BuildingConfiguration->ExtraWallBottom > 0)
	{
		AppendAlongSpline(
			TargetMesh, true, 0, BaseClockwiseSplineComponent->GetSplineLength(),
			BuildingConfiguration->ExtraWallBottom, MinHeightLocal, GetInteriorMaterialID()
		);
	}

	// ExtraWallBottom (outside wall)

	if (BuildingConfiguration->bBuildExternalWalls && BuildingConfiguration->ExtraWallBottom > 0)
	{
		AppendAlongSpline(
			TargetMesh, false, 0, BaseClockwiseSplineComponent->GetSplineLength(),
			BuildingConfiguration->ExtraWallBottom, MinHeightLocal, GetExteriorMaterialID()
		);
	}

	// ExtraWallTop (inside wall)

	if (BuildingConfiguration->bBuildInternalWalls && BuildingConfiguration->ExtraWallTop > 0)
	{
		AppendAlongSpline(
			TargetMesh, true, 0, BaseClockwiseSplineComponent->GetSplineLength(),
			BuildingConfiguration->ExtraWallTop,
			MinHeightLocal + BuildingConfiguration->ExtraWallBottom + BuildingConfiguration->NumFloors * BuildingConfiguration->FloorHeight,
			GetInteriorMaterialID()
		);
	}

	// ExtraWallTop (outside wall)
	
	if (BuildingConfiguration->bBuildExternalWalls && BuildingConfiguration->ExtraWallTop > 0)
	{
		AppendAlongSpline(
			TargetMesh, false, 0, BaseClockwiseSplineComponent->GetSplineLength(),
			BuildingConfiguration->ExtraWallTop, MinHeightLocal + BuildingConfiguration->ExtraWallBottom +
			BuildingConfiguration->NumFloors * BuildingConfiguration->FloorHeight, GetExteriorMaterialID()
		);
	}


	double InternalWallHeight = BuildingConfiguration->FloorHeight;
	if (BuildingConfiguration->bBuildFloorTiles) InternalWallHeight -= BuildingConfiguration->FloorThickness;

	TMap<FWindowsSpecification, UDynamicMesh*> DynamicMeshes;

	auto AddMesh = [this, TargetMesh, InternalWallHeight, &DynamicMeshes](FWindowsSpecification WindowsSpecification, int i)
	{
		if (i >= BuildingConfiguration->NumFloors) return;

		if (!DynamicMeshes.Contains(WindowsSpecification))
		{
			UDynamicMesh *DynamicMesh = NewObject<UDynamicMesh>(this);
			DynamicMeshes.Add(WindowsSpecification, DynamicMesh);
			if (BuildingConfiguration->bBuildInternalWalls)
			{
				AppendWallsWithHoles(DynamicMesh, true, InternalWallHeight, 0, WindowsSpecification, GetInteriorMaterialID());
			}
			if (BuildingConfiguration->bBuildExternalWalls)
			{
				AppendWallsWithHoles(DynamicMesh, false, BuildingConfiguration->FloorHeight, 0, WindowsSpecification, GetExteriorMaterialID());
			}
		}

		UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
			TargetMesh, DynamicMeshes[WindowsSpecification],
			FTransform(FVector(0, 0, MinHeightLocal + BuildingConfiguration->ExtraWallBottom + i * BuildingConfiguration->FloorHeight)),
			true
		);

		return;
	};

	int i = 0;
	for (i = 0; i < BuildingConfiguration->WindowsSpecifications.Num(); i++)
	{
		FWindowsSpecification& WindowsSpecification = BuildingConfiguration->WindowsSpecifications[i];
		AddMesh(WindowsSpecification, i);
	}

	FWindowsSpecification LastWindowsSpecification;
	if (i > 0) LastWindowsSpecification = BuildingConfiguration->WindowsSpecifications[i-1];

	for (; i < BuildingConfiguration->NumFloors; i++)
	{
		AddMesh(LastWindowsSpecification, i);
	}

	for (auto& DynamicMesh : DynamicMeshes)
	{
		if (IsValid(DynamicMesh.Value))
		{
			DynamicMesh.Value->MarkAsGarbage();
		}
	}
}

void ABuilding::AppendRoof(UDynamicMesh* TargetMesh)
{

	/* Allocate RoofMesh */

	TObjectPtr<UDynamicMesh> RoofMesh = NewObject<UDynamicMesh>(this);
	
	/* Top of the roof */
	
	int NumFrames = BaseClockwiseFrames.Num();
	if (NumFrames == 0) return;
	
	const double WallTopHeight = MinHeightLocal + BuildingConfiguration->ExtraWallBottom + BuildingConfiguration->NumFloors * BuildingConfiguration->FloorHeight + BuildingConfiguration->ExtraWallTop;
	const double RoofTopHeight = WallTopHeight + BuildingConfiguration->RoofHeight;

	TArray<FTransform> Frames;
	Frames.Append(BaseClockwiseFrames);

	TArray<FVector2D> RoofPolygon;
	TArray<int> IndexToRoofIndex;

	if (BuildingConfiguration->RoofKind == ERoofKind::InnerSpline)
	{
		DeflateFrames(Frames, RoofPolygon, IndexToRoofIndex, BuildingConfiguration->InnerRoofDistance);
	}
	else
	{
		IndexToRoofIndex.Empty();
		IndexToRoofIndex.SetNum(NumFrames);
	}
	
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

		UGeometryScriptLibrary_MeshMaterialFunctions::RemapMaterialIDs(RoofMesh, 0, GetRoofMaterialID());

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
			UE_LOGFMT(LogBuildingFromSpline, Error, "Internal error: something went wrong with the materials");
			return;
		}

		bool bIsValidPolygroupID;
		UGeometryScriptLibrary_MeshMaterialFunctions::SetPolygroupMaterialID(
			RoofMesh,
			FGeometryScriptGroupLayer(),
			PolygroupIDs[2], // TODO: polygroup ID of the ceiling, is there a way to ensure it?
			GetCeilingMaterialID(), // new material ID
			bIsValidPolygroupID,
			false,
			nullptr
		);
	}

	/* Connection from the walls to the roof, outside */
	
	FTransform BuildingTransform = this->GetTransform();

	FlushPersistentDebugLines(this->GetWorld());
	TArray<FTransform> SweepPath;
	for (int i = 0; i < NumFrames; i++)
	{
		FVector2D WallPoint = ExternalWallPolygon[i];
		FVector2D RoofPoint = RoofPolygon[IndexToRoofIndex[i]];

		FVector Source(WallPoint[0], WallPoint[1], WallTopHeight);
		FVector Target(RoofPoint[0], RoofPoint[1], RoofTopHeight + BuildingConfiguration->RoofThickness);

		const FVector UnitDirection = (Target - Source).GetSafeNormal();
		Source -= UnitDirection * BuildingConfiguration->OuterRoofDistance;
		
		const FVector UnitDirectionXY = FVector(UnitDirection.X, UnitDirection.Y, 0);
		const FVector UnitDirectionYZ = FVector(0, UnitDirection.Y, UnitDirection.Z);

		FTransform NewTransform;
		NewTransform.SetLocation(Source);
		
		FQuat QuatYaw = FQuat::FindBetweenVectors(FVector(0, 1, 0), UnitDirectionXY);
		FQuat QuatRoll = FQuat::FindBetweenVectors(FVector(0, 0, 1), UnitDirection);
		NewTransform.SetRotation(QuatRoll * QuatYaw);
		NewTransform.SetScale3D(FVector(1, 1, FVector::Distance(Source, Target)));

		SweepPath.Add(NewTransform);
	}

	FGeometryScriptPrimitiveOptions Options;
	Options.bFlipOrientation = true;
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolyline(
		RoofMesh, Options,
		FTransform(), { {0, 0}, {0, 0.01}, {0, 0.02}, {0, 0.05}, {0, 0.1}, {0, 0.2}, {0, 0.4}, {0, 0.6}, {0, 0.8},  {0, 0.9},  {0, 0.95},  {0, 0.98},  {0, 0.99}, {0, 1} },
		SweepPath, {}, {}, true
	);

	UGeometryScriptLibrary_MeshMaterialFunctions::RemapMaterialIDs(RoofMesh, 0, GetRoofMaterialID());

	/* Connection from the walls to the roof, inside */

	if (BuildingConfiguration->BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside && !InternalWallPolygon.IsEmpty())
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
			FTransform(), { {0, 0}, {0, 0.01}, {0, 0.02}, {0, 0.05}, {0, 0.1}, {0, 0.2}, {0, 0.4}, {0, 0.6}, {0, 0.8},  {0, 0.9},  {0, 0.95},  {0, 0.98},  {0, 0.99}, {0, 1} },
			SweepPath, {}, {}, true
		);
		UGeometryScriptLibrary_MeshMaterialFunctions::RemapMaterialIDs(RoofMesh, 0, GetInteriorMaterialID());
	}
	

	/* Add the RoofMesh to our TargetMesh */

	UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(TargetMesh, RoofMesh, FTransform(), true);

	RoofMesh->MarkAsGarbage();
}

void ABuilding::ComputeMinMaxHeight()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("ComputeMinMaxHeight");
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

void ABuilding::SetReceivesDecals()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("SetReceivesDecals");
	
	if (IsValid(StaticMeshComponent)) StaticMeshComponent->bReceivesDecals = BuildingConfiguration->bBuildingReceiveDecals; 
	if (IsValid(DynamicMeshComponent)) DynamicMeshComponent->bReceivesDecals = BuildingConfiguration->bBuildingReceiveDecals;

	for (auto& SplineMeshComponent : SplineMeshComponents)
	{
		if (IsValid(SplineMeshComponent)) SplineMeshComponent->bReceivesDecals = BuildingConfiguration->bBuildingReceiveDecals;
	}

	for (auto& InstancedStaticMeshComponent : InstancedStaticMeshComponents)
	{
		if (IsValid(InstancedStaticMeshComponent)) InstancedStaticMeshComponent->bReceivesDecals = BuildingConfiguration->bBuildingReceiveDecals;
	}
}

void ABuilding::GenerateBuilding()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("GenerateBuilding");

	if (bIsGenerating) return;

	bIsGenerating = true;
	
	DeleteBuilding();

	if (!IsValid(BuildingConfiguration))
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("NoBuildingConfiguration", "Internal Error: BuildingConfiguration is null.")
		);
		return;
	}

	if (!IsValid(DynamicMeshComponent))
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("NoBuildingConfiguration", "Internal Error: DynamicMeshComponent is null.")
		);
		return;
	}

	bool bFetchFromUserData = BuildingConfiguration->AutoComputeNumFloors();

	if (!bFetchFromUserData && BuildingConfiguration->bUseRandomNumFloors)
	{
		BuildingConfiguration->NumFloors = UKismetMathLibrary::RandomIntegerInRange(BuildingConfiguration->MinNumFloors, BuildingConfiguration->MaxNumFloors);
	}
	
	AppendBuilding(DynamicMeshComponent->GetDynamicMesh());
	SetReceivesDecals();

	bIsGenerating = false;
}

void ABuilding::AddWindowsMeshes(FWindowsSpecification &WindowsSpecification, int i)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("AddWindowsMeshes");
	
	if (!WindowsSpecification.WindowsMesh) return;

	if (i >= BuildingConfiguration->NumFloors) return;

	TArray<float> SafeWindowsPositions = GetSafeWindowsPositions(WindowsSpecification.WindowsPositions, WindowsSpecification.WindowsWidth);

	switch (WindowsSpecification.WindowsMeshKind)
	{
		case EWindowsMeshKind::None:
			return;

		case EWindowsMeshKind::InstancedStaticMeshComponent:
		{
			UInstancedStaticMeshComponent *ISM = nullptr;
			if (!MeshToISM.Contains(WindowsSpecification.WindowsMesh))
			{
				ISM = NewObject<UInstancedStaticMeshComponent>(RootComponent);
				ISM->SetStaticMesh(WindowsSpecification.WindowsMesh);
				ISM->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
				ISM->CreationMethod = EComponentCreationMethod::UserConstructionScript;
				ISM->RegisterComponent(); 
				AddInstanceComponent(ISM);
				MeshToISM.Add(WindowsSpecification.WindowsMesh, ISM);
				InstancedStaticMeshComponents.Add(ISM);
			}
			else
			{
				ISM = MeshToISM[WindowsSpecification.WindowsMesh];
			}

			if (!ISM)
			{
				UE_LOG(LogBuildingFromSpline, Error, TEXT("Could not create ISM for window meshes"));
				return;
			}
			
			FBox BoundingBox = WindowsSpecification.WindowsMesh->GetBoundingBox();
			for (auto& WindowPosition : SafeWindowsPositions)
			{
				FVector HoleLocation = BaseClockwiseSplineComponent->GetLocationAtDistanceAlongSpline(WindowPosition, ESplineCoordinateSpace::World);
				FVector HoleTangent = BaseClockwiseSplineComponent->GetTangentAtDistanceAlongSpline(WindowPosition, ESplineCoordinateSpace::World);
				HoleLocation.Z += BuildingConfiguration->ExtraWallBottom + i * BuildingConfiguration->FloorHeight + WindowsSpecification.WindowsDistanceToFloor;
				
				FVector Scale(
					WindowsSpecification.WindowsWidth/ BoundingBox.GetExtent()[0] / 2,
					1,
					WindowsSpecification.WindowsHeight / BoundingBox.GetExtent()[2] / 2
				);

				FTransform Transform;
				Transform.SetLocation(HoleLocation);
				Transform.SetRotation(FQuat::FindBetweenVectors(FVector(1, 0, 0), HoleTangent));
				Transform.SetScale3D(Scale);
				ISM->AddInstance(Transform, true);
			}

			return;
		}

		case EWindowsMeshKind::SplineMeshComponent:
		{
			for (auto& WindowPosition : SafeWindowsPositions)
			{
				AddSplineMesh(
					WindowsSpecification.WindowsMesh, WindowPosition, WindowsSpecification.WindowsWidth, WindowsSpecification.WindowsHeight,
					MinHeightLocal + BuildingConfiguration->ExtraWallBottom + i * BuildingConfiguration->FloorHeight + WindowsSpecification.WindowsDistanceToFloor
				);
			}
			return;
		}

		default:
			return;
	}
}
	
void ABuilding::AddWindowsMeshes()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("AddWindowsMeshes");
	
	MeshToISM.Empty();

	int i;
	for (i = 0; i < BuildingConfiguration->WindowsSpecifications.Num(); i++)
	{
		AddWindowsMeshes(BuildingConfiguration->WindowsSpecifications[i], i);
	}

	FWindowsSpecification LastWindowsSpecification;
	if (i > 0) LastWindowsSpecification = BuildingConfiguration->WindowsSpecifications[i-1];

	for (; i < BuildingConfiguration->NumFloors; i++)
	{
		AddWindowsMeshes(LastWindowsSpecification, i);
	}
}

void ABuilding::AppendBuildingStructure(UDynamicMesh* TargetMesh)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("AppendBuildingStructure");

	if (BuildingConfiguration->bAutoPadWallBottom)
	{
		BuildingConfiguration->ExtraWallBottom = MaxHeightLocal - MinHeightLocal + BuildingConfiguration->PadBottom;
	}

	if (
		BuildingConfiguration->BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside ||
		BuildingConfiguration->RoofKind == ERoofKind::Point ||
		BuildingConfiguration->RoofKind == ERoofKind::InnerSpline
	)
	{
		ComputeOffsetPolygons();
	}

	if (BuildingConfiguration->BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside)
	{
		AppendWallsWithHoles(TargetMesh);
		
		if (BuildingConfiguration->bBuildFloorTiles || BuildingConfiguration->RoofKind == ERoofKind::Flat)
		{
			if (!AppendFloors(TargetMesh)) return;
		}

		DynamicMeshComponent->SetMaterial(0, BuildingConfiguration->FloorMaterial);
		DynamicMeshComponent->SetMaterial(1, BuildingConfiguration->CeilingMaterial);
		DynamicMeshComponent->SetMaterial(2, BuildingConfiguration->ExteriorMaterial);
		DynamicMeshComponent->SetMaterial(3, BuildingConfiguration->InteriorMaterial);
		DynamicMeshComponent->SetMaterial(4, BuildingConfiguration->RoofMaterial);
	}
	else
	{
		AppendBuildingWithoutInside(TargetMesh);

		DynamicMeshComponent->SetMaterial(0, BuildingConfiguration->ExteriorMaterial);
		DynamicMeshComponent->SetMaterial(1, BuildingConfiguration->RoofMaterial);
	}
}

void ABuilding::AppendBuilding(UDynamicMesh* TargetMesh)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("AppendBuilding");

	ComputeMinMaxHeight();
	ComputeBaseVertices();

	for (auto& WindowsSpecification : BuildingConfiguration->WindowsSpecifications)
	{
		ComputeWindowsPositionsParameterizedByDistance(WindowsSpecification);
	}

	AppendBuildingStructure(TargetMesh);
		
	if (
		BuildingConfiguration->RoofKind == ERoofKind::Point ||
		BuildingConfiguration->RoofKind == ERoofKind::InnerSpline
	)
	{
		AppendRoof(TargetMesh);
	}

	
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("AppendBuilding");

		UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(TargetMesh, FGeometryScriptSplitNormalsOptions(), FGeometryScriptCalculateNormalsOptions());
	}

#if WITH_EDITOR
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

#else

	if (BuildingConfiguration->bConvertToStaticMesh && BuildingConfiguration->bConvertToVolume)
	{
		UE_LOG(LogBuildingFromSpline, Warning, TEXT("Cannot convert building to static mesh or volume at runtime"));
	}

#endif

	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("AppendBuilding");

		if (BuildingConfiguration->bAutoGenerateXAtlasMeshUVs)
		{
			UGeometryScriptLibrary_MeshUVFunctions::AutoGenerateXAtlasMeshUVs(TargetMesh, 0, FGeometryScriptXAtlasOptions());
		}

	}


	AddWindowsMeshes();
	BaseClockwiseSplineComponent->ClearSplinePoints();

#if WITH_EDITOR
	GEditor->NoteSelectionChange();
#endif
}

#if WITH_EDITOR

void ABuilding::ConvertToStaticMesh()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("ConvertToStaticMesh");
	GenerateStaticMesh();
	DynamicMeshComponent->GetDynamicMesh()->Reset();
}

void ABuilding::GenerateStaticMesh()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("GenerateStaticMesh");
	EGeometryScriptOutcomePins Outcome;

	FGeometryScriptCreateNewStaticMeshAssetOptions Options;
	FMeshNaniteSettings NaniteSettings;
	NaniteSettings.bEnabled = true;
	NaniteSettings.bPreserveArea = true;

	FString Unused;
	if (StaticMeshPath.IsEmpty())
	{
		UGeometryScriptLibrary_CreateNewAssetFunctions::CreateUniqueNewAssetPathName(FString("/Game/Buildings"), FString("SM_Building"), StaticMeshPath, Unused, FGeometryScriptUniqueAssetNameOptions(), Outcome);

		if (Outcome != EGeometryScriptOutcomePins::Success)
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				LOCTEXT("StaticMeshError", "Internal error while creating unique asset path name.")
			);
			return;
		}
	}

	UStaticMesh *StaticMesh = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(
		DynamicMeshComponent->GetDynamicMesh(), StaticMeshPath,
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
		FStaticMaterial Material(DynamicMeshComponent->GetMaterial(i));
		Materials.Add(Material);
	}
	StaticMesh->SetStaticMaterials(Materials);
	StaticMesh->InitResources(); // calls StaticMesh->UpdateUVChannelData() to avoid Error: Ensure condition failed: GetStaticMaterials()[MaterialIndex].UVChannelData.bInitialized

	StaticMeshComponent = NewObject<UStaticMeshComponent>(RootComponent);
	StaticMeshComponent->SetStaticMesh(StaticMesh);
	StaticMeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	StaticMeshComponent->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	StaticMeshComponent->RegisterComponent();

	AddInstanceComponent(StaticMeshComponent);
}

void ABuilding::ConvertToVolume()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("ConvertToVolume");

	GenerateVolume();
	DynamicMeshComponent->GetDynamicMesh()->Reset();
}

void ABuilding::GenerateVolume()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("GenerateVolume");

	if (Volume.IsValid())
	{
		Volume->Destroy();
	}

	FGeometryScriptCreateNewVolumeFromMeshOptions Options;
	EGeometryScriptOutcomePins Outcome;
	Volume = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewVolumeFromMesh(
		DynamicMeshComponent->GetDynamicMesh(),
		this->GetWorld(),
		this->GetTransform(),
		this->GetActorNameOrLabel() + "Volume",
		Options, Outcome
	);

	if (Outcome != EGeometryScriptOutcomePins::Success || !Volume.IsValid())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("StaticMeshError", "Internal error while creating volume.")
		);
		return;
	}
	
	Volume->SetFolderPath(FName(this->GetFolderPath().ToString() + "Volume"));

	UOSMUserData *BuildingOSMUserData = Cast<UOSMUserData>(this->GetRootComponent()->GetAssetUserDataOfClass(UOSMUserData::StaticClass()));
	UOSMUserData *VolumeOSMUserData = NewObject<UOSMUserData>(Volume->GetRootComponent());
	VolumeOSMUserData->Fields = BuildingOSMUserData->Fields;
	Volume->GetRootComponent()->AddAssetUserData(VolumeOSMUserData);
}

void ABuilding::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (IsValid(BuildingConfiguration) && BuildingConfiguration->bGenerateWhenModified)
	{
		GenerateBuilding();
	}
}

#endif

#undef LOCTEXT_NAMESPACE
