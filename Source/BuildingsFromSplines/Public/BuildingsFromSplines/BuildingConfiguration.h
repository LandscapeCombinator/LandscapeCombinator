// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Engine/StaticMesh.h"
#include "Engine/DataTable.h"
#include "Components/ActorComponent.h" 
#include "Components/SplineMeshComponent.h"

#include "ConcurrencyHelpers/LCReporter.h"
#include "BuildingsFromSplines/LogBuildingsFromSplines.h"
#include "LCCommon/LCBlueprintLibrary.h"

#include "BuildingConfiguration.generated.h"

#define LOCTEXT_NAMESPACE "FBuildingsFromSplinesModule"

UENUM(BlueprintType)
enum class EBuildingGeometry : uint8
{
	/* Simple block without any geometry inside (best performance) */
	BuildingWithoutInside,

	/* Simple block with an empty inside */
	BuildingWithFloorsAndEmptyInside,
};

UENUM(BlueprintType)
enum class EAttachmentKind : uint8
{
	/* Choose this if you need simple instanced static meshes for doors/windows (fastest) */
	InstancedStaticMeshComponent,

	/* Choose this if you need curved doors/windows (along the spline). Slower than ISM. */
	SplineMeshComponent,

	/* Choose this to attach actor or blueprints to your buildings. Slower than ISM. */
	Actor,
};

UENUM(BlueprintType)
enum class EAxisKind : uint8
{
	Width, Thickness, Height
};

USTRUCT(BlueprintType)
struct FWeightedObject
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeightedConfig")
	TObjectPtr<UObject> Object;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeightedConfig")
	double Weight = 1;

	static TObjectPtr<UObject> GetRandomObject(const TArray<FWeightedObject>& Objects)
	{
		int Index = ULCBlueprintLibrary::GetRandomIndex<FWeightedObject>(Objects);
		if (0 <= Index && Index < Objects.Num()) return Objects[Index].Object;
		else return nullptr;
	}
};


USTRUCT(BlueprintType)
struct FAttachment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment", meta=(DisplayPriority = "-1"))
	// between 0 and 1
	float Probability = 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment", meta=(DisplayPriority = "0"))
	EAttachmentKind AttachmentKind = EAttachmentKind::InstancedStaticMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment",
		meta=(EditConditionHides, EditCondition="AttachmentKind == EAttachmentKind::Actor", DisplayPriority = "1"))
	TSubclassOf<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment",
		meta=(EditConditionHides, EditCondition="AttachmentKind != EAttachmentKind::Actor", DisplayPriority = "2"))
	TArray<FWeightedObject> MeshSelection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment", meta=(DisplayPriority = "3"))
	/* offset from the bottom/start of the wall segment:
	 * X is the tangent direction, Y is the inside direction, Z is the up direction */
	FVector Offset = FVector(0, 0, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment",
		meta=(EditConditionHides, EditCondition="AttachmentKind != EAttachmentKind::SplineMeshComponent", DisplayPriority = "4"))
	FVector AxisOffsetMultiplier = FVector(0, 0, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment",
		meta=(EditConditionHides, EditCondition="AttachmentKind != EAttachmentKind::SplineMeshComponent", DisplayPriority = "5"))
	/* by default, the attachment is rotated alongside the tangent, you can use this variable
	 * to add an extra rotation to the attachment */
	FRotator ExtraRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment",
		meta=(EditConditionHides, EditCondition="AttachmentKind == EAttachmentKind::SplineMeshComponent", DisplayPriority = "6"))
	TEnumAsByte<ESplineMeshAxis::Type> SplineMeshAxis = ESplineMeshAxis::X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment", meta=(DisplayPriority = "7"))
	bool bAddHoleZOffset = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment", meta=(DisplayPriority = "20"))
	bool bFitToWallSegmentWidth = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment", meta=(DisplayPriority = "21"))
	bool bFitToWallSegmentHeight = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment", meta=(DisplayPriority = "22"))
	bool bFitToHoleHeight = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment", meta=(DisplayPriority = "23"))
	bool bFitToWallSegmentThickness = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment",
		meta=(EditConditionHides, EditCondition="!bFitToWallSegmentWidth", DisplayPriority = "24")
	)
	double OverrideWidth = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment",
		meta=(EditConditionHides, EditCondition="!bFitToWallSegmentHeight && !bFitToHoleHeight", DisplayPriority = "25")
	)
	double OverrideHeight = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment",
		meta=(EditConditionHides, EditCondition="!bFitToWallSegmentThickness", DisplayPriority = "26")
	)
	double OverrideThickness = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment",
		meta=(EditConditionHides, EditCondition="AttachmentKind != EAttachmentKind::SplineMeshComponent", DisplayPriority = "100")
	)
	EAxisKind XAxis = EAxisKind::Width;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment",
		meta=(EditConditionHides, EditCondition="AttachmentKind != EAttachmentKind::SplineMeshComponent", DisplayPriority = "101")
	)
	EAxisKind YAxis = EAxisKind::Thickness;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment",
		meta=(EditConditionHides, EditCondition="AttachmentKind != EAttachmentKind::SplineMeshComponent", DisplayPriority = "102")
	)
	EAxisKind ZAxis = EAxisKind::Height;
};

UENUM(BlueprintType)
enum class EWallSegmentKind : uint8
{
	Wall,
	Hole
};

UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class UWallSegment : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallSegment", meta = (DisplayPriority = "-1"))
	EWallSegmentKind WallSegmentKind = EWallSegmentKind::Wall;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallSegment", meta = (DisplayPriority = "0"))
	bool bAutoExpand = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallSegment", meta = (DisplayPriority = "1"))
	double SegmentLength = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallSegment",
		meta = (EditCondition = "WallSegmentKind == EWallSegmentKind::Hole", EditConditionHides, DisplayPriority = "1")
	)
	double HoleDistanceToFloor = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallSegment",
		meta = (EditCondition = "WallSegmentKind == EWallSegmentKind::Hole", EditConditionHides, DisplayPriority = "1")
	)
	double HoleHeight = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "WallSegment",
		meta = (EditCondition = "WallSegmentKind == EWallSegmentKind::Hole", EditConditionHides, DisplayPriority = "1")
	)
	FString UnderHoleInteriorMaterialExpr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "WallSegment",
		meta = (EditCondition = "WallSegmentKind == EWallSegmentKind::Hole", EditConditionHides, DisplayPriority = "2")
	)
	FString OverHoleInteriorMaterialExpr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "WallSegment",
		meta = (EditCondition = "WallSegmentKind == EWallSegmentKind::Hole", EditConditionHides, DisplayPriority = "3")
	)
	FString UnderHoleExteriorMaterialExpr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "WallSegment",
		meta = (EditCondition = "WallSegmentKind == EWallSegmentKind::Hole", EditConditionHides, DisplayPriority = "4")
	)
	FString OverHoleExteriorMaterialExpr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "WallSegment",
		meta = (EditCondition = "WallSegmentKind != EWallSegmentKind::Hole", EditConditionHides, DisplayPriority = "1")
	)
	FString InteriorWallMaterialExpr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "WallSegment",
		meta = (EditCondition = "WallSegmentKind != EWallSegmentKind::Hole", EditConditionHides, DisplayPriority = "2")
	)
	FString ExteriorWallMaterialExpr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "WallSegment", meta = (DisplayPriority = "10"))
	bool bOverrideWallThickness = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "WallSegment",
		meta = (EditCondition = "bOverrideWallThickness", EditConditionHides, DisplayPriority = "11")
	)
	double InternalWallThickness = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "WallSegment",
		meta = (EditCondition = "bOverrideWallThickness", EditConditionHides, DisplayPriority = "12")
	)
	double ExternalWallThickness = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallSegment", meta = (DisplayPriority = "100"))
	TArray<FAttachment> Attachments;

	FString ToString() const
	{
		FString Result;
		Result += "WallSegmentKind: ";
		Result += UEnum::GetValueAsString(WallSegmentKind);
		Result += ", SegmentLength: ";
		Result += FString::SanitizeFloat(SegmentLength);
		Result += ", HoleDistanceToFloor: ";
		Result += FString::SanitizeFloat(HoleDistanceToFloor);
		Result += ", HoleHeight: ";
		Result += FString::SanitizeFloat(HoleHeight);
		Result += ", Attachments: ";
		Result += FString::FromInt(Attachments.Num());
		return Result;
	}
};

UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class ULevelDescription : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LevelDescription",
		meta = (DisplayPriority = "1")
	)
	double LevelHeight = 300;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LevelDescription",
		meta = (DisplayPriority = "2")
	)
	double FloorThickness = 20;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LevelDescription",
		meta = (DisplayPriority = "10")
	)
	FString FloorMaterialExpr;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LevelDescription",
		meta = (DisplayPriority = "11")
	)
	/* Ceiling Material Index for the floor below this one */
	FString UnderFloorMaterialExpr;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LevelDescription",
		meta = (DisplayPriority = "12")
	)
	/**
	 * if checked, the wall segments correspond to the space between two corners
	 * if unchecked, the wall segments correspond to the whole level, all around the building 
	 */
	bool bResetWallSegmentsOnCorners = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "LevelDescription", meta = (DisplayPriority = "100"))
    TMap<FString, TObjectPtr<UWallSegment>> WallSegmentsMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelDescription", meta = (DisplayPriority = "101"))
    FString WallSegmentsExpression;

	bool IsValidKey(FString Key) const
	{
		return WallSegmentsMap.Contains(Key) && IsValid(WallSegmentsMap[Key]);
	}

	bool CheckValidKey(FString Key) const
	{
		if (IsValidKey(Key)) return true;
		else
		{
			LCReporter::ShowError(FText::Format(
				LOCTEXT("Invalid Key", "Unknown WallSegment: '{0}'. Please adjust your expression."),
				FText::FromString(Key)
			));
			return false;
		}
	}
};

UENUM(BlueprintType)
enum class ERoofKind : uint8
{
	None,
	Flat,
	Point,
	InnerSpline,
	Hip,
	Gable
};

// this doesn't need to be UActorComponent, but changing back to UObject will make old maps crash on load
UCLASS(PrioritizeCategories = "Building", BlueprintType, Blueprintable, EditInlineNew)
class BUILDINGSFROMSPLINES_API UBuildingConfiguration : public UActorComponent
{
	GENERATED_BODY()

public:
	UBuildingConfiguration();

#if WITH_EDITOR

	TSubclassOf<UBuildingConfiguration> CreateClass(const FString &AssetPath, const FString& AssetName);
	
	bool LoadFromClass(TSubclassOf<UBuildingConfiguration> BuildingConfigurationClass);

#endif

	/** Structural Settings */

	/* Choose whether you want geometry inside your building, and holes for windows  */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (DisplayPriority = "-5")
	)
	EBuildingGeometry BuildingGeometry = EBuildingGeometry::BuildingWithoutInside;

	/* The thickness of the external walls */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (EditCondition = "BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside", EditConditionHides, DisplayPriority = "0")
	)
	double ExternalWallThickness = 10;
	
	/* The thickness of the internal walls */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (EditCondition = "BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside", EditConditionHides, DisplayPriority = "1")
	)
	double InternalWallThickness = 1;

	/* Build floor tiles. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (EditCondition = "BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside", EditConditionHides, DisplayPriority = "4")
	)
	bool bBuildFloorTiles = true;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (DisplayPriority = "19")
	)
	/**
	  * If true (this takes precedence over NumFloors or UseRandomNumFloors) and if the Asset User Data
	  * contains a levels value, the number of floors is set to this value .
	  * If it contains a height value, the number of floors is set to the height in meters divided by 3 meters. */
	bool bAutoComputeNumFloors = true;

	/* Random number of floors */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (DisplayPriority = "20")
	)
	bool bUseRandomNumFloors = false;
	
	/* The number of floors (including ground level) */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (EditCondition = "!bUseRandomNumFloors", EditConditionHides, DisplayPriority = "21")
	)
	int NumFloors = 5;
	
	/* Minimum number of floors */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (EditCondition = "bUseRandomNumFloors", EditConditionHides, DisplayPriority = "22")
	)
	int MinNumFloors = 4;
	
	/* Maximum number of floors */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (EditCondition = "bUseRandomNumFloors", EditConditionHides, DisplayPriority = "23")
	)
	int MaxNumFloors = 8;

	/* The number of subdivisions in a wall (increase it if you want round buildings) */
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

	/* Recompute UVs using AutoGenerateXAtlasMeshUVs for floors (slow operation). */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (DisplayPriority = "1002")
	)
	bool bAutoGenerateXAtlasMeshUVsFloors = true;

	/* Recompute UVs using AutoGenerateXAtlasMeshUVs for full building (slow operation). */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (DisplayPriority = "1003")
	)
	bool bAutoGenerateXAtlasMeshUVs = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (DisplayPriority = "1004")
	)
	bool bEnableComplexCollision = false;

	
	/** Materials */

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials",
		meta = (DisplayPriority = "0")
	)
	TMap<FString, TObjectPtr<UMaterialInterface>> Materials;

	TArray<FString> MaterialNamesArray;
	TArray<TObjectPtr<UMaterialInterface>> MaterialsArray;

	int ResolveMaterial(FString ExprStr);

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials",
		meta = (EditCondition = "BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside", EditConditionHides, DisplayPriority = "0")
	)
	FString InteriorMaterialExpr;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials",
		meta = (DisplayPriority = "1")
	)
	FString ExteriorMaterialExpr;

	/** Levels */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Levels", meta = (DisplayPriority = "0"))
	/**
	  * if false, then the same level can be generated in different ways due to randomization in the wall segments
	  * if true, then all occurrences of a given level in the building will be identical
	  */
	bool bCacheLevelsWithinBuilding = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Building|Levels", meta = (DisplayPriority = "1"))
	TMap<FString, TObjectPtr<ULevelDescription>> LevelsMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Levels", meta = (DisplayPriority = "2"))
	// an expression using the level names above, with space for concatenation, star (*) for repetition {A:2, B:3, C:5} for random choice
	FString LevelsExpression = "GroundLevel OtherLevel*";

	bool CheckValidKey(FString LevelDescriptionKey) const;


	/** Roof Settings */

	/* Build roof */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Roof",
		meta = (DisplayPriority = "1")
	)
	ERoofKind RoofKind = ERoofKind::Flat;

	/* The length of the roof that goes outside the building (overhang) */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Roof",
		meta = (
			EditCondition = "RoofKind != ERoofKind::None && RoofKind != ERoofKind::Flat",
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
			EditCondition = "RoofKind != ERoofKind::None && RoofKind != ERoofKind::Flat",
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
	
	/* The angle of roof faces in °, 0° means fully horizontal, and 90° fully vertical */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Roof",
		meta = (
			EditCondition = "RoofKind == ERoofKind::Hip || RoofKind == ERoofKind::Gable",
			EditConditionHides, DisplayPriority = "6"
		)
	)
	double RoofAngle = 30;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Roof",
		meta = (EditCondition = "RoofKind != ERoofKind::None", EditConditionHides, DisplayPriority = "100")
	)
	FString RoofMaterialExpr;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Roof",
		meta = (EditCondition = "BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside && RoofKind != ERoofKind::None", EditConditionHides, DisplayPriority = "101")
	)
	FString UnderRoofMaterialExpr;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Roof",
		meta = (EditCondition = "RoofKind == ERoofKind::Gable", EditConditionHides, DisplayPriority = "101")
	)
	FString GableMaterialExpr;

	
	/** Conversions to Static Mesh or Volume */
	
	/* Convert to static mesh after the building is generated. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Conversions",
		meta = (DisplayPriority = "0")
	)
	bool bConvertToStaticMesh = false;

	/* Enable Nanite when converting building to static mesh. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Conversions",
		meta = (EditCondition = "bConvertToStaticMesh", EditConditionHides, DisplayPriority = "1")
	)
	bool bEnableNanite = true;

	/* Whether the generated building can receive decals. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Conversions",
		meta = (EditCondition = "bConvertToStaticMesh", EditConditionHides, DisplayPriority = "2")
	)
	bool bBuildingReceiveDecals = false;

	/* Convert to volume after the building is generated. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Conversions",
		meta = (DisplayPriority = "3")
	)
	bool bConvertToVolume = false;


	UFUNCTION()
	bool AutoComputeNumFloors(UOSMUserData *BuildingOSMUserData);

};

#undef LOCTEXT_NAMESPACE