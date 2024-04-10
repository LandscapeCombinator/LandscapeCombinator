// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Engine/StaticMesh.h"
#include "Components/ActorComponent.h" 
#include "CoreMinimal.h"

#include "BuildingConfiguration.generated.h"

#define LOCTEXT_NAMESPACE "FBuildingFromSplineModule"

UENUM(BlueprintType)
enum class EBuildingGeometry : uint8
{
	/* Simple block without any geometry inside (best performance) */
	BuildingWithoutInside,

	/* Simple block with an empty inside */
	BuildingWithFloorsAndEmptyInside,
};

UENUM(BlueprintType)
enum class EWindowsPlacement : uint8
{
	ParameterizedByDistance,
	Manual
};

UENUM(BlueprintType)
enum class EWindowsMeshKind : uint8
{
	None,

	/* Choose this if you need simple instanced static meshes for doors/windows */
	InstancedStaticMeshComponent,

	/* Choose this if you need curved doors/windows (along the spline). This creates one component per Window, so it's slow. */
	SplineMeshComponent
};

USTRUCT(BlueprintType)
struct FWindowsSpecification
{
	GENERATED_BODY()
	
	/* Add holes in the buildings for the windows. Ignored with BuildingWithoutInside geometry. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "WindowsSpecification",
		meta = (DisplayPriority = "0")
	)
	bool bMakeWindowsHoles = true;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "WindowsSpecification",
		meta = (DisplayPriority = "10")
	)
	EWindowsMeshKind WindowsMeshKind = EWindowsMeshKind::None;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "WindowsSpecification",
		meta = (EditCondition = "WindowsMeshKind != EWindowsMeshKind::None", EditConditionHides, DisplayPriority = "11")
	)
	TObjectPtr<UStaticMesh> WindowsMesh = nullptr;

	/* The width of windows */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "WindowsSpecification",
		meta = (DisplayPriority = "20")
	)
	double WindowsWidth = 140;
	
	/* The height of windows */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "WindowsSpecification",
		meta = (DisplayPriority = "21")
	)
	double WindowsHeight = 140;
	
	/* The (Z) distance between the floor and the windows */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "WindowsSpecification",
		meta = (DisplayPriority = "22")
	)
	double WindowsDistanceToFloor = 110;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "WindowsSpecification",
		meta = (DisplayPriority = "100")
	)
	EWindowsPlacement WindowsPlacement = EWindowsPlacement::ParameterizedByDistance;

	/* Minimum distance between a window and a spline point. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "WindowsSpecification",
		meta = (EditCondition = "WindowsPlacement == EWindowsPlacement::ParameterizedByDistance", EditConditionHides, DisplayPriority = "101")
	)
	double MinDistanceWindowToCorner = 150;
	
	/* Minimum distance between two windows. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "WindowsSpecification",
		meta = (EditCondition = "WindowsPlacement == EWindowsPlacement::ParameterizedByDistance", EditConditionHides, DisplayPriority = "102")
	)
	double MinDistanceWindowToWindow = 350;
	
	/* Distance along the spline for each window. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "WindowsSpecification",
		meta = (EditCondition = "WindowsPlacement == EWindowsPlacement::Manual", EditConditionHides, DisplayPriority = "103")
	)
	TArray<float> WindowsPositions;

	bool operator==(const FWindowsSpecification& Other) const = default;

	friend uint32 GetTypeHash(const FWindowsSpecification& WindowsSpecification)
	{
		return FCrc::MemCrc32(&WindowsSpecification, sizeof(FWindowsSpecification));
	}
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

	UFUNCTION()
	bool OwnerIsBFS();

	UFUNCTION()
	bool AutoComputeNumFloors();

	/** Choices */

	/* Choose whether you want geometry inside your building, and holes for windows  */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|General",
		meta = (DisplayPriority = "-5")
	)
	EBuildingGeometry BuildingGeometry = EBuildingGeometry::BuildingWithoutInside;

	/* Convert to static mesh after the building is generated. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|General",
		meta = (DisplayPriority = "-4")
	)
	bool bConvertToStaticMesh = true;

	/* Convert to volume after the building is generated (better used with Simple building). */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|General",
		meta = (DisplayPriority = "-1")
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
		meta = (EditCondition = "BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside", EditConditionHides, DisplayPriority = "2")
	)
	bool bBuildExternalWalls = true;
	
	/* Build internal wall.s */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|General",
		meta = (EditCondition = "BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside", EditConditionHides, DisplayPriority = "3")
	)
	bool bBuildInternalWalls = true;

	/* Build floor tiles. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|General",
		meta = (EditCondition = "BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside", EditConditionHides, DisplayPriority = "4")
	)
	bool bBuildFloorTiles = true;

	/* Recompute UVs using AutoGenerateXAtlasMeshUVs (slow operation). */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|General",
		meta = (DisplayPriority = "5")
	)
	bool bAutoGenerateXAtlasMeshUVs = false;

	/* Whether the generated building can receive decals. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|General",
		meta = (DisplayPriority = "6")
	)
	bool bBuildingReceiveDecals = false;


	/** Materials */

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials",
		meta = (EditCondition = "BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside", EditConditionHides, DisplayPriority = "2")
	)
	TObjectPtr<UMaterialInterface> InteriorMaterial;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials",
		meta = (DisplayPriority = "3")
	)
	TObjectPtr<UMaterialInterface> ExteriorMaterial;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials",
		meta = (EditCondition = "BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside", EditConditionHides, DisplayPriority = "4")
	)
	TObjectPtr<UMaterialInterface> FloorMaterial;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials",
		meta = (EditCondition = "BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside", EditConditionHides, DisplayPriority = "5")
	)
	TObjectPtr<UMaterialInterface> CeilingMaterial;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials",
		meta = (DisplayPriority = "6")
	)
	TObjectPtr<UMaterialInterface> RoofMaterial;
	

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
		meta = (EditCondition = "BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside", EditConditionHides, DisplayPriority = "11")
	)
	double FloorThickness = 20;
	
	/* The tickness of the internal walls */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (EditCondition = "BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside", EditConditionHides, DisplayPriority = "12")
	)
	double InternalWallThickness = 1;

	/* The tickness of the external walls */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (EditCondition = "BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside", EditConditionHides, DisplayPriority = "13")
	)
	double ExternalWallThickness = 10;
	

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (DisplayPriority = "19")
	)
	/**
	 * If true and if the Asset User Data contains a levels value, the number of floors
	 * is set to this value. If it contains a height value, the number of
	 * floors is set to the height divided by the floor height. */
	bool bAutoComputeNumFloors = true;

	/* Random number of floors */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (EditCondition = "!AutoComputeNumFloors()", EditConditionHides, DisplayPriority = "20")
	)
	bool bUseRandomNumFloors = false;
	
	/* The number of floors (including ground level) */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (EditCondition = "!AutoComputeNumFloors() && !bUseRandomNumFloors", EditConditionHides, DisplayPriority = "21")
	)
	int NumFloors = 3;
	
	/* Minimum number of floors */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (EditCondition = "!AutoComputeNumFloors() && bUseRandomNumFloors", EditConditionHides, DisplayPriority = "22")
	)
	int MinNumFloors = 3;
	
	/* Maximum number of floors */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (EditCondition = "!AutoComputeNumFloors() && bUseRandomNumFloors", EditConditionHides, DisplayPriority = "23")
	)
	int MaxNumFloors = 5;


	/* The number of subdivisions in a wall (increase it if you want smoothly bended borders) */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (DisplayPriority = "100")
	)
	int WallSubdivisions = 0;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (DisplayPriority = "102")
	)
	/**
	 * If true, then the next time the Building is generated, ExtraWallBottom is set to the difference
	 * between the maximum and minimum Z coordinates of the splines points, to which we add PadBottom.
	 */
	bool bAutoPadWallBottom = true;
	
	/* Wall height below the first floor */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (EditCondition = "!bAutoPadWallBottom", EditConditionHides, DisplayPriority = "103")
	)
	int ExtraWallBottom = 0;
	
	/* Wall height below the first floor */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (EditCondition = "bAutoPadWallBottom", EditConditionHides, DisplayPriority = "103")
	)
	int PadBottom = 0;
	
	/* Wall height above the last floor */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (DisplayPriority = "104")
	)
	int ExtraWallTop = 20;


	/** Windows Settings */

	/**
	 * Starting from the ground floor, specify for each level how you want doors/windows placed.
	 * If the array is smaller than the number of floors in your buildings, then the top floors
	 * will all use the last windows specification in the array.
	 * By default, there is one windows specification for the ground floor, and one for all the
	 * other floors. Feel free to change these by modifying the array. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Windows",
		meta = (DisplayPriority = "1")
	)
	TArray<FWindowsSpecification> WindowsSpecifications;


	/** Roof Settings */


	/* Build roof */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Roof",
		meta = (DisplayPriority = "1")
	)
	ERoofKind RoofKind = ERoofKind::Flat;

	
	/* The length of the roof that goes outside the building */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Roof",
		meta = (
			EditCondition = "RoofKind == ERoofKind::Point || RoofKind == ERoofKind::InnerSpline",
			EditConditionHides, DisplayPriority = "3"
		)
	)
	double OuterRoofDistance = 100;
	
	/* The distance from the walls to the inner/top part of the roof */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Roof",
		meta = (
			EditCondition = "RoofKind == ERoofKind::InnerSpline",
			EditConditionHides, DisplayPriority = "4"
		)
	)
	double InnerRoofDistance = 50;
	
	/* Roof thickness */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Roof",
		meta = (
			EditCondition = "RoofKind == ERoofKind::Point || RoofKind == ERoofKind::InnerSpline",
			EditConditionHides, DisplayPriority = "5"
		)
	)
	double RoofThickness = 5;
	
	/* The distance from the top of the walls to the top of the roof */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Roof",
		meta = (
			EditCondition = "RoofKind == ERoofKind::Point || RoofKind == ERoofKind::InnerSpline",
			EditConditionHides, DisplayPriority = "6"
		)
	)
	double RoofHeight = 250;
};

#undef LOCTEXT_NAMESPACE