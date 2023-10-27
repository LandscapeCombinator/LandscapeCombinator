// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "GlobalCoordinates.h"

#include "LevelCoordinates.generated.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

UCLASS(BlueprintType)
class COORDINATES_API ALevelCoordinates : public AActor
{
	GENERATED_BODY()

public:
	ALevelCoordinates();
	
	UPROPERTY(VisibleAnywhere, Category = "Building")
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates;

	static TObjectPtr<UGlobalCoordinates> GetGlobalCoordinates(UWorld *World, bool bShowDialog = true);
	
	static OGRCoordinateTransformation *GetEPSGTransformer(UWorld *World, int EPSG);
	static bool GetUnrealCoordinatesFromEPSG(UWorld *World, double Longitude, double Latitude, int EPSG, FVector2D &OutXY);
	static bool GetEPSGCoordinatesFromUnrealLocation(UWorld* World, FVector2D Location, int EPSG, FVector2D &OutCoordinates);
	static bool GetEPSGCoordinatesFromUnrealLocations(UWorld* World, FVector4d Locations, int EPSG, FVector4d &OutCoordinates);
	static bool GetEPSGCoordinatesFromFBox(UWorld* World, FBox Box, int EPSG, FVector4d &OutCoordinates);
	static bool GetEPSGCoordinatesFromOriginExtent(UWorld* World, FVector Origin, FVector Extent, int EPSG, FVector4d &OutCoordinates);
};

#undef LOCTEXT_NAMESPACE