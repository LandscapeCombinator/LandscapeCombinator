// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "BuildingConfiguration.generated.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

UCLASS(meta = (PrioritizeCategories = "Building"), BlueprintType)
class LANDSCAPECOMBINATOR_API UBuildingConfiguration : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Choices */

	/* Add doors */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Choices",
		meta = (DisplayPriority = "1")
	)
	bool bAddDoors = true;

	/* Add windows */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Choices",
		meta = (DisplayPriority = "1")
	)
	bool bAddWindows = true;

	/* Build external walls */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Choices",
		meta = (DisplayPriority = "1")
	)
	bool bBuildExternalWalls = true;
	
	/* Build internal walls */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Choices",
		meta = (DisplayPriority = "1")
	)
	bool bBuildInternalWalls = true;
	
	/* Build floor tiles */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Choices",
		meta = (DisplayPriority = "1")
	)
	bool bBuildFloorTiles = true;
	
	/* Build roof */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Choices",
		meta = (DisplayPriority = "1")
	)
	bool bBuildRoof = true;


	/** Materials */

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials",
		meta = (DisplayPriority = "2")
	)
	TObjectPtr<UMaterialInterface> InteriorMaterial;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials",
		meta = (DisplayPriority = "2")
	)
	TObjectPtr<UMaterialInterface> ExteriorMaterial;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials",
		meta = (DisplayPriority = "2")
	)
	TObjectPtr<UMaterialInterface> FloorMaterial;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials",
		meta = (DisplayPriority = "2")
	)
	TObjectPtr<UMaterialInterface> CeilingMaterial;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials",
		meta = (DisplayPriority = "2")
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
		meta = (DisplayPriority = "3")
	)
	double FloorHeight = 400;
	
	/* The tickness of the floor tiles */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (DisplayPriority = "3")
	)
	double FloorThickness = 20;
	
	/* The tickness of the internal walls */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (DisplayPriority = "3")
	)
	double InternalWallThickness = 5;
	
	/* The tickness of the external walls */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (DisplayPriority = "3")
	)
	double ExternalWallThickness = 10;
	
	/* The number of floors above ground */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (DisplayPriority = "3")
	)
	int NumFloors = 3;
	
	/* The number of subdivisions in a wall (increase it if you want smoothly bended walls) */
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
	int ExtraWallTop = 120;


	/** Roof Settings */
	
	/* The length of the roof that goes outside the building */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Roof",
		meta = (DisplayPriority = "3")
	)
	double OuterRoofDistance = 100;
	
	/* The distance from the walls to the inner/top part of the roof */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Roof",
		meta = (DisplayPriority = "3")
	)
	double InnerRoofDistance = 300;
	
	/* Roof thickness */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Roof",
		meta = (DisplayPriority = "3")
	)
	double RoofThickness = 20;
	
	/* The height of the roof */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Roof",
		meta = (DisplayPriority = "3")
	)
	double RoofHeight = 400;


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
	double WindowsWidth = 120;
	
	/* The height of windows */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Windows",
		meta = (DisplayPriority = "3")
	)
	double WindowsHeight = 120;
	
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