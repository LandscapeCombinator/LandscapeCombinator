// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "GDALInterface/GDALInterface.h"

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
	int EPSG = 4326;

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
	double WorldOriginLong;
	
	/* Latitude of the World Origin (in the given Coordinate System) */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "GlobalCoordinates",
		meta = (DisplayPriority = "6")
	)
	double WorldOriginLat;
	


	/* This function is slow. If you need to call it many times, make a single OGRCoordinateTransformation
	 * with `GetEPSGTransformer`and use this instead. */
	bool GetUnrealCoordinatesFromEPSG(double Longitude, double Latitude, int FromEPSG, FVector2D &XY);
	
	OGRCoordinateTransformation *GetEPSGTransformer(int FromEPSG);
	void GetEPSGCoordinatesFromUnrealLocation(FVector2D Location, FVector2D& OutCoordinates);
	void GetUnrealCoordinatesFromEPSG(double Longitude, double Latitude, FVector2D &XY);
	bool GetEPSGCoordinatesFromUnrealLocation(FVector2D Location, int ToEPSG, FVector2D& OutCoordinates);
	bool GetEPSGCoordinatesFromUnrealLocations(FVector4d Locations, int ToEPSG, FVector4d& OutCoordinates);
	bool GetEPSGCoordinatesFromFBox(FBox Box, int ToEPSG, FVector4d& OutCoordinates);
	bool GetEPSGCoordinatesFromOriginExtent(FVector Origin, FVector Extent, int ToEPSG, FVector4d& OutCoordinates);
};
