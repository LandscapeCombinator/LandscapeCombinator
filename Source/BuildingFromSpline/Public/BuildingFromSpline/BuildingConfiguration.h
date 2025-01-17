// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Engine/StaticMesh.h"
#include "Components/ActorComponent.h" 
#include "CoreMinimal.h"

#include "LCCommon/LCReporter.h"

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
enum class EAttachmentKind : uint8
{
	/* Choose this if you need simple instanced static meshes for doors/windows (fastest) */
	InstancedStaticMeshComponent,

	/* Choose this if you need curved doors/windows (along the spline). Slower than ISM. */
	SplineMeshComponent,

	/* Choose this to attach actor or blueprints to your buildings. Slower than ISM. */
	Actor,
};

USTRUCT(BlueprintType)
struct FAttachment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	EAttachmentKind AttachmentKind = EAttachmentKind::InstancedStaticMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment",
		meta=(EditConditionHides, EditCondition="AttachmentKind == EAttachmentKind::Actor"))
	TSubclassOf<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment",
		meta=(EditConditionHides, EditCondition="AttachmentKind != EAttachmentKind::Actor"))
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	// offset from the bottom/start of the wall segment
	FVector Offset = FVector(0, 0, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	// rotation relative to the wall segment
	FRotator Rotator = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	double OverrideWidth = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	double OverrideHeight = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	double OverrideThickness = 0;

	bool operator==(const FAttachment& Other) const = default;

	friend uint32 GetTypeHash(const FAttachment& Attachment)
	{
		return FCrc::MemCrc32(&Attachment, sizeof(FAttachment));
	}
};

UENUM(BlueprintType)
enum class EWallSegmentKind : uint8
{
	FillerWall,
	FixedSizeWall,
	Hole
};

USTRUCT(BlueprintType)
struct FWallSegment : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallSegment",
		meta = (DisplayPriority = "0")
	)
	EWallSegmentKind WallSegmentKind = EWallSegmentKind::FillerWall;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallSegment",
		meta = (EditCondition = "WallSegmentKind == EWallSegmentKind::FixedSizeWall || WallSegmentKind == EWallSegmentKind::Hole", EditConditionHides, DisplayPriority = "1")
	)
	double SegmentLength = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallSegment",
		meta = (EditCondition = "WallSegmentKind == EWallSegmentKind::FillerWall", EditConditionHides, DisplayPriority = "1")
	)
	double MinSegmentLength = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallSegment",
		meta = (EditCondition = "WallSegmentKind == EWallSegmentKind::Hole", EditConditionHides, DisplayPriority = "1")
	)
	double HoleDistanceToFloor = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallSegment",
		meta = (EditCondition = "WallSegmentKind == EWallSegmentKind::Hole", EditConditionHides, DisplayPriority = "1")
	)
	double HoleHeight = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallSegment",
		meta = (DisplayPriority = "1")
	)
	TArray<FAttachment> Attachments;

	double GetReferenceSize() const
	{
		switch (WallSegmentKind)
		{
			case EWallSegmentKind::FillerWall:
				return MinSegmentLength;
			case EWallSegmentKind::FixedSizeWall:
				return SegmentLength;
			case EWallSegmentKind::Hole:
				return SegmentLength;
			default:
			{
				check(false);
				return 0;
			}
		}
	}

	bool operator==(const FWallSegment& Other) const
	{
		if (this == &Other) return true;
		if (WallSegmentKind != Other.WallSegmentKind) return false;
		if (Attachments != Other.Attachments) return false;

		switch (WallSegmentKind)
		{
			case EWallSegmentKind::FillerWall:
				return MinSegmentLength == Other.MinSegmentLength;
			case EWallSegmentKind::FixedSizeWall:
				return SegmentLength == Other.SegmentLength;
			case EWallSegmentKind::Hole:
				return
					SegmentLength == MinSegmentLength &&
					HoleDistanceToFloor == Other.HoleDistanceToFloor &&
					HoleHeight == Other.HoleHeight;
			default:
				return false;
		}
	}

	friend uint32 GetTypeHash(const FWallSegment& WallSegment)
	{
		return FCrc::MemCrc32(&WallSegment, sizeof(FWallSegment));
	}
};

USTRUCT(BlueprintType)
struct FLevelDescription : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "WindowsSpecification",
		meta = (DisplayPriority = "1")
	)
	double LevelHeight = 300;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "WindowsSpecification",
		meta = (DisplayPriority = "1")
	)
	double FloorThickness = 20;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "WindowsSpecification",
		meta = (GetOptions="BuildingConfiguration.GetWallSegmentNames", DisplayPriority = "2")
	)
    TArray<FName> WallSegmentsIds;

	bool GetWallSegments(TArray<FWallSegment*> &OutWallSegments, UDataTable *WallSegmentsTable, double TargetSize) const;

	bool operator==(const FLevelDescription& Other) const
	{
		return LevelHeight == Other.LevelHeight
			&& FloorThickness == Other.FloorThickness
			&& WallSegmentsIds == Other.WallSegmentsIds;
	}

	friend uint32 GetTypeHash(const FLevelDescription& LevelDescription)
	{
		return FCrc::MemCrc32(&LevelDescription, sizeof(FLevelDescription));
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

	// check that all ids are valid, and that BeginRepeat and EndRepeat only appear in a well
	// parenthesized manner
	template<class T>
	static bool AreValidIds(
		const TArray<FName> Ids,
		TFunction<T*(FName)> GetItem
	)
	// template code goes in header to avoid linker error
	{
		bool bInRepeat = false;
		for (const FName& Id : Ids)
		{
			if (Id == "BeginRepeat")
			{
				if (bInRepeat)
				{
					ULCReporter::ShowError(
						LOCTEXT("InvalidBeginRepeat", "Found 'BeginRepeat' but already in repeat block."),
						LOCTEXT("InvalidIds", "Invalid List of Ids")
					);
					return false;
				}
				bInRepeat = true;
			}
			else if (Id == "EndRepeat")
			{
				if (!bInRepeat)
				{
					ULCReporter::ShowError(
						LOCTEXT("InvalidEndRepeat", "Found 'EndRepeat' but not in repeat block."),
						LOCTEXT("InvalidIds", "Invalid List of Ids")
					);
					return false;
				}
				bInRepeat = false;
			}
			else
			{
				T* Item = GetItem(Id);
				if (!Item)
				{
					ULCReporter::ShowError(
						FText::Format(LOCTEXT("InvalidId", "Found invalid Id: '{0}.'"), { FText::FromString(Id.ToString()) }),
						LOCTEXT("InvalidIds", "Invalid List of Ids")
					);
					return false;
				}
			}
		}

		if (bInRepeat)
		{
			ULCReporter::ShowError(
				LOCTEXT("InCompleteRepeatBlock", "Incomplete repeat block, no 'EndRepeat' found."),
				LOCTEXT("InvalidIds", "Invalid List of Ids")
			);
			return false;
		}

		return true;
	}

	template<class T>
	static bool Expand(
		TArray<T*> &OutItems,
		const TArray<FName> Ids,
		TFunction<T*(FName)> GetItem,
		TFunction<double(T*)> GetSize,
		double TargetSize
	);

	UFUNCTION()
	static TArray<FName> GetWallSegmentNames();

	UFUNCTION()
	static TArray<FName> GetLevelNames();

	UFUNCTION()
	bool OwnerIsBFS();

	UFUNCTION()
	bool AutoComputeNumFloors();

	/** General */

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
	 * floors is set to the height in meters divided by 3 meters. */
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


	/** Levels */

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Levels",
		meta = (GetOptions="BuildingConfiguration.GetLevelNames", DisplayPriority = "1")
	)
	TArray<FName> Levels = {
		"GroundLevel",
		"BeginRepeat",
		"LevelA",
		"LevelB",
		"EndRepeat"
	};


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