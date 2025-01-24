// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "BuildingFromSpline/LogBuildingFromSpline.h"
#include "BuildingFromSpline/Building.h"
#include "LCCommon/LCGenerator.h"

#include "Components/SplineComponent.h" 

#include "BuildingsFromSplines.generated.h"

#define LOCTEXT_NAMESPACE "FBuildingFromSplineModule"

UCLASS(meta = (PrioritizeCategories = "Buildings"))
class BUILDINGFROMSPLINE_API ABuildingsFromSplines : public AActor, public ILCGenerator
{
	GENERATED_BODY()

public:
	ABuildingsFromSplines();

	/* The folder in which to create the buildings.  This setting is unused when generating from a combination or from blueprints. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Buildings",
		meta = (DisplayPriority = "-3")
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
		meta = (DisplayPriority = "2")
	)
	TObjectPtr<UBuildingConfiguration> BuildingConfiguration;

	UPROPERTY(
		EditAnywhere, DuplicateTransient, Category = "Buildings",
		meta = (EditCondition = "false", EditConditionHides)
	)
	TArray<TWeakObjectPtr<ABuilding>> Buildings;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Buildings",
		meta = (DisplayPriority = "3")
	)
	void GenerateBuildings(FName SpawnedActorsPathOverride);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Buildings",
		meta = (DisplayPriority = "3", DisplayName = "Generate Buildings")
	)
	void GenerateBuildingsEditor() { GenerateBuildings(FName()); }

	virtual void OnGenerate(FName SpawnedActorsPathOverride, TFunction<void(bool)> OnComplete) override;

	virtual TArray<UObject*> GetGeneratedObjects() const {
		TArray<UObject*> GeneratedObjects;
		for (const TWeakObjectPtr<ABuilding>& Building : Buildings)
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

	void GenerateBuilding(USplineComponent* SplineComponent, FName SpawnedActorsPathOverride);
	TArray<USplineComponent*> FindSplineComponents();
	TArray<AActor*> FindActors();
};

#undef LOCTEXT_NAMESPACE