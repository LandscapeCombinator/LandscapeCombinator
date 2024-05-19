// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "BuildingFromSpline/LogBuildingFromSpline.h"
#include "BuildingFromSpline/Building.h"

#include "Components/SplineComponent.h" 

#include "BuildingsFromSplines.generated.h"

#define LOCTEXT_NAMESPACE "FBuildingFromSplineModule"

UCLASS(meta = (PrioritizeCategories = "Buildings"))
class BUILDINGFROMSPLINE_API ABuildingsFromSplines : public AActor
{
	GENERATED_BODY()

public:
	ABuildingsFromSplines();
	
	/* The tag of the actors containing spline components to search for. If None, all actors will be used. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Buildings",
		meta = (DisplayPriority = "-3")
	)
	FName ActorsTag;
	
	/* The tag of the Spline Components to use to generate buildings. If None, all spline components will be used. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Buildings",
		meta = (DisplayPriority = "-2")
	)
	FName SplinesTag;
	
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
		EditAnywhere, BlueprintReadWrite, DuplicateTransient, Category = "Buildings",
		meta = (EditCondition = "false", EditConditionHides)
	)
	TArray<TObjectPtr<ABuilding>> SpawnedBuildings;

#if WITH_EDITOR

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Buildings",
		meta = (DisplayPriority = "3")
	)
	void GenerateBuildings();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Buildings",
		meta = (DisplayPriority = "4")
	)
	void ClearBuildings();

#endif

protected:

#if WITH_EDITOR

	void GenerateBuilding(USplineComponent* SplineComponent);
	TArray<USplineComponent*> FindSplineComponents();
	TArray<AActor*> FindActors();

#endif
};

#undef LOCTEXT_NAMESPACE