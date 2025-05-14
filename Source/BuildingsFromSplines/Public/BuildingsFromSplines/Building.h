// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "BuildingsFromSplines/BuildingConfiguration.h"
#include "LCCommon/LCGenerator.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "SegmentTypes.h"
#include "GameFramework/Volume.h" 

#include "Building.generated.h"

#define LOCTEXT_NAMESPACE "FBuildingsFromSplinesModule"

using namespace UE::Geometry;

UCLASS()
class BUILDINGSFROMSPLINES_API ABuilding : public AActor, public ILCGenerator
{
	GENERATED_BODY()

public:
	ABuilding();

	virtual void Destroyed() override;
	
	double MinHeightLocal = MAX_dbl;
	double MaxHeightLocal = MAX_dbl;
	double MinHeightWorld = MAX_dbl;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	TObjectPtr<USceneComponent> EmptySceneComponent;
	
	UPROPERTY(
		VisibleAnywhere, BlueprintReadWrite, Category = "Building",
		meta = (DisplayPriority = "50")
	)
	TObjectPtr<USplineComponent> SplineComponent;
	
	UPROPERTY(VisibleAnywhere, Category = "Building")
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;
	
	UPROPERTY(VisibleAnywhere, Category = "Building")
	TObjectPtr<UDynamicMeshComponent> DynamicMeshComponent;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building",
		meta = (DisplayPriority = "-1")
	)
	/* Folder used to spawn the volume. This setting is unused when generating from a combination or from blueprints. */
	FName SpawnedActorsPath;

	/* Automatically regenerate the building when a property is modified. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building",
		meta = (DisplayPriority = "0")
	)
	bool bGenerateWhenModified = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building",
		meta = (DisplayPriority = "1")
	)
	bool bUseInlineBuildingConfiguration = true;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building",
		meta = (EditConditionHides, EditCondition = "!bUseInlineBuildingConfiguration", DisplayPriority = "2")
	)
	TSubclassOf<UBuildingConfiguration> BuildingConfigurationClass = UBuildingConfiguration::StaticClass();

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Building",
		meta = (
			EditConditionHides, EditCondition = "bUseInlineBuildingConfiguration",
			ShowOnlyInnerProperties,
			DisplayPriority = "4",
			DisplayName = "Inline Building Configuration"
		)
	)
	TObjectPtr<UBuildingConfiguration> BuildingConfiguration;

	virtual bool Cleanup_Implementation(bool bSkipPrompt) override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Building",
		meta = (DisplayPriority = "101")
	)
	void DeleteBuilding();

	bool GenerateBuilding_Internal(FName SpawnedActorsPathOverride);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Building",
		meta = (DisplayPriority = "100")
	)
	void GenerateBuilding();

	virtual bool OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated) override;
	virtual TArray<UObject*> GetGeneratedObjects() const override;

#if WITH_EDITOR
	
	UFUNCTION(BlueprintCallable, Category = "Building")
	void GenerateStaticMesh();

	UFUNCTION(BlueprintCallable, Category = "Building")
	void GenerateVolume(FName SpawnedActorsPathOverride);

#endif
	
	UFUNCTION(BlueprintCallable, Category = "Building")
	bool AppendBuilding(UDynamicMesh* TargetMesh, FName SpawnedActorsPathOverride);

	void ComputeMinMaxHeight();

	UFUNCTION()
	void SetReceivesDecals();
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "BuildingConfigurationConversion",
		meta = (DisplayPriority = "1000")
	)
	FString BuildingConfigurationClassPath = "/Game";
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "BuildingConfigurationConversion",
		meta = (DisplayPriority = "1001")
	)
	FString BuildingConfigurationClassName = "BCFG_Converted";

#if WITH_EDITOR

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "BuildingConfigurationConversion",
		meta = (DisplayPriority = "111")
	)
	void CreateBuildingConfigurationClassFromInlineBuildingConfiguration()
	{
		if (IsValid(BuildingConfiguration))
		{
			BuildingConfigurationClass = BuildingConfiguration->CreateClass(BuildingConfigurationClassPath, BuildingConfigurationClassName);
			if (IsValid(BuildingConfigurationClass)) bUseInlineBuildingConfiguration = false;
		}
		else
		{
			LCReporter::ShowError(
				LOCTEXT("InvalidInlineBuildingConfiguration", "Inline Building Configuration is not valid.")
			);
			return;
		}
	}

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "BuildingConfigurationConversion",
		meta = (DisplayPriority = "112")
	)
	void CopyBuildingConfigurationClassToInlineBuildingConfiguration()
	{
		if (!IsValid(BuildingConfiguration))
		{
			LCReporter::ShowError(
				LOCTEXT("InvalidInlineBuildingConfiguration", "The Inline Building Configuration is not valid.")
			);
			return;
		}

		if (BuildingConfiguration->LoadFromClass(BuildingConfigurationClass))
		{
			bUseInlineBuildingConfiguration = true;
			LCReporter::ShowInfo(
				FText::Format(
					LOCTEXT("CopyDataAssetToInlineBuildingConfiguration", "Class {0} was copied to Inline Building Configuration."),
					BuildingConfigurationClass->GetDisplayNameText()
				),
				"SuppressCopyDataAssetToInlineBuildingConfiguration"
			);
		}
		else
		{
			LCReporter::ShowError(
				FText::Format(
					LOCTEXT("CopyDataAssetToInlineBuildingConfigurationFailed", "Failed to copy class {0} to Inline Building Configuration."),
					BuildingConfigurationClass->GetDisplayNameText()
				)
			);
		}
	}

#endif


protected:
	TObjectPtr<UBuildingConfiguration> InternalBuildingConfiguration = nullptr;
	FWallSegment DummyFiller;
	double LevelsHeightsSum = 0;
	UDataTable *LevelsTable;
	UDataTable *WallSegmentsTable;
	TArray<int> LevelDescriptionsIndices;
	TArray<FLevelDescription> ExpandedLevels;

	TArray<TArray<TArray<FWallSegment>>> WallSegmentsAtSplinePoint;
	TArray<TArray<double>> FillersSizeAtSplinePoint;
	bool InitializeWallSegments();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;
#endif
	
	UPROPERTY()
	bool bIsGenerating = false;
	
	/* Holds a pointer to the volume generated by this actor. We destroy the volume when generating a new one. */
	UPROPERTY(
		EditAnywhere, DuplicateTransient, Category = "Building",
		meta = (EditCondition = "false", EditConditionHides)
	)
	TSoftObjectPtr<AVolume> Volume = nullptr;
	
	UPROPERTY(
		EditAnywhere, DuplicateTransient, Category = "Building",
		meta = (EditCondition = "false", EditConditionHides)
	)
	TArray<TSoftObjectPtr<USplineMeshComponent>> SplineMeshComponents;
	
	UPROPERTY(
		EditAnywhere, DuplicateTransient, Category = "Building",
		meta = (EditCondition = "false", EditConditionHides)
	)
	TArray<TSoftObjectPtr<UInstancedStaticMeshComponent>> InstancedStaticMeshComponents;

	UPROPERTY(DuplicateTransient)
	FString StaticMeshPath;

	// same as SplineComponent, but all points have the same Z coordinate as the lowest point,
	// and there are subdivisions (depending on the WallSubdivions property of the BuildingConfiguration)
	// and the points are clockwise (when seen from above in Unreal, which isn't the same as clockwise in TPolygon2
	// because of inverted Y-axis)
	TObjectPtr<USplineComponent> BaseClockwiseSplineComponent;
	TArray<FTransform> BaseClockwiseFrames;
	TArray<double> BaseClockwiseFramesTimes;
	TArray<FVector2D> BaseVertices2D;

	// these three maps contain the shifted polygons, one entry per thickness given in the user's BuildingConfiguration
	TMap<double, TArray<FVector2D>> InternalWallPolygons;
	TMap<double, TArray<FVector2D>> ExternalWallPolygons;
	TMap<double, TArray<int>> IndexToInternalIndex;
	void ComputeBaseVertices();
	void AddInternalThickness(double Thickness);
	void AddExternalThickness(double Thickness);

	TMap<UStaticMesh*, UInstancedStaticMeshComponent*> MeshToISM;
	FVector2D GetIntersection(FVector2D Point1, FVector2D Direction1, FVector2D Point2, FVector2D Direction2);
	void DeflateFrames(TArray<FTransform> Frames, TArray<FVector2D>& OutOffsetPolygon, TArray<int>& OutIndexToOffsetIndex, double Offset);

	TArray<int> SplineIndexToBaseSplineIndex;
	void ComputeOffsetPolygons();
	TArray<FVector2D> MakePolygon(bool bInternalWall, double BeginDistance, double Length, double Thickness);
	FVector2D GetShiftedPoint(TArray<FTransform> Frames, int Index, double Shift, bool bIsLoop);

	bool AppendWallsWithHoles(UDynamicMesh* TargetMesh, bool bInternalWall, double ZOffset, int LevelDescriptionIndex);
	bool AppendWallsWithHoles(UDynamicMesh* TargetMesh);
	void AddSplineMesh(UStaticMesh* StaticMesh, double BeginDistance, double Length, double Thickness, double Height, FVector Offset, ESplineMeshAxis::Type SplineMeshAxis);
	void AppendAlongSpline(UDynamicMesh* TargetMesh, bool bInternalWall, double BeginDistance, double Length, double Height, double ZOffset, double Thickness, int MaterialID);
	void AppendRoof(UDynamicMesh* TargetMesh);
	bool AppendFloors(UDynamicMesh *TargetMesh);
	void AppendBuildingStructure(UDynamicMesh* TargetMesh);
	bool AppendBuildingWithoutInside(UDynamicMesh *TargetMesh);
	
	bool AddAttachments();
	bool AddAttachments(int LevelDescriptionIndex, double ZOffset);

};

#undef LOCTEXT_NAMESPACE
