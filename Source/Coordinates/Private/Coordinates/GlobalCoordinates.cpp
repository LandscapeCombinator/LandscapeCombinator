// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "Coordinates/GlobalCoordinates.h"

#define LOCTEXT_NAMESPACE "FCoordinatesModule"

void UGlobalCoordinates::GetUnrealCoordinatesFromEPSG(double Longitude, double Latitude, FVector2D &XY)
{
	XY[0] = (Longitude - WorldOriginLong) * CmPerLongUnit;
	XY[1] = (Latitude - WorldOriginLat) * CmPerLatUnit;
}

OGRCoordinateTransformation* UGlobalCoordinates::GetEPSGTransformer(int FromEPSG)
{
	OGRSpatialReference InRs, OutRs;
	if (
		!GDALInterface::SetCRSFromEPSG(InRs, FromEPSG) ||
		!GDALInterface::SetCRSFromEPSG(OutRs, EPSG)
	)
	{ 
		return nullptr;
	}
	
	return OGRCreateCoordinateTransformation(&InRs, &OutRs);
}

bool UGlobalCoordinates::GetUnrealCoordinatesFromEPSG(double Longitude, double Latitude, int FromEPSG, FVector2D &XY)
{
	double ConvertedLongitude = Longitude;
	double ConvertedLatitude = Latitude;
	
	OGRSpatialReference InRs, OutRs;
	if (
		!GDALInterface::SetCRSFromEPSG(InRs, FromEPSG) ||
		!GDALInterface::SetCRSFromEPSG(OutRs, EPSG) ||
		!OGRCreateCoordinateTransformation(&InRs, &OutRs)->Transform(1, &ConvertedLongitude, &ConvertedLatitude)
	)
	{	
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("GetUnrealCoordinatesFromEPSG", "Internal error while transforming coordinates.")
		);
		return false;
	}

	XY[0] = (ConvertedLongitude - WorldOriginLong) * CmPerLongUnit;
	XY[1] = (ConvertedLatitude - WorldOriginLat) * CmPerLatUnit;

	return true;
}


void UGlobalCoordinates::GetEPSGCoordinatesFromUnrealLocation(FVector2D Location, FVector2D& OutCoordinates)
{
	// in Global EPSG
	OutCoordinates[0] = Location.X / CmPerLongUnit + WorldOriginLong;
	OutCoordinates[1] = Location.Y / CmPerLatUnit + WorldOriginLat;
}

bool UGlobalCoordinates::GetEPSGCoordinatesFromUnrealLocation(FVector2D Location, int ToEPSG, FVector2D& OutCoordinates)
{
	// in Global EPSG
	OutCoordinates[0] = Location.X / CmPerLongUnit + WorldOriginLong;
	OutCoordinates[1] = Location.Y / CmPerLatUnit + WorldOriginLat;
	
	// convert to ToEPSG
	OGRSpatialReference InRs, OutRs;
	if (
		!GDALInterface::SetCRSFromEPSG(InRs, EPSG) ||
		!GDALInterface::SetCRSFromEPSG(OutRs, ToEPSG) ||
		!OGRCreateCoordinateTransformation(&InRs, &OutRs)->Transform(1, &OutCoordinates[0], &OutCoordinates[1])
	)
	{	
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("GetEPSGCoordinatesFromUnrealLocation", "Internal error while transforming coordinates.")
		);
		return false;
	}

	return true;
}

bool UGlobalCoordinates::GetEPSGCoordinatesFromUnrealLocations(FVector4d Locations, int ToEPSG, FVector4d& OutCoordinates)
{
	// in Global EPSG
	double xs[2] = { Locations[0] / CmPerLongUnit + WorldOriginLong,  Locations[1] / CmPerLongUnit + WorldOriginLong  };
	double ys[2] = { Locations[3] / CmPerLatUnit + WorldOriginLat, Locations[2] / CmPerLatUnit + WorldOriginLat };
	
	// convert to ToEPSG
	OGRSpatialReference InRs, OutRs;
	if (
		!GDALInterface::SetCRSFromEPSG(InRs, EPSG) ||
		!GDALInterface::SetCRSFromEPSG(OutRs, ToEPSG) ||
		!OGRCreateCoordinateTransformation(&InRs, &OutRs)->Transform(2, xs, ys)
	)
	{	
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("GetEPSGCoordinatesFromUnrealLocation", "Internal error while transforming coordinates.")
		);
		return false;
	}
	
	OutCoordinates[0] = xs[0];
	OutCoordinates[1] = xs[1];
	OutCoordinates[2] = ys[1];
	OutCoordinates[3] = ys[0];

	return true;
}

bool UGlobalCoordinates::GetEPSGCoordinatesFromFBox(FBox Box, int ToEPSG, FVector4d& OutCoordinates)
{
	return GetEPSGCoordinatesFromOriginExtent(Box.GetCenter(), Box.GetExtent(), ToEPSG, OutCoordinates);
}

bool UGlobalCoordinates::GetEPSGCoordinatesFromOriginExtent(FVector Origin, FVector Extent, int ToEPSG, FVector4d& OutCoordinates)
{
	FVector4d Locations;
	Locations[0] = Origin.X - Extent.X;
	Locations[1] = Origin.X + Extent.X;
	Locations[2] = Origin.Y + Extent.Y;
	Locations[3] = Origin.Y - Extent.Y;

	return GetEPSGCoordinatesFromUnrealLocations(Locations, ToEPSG, OutCoordinates);
}


#undef LOCTEXT_NAMESPACE