// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "BuildingConfiguration.generated.h"

#define LOCTEXT_NAMESPACE "FBuildingFromSplineModule"

UENUM(BlueprintType)
enum class EBuildingKind : uint8
{
	Simple,
	Custom
};

UENUM(BlueprintType)
enum class ERoofKind : uint8
{
	None,
	Flat,
	Point,
	InnerSpline
};

UCLASS(meta = (PrioritizeCategories = "Building"), BlueprintType)
class BUILDINGFROMSPLINE_API UBuildingConfiguration : public UActorComponent
{
	GENERATED_BODY()

public:
	UBuildingConfiguration();

	/** Choices */

	/* The kind of building that should be spawned. (Use Simple for best performance.) */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|General",
		meta = (DisplayPriority = "-5")
	)
	EBuildingKind BuildingKind = EBuildingKind::Simple;

	/* Convert to static mesh after the building is generated. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|General",
		meta = (DisplayPriority = "-4")
	)
	bool bConvertToStaticMesh = true;

	/* Convert to volume after the building is generated (better used with Simple building). */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|General",
		meta = (DisplayPriority = "-3")
	)
	bool bConvertToVolume = false;

	/* Automatically regenerate the building when a property is modified. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|General",
		meta = (DisplayPriority = "1")
	)
	bool bGenerateWhenModified = false;

	/* Build external walls. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|General",
		meta = (EditCondition = "BuildingKind == EBuildingKind::Custom", EditConditionHides, DisplayPriority = "2")
	)
	bool bBuildExternalWalls = true;
	
	/* Build internal wall.s */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|General",
		meta = (EditCondition = "BuildingKind == EBuildingKind::Custom", EditConditionHides, DisplayPriority = "3")
	)
	bool bBuildInternalWalls = true;

	/* Build floor tiles. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|General",
		meta = (EditCondition = "BuildingKind == EBuildingKind::Custom", EditConditionHides, DisplayPriority = "4")
	)
	bool bBuildFloorTiles = true;

	/* Recompute UVs using AutoGenerateXAtlasMeshUVs (slow operation). */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|General",
		meta = (DisplayPriority = "5")
	)
	bool bAutoGenerateXAtlasMeshUVs = false;


	/** Materials */

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials",
		meta = (EditCondition = "BuildingKind == EBuildingKind::Custom", EditConditionHides, DisplayPriority = "2")
	)
	TObjectPtr<UMaterialInterface> InteriorMaterial;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials",
		meta = (DisplayPriority = "3")
	)
	TObjectPtr<UMaterialInterface> ExteriorMaterial;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials",
		meta = (EditCondition = "BuildingKind == EBuildingKind::Custom", EditConditionHides, DisplayPriority = "4")
	)
	TObjectPtr<UMaterialInterface> FloorMaterial;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials",
		meta = (EditCondition = "BuildingKind == EBuildingKind::Custom", EditConditionHides, DisplayPriority = "5")
	)
	TObjectPtr<UMaterialInterface> CeilingMaterial;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials",
		meta = (DisplayPriority = "6")
	)
	TObjectPtr<UMaterialInterface> RoofMaterial;


	/** Mesh settings */

	/* Door mesh */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Meshes",
		meta = (DisplayPriority = "3")
	)
	UStaticMesh *DoorMesh;

	/* Window Mesh */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Meshes",
		meta = (DisplayPriority = "3")
	)
	UStaticMesh *WindowMesh;
	

	/** Structural Settings */
	
	/* The height of a floor */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (DisplayPriority = "10")
	)
	double FloorHeight = 300;
	
	/* The tickness of the floor tiles */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (EditCondition = "BuildingKind == EBuildingKind::Custom", EditConditionHides, DisplayPriority = "11")
	)
	double FloorThickness = 20;
	
	/* The tickness of the internal walls */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (EditCondition = "BuildingKind == EBuildingKind::Custom", EditConditionHides, DisplayPriority = "12")
	)
	double InternalWallThickness = 1;

	/* The tickness of the external walls */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (EditCondition = "BuildingKind == EBuildingKind::Custom", EditConditionHides, DisplayPriority = "13")
	)
	double ExternalWallThickness = 10;

	/* Random number of floors */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (DisplayPriority = "20")
	)
	bool bUseRandomNumFloors = false;
	
	/* The number of floors (including ground level) */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (
			EditCondition = "!bUseRandomNumFloors",
			EditConditionHides, DisplayPriority = "21"
		)
	)
	int NumFloors = 3;
	
	/* Minimum number of floors */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (
			EditCondition = "bUseRandomNumFloors",
			EditConditionHides, DisplayPriority = "22"
		)
	)
	int MinNumFloors = 3;
	
	/* Maximum number of floors */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (
			EditCondition = "bUseRandomNumFloors",
			EditConditionHides, DisplayPriority = "23"
		)
	)
	int MaxNumFloors = 5;


	/* The number of subdivisions in a wall (increase it if you want smoothly bended borders) */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (DisplayPriority = "3")
	)
	int WallSubdivisions = 0;
	
	/* Wall height below the first floor */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (DisplayPriority = "3")
	)
	int ExtraWallBottom = 20;
	
	/* Wall height above the last floor */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (DisplayPriority = "3")
	)
	int ExtraWallTop = 20;


	/** Roof Settings */


	/* Build roof */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Roof",
		meta = (EditCondition = "BuildingKind == EBuildingKind::Custom", EditConditionHides, DisplayPriority = "1")
	)
	ERoofKind RoofKind = ERoofKind::Flat;

	
	/* The length of the roof that goes outside the building */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Roof",
		meta = (
			EditCondition = "BuildingKind == EBuildingKind::Custom && (RoofKind == ERoofKind::Point || RoofKind == ERoofKind::InnerSpline)",
			EditConditionHides, DisplayPriority = "3"
		)
	)
	double OuterRoofDistance = 100;
	
	/* The distance from the walls to the inner/top part of the roof */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Roof",
		meta = (
			EditCondition = "BuildingKind == EBuildingKind::Custom && RoofKind == ERoofKind::InnerSpline",
			EditConditionHides, DisplayPriority = "4"
		)
	)
	double InnerRoofDistance = 50;
	
	/* Roof thickness */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Roof",
		meta = (
			EditCondition = "BuildingKind == EBuildingKind::Custom && (RoofKind == ERoofKind::Point || RoofKind == ERoofKind::InnerSpline)",
			EditConditionHides, DisplayPriority = "5"
		)
	)
	double RoofThickness = 5;
	
	/* The distance from the top of the walls to the top of the roof */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Roof",
		meta = (
			EditCondition = "BuildingKind == EBuildingKind::Custom && (RoofKind == ERoofKind::Point || RoofKind == ERoofKind::InnerSpline)",
			EditConditionHides, DisplayPriority = "6"
		)
	)
	double RoofHeight = 250;


	/** Windows Settings */
	
	/* Minimum distance between a window and a spline point */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Windows",
		meta = (DisplayPriority = "3")
	)
	double MinDistanceWindowToCorner = 150;
	
	/* Minimum distance between two windows */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Windows",
		meta = (DisplayPriority = "3")
	)
	double MinDistanceWindowToWindow = 350;
	
	/* The width of windows */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Windows",
		meta = (DisplayPriority = "3")
	)
	double WindowsWidth = 140;
	
	/* The height of windows */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Windows",
		meta = (DisplayPriority = "3")
	)
	double WindowsHeight = 140;
	
	/* The (Z) distance between the floor and the windows */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Windows",
		meta = (DisplayPriority = "3")
	)
	double WindowsDistanceToFloor = 110;
	

	/** Doors Settings */
	
	/* Minimum distance between a door and a spline point */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Doors",
		meta = (DisplayPriority = "3")
	)
	double MinDistanceDoorToCorner = 150;
	
	/* Minimum distance between two doors */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Doors",
		meta = (DisplayPriority = "3")
	)
	double MinDistanceDoorToDoor = 450;
	
	/* The width of doors */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Doors",
		meta = (DisplayPriority = "3")
	)
	double DoorsWidth = 240;
	
	/* The height of doors */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Doors",
		meta = (DisplayPriority = "3")
	)
	double DoorsHeight = 210;
	
	/* The (Z) distance between the floor and the doors */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Doors",
		meta = (DisplayPriority = "3")
	)
	double DoorsDistanceToFloor = 5;

};

#undef LOCTEXT_NAMESPACE