
// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ParametersSelection.generated.h"

UENUM(BlueprintType)
enum class EParametersSelectionMethod: uint8
{
	Manual,
	FromEPSG4326Box,
	FromEPSG4326Coordinates,
	FromBoundingActor,
};

USTRUCT(BlueprintType)
struct FParametersSelection
{
    GENERATED_BODY()

    UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditConditionHides, DisplayPriority = "0")
	)
	/* Whether to enter coordinates manually or using a bounding actor. */
	EParametersSelectionMethod ParametersSelectionMethod = EParametersSelectionMethod::Manual;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "ParametersSelectionMethod == EParametersSelectionMethod::FromBoundingActor",
			EditConditionHides, DisplayPriority = "1"
		)
	)
	/* Select a Cube (or any other actor) that will be used to compute the WMS coordinates or XYZ tiles.
	   You can click the "Set Source Parameters From Actor" button to force refresh after you move the actor */
	TObjectPtr<AActor> ParametersBoundingActor = nullptr;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "ParametersSelectionMethod == EParametersSelectionMethod::FromEPSG4326Box",
			EditConditionHides, DisplayPriority = "10"
		)
	)
	double MinLong = 0;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "ParametersSelectionMethod == EParametersSelectionMethod::FromEPSG4326Box",
			EditConditionHides, DisplayPriority = "11"
		)
	)
	double MaxLong = 0;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "ParametersSelectionMethod == EParametersSelectionMethod::FromEPSG4326Box",
			EditConditionHides, DisplayPriority = "12"
		)
	)
	double MinLat = 0;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "ParametersSelectionMethod == EParametersSelectionMethod::FromEPSG4326Box",
			EditConditionHides, DisplayPriority = "13"
		)
	)
	double MaxLat = 0;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "ParametersSelectionMethod == EParametersSelectionMethod::FromEPSG4326Coordinates",
			EditConditionHides, DisplayPriority = "20"
		)
	)
	double Longitude = 0;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "ParametersSelectionMethod == EParametersSelectionMethod::FromEPSG4326Coordinates",
			EditConditionHides, DisplayPriority = "21"
		)
	)
	double Latitude = 0;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "ParametersSelectionMethod == EParametersSelectionMethod::FromEPSG4326Coordinates",
			EditConditionHides, DisplayPriority = "22"
		)
	)
	// Approximate real-world size in meters
	double RealWorldWidth = 200;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "ParametersSelectionMethod == EParametersSelectionMethod::FromEPSG4326Coordinates",
			EditConditionHides, DisplayPriority = "23"
		)
	)
	// Approximate real-world size in meters
	double RealWorldHeight = 200;

};
