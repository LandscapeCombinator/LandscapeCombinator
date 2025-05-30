// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Engine/StaticMesh.h"
#include "Engine/DataTable.h"
#include "Components/ActorComponent.h" 
#include "Components/SplineMeshComponent.h"
#include "CoreMinimal.h"

#include "ConcurrencyHelpers/LCReporter.h"
#include "BuildingsFromSplines/LogBuildingsFromSplines.h"

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
struct FAttachment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment", meta=(DisplayPriority = "0"))
	EAttachmentKind AttachmentKind = EAttachmentKind::InstancedStaticMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment",
		meta=(EditConditionHides, EditCondition="AttachmentKind == EAttachmentKind::Actor", DisplayPriority = "1"))
	TSubclassOf<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment",
		meta=(EditConditionHides, EditCondition="AttachmentKind != EAttachmentKind::Actor", DisplayPriority = "2"))
	TObjectPtr<UStaticMesh> Mesh;

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
		meta=(EditConditionHides, EditCondition="AttachmentKind == EAttachmentKind::SplineMeshComponent", DisplayPriority = "5"))
	TEnumAsByte<ESplineMeshAxis::Type> SplineMeshAxis = ESplineMeshAxis::X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment", meta=(DisplayPriority = "6"))
	bool bFitToWallSegmentWidth = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment", meta=(DisplayPriority = "7"))
	bool bFitToWallSegmentHeight = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment", meta=(DisplayPriority = "7"))
	bool bFitToWallSegmentThickness = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment",
		meta=(EditConditionHides, EditCondition="!bFitToWallSegmentWidth", DisplayPriority = "10")
	)
	double OverrideWidth = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment",
		meta=(EditConditionHides, EditCondition="!bFitToWallSegmentHeight", DisplayPriority = "11")
	)
	double OverrideHeight = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment",
		meta=(EditConditionHides, EditCondition="!bFitToWallSegmentThickness", DisplayPriority = "12")
	)
	double OverrideThickness = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment",
		meta=(EditConditionHides, EditCondition="AttachmentKind != EAttachmentKind::SplineMeshComponent", DisplayPriority = "20")
	)
	EAxisKind XAxis = EAxisKind::Width;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment",
		meta=(EditConditionHides, EditCondition="AttachmentKind != EAttachmentKind::SplineMeshComponent", DisplayPriority = "21")
	)
	EAxisKind YAxis = EAxisKind::Thickness;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment",
		meta=(EditConditionHides, EditCondition="AttachmentKind != EAttachmentKind::SplineMeshComponent", DisplayPriority = "22")
	)
	EAxisKind ZAxis = EAxisKind::Height;
};

UENUM(BlueprintType)
enum class EWallSegmentKind : uint8
{
	Wall,
	Hole
};

USTRUCT(BlueprintType)
struct FWallSegment
{
	GENERATED_BODY()

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
	int UnderHoleInteriorMaterialIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "WallSegment",
		meta = (EditCondition = "WallSegmentKind == EWallSegmentKind::Hole", EditConditionHides, DisplayPriority = "2")
	)
	int OverHoleInteriorMaterialIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "WallSegment",
		meta = (EditCondition = "WallSegmentKind == EWallSegmentKind::Hole", EditConditionHides, DisplayPriority = "3")
	)
	int UnderHoleExteriorMaterialIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "WallSegment",
		meta = (EditCondition = "WallSegmentKind == EWallSegmentKind::Hole", EditConditionHides, DisplayPriority = "4")
	)
	int OverHoleExteriorMaterialIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "WallSegment",
		meta = (EditCondition = "WallSegmentKind != EWallSegmentKind::Hole", EditConditionHides, DisplayPriority = "1")
	)
	int InteriorWallMaterialIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "WallSegment",
		meta = (EditCondition = "WallSegmentKind != EWallSegmentKind::Hole", EditConditionHides, DisplayPriority = "2")
	)
	int ExteriorWallMaterialIndex = 0;

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

USTRUCT(BlueprintType)
struct FLoop
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loop")
	int StartIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loop")
	int EndIndex = 0;
};

USTRUCT(BlueprintType)
struct FLevelDescription
{
	GENERATED_BODY()

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "WindowsSpecification",
		meta = (DisplayPriority = "1")
	)
	double LevelHeight = 300;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "WindowsSpecification",
		meta = (DisplayPriority = "2")
	)
	double FloorThickness = 20;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "WindowsSpecification",
		meta = (DisplayPriority = "10")
	)
	int FloorMaterialIndex = 0;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "WindowsSpecification",
		meta = (DisplayPriority = "11")
	)
	/* Ceiling Material Index for the floor below this one */
	int UnderFloorMaterialIndex = 0;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "WindowsSpecification",
		meta = (DisplayPriority = "12")
	)
	/**
	 * if checked, the wall segments correspond to the space between two corners
	 * if unchecked, the wall segments correspond to the whole level, all around the building 
	 */
	bool bResetWallSegmentsOnCorners = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindowsSpecification", meta = (DisplayPriority = "100"))
    TArray<FWallSegment> WallSegments;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindowsSpecification", meta = (DisplayPriority = "101"))
    TArray<FLoop> WallSegmentLoops;

	FString ToString() const
	{
		FString Result;
		Result += "LevelHeight: ";
		Result += FString::SanitizeFloat(LevelHeight);
		Result += ", FloorThickness: ";
		Result += FString::SanitizeFloat(FloorThickness);
		Result += ", WallSegments: ";
		Result += FString::FromInt(WallSegments.Num());
		for (auto &WallSegment : WallSegments)
		{
			Result += ", ";
			Result += WallSegment.ToString();
		}
		Result += ", WallSegmentLoops: ";
		Result += FString::FromInt(WallSegmentLoops.Num());
		return Result;
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

UCLASS(PrioritizeCategories = "Building", BlueprintType, Blueprintable)
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

	/* The tickness of the external walls */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (EditCondition = "BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside", EditConditionHides, DisplayPriority = "0")
	)
	double ExternalWallThickness = 10;
	
	/* The tickness of the internal walls */
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

	/* Recompute UVs using AutoGenerateXAtlasMeshUVs (slow operation). */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Structure",
		meta = (DisplayPriority = "1000")
	)
	bool bAutoGenerateXAtlasMeshUVs = false;


	/** Materials */

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials",
		meta = (DisplayPriority = "0")
	)
	TArray<TObjectPtr<UMaterialInterface>> Materials;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials",
		meta = (EditCondition = "BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside", EditConditionHides, DisplayPriority = "0")
	)
	int InteriorMaterialIndex = 0;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials",
		meta = (DisplayPriority = "1")
	)
	int ExteriorMaterialIndex = 0;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials", meta = (DisplayPriority = "2")
	)
	int RoofMaterialIndex = 0;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building|Materials",
		meta = (EditCondition = "BuildingGeometry == EBuildingGeometry::BuildingWithFloorsAndEmptyInside && RoofKind != ERoofKind::None", EditConditionHides, DisplayPriority = "3")
	)
	int UnderRoofMaterialIndex = 0;

	/** Levels */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Levels", meta = (DisplayPriority = "1"))
	TArray<FLevelDescription> Levels;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Levels", meta = (DisplayPriority = "2"))
	TArray<FLoop> LevelLoops;


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
	bool AutoComputeNumFloors(ABuilding *Building);

	// check that all ids are valid, and that BeginRepeat and EndRepeat only appear in a well
	// parenthesized manner
	static bool AreValidLoops(int NumItems, const TArray<FLoop> &Loops)
	// template code goes in header to avoid linker error
	{
		int LastSeenIndex = -1;
		for (const FLoop& Loop : Loops)
		{
			if (LastSeenIndex < Loop.StartIndex && 0 <= Loop.StartIndex && Loop.StartIndex <= Loop.EndIndex && Loop.EndIndex < NumItems)
			{
				// valid loop
				LastSeenIndex = Loop.EndIndex;
			}
			else
			{
				if (Loop.StartIndex < 0)
				{
					LCReporter::ShowError(
						FText::Format(
							LOCTEXT(
								"InvalidStartIndex", "Invalid loop ({0}, {1}) found in loops array:\n"
								"StartIndex ({0}) must be >= 0"
							),
							FText::AsNumber(Loop.StartIndex),
							FText::AsNumber(Loop.EndIndex)
						)
					);
				}
				else if (LastSeenIndex >= Loop.StartIndex)
				{
					LCReporter::ShowError(
						FText::Format(
							LOCTEXT(
								"InvalidEndIndex", "Invalid loop ({0}, {1}) found in loops array:\n"
								"EndIndex ({1}) of previous loop must be >= StartIndex ({0})"
							),
							FText::AsNumber(Loop.StartIndex),
							FText::AsNumber(Loop.EndIndex)
						)
					);
				}
				else if (Loop.EndIndex >= NumItems)
				{
					LCReporter::ShowError(
						FText::Format(
							LOCTEXT(
								"InvalidEndIndex", "Invalid loop ({0}, {1}) found in loops array:\n"
								"EndIndex ({1}) must be < NumItems ({2})"
							),
							FText::AsNumber(Loop.StartIndex),
							FText::AsNumber(Loop.EndIndex),
							FText::AsNumber(NumItems)
						)
					);
				}
				else if (Loop.StartIndex > Loop.EndIndex)
				{
					LCReporter::ShowError(
						FText::Format(
							LOCTEXT(
								"InvalidLoop", "Invalid loop ({0}, {1}) found in loops array:\n"
								"StartIndex ({0}) must be <= EndIndex ({1})"
							),
							FText::AsNumber(Loop.StartIndex),
							FText::AsNumber(Loop.EndIndex)
						)
					);
				}
				else
				{
					check(false);
				}
				return false;
				
			}
		}
		return true;
	}

	template<class T>
	static bool Expand(
		TArray<T> &OutItems,
		const TArray<T> &Items,
		const TArray<FLoop> &Loops,
		TFunction<double(const T&)> GetSize,
		double TargetSize
	)
	// template code goes in header to avoid linker error
	{
		OutItems.Empty();
		if (!AreValidLoops(Items.Num(), Loops)) return false;

		int NumLoops = Loops.Num();
		TArray<double> LoopSizes;
		LoopSizes.SetNum(NumLoops);
		// bool bInsideLoop = false;
		int NumItems = Items.Num();
		double OverallSizes = 0; // initialized with items that are outside loops

		int LoopIndex = 0;
		double CurrentLoopSize = 0;

		// first pass on the array to find the size of each loop
		for (int IdIndex = 0; IdIndex < NumItems; IdIndex++)
		{
			if (LoopIndex < NumLoops)
			{
				// before the next loop
				if (IdIndex < Loops[LoopIndex].StartIndex)
				{
					OverallSizes += GetSize(Items[IdIndex]);
				}
				// in the middle of a loop
				else if (IdIndex < Loops[LoopIndex].EndIndex)
				{
					CurrentLoopSize += GetSize(Items[IdIndex]);
				}
				// at the end of a loop
				else if (IdIndex == Loops[LoopIndex].EndIndex)
				{
					CurrentLoopSize += GetSize(Items[IdIndex]);
					LoopSizes[LoopIndex] = CurrentLoopSize;
					CurrentLoopSize = 0;
					LoopIndex++;
				}
				// at the beginning of a loop (different than the end)
				else if (IdIndex == Loops[LoopIndex].StartIndex)
				{
					CurrentLoopSize = GetSize(Items[IdIndex]);
				}
			}
			// there are no more loops
			else
			{
				OverallSizes += GetSize(Items[IdIndex]);
			}
		}

		// if we are already above the threshold, return an empty array
		if (OverallSizes > TargetSize) return true;

		// compute how many times each loop can be unrolled, in a fair way
		TArray<int> LoopUnrollCounts;
		LoopUnrollCounts.Init(0, LoopSizes.Num());
		while (true)
		{
			bool bUnrolled = false;

			for (LoopIndex = 0; LoopIndex < LoopSizes.Num(); LoopIndex++)
			{
				if (OverallSizes + LoopSizes[LoopIndex] <= TargetSize)
				{
					if (LoopSizes[LoopIndex] <= 0)
					{
						UE_LOG(LogBuildingsFromSplines, Warning, TEXT("Invalid loop size: %f, continuing anyway"), LoopSizes[LoopIndex]);
						continue;
					}
					LoopUnrollCounts[LoopIndex]++;
					OverallSizes += LoopSizes[LoopIndex];
					bUnrolled = true;
				}
			}
			
			if (!bUnrolled) break;
		}

		// second pass on the loop to unroll the loops
		int IdIndex = 0;
		LoopIndex = 0;
		while (IdIndex < NumItems)
		{
			if (LoopIndex < NumLoops)
			{
				// before the next loop
				if (IdIndex < Loops[LoopIndex].StartIndex)
				{
					OutItems.Add(Items[IdIndex]);
					IdIndex++;
				}
				// at the beginning of a loop
				else if (IdIndex == Loops[LoopIndex].StartIndex)
				{
					for (int k = 0; k < LoopUnrollCounts[LoopIndex]; k++)
					{
						for (int j = Loops[LoopIndex].StartIndex; j <= Loops[LoopIndex].EndIndex; j++)
						{
							OutItems.Add(Items[j]);
						}
					}
					IdIndex = Loops[LoopIndex].EndIndex + 1;
					LoopIndex++;
				}
			}
			// there are no more loops
			else
			{
				OutItems.Add(Items[IdIndex]);
				IdIndex++;
			}
		}
		return true;
	}


};

#undef LOCTEXT_NAMESPACE