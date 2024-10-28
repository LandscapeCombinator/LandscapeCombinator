// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "GDALInterface/GDALInterface.h"

#include "Landscape.h"

#include "GlobalCoordinates.generated.h"

UCLASS(BlueprintType)
class COORDINATES_API UGlobalCoordinates : public UActorComponent
{
	GENERATED_BODY()

public:

	/* Coordinate System */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "GlobalCoordinates",
		meta = (DisplayPriority = "1")
	)
	FString CRS = "EPSG:4326";

	/* Number of Unreal Engine centimeters per longitude coordinate system unit */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "GlobalCoordinates",
		meta = (DisplayPriority = "2")
	)
	double CmPerLongUnit = 11111111;
	
	/* Number of Unreal Engine centimeters per latitude coordinate system unit */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "GlobalCoordinates",
		meta = (DisplayPriority = "3")
	)
	double CmPerLatUnit = -11111111;
	
	/* Longitude of the World Origin (in the given Coordinate System) */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "GlobalCoordinates",
		meta = (DisplayPriority = "5")
	)
	double WorldOriginLong = 0;
	
	/* Latitude of the World Origin (in the given Coordinate System) */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "GlobalCoordinates",
		meta = (DisplayPriority = "6")
	)
	double WorldOriginLat = 0;
	


	/* This function is slow. If you need to call it many times, make a single OGRCoordinateTransformation
	 * with `GetCRSTransformer`and use this instead. */
	bool GetUnrealCoordinatesFromCRS(double Longitude, double Latitude, FString FromCRS, FVector2D &XY);
	
	OGRCoordinateTransformation *GetCRSTransformer(FString FromCRS);
	void GetCRSCoordinatesFromUnrealLocation(FVector2D Location, FVector2D& OutCoordinates);
	void GetUnrealCoordinatesFromCRS(double Longitude, double Latitude, FVector2D &XY);
	bool GetUnrealCoordinatesFromCRS(double Longitude, double Latitude, OGRCoordinateTransformation *CoordinateTransformation, FVector2D &XY);
	bool GetCRSCoordinatesFromUnrealLocation(FVector2D Location, FString ToCRS, FVector2D& OutCoordinates);
	bool GetCRSCoordinatesFromUnrealLocation(FVector2D Location, OGRCoordinateTransformation *CoordinateTransformation, FVector2D& OutCoordinates);
	void GetCRSCoordinatesFromUnrealLocations(FVector4d Locations, FVector4d& OutCoordinates);
	bool GetCRSCoordinatesFromUnrealLocations(FVector4d Locations, FString ToCRS, FVector4d& OutCoordinates);
	bool GetCRSCoordinatesFromFBox(FBox Box, FString ToCRS, FVector4d& OutCoordinates);
	bool GetCRSCoordinatesFromOriginExtent(FVector Origin, FVector Extent, FString ToCRS, FVector4d& OutCoordinates);
	bool GetLandscapeCRSBounds(ALandscape *Landscape, FString ToCRS, FVector4d &OutCoordinates);
	bool GetLandscapeCRSBounds(ALandscape *Landscape, FVector4d &OutCoordinates);
	bool GetActorCRSBounds(AActor *Actor, FString ToCRS, FVector4d &OutCoordinates);
	bool GetActorCRSBounds(AActor *Actor, FVector4d &OutCoordinates);
};
