// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "BuildingsFromSplines/LogBuildingsFromSplines.h"
#include "BuildingsFromSplines/Building.h"
#include "LCCommon/LCGenerator.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Components/SplineComponent.h" 

#include "BuildingsFromSplines.generated.h"

#define LOCTEXT_NAMESPACE "FBuildingsFromSplinesModule"

USTRUCT(BlueprintType)
struct FWeightedBuildingConfigurationClass
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeightedConfig")
	TSubclassOf<UBuildingConfiguration> BuildingConfigurationClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeightedConfig")
	double Weight = 1;

	static TSubclassOf<UBuildingConfiguration> GetRandomBuildingConfigurationClass(TArray<FWeightedBuildingConfigurationClass>& BuildingConfigurationClasses);
};


UCLASS(PrioritizeCategories = "Buildings")
class BUILDINGSFROMSPLINES_API ABuildingsFromSplines : public AActor, public ILCGenerator
{
	GENERATED_BODY()

public:
	ABuildingsFromSplines();

	/* Delete previous buildings before generating new ones */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Buildings",
		meta = (DisplayPriority = "-7")
	)
	bool bDeleteOldBuildingsWhenCreatingBuildings = true;

	/* Check this to skip splines for which there already exist a building from previous runs */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Buildings",
		meta = (DisplayPriority = "-6")
	)
	bool bSkipExistingBuildings = true;

	/* Continue generating buildings even when some have errors */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Buildings",
		meta = (DisplayPriority = "-5")
	)
	bool bContinueDespiteErrors = true;

	/* The folder in which to create the buildings.  This setting is unused when generating from a combination or from blueprints. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Buildings",
		meta = (DisplayPriority = "-4")
	)
	FName SpawnedActorsPath = TEXT("BuildingsFromSplines");

	/* The tag of the actors containing spline components to search for. If None, all actors will be used. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Buildings",
		meta = (DisplayPriority = "-3")
	)
	FName SplinesTag;
	
	/* The tag of the Spline Components to use to generate buildings. If None, all spline components will be used. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Buildings",
		meta = (DisplayPriority = "-2")
	)
	FName SplineComponentsTag;
	
	/* Whether the generated buildings are spatially loaded. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Buildings",
		meta = (DisplayPriority = "0")
	)
	bool bBuildingsSpatiallyLoaded = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Buildings",
		meta = (DisplayPriority = "1")
	)
	bool bUseInlineBuildingConfiguration = true;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Buildings",
		meta = (EditConditionHides, EditCondition = "!bUseInlineBuildingConfiguration", DisplayPriority = "2")
	)
	TArray<FWeightedBuildingConfigurationClass> BuildingConfigurationClasses;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Buildings",
		meta = (
			EditConditionHides, EditCondition = "bUseInlineBuildingConfiguration",
			ShowOnlyInnerProperties,
			DisplayPriority = "4",
			DisplayName = "Inline Building Configuration"
		)
	)
	TObjectPtr<UBuildingConfiguration> BuildingConfiguration;

	UPROPERTY(
		EditAnywhere, DuplicateTransient, Category = "Buildings",
		meta = (EditCondition = "false", EditConditionHides)
	)
	TArray<TSoftObjectPtr<ABuilding>> Buildings;

	UFUNCTION(BlueprintCallable, Category = "Buildings",
		meta = (DisplayPriority = "100")
	)
	bool GenerateBuildings(FName SpawnedActorsPathOverride, bool bIsUserInitiated);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Buildings",
		meta = (DisplayPriority = "101")
	)
	void GenerateBuildingsEditor() { GenerateBuildings(FName(), true); }


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
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "BuildingConfigurationConversion",
		meta = (DisplayPriority = "1001")
	)
	TSubclassOf<UBuildingConfiguration> LoadBuildingConfigurationClass = nullptr;

#if WITH_EDITOR

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "BuildingConfigurationConversion",
		meta = (DisplayPriority = "111")
	)
	void CreateBuildingConfigurationClassFromInlineBuildingConfiguration()
	{
		if (IsValid(BuildingConfiguration))
		{
			FWeightedBuildingConfigurationClass WeightedBuildingConfigurationClass;
			WeightedBuildingConfigurationClass.BuildingConfigurationClass = BuildingConfiguration->CreateClass(BuildingConfigurationClassPath, BuildingConfigurationClassName);;
			WeightedBuildingConfigurationClass.Weight = 1.0f;

			if (IsValid(WeightedBuildingConfigurationClass.BuildingConfigurationClass))
			{
				bUseInlineBuildingConfiguration = false;
				BuildingConfigurationClasses.Add(MoveTemp(WeightedBuildingConfigurationClass));
			}
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
		if (!IsValid(LoadBuildingConfigurationClass))
		{
			LCReporter::ShowError(
				LOCTEXT("InvalidInlineBuildingConfiguration", "The Inline Building Configuration is not valid.")
			);
			return;
		}

		if (BuildingConfiguration->LoadFromClass(LoadBuildingConfigurationClass))
		{
			bUseInlineBuildingConfiguration = true;
			LCReporter::ShowInfo(
				FText::Format(
					LOCTEXT("CopyDataAssetToInlineBuildingConfiguration", "Class {0} was copied to Inline Building Configuration."),
					LoadBuildingConfigurationClass->GetDisplayNameText()
				),
				"SuppressCopyDataAssetToInlineBuildingConfiguration"
			);
		}
		else
		{
			LCReporter::ShowError(
				FText::Format(
					LOCTEXT("CopyDataAssetToInlineBuildingConfigurationFailed", "Failed to copy class {0} to Inline Building Configuration."),
					LoadBuildingConfigurationClass->GetDisplayNameText()
				)
			);
		}
	}

#endif

	virtual bool OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated) override;

	virtual TArray<UObject*> GetGeneratedObjects() const {
		TArray<UObject*> GeneratedObjects;
		for (const TSoftObjectPtr<ABuilding>& Building : Buildings)
		{
			if (Building.IsValid())
			{
				GeneratedObjects.Add(Building.Get());
			}
		}
		return GeneratedObjects;
	}

	virtual bool Cleanup_Implementation(bool bSkipPrompt) override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Buildings",
		meta = (DisplayPriority = "4", DisplayName = "Clear Buildings")
	)
	void ClearBuildings();

#if WITH_EDITOR
	virtual AActor* Duplicate(FName FromName, FName ToName) override;
#endif

protected:

	bool GenerateBuilding(USplineComponent* SplineComponent, FName SpawnedActorsPathOverride, bool bIsUserInitiated);
	TArray<USplineComponent*> FindSplineComponents(bool bIsUserInitiated);
	TArray<AActor*> FindActors();

	TSet<USplineComponent*> ProcessedSplines;
};

#undef LOCTEXT_NAMESPACE