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
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Buildings",
		meta = (DisplayPriority = "-5")
	)
	/* If true, ExtraWallBottom will be set to be equal to the difference between the maximum and minimum Z
	   coordinates of spline points + 100 (1 meter). */
	bool bAutoComputeWallBottom = true;	

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Buildings",
		meta = (DisplayPriority = "-4")
	)
	/* If true, the height is set to the height found in the Asset User Data (*100),
	   and the number of floors is set to the height divided by the floor height.
	   This overrides the Random Num Floors and Random Building Height options when the height is available. */
	bool bAutoComputeNumFloors = true;
	
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
	
	/* Whether the generated buildings can receive decals. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Buildings",
		meta = (DisplayPriority = "0")
	)
	bool bBuildingsReceivesDecals = true;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Buildings",
		meta = (DisplayPriority = "2")
	)
	TObjectPtr<UBuildingConfiguration> BuildingConfiguration;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Buildings",
		meta = (DisplayPriority = "3")
	)
	void GenerateBuildings();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Buildings",
		meta = (DisplayPriority = "4")
	)
	void ClearBuildings();

private:
	UPROPERTY()
	TArray<TObjectPtr<ABuilding>> SpawnedBuildings;

	void GenerateBuilding(USplineComponent* SplineComponent);
	TArray<USplineComponent*> FindSplineComponents();
	TArray<AActor*> FindActors();
};

#undef LOCTEXT_NAMESPACE