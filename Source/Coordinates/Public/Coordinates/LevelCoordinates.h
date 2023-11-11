// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "GlobalCoordinates.h"

#include "Landscape.h"

#include "LevelCoordinates.generated.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

UCLASS(BlueprintType)
class COORDINATES_API ALevelCoordinates : public AActor
{
	GENERATED_BODY()

public:
	ALevelCoordinates();
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates;

	static TObjectPtr<UGlobalCoordinates> GetGlobalCoordinates(UWorld *World, bool bShowDialog = true);
	
	static OGRCoordinateTransformation *GetCRSTransformer(UWorld *World, FString CRS);
	static bool GetUnrealCoordinatesFromCRS(UWorld *World, double Longitude, double Latitude, FString CRS, FVector2D &OutXY);
	static bool GetCRSCoordinatesFromUnrealLocation(UWorld* World, FVector2D Location, FString CRS, FVector2D &OutCoordinates);
	static bool GetCRSCoordinatesFromUnrealLocations(UWorld* World, FVector4d Locations, FString CRS, FVector4d &OutCoordinates);
	static bool GetCRSCoordinatesFromUnrealLocations(UWorld* World, FVector4d Locations, FVector4d &OutCoordinates);
	static bool GetCRSCoordinatesFromFBox(UWorld* World, FBox Box, FString CRS, FVector4d &OutCoordinates);
	static bool GetCRSCoordinatesFromOriginExtent(UWorld* World, FVector Origin, FVector Extent, FString CRS, FVector4d &OutCoordinates);
	static bool GetLandscapeBounds(UWorld* World, ALandscape* Landscape, FString CRS, FVector4d &OutCoordinates);
};

#undef LOCTEXT_NAMESPACE