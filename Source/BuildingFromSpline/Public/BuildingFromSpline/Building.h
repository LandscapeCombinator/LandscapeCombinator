// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "BuildingFromSpline/BuildingConfiguration.h"

#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "DynamicMeshActor.h"
#include "SegmentTypes.h"

#include "Building.generated.h"

#define LOCTEXT_NAMESPACE "FBuildingFromSplineModule"

using namespace UE::Geometry;

UCLASS(meta = (PrioritizeCategories = "Building"))
class BUILDINGFROMSPLINE_API ABuilding : public ADynamicMeshActor
{
	GENERATED_BODY()

public:
	ABuilding();
	
	double MinHeightLocal = MAX_dbl;
	double MaxHeightLocal = MAX_dbl;
	double MinHeightWorld = MAX_dbl;
	
	UPROPERTY(
		VisibleAnywhere, BlueprintReadWrite, Category = "Building",
		meta = (DisplayPriority = "3")
	)
	TObjectPtr<USplineComponent> SplineComponent;
	
	UPROPERTY(VisibleAnywhere, Category = "Building")
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building",
		meta = (DisplayPriority = "3")
	)
	TObjectPtr<UBuildingConfiguration> BuildingConfiguration;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Building",
		meta = (DisplayPriority = "5")
	)
	void GenerateBuilding();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Building",
		meta = (DisplayPriority = "5")
	)
	void ResetDynamicMesh();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Building",
		meta = (DisplayPriority = "5")
	)
	void ResetStaticMesh();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Building",
		meta = (DisplayPriority = "5")
	)
	void ConvertToStaticMesh();
	
	UFUNCTION(BlueprintCallable, Category = "Building")
	void AppendBuilding(UDynamicMesh* TargetMesh);

	void ComputeMinMaxHeight();

protected:

	virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;


private:
	const int FloorMaterialID = 0;
	const int CeilingMaterialID = 1;
	const int ExteriorMaterialID = 2;
	const int InteriorMaterialID = 3;
	const int RoofMaterialID = 4;

	UPROPERTY()
	TArray<TObjectPtr<USplineMeshComponent>> SplineMeshComponents;

	// returns Point2 when the lines are colinear
	FVector2D GetIntersection(FVector2D Point1, FVector2D Direction1, FVector2D Point2, FVector2D Direction2);

	// same as SplineComponent, but all points have the same Z coordinate as the lowest point,
	// and the points are clockwise (when seen from above in Unreal, which isn't the same as clockwise in TPolygon2
	// because of inverted Y-axis
	UPROPERTY()
	TObjectPtr<USplineComponent> BaseClockwiseSplineComponent;

	TArray<FTransform> BaseClockwiseFrames;
	TArray<double> BaseClockwiseFramesTimes;
	TArray<FVector2D> BaseVertices2D;
	TArray<FVector2D> InternalWallPolygon;
	TArray<FVector2D> ExternalWallPolygon;
	TArray<int> IndexToInternalIndex;
	
	void DeflateFrames(TArray<FTransform> Frames, TArray<FVector2D>& OutOffsetPolygon, TArray<int>& OutIndexToOffsetIndex, double Offset);

	TArray<int> SplineIndexToBaseSplineIndex;
	void ComputeBaseVertices();
	void ComputeOffsetPolygons();
	TArray<FTransform> FindFrames(double BeginDistance, double Length);
	FVector2D GetShiftedPoint(TArray<FTransform> Frames, int Index, int Shift);
	void AppendWallsWithHoles(
		UDynamicMesh* TargetMesh, double InternalWidth, double ExternalWidth, double WallHeight,
		double MinDistanceHoleToCorner, double MinDistanceHoleToHole, double HolesWidth, double HolesHeight, double HoleDistanceToFloor, double ZOffset,
		bool bBuildWalls,
		UStaticMesh *StaticMesh,
		int MaterialID
	);
	void AppendWallsWithHoles(UDynamicMesh* TargetMesh);
	void AddSplineMesh(UStaticMesh* StaticMesh, double BeginDistance, double Length, double Height, double ZOffset);
	void AppendAlongSpline(UDynamicMesh* TargetMesh, double InsideWidth, double OutsideWidth, double BeginDistance, double Length, double Height, double ZOffset, int MaterialID);
	void AppendRoof(UDynamicMesh* TargetMesh);
	bool AppendFloors(UDynamicMesh *TargetMesh);
	void ClearSplineMeshComponents();
};

#undef LOCTEXT_NAMESPACE