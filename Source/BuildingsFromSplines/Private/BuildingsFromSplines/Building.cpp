// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.


#include "BuildingsFromSplines/Building.h"
#include "BuildingsFromSplines/LogBuildingsFromSplines.h"
#include "OSMUserData/OSMUserData.h"
#include "LCCommon/LCBlueprintLibrary.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"

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
#include "Engine/World.h"


#if WITH_EDITOR

#include "GeometryScript/CreateNewAssetUtilityFunctions.h"
#include "Editor/EditorEngine.h"

extern UNREALED_API class UEditorEngine* GEditor;

#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(Building)

#define LOCTEXT_NAMESPACE "FBuildingsFromSplinesModule"
#define MILLIMETER 0.1

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
	SplineComponent->AddSplinePoint(FVector(0, 1200, 0), ESplineCoordinateSpace::Local, false);
	SplineComponent->AddSplinePoint(FVector(1200, 1200, 0), ESplineCoordinateSpace::Local, false);
	SplineComponent->AddSplinePoint(FVector(1200, 0, 0), ESplineCoordinateSpace::Local, false);
	SplineComponent->SetSplinePointType(0, ESplinePointType::Linear, false);
	SplineComponent->SetSplinePointType(1, ESplinePointType::Linear, false);
	SplineComponent->SetSplinePointType(2, ESplinePointType::Linear, false);
	SplineComponent->SetSplinePointType(3, ESplinePointType::Linear, false);
	SplineComponent->UpdateSpline();
	SplineComponent->bSplineHasBeenEdited = true;

	BaseClockwiseSplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("BaseClockwiseSplineComponent"));
	BaseClockwiseSplineComponent->SetMobility(EComponentMobility::Static);
	BaseClockwiseSplineComponent->SetupAttachment(RootComponent);
	BaseClockwiseSplineComponent->SetClosedLoop(true);
	BaseClockwiseSplineComponent->ClearSplinePoints();

	BuildingConfiguration = CreateDefaultSubobject<UBuildingConfiguration>(TEXT("Building Configuration"));

	DummyFiller.WallSegmentKind = EWallSegmentKind::Wall;
	DummyFiller.bAutoExpand = true;
	DummyFiller.SegmentLength = 0;
}

bool ABuilding::Cleanup_Implementation(bool bSkipPrompt)
{
	Modify();

	if (!DeleteGeneratedObjects(bSkipPrompt)) return false;

	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors);

	for (AActor* Actor : AttachedActors)
	{
		if (IsValid(Actor)) Actor->Destroy();
	}

	if (IsValid(DynamicMeshComponent))
	{
		DynamicMeshComponent->SetNumMaterials(0);
		DynamicMeshComponent->GetDynamicMesh()->Reset();
	}

	if (IsValid(BaseClockwiseSplineComponent))
		BaseClockwiseSplineComponent->ClearSplinePoints();

	WallSegmentsAtSplinePoint.Empty();
	FillersSizeAtSplinePoint.Empty();
	Volume = nullptr;
	SplineMeshComponents.Empty();
	InstancedStaticMeshComponents.Empty();
	MeshToISM.Empty();

	return true;
}

void ABuilding::DeleteBuilding()
{
	Execute_Cleanup(this, true);
}

void ABuilding::Destroyed()
{
	Execute_Cleanup(this, true);
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
		for (int j = 0; j < InternalBuildingConfiguration->WallSubdivisions; j++)
		{
			FVector SubLocation =
				Transform.TransformPosition(
					SplineComponent->GetLocationAtDistanceAlongSpline(
						Distance1 + (j + 1) * Length / (InternalBuildingConfiguration->WallSubdivisions + 1),
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

void ABuilding::AddExternalThickness(double Thickness)
{
	if (ExternalWallPolygons.Contains(Thickness)) return;
	int NumFrames = BaseClockwiseFrames.Num();
	ExternalWallPolygons.Add(Thickness, TArray<FVector2D>());
	ExternalWallPolygons[Thickness].Reserve(NumFrames);
	for (int i = 0; i < NumFrames; i++)
	{
		ExternalWallPolygons[Thickness].Add(GetShiftedPoint(BaseClockwiseFrames, i, - Thickness, true));
	}
}

void ABuilding::AddInternalThickness(double Thickness)
{
	if (InternalWallPolygons.Contains(Thickness)) return;
	int NumFrames = BaseClockwiseFrames.Num();
	InternalWallPolygons.Add(Thickness, TArray<FVector2D>());
	InternalWallPolygons[Thickness].Reserve(NumFrames);
	IndexToInternalIndex.Add(Thickness, TArray<int>());
	DeflateFrames(
		BaseClockwiseFrames,
		InternalWallPolygons[Thickness],
		IndexToInternalIndex[Thickness],
		Thickness
	);
}

void ABuilding::ComputeOffsetPolygons()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("ComputeOffsetPolygons");

	if (BaseClockwiseFrames.Num() == 0) return;
	
	ExternalWallPolygons.Empty();
	InternalWallPolygons.Empty();
	IndexToInternalIndex.Empty();

	AddExternalThickness(InternalBuildingConfiguration->ExternalWallThickness);
	AddInternalThickness(InternalBuildingConfiguration->InternalWallThickness);

	for (auto &LevelDescriptionIndex : LevelDescriptionsIndices)
	{
		for (auto &WallSegment : ExpandedLevels[LevelDescriptionIndex].WallSegments)
		{
			if (WallSegment.bOverrideWallThickness)
			{
				AddExternalThickness(WallSegment.ExternalWallThickness);
				AddInternalThickness(WallSegment.InternalWallThickness);
			}
		}
	}
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

FVector2D ABuilding::GetIntersection(FVector2D Point1, FVector2D Direction1, FVector2D Point2, FVector2D Direction2)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("GetIntersection");

	// Point1.X + t * Direction1.X = Point2.X + t' * Direction2.X
	// Point1.Y + t * Direction1.Y = Point2.Y + t' * Direction2.Y

	// t * Direction1.X - t' * Direction2.X = Point2.X - Point1.X
	// t * Direction1.Y - t' * Direction2.Y = Point2.Y - Point1.Y

	double Det = -Direction1.X * Direction2.Y + Direction1.Y * Direction2.X;

	FVector2D Intersection;

	if (FMath::IsNearlyEqual(Det, 0, MILLIMETER))
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
				if (Distance < Offset * Offset - MILLIMETER)
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

void ABuilding::AppendAlongSpline(UDynamicMesh* TargetMesh, bool bInternalWall, double BeginDistance, double Length, double Height, double ZOffset, double Thickness, int MaterialID)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("AppendAlongSpline");

	if (Length <= 0) return;
	
	TArray<FVector2D> Polygon = MakePolygon(bInternalWall, BeginDistance, Length, Thickness);

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

bool SetPolygroupMaterialID(UDynamicMesh *Mesh, int Index, int MaterialID)
{
	FGeometryScriptIndexList PolygroupIDs0;
	UGeometryScriptLibrary_MeshPolygroupFunctions::GetPolygroupIDsInMesh(
		Mesh,
		FGeometryScriptGroupLayer(),
		PolygroupIDs0
	);

	TArray<int> PolygroupIDs = *PolygroupIDs0.List;

	if (Index >= PolygroupIDs.Num())
	{
		UE_LOG(LogBuildingsFromSplines, Error, TEXT("Internal error: something went wrong with the materials"));
		return false;
	}

	bool bIsValidPolygroupID;
	UGeometryScriptLibrary_MeshMaterialFunctions::SetPolygroupMaterialID(
		Mesh,
		FGeometryScriptGroupLayer(),
		PolygroupIDs[Index],
		MaterialID,
		bIsValidPolygroupID,
		false,
		nullptr
	);

	return true;
}

bool ABuilding::AppendFloors(UDynamicMesh* TargetMesh)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("AppendFloors");

	/* Create one floor tile in FloorMesh */

	TObjectPtr<UDynamicMesh> FloorMesh = nullptr;
	
	if (!Concurrency::RunOnGameThreadAndWait([this, &FloorMesh]() {
		FloorMesh = NewObject<UDynamicMesh>(this);
		return IsValid(FloorMesh);
	}))
	{
		LCReporter::ShowError(
			LOCTEXT("NewObjectError", "Internal Error: Could not create new object")
		);
		return false;
	}

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSimpleExtrudePolygon(
		FloorMesh,
		FGeometryScriptPrimitiveOptions(),
		FTransform(),
		BaseVertices2D,
		1
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
		UE_LOGFMT(LogBuildingsFromSplines, Error, "Internal error: something went wrong with the floor tile materials");
		return false;
	}

	/* Add several copies of the FloorMesh at every floor */

	double CurrentHeight = MinHeightLocal + InternalBuildingConfiguration->ExtraWallBottom;
	for (auto &LevelDescriptionIndex: LevelDescriptionsIndices)
	{
		if (InternalBuildingConfiguration->bBuildFloorTiles)
		{
			bool bIsValidPolygroupID;
			UGeometryScriptLibrary_MeshMaterialFunctions::SetPolygroupMaterialID(
				FloorMesh,
				FGeometryScriptGroupLayer(),
				PolygroupIDs[2], // TODO: polygroup ID of the ceiling, is there a way to ensure it?
				ExpandedLevels[LevelDescriptionIndex].UnderFloorMaterialIndex, // new material ID
				bIsValidPolygroupID,
				false,
				nullptr
			);
			UGeometryScriptLibrary_MeshMaterialFunctions::SetPolygroupMaterialID(
				FloorMesh,
				FGeometryScriptGroupLayer(),
				PolygroupIDs[3], // TODO: polygroup ID of the floor, is there a way to ensure it?
				ExpandedLevels[LevelDescriptionIndex].FloorMaterialIndex, // new material ID
				bIsValidPolygroupID,
				false,
				nullptr
			);
			UGeometryScriptLibrary_MeshMaterialFunctions::SetPolygroupMaterialID(
				FloorMesh,
				FGeometryScriptGroupLayer(),
				PolygroupIDs[0], // TODO: maybe the sides of the floor
				ExpandedLevels[LevelDescriptionIndex].FloorMaterialIndex, // new material ID
				bIsValidPolygroupID,
				false,
				nullptr
			);
			UGeometryScriptLibrary_MeshMaterialFunctions::SetPolygroupMaterialID(
				FloorMesh,
				FGeometryScriptGroupLayer(),
				PolygroupIDs[1], // TODO: maybe the sides of the floor
				ExpandedLevels[LevelDescriptionIndex].FloorMaterialIndex, // new material ID
				bIsValidPolygroupID,
				false,
				nullptr
			);

			UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
				TargetMesh, FloorMesh,
				FTransform(
					FRotator::ZeroRotator,
					FVector(0, 0, CurrentHeight),
					FVector(1, 1, ExpandedLevels[LevelDescriptionIndex].FloorThickness)
				),
				true
			);
		}

		// we update the CurrentHeight outside the "if" to use for the flat roof
		CurrentHeight += ExpandedLevels[LevelDescriptionIndex].LevelHeight;
	}
	

	/* Add the last floor with roof material for flat roof kind */
	if (InternalBuildingConfiguration->RoofKind == ERoofKind::Flat)
	{
		UGeometryScriptLibrary_MeshMaterialFunctions::RemapMaterialIDs(FloorMesh, 0, InternalBuildingConfiguration->RoofMaterialIndex);
		if (!SetPolygroupMaterialID(FloorMesh, 2, InternalBuildingConfiguration->UnderRoofMaterialIndex)) return false;
	
		UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
			TargetMesh, FloorMesh,
			FTransform(
				FRotator::ZeroRotator,
				FVector(0, 0, CurrentHeight),
				FVector(1, 1, 20)
			),
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
		InternalBuildingConfiguration->ExtraWallBottom + LevelsHeightsSum + InternalBuildingConfiguration->ExtraWallTop
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
		UE_LOG(LogBuildingsFromSplines, Error, TEXT("Internal error: something went wrong with the simple building materials"));
		SimpleBuildingMesh->MarkAsGarbage();
		return false;
	}

	bool bIsValidPolygroupID;
	UGeometryScriptLibrary_MeshMaterialFunctions::SetPolygroupMaterialID(
		SimpleBuildingMesh,
		FGeometryScriptGroupLayer(),
		PolygroupIDs[0], // TODO: polygroup ID of the sides of the polygon, is there a way to ensure it?
		InternalBuildingConfiguration->ExteriorMaterialIndex,
		bIsValidPolygroupID,
		false,
		nullptr
	);

	if (InternalBuildingConfiguration->RoofKind == ERoofKind::None || InternalBuildingConfiguration->RoofKind == ERoofKind::Flat)
	{
		UGeometryScriptLibrary_MeshMaterialFunctions::SetPolygroupMaterialID(
			SimpleBuildingMesh,
			FGeometryScriptGroupLayer(),
			PolygroupIDs[3], // TODO: polygroup ID of the top of the polygon, is there a way to ensure it?
			InternalBuildingConfiguration->RoofMaterialIndex,
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

TArray<FVector2D> ABuilding::MakePolygon(bool bInternalWall, double BeginDistance, double Length, double Thickness)
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
		if (Result.IsEmpty() || !(Result.Last() - Location).IsNearlyZero(MILLIMETER))
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
	if (Result.Num() >= 2 && (!bAddedPoints || !(To2D(BaseClockwiseFrames[LastIndex % NumFrames].GetLocation()) - LastLocation).IsNearlyZero(MILLIMETER)))
	{
		// add a shifted location corresponding to LastLocation, on the internal or external wall
		FVector2D PrevPoint = Result.Last(1);
		FVector2D WallDirection = (LastLocation - PrevPoint).GetSafeNormal();
		FVector2D InsideDirection = WallDirection.GetRotated(90);
		if (bInternalWall)
			Add(LastLocation + Thickness * InsideDirection);
		else
			Add(LastLocation - Thickness * InsideDirection);
	}

	if (bInternalWall && !InternalWallPolygons[Thickness].IsEmpty())
		for (i = LastIndex; i >= BeginIndex; i--)
			Add(InternalWallPolygons[Thickness][IndexToInternalIndex[Thickness][i % NumFrames]]);

	if (!bInternalWall)
		for (i = LastIndex; i >= BeginIndex; i--)
			Add(ExternalWallPolygons[Thickness][i % NumFrames]);
	
	// If the first point is not at the beginning of the spline, and
	// If nothing was added in the loop, or if the FirstLocation is distinct from what was added
	if (Result.Num() >= 2 && (!bAddedPoints || !(To2D(BaseClockwiseFrames[BeginIndex].GetLocation()) - FirstLocation).IsNearlyZero(MILLIMETER)))
	{
		// add a shifted location corresponding to LastLocation, on the internal or external wall
		FVector2D NextPoint = Result[1];
		FVector2D WallDirection = (NextPoint - FirstLocation).GetSafeNormal();
		FVector2D InsideDirection = WallDirection.GetRotated(90);

		if (bInternalWall)
			Add(FirstLocation + Thickness * InsideDirection);
		else
			Add(FirstLocation - Thickness * InsideDirection);
	}

	if (!bInternalWall)
	{
		Algo::Reverse(Result);
	}

	return Result;
}

void ABuilding::AddSplineMesh(UStaticMesh* StaticMesh, double BeginDistance, double Length, double Thickness, double Height, FVector Offset, ESplineMeshAxis::Type SplineMeshAxis)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("AddSplineMesh");

	if (!StaticMesh) return;

	USplineMeshComponent *SplineMeshComponent = NewObject<USplineMeshComponent>(RootComponent);
	SplineMeshComponents.Add(SplineMeshComponent);
	SplineMeshComponent->SetStaticMesh(StaticMesh);
	SplineMeshComponent->SetForwardAxis(SplineMeshAxis, false);
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
	int YIndex, ZIndex;
	if (SplineMeshAxis == ESplineMeshAxis::X)
	{
		YIndex = 1;
		ZIndex = 2;
	}
	else if (SplineMeshAxis == ESplineMeshAxis::Y)
	{
		YIndex = 2;
		ZIndex = 0;
	}
	else
	{
		YIndex = 0;
		ZIndex = 1;
	}
	double NewScaleY = Thickness > 0 ? Thickness / BoundingBox.GetExtent()[YIndex] / 2 : 1;
	double NewScaleZ = Height > 0 ? Height / BoundingBox.GetExtent()[ZIndex] / 2 : 1;
	SplineMeshComponent->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent, false);
	SplineMeshComponent->SetStartScale( { NewScaleY, NewScaleZ }, false);
	SplineMeshComponent->SetEndScale( { NewScaleY, NewScaleZ }, false);
	SplineMeshComponent->SetRelativeLocation(Offset - FVector(0, 0, MinHeightLocal));

	SplineMeshComponent->MarkRenderStateDirty();
}

bool ABuilding::AppendWallsWithHoles(UDynamicMesh* TargetMesh, bool bInternalWall, double ZOffset, int LevelDescriptionIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("AppendWallsWithHoles");

	double OffsetIfInternal = 0;
	if (bInternalWall && InternalBuildingConfiguration->bBuildFloorTiles)
	{
		OffsetIfInternal = ExpandedLevels[LevelDescriptionIndex].FloorThickness;
	}

	// original number of spline points (without subdivisions)
	const int NumSplinePoints = SplineComponent->GetNumberOfSplinePoints();
	const int MaxIterations = ExpandedLevels[LevelDescriptionIndex].bResetWallSegmentsOnCorners ? NumSplinePoints : 1;
	for (int i = 0; i < MaxIterations; i++)
	{
		double CurrentDistance = BaseClockwiseSplineComponent->GetDistanceAlongSplineAtSplinePoint(SplineIndexToBaseSplineIndex[i]);
		for (auto WallSegment : WallSegmentsAtSplinePoint[LevelDescriptionIndex][i])
		{
			double FinalSegmentLength = WallSegment.bAutoExpand ? FillersSizeAtSplinePoint[LevelDescriptionIndex][i] : WallSegment.SegmentLength;
			if (FinalSegmentLength <= 0) continue;
			
			double Thickness = bInternalWall ? InternalBuildingConfiguration->InternalWallThickness : InternalBuildingConfiguration->ExternalWallThickness;
			if (WallSegment.bOverrideWallThickness)
			{
				Thickness = bInternalWall ? WallSegment.InternalWallThickness : WallSegment.ExternalWallThickness;
			}
			if (Thickness <= 0)
			{
				CurrentDistance += FinalSegmentLength;
				continue;
			}

			switch (WallSegment.WallSegmentKind)
			{
			case EWallSegmentKind::Wall:
			{
				AppendAlongSpline(
					TargetMesh, bInternalWall, CurrentDistance, FinalSegmentLength,
					ExpandedLevels[LevelDescriptionIndex].LevelHeight - OffsetIfInternal, ZOffset + OffsetIfInternal, Thickness,
					bInternalWall ? WallSegment.InteriorWallMaterialIndex : WallSegment.ExteriorWallMaterialIndex
				);
				CurrentDistance += FinalSegmentLength;
				break;
			}
			case EWallSegmentKind::Hole:
			{
				// below the hole
				double BelowHoleHeight = FMath::Max(0, WallSegment.HoleDistanceToFloor - OffsetIfInternal);
				if (BelowHoleHeight > 0)
				{
					AppendAlongSpline(
						TargetMesh, bInternalWall, CurrentDistance, FinalSegmentLength,
						BelowHoleHeight, ZOffset + OffsetIfInternal, Thickness,
						bInternalWall ? WallSegment.UnderHoleInteriorMaterialIndex : WallSegment.UnderHoleExteriorMaterialIndex
					);
				}

				// above the hole
				const double RemainingHeight = ExpandedLevels[LevelDescriptionIndex].LevelHeight - OffsetIfInternal - BelowHoleHeight - WallSegment.HoleHeight;
				if (RemainingHeight > 0)
				{
					AppendAlongSpline(
						TargetMesh, bInternalWall, CurrentDistance, FinalSegmentLength,
						RemainingHeight, ZOffset + OffsetIfInternal + BelowHoleHeight + WallSegment.HoleHeight, Thickness,
						bInternalWall ? WallSegment.OverHoleInteriorMaterialIndex : WallSegment.OverHoleExteriorMaterialIndex
					);
				}

				CurrentDistance += FinalSegmentLength;
				break;
			}
			}
		}
	}

	return true;
}

bool ABuilding::AppendWallsWithHoles(UDynamicMesh* TargetMesh)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("AppendWallsWithHoles3");

	// ExtraWallBottom (inside wall)

	if (InternalBuildingConfiguration->InternalWallThickness > 0 && InternalBuildingConfiguration->ExtraWallBottom > 0)
	{
		AppendAlongSpline(
			TargetMesh, true, 0, BaseClockwiseSplineComponent->GetSplineLength(),
			InternalBuildingConfiguration->ExtraWallBottom, MinHeightLocal,
			InternalBuildingConfiguration->InternalWallThickness, InternalBuildingConfiguration->InteriorMaterialIndex
		);
	}

	// ExtraWallBottom (outside wall)

	if (InternalBuildingConfiguration->ExternalWallThickness > 0 && InternalBuildingConfiguration->ExtraWallBottom > 0)
	{
		AppendAlongSpline(
			TargetMesh, false, 0, BaseClockwiseSplineComponent->GetSplineLength(),
			InternalBuildingConfiguration->ExtraWallBottom, MinHeightLocal,
			InternalBuildingConfiguration->ExternalWallThickness, InternalBuildingConfiguration->ExteriorMaterialIndex
		);
	}

	// ExtraWallTop (inside wall)

	if (InternalBuildingConfiguration->InternalWallThickness > 0 && InternalBuildingConfiguration->ExtraWallTop > 0)
	{
		AppendAlongSpline(
			TargetMesh, true, 0, BaseClockwiseSplineComponent->GetSplineLength(),
			InternalBuildingConfiguration->ExtraWallTop,
			MinHeightLocal + InternalBuildingConfiguration->ExtraWallBottom + LevelsHeightsSum,
			InternalBuildingConfiguration->InternalWallThickness, InternalBuildingConfiguration->InteriorMaterialIndex
		);
	}

	// ExtraWallTop (outside wall)
	
	if (InternalBuildingConfiguration->ExternalWallThickness > 0 && InternalBuildingConfiguration->ExtraWallTop > 0)
	{
		AppendAlongSpline(
			TargetMesh, false, 0, BaseClockwiseSplineComponent->GetSplineLength(),
			InternalBuildingConfiguration->ExtraWallTop,
			MinHeightLocal + InternalBuildingConfiguration->ExtraWallBottom + LevelsHeightsSum,
			InternalBuildingConfiguration->ExternalWallThickness, InternalBuildingConfiguration->ExteriorMaterialIndex
		);
	}

	TMap<int, UDynamicMesh*> LevelMeshes;

	auto AddMesh = [this, TargetMesh, &LevelMeshes](int LevelDescriptionIndex, double CurrentHeight) -> bool
	{
		if (!LevelMeshes.Contains(LevelDescriptionIndex))
		{
			UDynamicMesh *DynamicMesh = NewObject<UDynamicMesh>(this);
			LevelMeshes.Add(LevelDescriptionIndex, DynamicMesh);

			if (InternalBuildingConfiguration->InternalWallThickness > 0)
			{
				if (!AppendWallsWithHoles(DynamicMesh, true, 0, LevelDescriptionIndex)) return false;
			}
			if (InternalBuildingConfiguration->ExternalWallThickness > 0)
			{
				if (!AppendWallsWithHoles(DynamicMesh, false, 0, LevelDescriptionIndex)) return false;
			}
		}

		UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
			TargetMesh, LevelMeshes[LevelDescriptionIndex],
			FTransform(FVector(0, 0, CurrentHeight)),
			true
		);

		return true;
	};
	
	double CurrentHeigth = MinHeightLocal + InternalBuildingConfiguration->ExtraWallBottom;
	for (auto &LevelDescriptionIndex : LevelDescriptionsIndices)
	{
		if (!AddMesh(LevelDescriptionIndex, CurrentHeigth)) return false;
		CurrentHeigth += ExpandedLevels[LevelDescriptionIndex].LevelHeight;
	}

	for (auto& [_, LevelMesh] : LevelMeshes)
	{
		if (IsValid(LevelMesh))
		{
			LevelMesh->MarkAsGarbage();
		}
	}

	return true;
}

void ABuilding::AppendRoof(UDynamicMesh* TargetMesh)
{
	/* Allocate RoofMesh */

	TObjectPtr<UDynamicMesh> RoofMesh = NewObject<UDynamicMesh>(this);
	
	/* Top of the roof */
	
	int NumFrames = BaseClockwiseFrames.Num();
	if (NumFrames == 0) return;
	
	const double WallTopHeight = MinHeightLocal + InternalBuildingConfiguration->ExtraWallBottom + LevelsHeightsSum + InternalBuildingConfiguration->ExtraWallTop;
	const double RoofTopHeight = WallTopHeight + InternalBuildingConfiguration->RoofHeight;

	TArray<FTransform> Frames;
	Frames.Append(BaseClockwiseFrames);

	TArray<FVector2D> RoofPolygon;
	TArray<int> IndexToRoofIndex;

	if (InternalBuildingConfiguration->RoofKind == ERoofKind::InnerSpline)
	{
		DeflateFrames(Frames, RoofPolygon, IndexToRoofIndex, InternalBuildingConfiguration->InnerRoofDistance);
	}
	else
	{
		IndexToRoofIndex.Empty();
		IndexToRoofIndex.SetNum(NumFrames);
	}
	
	if (RoofPolygon.IsEmpty() || InternalBuildingConfiguration->RoofKind == ERoofKind::Point)
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
			InternalBuildingConfiguration->RoofThickness
		);

		UGeometryScriptLibrary_MeshMaterialFunctions::RemapMaterialIDs(RoofMesh, 0, InternalBuildingConfiguration->RoofMaterialIndex);
		if (!SetPolygroupMaterialID(RoofMesh, 2, InternalBuildingConfiguration->UnderRoofMaterialIndex)) return;
	}

	/* Connection from the walls to the roof, outside */
	
	FTransform BuildingTransform = this->GetTransform();

	
	double ExternalWallThickness = InternalBuildingConfiguration->ExternalWallThickness;
	double InternalWallThickness = InternalBuildingConfiguration->InternalWallThickness;

	TArray<FTransform> SweepPath;
	for (int i = 0; i < NumFrames; i++)
	{
		FVector2D WallPoint = ExternalWallPolygons[ExternalWallThickness][i];
		FVector2D RoofPoint = RoofPolygon[IndexToRoofIndex[i]];

		FVector Source(WallPoint[0], WallPoint[1], WallTopHeight);
		FVector Target(RoofPoint[0], RoofPoint[1], RoofTopHeight + InternalBuildingConfiguration->RoofThickness);

		const FVector UnitDirection = (Target - Source).GetSafeNormal();
		Source -= UnitDirection * InternalBuildingConfiguration->OuterRoofDistance;
		
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

	UGeometryScriptLibrary_MeshMaterialFunctions::RemapMaterialIDs(RoofMesh, 0, InternalBuildingConfiguration->RoofMaterialIndex);

	/* Connection from the walls to the roof, inside */

	if (
		InternalBuildingConfiguration->BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside &&
		!InternalWallPolygons[InternalWallThickness].IsEmpty()
	)
	{
		SweepPath.Empty();
		for (int i = 0; i < NumFrames; i++)
		{
			FVector2D WallPoint = InternalWallPolygons[InternalWallThickness][IndexToInternalIndex[InternalWallThickness][i]];
			FVector2D RoofPoint = RoofPolygon[IndexToRoofIndex[i]];

			FVector Source(WallPoint[0], WallPoint[1], WallTopHeight);
			const FVector Target(RoofPoint[0], RoofPoint[1], RoofTopHeight);

			const FVector UnitDirection = (Target - Source).GetSafeNormal();
			Source -= UnitDirection * InternalBuildingConfiguration->OuterRoofDistance;

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
		UGeometryScriptLibrary_MeshMaterialFunctions::RemapMaterialIDs(RoofMesh, 0, InternalBuildingConfiguration->InteriorMaterialIndex);
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
	
	if (IsValid(StaticMeshComponent)) StaticMeshComponent->bReceivesDecals = InternalBuildingConfiguration->bBuildingReceiveDecals; 
	if (IsValid(DynamicMeshComponent)) DynamicMeshComponent->bReceivesDecals = InternalBuildingConfiguration->bBuildingReceiveDecals;

	for (auto& SplineMeshComponent : SplineMeshComponents)
	{
		if (SplineMeshComponent.IsValid()) SplineMeshComponent->bReceivesDecals = InternalBuildingConfiguration->bBuildingReceiveDecals;
	}

	for (auto& InstancedStaticMeshComponent : InstancedStaticMeshComponents)
	{
		if (InstancedStaticMeshComponent.IsValid()) InstancedStaticMeshComponent->bReceivesDecals = InternalBuildingConfiguration->bBuildingReceiveDecals;
	}
}

bool ABuilding::InitializeWallSegments()
{
	WallSegmentsAtSplinePoint.SetNum(InternalBuildingConfiguration->Levels.Num());
	FillersSizeAtSplinePoint.SetNum(InternalBuildingConfiguration->Levels.Num());

	// original number of spline points (without subdivisions)
	const int NumSplinePoints = SplineComponent->GetNumberOfSplinePoints();
	if (NumSplinePoints == 0)
	{
		LCReporter::ShowError(LOCTEXT("EmptySpline", "Attempting to create a building with an empty spline"));
		return false;
	}

	ExpandedLevels = InternalBuildingConfiguration->Levels;
	int NumLevels = ExpandedLevels.Num();

	for (int LevelDescriptionIndex = 0; LevelDescriptionIndex < NumLevels; LevelDescriptionIndex++)
	{
		FLevelDescription &LevelDescription = ExpandedLevels[LevelDescriptionIndex];
		if (LevelDescription.LevelHeight < 0)
		{
			LCReporter::ShowError(LOCTEXT("NegativeWallHeight", "Attempting to create a building with negative Wall height"));
			return false;
		}

		TArray<TArray<FWallSegment>> ThisLevelWallSegmentsAtSplinePoint;
		ThisLevelWallSegmentsAtSplinePoint.SetNum(NumSplinePoints);
		WallSegmentsAtSplinePoint[LevelDescriptionIndex] = ThisLevelWallSegmentsAtSplinePoint;

		TArray<double> ThisLevelFillersSizeAtSplinePoint;
		ThisLevelFillersSizeAtSplinePoint.SetNum(NumSplinePoints);
		FillersSizeAtSplinePoint[LevelDescriptionIndex] = ThisLevelFillersSizeAtSplinePoint;

		const int MaxIterations = LevelDescription.bResetWallSegmentsOnCorners ? NumSplinePoints : 1;
		for (int i = 0; i < MaxIterations; i++)
		{
			double Length;

			if (LevelDescription.bResetWallSegmentsOnCorners)
			{
				int BaseIndex = SplineIndexToBaseSplineIndex[i];
				int BaseIndexNext = SplineIndexToBaseSplineIndex[i + 1];
				double Distance1 = BaseClockwiseSplineComponent->GetDistanceAlongSplineAtSplinePoint(BaseIndex);
				double Distance2 = BaseClockwiseSplineComponent->GetDistanceAlongSplineAtSplinePoint(BaseIndexNext);
				Length = Distance2 - Distance1;
			}
			else
			{
				Length = BaseClockwiseSplineComponent->GetSplineLength();
			}

			
			const int NumSegments = LevelDescription.WallSegments.Num();
			TArray<int> WallSegmentIndices;
			for (int k = 0; k < NumSegments; k++) WallSegmentIndices.Add(k);

			TArray<int> ExpandedWallSegmentIndices;

			if (!UBuildingConfiguration::Expand<int>(
				ExpandedWallSegmentIndices,
				WallSegmentIndices,
				LevelDescription.WallSegmentLoops,
				[&](const int &Index) { return LevelDescription.WallSegments[Index].SegmentLength; },
				Length
			)) return false;

			int NumFillers = 0;
			for (auto WallSegment : WallSegmentsAtSplinePoint[LevelDescriptionIndex][i])
				if (WallSegment.bAutoExpand) NumFillers++;


			// if there are no fillers in the expanded wall segments, we add all fillers
			// (ignoring their user-set length), and putting them in the correct position in our final array
			if (NumFillers == 0)
			{
				int Index1 = 0;
				int Index2 = 0;
				while (Index1 < NumSegments)
				{
					if (LevelDescription.WallSegments[Index1].bAutoExpand)
					{
						// we found a filler, we'll insert its index in the correct position in ExpandedWallSegmentIndices

						// we compute the number of expanded wall segments here, and not outside the main loop,
						// because the number increases due to our insertions
						const int NumExpandedWallSegments = ExpandedWallSegmentIndices.Num();

						while (Index2 < NumExpandedWallSegments && ExpandedWallSegmentIndices[Index2] < Index1) Index2++;

						ExpandedWallSegmentIndices.Insert(Index1, Index2);
						Index2++;
					}
					Index1++;
				}
			}

			// this is the count after all insertions
			const int NumExpandedWallSegments = ExpandedWallSegmentIndices.Num();

			// Copy the actual wall segments in the array
			WallSegmentsAtSplinePoint[LevelDescriptionIndex][i].SetNum(NumExpandedWallSegments);
			for (int j = 0; j < NumExpandedWallSegments; j++)
				WallSegmentsAtSplinePoint[LevelDescriptionIndex][i][j] = LevelDescription.WallSegments[ExpandedWallSegmentIndices[j]]; 
			
			// we then recount the number of fillers, as well as the non-fillers segments size
			NumFillers = 0;
			double NonFillersSegmentsSize = 0;
			for (auto WallSegment : WallSegmentsAtSplinePoint[LevelDescriptionIndex][i])
			{
				if (WallSegment.bAutoExpand) NumFillers++;
				else NonFillersSegmentsSize += WallSegment.SegmentLength;
			}

			if (NonFillersSegmentsSize > Length)
			{
				LCReporter::ShowError(
					FText::Format(
						LOCTEXT("SegmentsSizeGreaterThanLengthFmt", "Internal error during Building Generation: NonFillersSegmentsSize ({0}) > Length ({1})"),
						FText::AsNumber(NonFillersSegmentsSize),
						FText::AsNumber(Length)
					)
				);
				return false;
			}

			// if there are still no fillers, we add two, one at the beginning and one at the end
			double FillersSize;
			if (NumFillers == 0)
			{
				WallSegmentsAtSplinePoint[LevelDescriptionIndex][i].EmplaceAt(0, DummyFiller);
				WallSegmentsAtSplinePoint[LevelDescriptionIndex][i].Add(DummyFiller);
				FillersSize = (Length - NonFillersSegmentsSize) / 2;
			}
			else
			{
				FillersSize = (Length - NonFillersSegmentsSize) / NumFillers;
			}

			FillersSizeAtSplinePoint[LevelDescriptionIndex][i] = FillersSize;
		}
	}

	return true;
}

void ABuilding::GenerateBuilding()
{
	Modify();
	GenerateBuilding_Internal(SpawnedActorsPath);
}

bool ABuilding::GenerateBuilding_Internal(FName SpawnedActorsPathOverride)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("GenerateBuilding");

	if (bIsGenerating) return false;

	bIsGenerating = true;

	ON_SCOPE_EXIT
	{
		bIsGenerating = false;
	};
	
	if (!Concurrency::RunOnGameThreadAndWait([this]() -> bool { return Execute_Cleanup(this, false); }))
	{
		return false;
	}

	if (bUseInlineBuildingConfiguration)
	{
		InternalBuildingConfiguration = BuildingConfiguration;
	}
	else
	{
		if (!IsValid(BuildingConfigurationClass))
		{
			LCReporter::ShowError(
				LOCTEXT("NoBuildingConfigurationClass", "Internal Error: BuildingConfigurationClass is not valid.")
			);
			return false;
		}
	
		InternalBuildingConfiguration = BuildingConfigurationClass->GetDefaultObject<UBuildingConfiguration>();
	}

	if (!IsValid(InternalBuildingConfiguration))
	{
		LCReporter::ShowError(
			LOCTEXT("NoBuildingConfiguration", "Internal Error: Building Configuration is not valid.")
		);
		return false;
	}

	bool bFetchFromUserData = InternalBuildingConfiguration->AutoComputeNumFloors(this);

	if (!bFetchFromUserData && InternalBuildingConfiguration->bUseRandomNumFloors)
	{
		InternalBuildingConfiguration->NumFloors = UKismetMathLibrary::RandomIntegerInRange(InternalBuildingConfiguration->MinNumFloors, InternalBuildingConfiguration->MaxNumFloors);
	}

	int NumLevels = InternalBuildingConfiguration->Levels.Num();
	TArray<int> OriginalIndices;
	for (int i = 0; i < NumLevels; i++) OriginalIndices.Add(i);

	if (!UBuildingConfiguration::Expand<int>(
		LevelDescriptionsIndices,
		OriginalIndices,
		InternalBuildingConfiguration->LevelLoops,
		[](int LevelDescriptionIndex) { return 1; },
		InternalBuildingConfiguration->NumFloors
	))
	{
		bIsGenerating = false;
		return false;
	}

	LevelsHeightsSum = 0;
	for (auto &LevelDescriptionIndex : LevelDescriptionsIndices)
		LevelsHeightsSum += InternalBuildingConfiguration->Levels[LevelDescriptionIndex].LevelHeight;

	if (!IsValid(DynamicMeshComponent))
	{
		LCReporter::ShowError(
			LOCTEXT("InvalidDynamicMeshComponent", "Internal Error: DynamicMeshComponent is null.")
		);
		bIsGenerating = false;
		return false;
	}
	
	if (!AppendBuilding(DynamicMeshComponent->GetDynamicMesh(), SpawnedActorsPathOverride))
	{
		bIsGenerating = false;
		return false;
	}

	SetReceivesDecals();

	bIsGenerating = false;
	return true;
}

bool ABuilding::OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated)
{
	return GenerateBuilding_Internal(SpawnedActorsPathOverride);
}

TArray<UObject *> ABuilding::GetGeneratedObjects() const
{
	TArray<UObject*> Result;
	for (auto& Component : InstancedStaticMeshComponents)
		if (Component.IsValid()) Result.Add(Component.Get());

	for (auto& Component : SplineMeshComponents)
		if (Component.IsValid()) Result.Add(Component.Get());

	if (Volume.IsValid()) Result.Add(Volume.Get());
	if (IsValid(StaticMeshComponent)) Result.Add(StaticMeshComponent);

	// sometimes the pointers are lost, this is a fallback to make sure the components are deleted

	TArray<UInstancedStaticMeshComponent*> InstancedStaticMeshComponents2;
	GetComponents<UInstancedStaticMeshComponent>(InstancedStaticMeshComponents2);
	for (auto& Component : InstancedStaticMeshComponents2)
		if (IsValid(Component)) Result.Add(Component);

	TArray<USplineMeshComponent*> SplineMeshComponents2;
	GetComponents<USplineMeshComponent>(SplineMeshComponents2);
	for (auto& Component : SplineMeshComponents2)
		if (IsValid(Component)) Result.Add(Component);

	return Result;
}

bool ABuilding::AddAttachments(int LevelDescriptionIndex, double ZOffset)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("AddAttachments");
	
	FLevelDescription &LevelDescription = ExpandedLevels[LevelDescriptionIndex];

	// original number of spline points (without subdivisions)
	const int NumSplinePoints = SplineComponent->GetNumberOfSplinePoints();
	const int MaxIterations = LevelDescription.bResetWallSegmentsOnCorners ? NumSplinePoints : 1;
	for (int i = 0; i < NumSplinePoints; i++)
	{
		double CurrentDistance = BaseClockwiseSplineComponent->GetDistanceAlongSplineAtSplinePoint(SplineIndexToBaseSplineIndex[i]);
		for (auto &WallSegment : WallSegmentsAtSplinePoint[LevelDescriptionIndex][i])
		{
			double FinalSegmentLength = WallSegment.bAutoExpand ? FillersSizeAtSplinePoint[LevelDescriptionIndex][i] : WallSegment.SegmentLength;
			
			for (auto &Attachment: WallSegment.Attachments)
			{
				double TargetWidth = Attachment.bFitToWallSegmentWidth ? FinalSegmentLength : Attachment.OverrideWidth;
				double TargetHeight = Attachment.bFitToWallSegmentHeight ? LevelDescription.LevelHeight : Attachment.OverrideHeight;
				double TargetThickness = 0;

				auto GetAxisIndex = [&Attachment](EAxisKind AxisKind) -> int
				{
					if (Attachment.XAxis == AxisKind) return 0;
					else if (Attachment.YAxis == AxisKind) return 1;
					else return 2;
				};

				auto GetSize = [&TargetWidth, &TargetThickness, &TargetHeight, &GetAxisIndex](EAxisKind AxisKind, FVector Extent) -> double
				{
					if (AxisKind == EAxisKind::Width)
						return TargetWidth > 0 ? TargetWidth : Extent[GetAxisIndex(AxisKind)] * 2;
					else if (AxisKind == EAxisKind::Thickness)
						return TargetThickness > 0 ? TargetThickness : Extent[GetAxisIndex(AxisKind)] * 2;
					else if (AxisKind == EAxisKind::Height)
						return TargetHeight > 0 ? TargetHeight : Extent[GetAxisIndex(AxisKind)] * 2;
					else
						return 1;
				};

				auto GetScale = [&TargetWidth, &TargetThickness, &TargetHeight](EAxisKind AxisKind, int AxisIndex, FVector Extent) -> double
				{
					if (AxisKind == EAxisKind::Width)
						return TargetWidth > 0 ? TargetWidth / Extent[AxisIndex] / 2 : 1;
					else if (AxisKind == EAxisKind::Thickness)
						return TargetThickness > 0 ? TargetThickness / Extent[AxisIndex] / 2 : 1;
					else if (AxisKind == EAxisKind::Height)
						return TargetHeight > 0 ? TargetHeight / Extent[AxisIndex] / 2 : 1;
					else
						return 1;
				};

				if (Attachment.bFitToWallSegmentThickness)
				{
					if (WallSegment.bOverrideWallThickness)
					{
						TargetThickness = WallSegment.InternalWallThickness + WallSegment.ExternalWallThickness;
					}
					else
					{
						TargetThickness = InternalBuildingConfiguration->InternalWallThickness + InternalBuildingConfiguration->ExternalWallThickness;
					}
				}
				else
				{
					TargetThickness = Attachment.OverrideThickness;
				}

				FVector AttachmentLocation = BaseClockwiseSplineComponent->GetLocationAtDistanceAlongSpline(CurrentDistance, ESplineCoordinateSpace::World);
				FVector AttachmentTangent = BaseClockwiseSplineComponent->GetTangentAtDistanceAlongSpline(CurrentDistance, ESplineCoordinateSpace::World);
				FQuat AttachmentRotation = FQuat::FindBetweenVectors(FVector(1, 0, 0), AttachmentTangent);
				double AttachmentTangentAngle = AttachmentRotation.Rotator().Yaw;

				FVector SimpleRotatedOffset = Attachment.Offset.RotateAngleAxis(AttachmentTangentAngle, FVector(0, 0, 1));

				switch (Attachment.AttachmentKind)
				{
					case EAttachmentKind::InstancedStaticMeshComponent:
					{
						if (!Attachment.Mesh) break;

						UInstancedStaticMeshComponent *ISM = nullptr;
						if (!MeshToISM.Contains(Attachment.Mesh))
						{
							ISM = NewObject<UInstancedStaticMeshComponent>(RootComponent);
							ISM->SetStaticMesh(Attachment.Mesh);
							ISM->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
							ISM->CreationMethod = EComponentCreationMethod::UserConstructionScript;
							ISM->RegisterComponent(); 
							AddInstanceComponent(ISM);
							MeshToISM.Add(Attachment.Mesh, ISM);
							InstancedStaticMeshComponents.Add(ISM);
						}
						else
						{
							ISM = MeshToISM[Attachment.Mesh];
						}

						if (!ISM)
						{
							UE_LOG(LogBuildingsFromSplines, Error, TEXT("Could not create ISM for window meshes"));
							return false;
						}
						
						FBox BoundingBox = Attachment.Mesh->GetBoundingBox();
						FVector Extent = BoundingBox.GetExtent();
						FVector Scale(
							GetScale(Attachment.XAxis, 0, Extent),
							GetScale(Attachment.YAxis, 1, Extent),
							GetScale(Attachment.ZAxis, 2, Extent)
						);
						FVector Offset2 = Attachment.AxisOffsetMultiplier;
						Offset2[0] *= GetSize(EAxisKind::Width, Extent);
						Offset2[1] *= GetSize(EAxisKind::Thickness, Extent);
						Offset2[2] *= GetSize(EAxisKind::Height, Extent);
						FVector FinalOffset = Attachment.Offset + Offset2;
						FVector RotatedOffset = FinalOffset.RotateAngleAxis(AttachmentTangentAngle, FVector(0, 0, 1));

						FTransform Transform;
						Transform.SetLocation(AttachmentLocation + RotatedOffset + FVector(0, 0, ZOffset));
						Transform.SetRotation(AttachmentRotation * FQuat(Attachment.ExtraRotation));
						Transform.SetScale3D(Scale);
						ISM->AddInstance(Transform, true);
						break;
					}
					case EAttachmentKind::SplineMeshComponent:
					{
						if (!Attachment.Mesh) break;

						double Width = TargetWidth > 0 ? TargetWidth : FinalSegmentLength;
						AddSplineMesh(
							Attachment.Mesh, CurrentDistance, Width, TargetThickness, TargetHeight,
							SimpleRotatedOffset + FVector(0, 0, MinHeightLocal + ZOffset), Attachment.SplineMeshAxis
						);
						break;
					}
					case EAttachmentKind::Actor:
					{
						if (!IsValid(Attachment.ActorClass)) break;

						AActor *NewActor = GetWorld()->SpawnActor<AActor>(Attachment.ActorClass);
						FVector ActorOrigin, ActorExtent;
						NewActor->GetActorBounds(true, ActorOrigin, ActorExtent);
						NewActor->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

						FVector Scale(
							GetScale(Attachment.XAxis, 0, ActorExtent),
							GetScale(Attachment.YAxis, 1, ActorExtent),
							GetScale(Attachment.ZAxis, 2, ActorExtent)
						);
						FVector Offset2 = Attachment.AxisOffsetMultiplier;
						Offset2[0] *= GetSize(EAxisKind::Width, ActorExtent);
						Offset2[1] *= GetSize(EAxisKind::Thickness, ActorExtent);
						Offset2[2] *= GetSize(EAxisKind::Height, ActorExtent);
						FVector FinalOffset = Attachment.Offset + Offset2;
						FVector RotatedOffset = FinalOffset.RotateAngleAxis(AttachmentTangentAngle, FVector(0, 0, 1));

						NewActor->SetActorScale3D(Scale);
						NewActor->SetActorLocation(AttachmentLocation + RotatedOffset + FVector(0, 0, ZOffset));
						NewActor->SetActorRotation(AttachmentRotation * FQuat(Attachment.ExtraRotation));
						break;
					}
				}
			}

			if (WallSegment.bAutoExpand)
			{
				CurrentDistance += FillersSizeAtSplinePoint[LevelDescriptionIndex][i];
			}
			else
			{
				CurrentDistance += WallSegment.SegmentLength;
			}
		}
	}

	return true;
}
	
bool ABuilding::AddAttachments()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("AddAttachments");

	double CurrentHeight = InternalBuildingConfiguration->ExtraWallBottom;
	for (auto& LevelDescriptionIndex : LevelDescriptionsIndices)
	{
		if (!AddAttachments(LevelDescriptionIndex, CurrentHeight)) return false;
		CurrentHeight += ExpandedLevels[LevelDescriptionIndex].LevelHeight;
	}

	return true;
}

void ABuilding::AppendBuildingStructure(UDynamicMesh* TargetMesh)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("AppendBuildingStructure");

	if (InternalBuildingConfiguration->bAutoPadWallBottom)
	{
		InternalBuildingConfiguration->ExtraWallBottom = MaxHeightLocal - MinHeightLocal + InternalBuildingConfiguration->PadBottom;
	}

	if (
		InternalBuildingConfiguration->BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside ||
		InternalBuildingConfiguration->RoofKind == ERoofKind::Point ||
		InternalBuildingConfiguration->RoofKind == ERoofKind::InnerSpline
	)
	{
		ComputeOffsetPolygons();
	}

	if (InternalBuildingConfiguration->BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside)
	{
		AppendWallsWithHoles(TargetMesh);
		
		if (InternalBuildingConfiguration->bBuildFloorTiles || InternalBuildingConfiguration->RoofKind == ERoofKind::Flat)
		{
			if (!AppendFloors(TargetMesh)) return;
		}
	}
	else
	{
		AppendBuildingWithoutInside(TargetMesh);
	}

	for (int i = 0; i < InternalBuildingConfiguration->Materials.Num(); i++)
		DynamicMeshComponent->SetMaterial(i, InternalBuildingConfiguration->Materials[i]);
}

bool ABuilding::AppendBuilding(UDynamicMesh* TargetMesh, FName SpawnedActorsPathOverride)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("AppendBuilding");

	ComputeMinMaxHeight();
	ComputeBaseVertices();

	if (!InitializeWallSegments()) return false;

	AppendBuildingStructure(TargetMesh);
		
	if (
		InternalBuildingConfiguration->RoofKind == ERoofKind::Point ||
		InternalBuildingConfiguration->RoofKind == ERoofKind::InnerSpline
	)
	{
		AppendRoof(TargetMesh);
	}
	

	return Concurrency::RunOnGameThreadAndWait([this, &TargetMesh, &SpawnedActorsPathOverride]()
	{
		{
			TRACE_CPUPROFILER_EVENT_SCOPE_STR("AppendBuilding/ComputeSplitNormals");

			UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(TargetMesh, FGeometryScriptSplitNormalsOptions(), FGeometryScriptCalculateNormalsOptions());
		}

	#if WITH_EDITOR
		if (InternalBuildingConfiguration->bConvertToStaticMesh)
		{
			GenerateStaticMesh();
		}

		if (InternalBuildingConfiguration->bConvertToVolume)
		{
			GenerateVolume(SpawnedActorsPathOverride);
		}

		if (InternalBuildingConfiguration->bConvertToStaticMesh || InternalBuildingConfiguration->bConvertToVolume)
		{
			DynamicMeshComponent->GetDynamicMesh()->Reset();
		}

	#else

		if (InternalBuildingConfiguration->bConvertToStaticMesh && InternalBuildingConfiguration->bConvertToVolume)
		{
			UE_LOG(LogBuildingsFromSplines, Warning, TEXT("Cannot convert building to static mesh or volume at runtime"));
		}

	#endif

		{
			TRACE_CPUPROFILER_EVENT_SCOPE_STR("AppendBuilding/AutoGenerateXAtlasMeshUVs");

			if (InternalBuildingConfiguration->bAutoGenerateXAtlasMeshUVs)
			{
				UGeometryScriptLibrary_MeshUVFunctions::AutoGenerateXAtlasMeshUVs(TargetMesh, 0, FGeometryScriptXAtlasOptions());
			}

		}


		AddAttachments();
		BaseClockwiseSplineComponent->ClearSplinePoints();

	#if WITH_EDITOR
		GEditor->NoteSelectionChange();
	#endif

		return true;
	});
}

#if WITH_EDITOR

void ABuilding::GenerateStaticMesh()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("GenerateStaticMesh");
	EGeometryScriptOutcomePins Outcome;

	FString Unused;
	FGeometryScriptUniqueAssetNameOptions GeometryScriptUniqueAssetNameOptions;
	GeometryScriptUniqueAssetNameOptions.UniqueIDDigits = 18;
	if (StaticMeshPath.IsEmpty())
	{
		UGeometryScriptLibrary_CreateNewAssetFunctions::CreateUniqueNewAssetPathName(
			FString("/Game/Buildings"),
			FString("SM_Building"),
			StaticMeshPath,
			Unused,
			GeometryScriptUniqueAssetNameOptions,
			Outcome
		);

		if (Outcome != EGeometryScriptOutcomePins::Success)
		{
			LCReporter::ShowError(
				LOCTEXT("StaticMeshError", "Internal error while creating unique asset path name.")
			);
			return;
		}
	}


	FGeometryScriptCreateNewStaticMeshAssetOptions Options;
	Options.bEnableCollision = true;
	Options.CollisionMode = DynamicMeshComponent->CollisionType;
	UStaticMesh *StaticMesh = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(
		DynamicMeshComponent->GetDynamicMesh(), StaticMeshPath,
		Options, Outcome
	);

	if (Outcome != EGeometryScriptOutcomePins::Success || !StaticMesh)
	{
		LCReporter::ShowError(
			LOCTEXT("StaticMeshError", "Internal error while creating static mesh for building. You may want to regenerate the building.")
		);
		return;
	}

	FMeshNaniteSettings NaniteSettings;
	NaniteSettings.bEnabled = InternalBuildingConfiguration->bEnableNanite;
	NaniteSettings.bPreserveArea = true;
	StaticMesh->NaniteSettings = NaniteSettings;

	int NumMaterials = DynamicMeshComponent->GetNumMaterials();
	TArray<FStaticMaterial> Materials;
	for (int i = 0; i < NumMaterials; i++)
	{
		FStaticMaterial Material(DynamicMeshComponent->GetMaterial(i));
		Materials.Add(Material);
	}
	StaticMesh->SetStaticMaterials(Materials);
	StaticMesh->InitResources(); // this calls StaticMesh->UpdateUVChannelData() to avoid Error: Ensure condition failed: GetStaticMaterials()[MaterialIndex].UVChannelData.bInitialized

	StaticMeshComponent = NewObject<UStaticMeshComponent>(RootComponent);
	StaticMeshComponent->SetStaticMesh(StaticMesh);
	StaticMeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	StaticMeshComponent->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	StaticMeshComponent->RegisterComponent();

	AddInstanceComponent(StaticMeshComponent);
}

void ABuilding::GenerateVolume(FName SpawnedActorsPathOverride)
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
		LCReporter::ShowError(
			LOCTEXT("StaticMeshError", "Internal error while creating volume.")
		);
		return;
	}
	
	ULCBlueprintLibrary::SetFolderPath2(Volume.Get(), SpawnedActorsPathOverride, SpawnedActorsPath);

	UOSMUserData *BuildingOSMUserData = Cast<UOSMUserData>(this->GetRootComponent()->GetAssetUserDataOfClass(UOSMUserData::StaticClass()));
	UOSMUserData *VolumeOSMUserData = NewObject<UOSMUserData>(Volume->GetRootComponent());
	VolumeOSMUserData->Fields = BuildingOSMUserData->Fields;
	Volume->GetRootComponent()->AddAssetUserData(VolumeOSMUserData);
}

void ABuilding::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (bGenerateWhenModified) GenerateBuilding();
}

#endif

#undef LOCTEXT_NAMESPACE
