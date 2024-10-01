// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"

#include "ActorCoordinates.generated.h"

#define LOCTEXT_NAMESPACE "FCoordinatesModule"

UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent))
class COORDINATES_API UActorCoordinates : public UActorComponent
{
	GENERATED_BODY()

public:

	/* Coordinate System */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Coordinates",
		meta = (DisplayPriority = "1")
	)
	FString CRS = "EPSG:4326";

	/* Longitude */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Coordinates",
		meta = (DisplayPriority = "2")
	)
	double Longitude = 0;

	/* Latitude */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Coordinates",
		meta = (DisplayPriority = "3")
	)
	double Latitude = 0;

	/* Move the actor to the given coordinates. The level must have a global coordinate system. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Coordinates",
		meta = (DisplayPriority = "5")
	)
	void MoveActor();

	/* Set the longitude and the latitude to the current actor position. The level must have a global coordinate system. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Coordinates",
		meta = (DisplayPriority = "5")
	)
	void SetCoordinates();
};

#undef LOCTEXT_NAMESPACE