// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "Coordinates/GlobalCoordinates.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Landscape.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FCoordinatesModule"

void UGlobalCoordinates::GetUnrealCoordinatesFromCRS(double Longitude, double Latitude, FVector2D &XY)
{
	XY[0] = (Longitude - WorldOriginLong) * CmPerLongUnit;
	XY[1] = (Latitude - WorldOriginLat) * CmPerLatUnit;
}

OGRCoordinateTransformation* UGlobalCoordinates::GetCRSTransformer(FString FromCRS)
{
	return GDALInterface::MakeTransform(FromCRS, CRS);
}

bool UGlobalCoordinates::GetUnrealCoordinatesFromCRS(double Longitude, double Latitude, FString FromCRS, FVector2D &XY)
{
	double ConvertedLongitude = Longitude;
	double ConvertedLatitude = Latitude;

	if (!GDALInterface::ConvertCoordinates(&ConvertedLongitude, &ConvertedLatitude, FromCRS, CRS))
	{
		return false;
	}

	XY[0] = (ConvertedLongitude - WorldOriginLong) * CmPerLongUnit;
	XY[1] = (ConvertedLatitude - WorldOriginLat) * CmPerLatUnit;

	return true;
}

bool UGlobalCoordinates::GetUnrealCoordinatesFromCRS(double Longitude, double Latitude, OGRCoordinateTransformation *CoordinateTransformation, FVector2D &XY)
{
	double ConvertedLongitude = Longitude;
	double ConvertedLatitude = Latitude;

	if (!GDALInterface::Transform(CoordinateTransformation, &ConvertedLongitude, &ConvertedLatitude))
	{
		return false;
	}

	XY[0] = (ConvertedLongitude - WorldOriginLong) * CmPerLongUnit;
	XY[1] = (ConvertedLatitude - WorldOriginLat) * CmPerLatUnit;

	return true;
}


void UGlobalCoordinates::GetCRSCoordinatesFromUnrealLocation(FVector2D Location, FVector2D& OutCoordinates)
{
	// in Global CRS
	OutCoordinates[0] = Location.X / CmPerLongUnit + WorldOriginLong;
	OutCoordinates[1] = Location.Y / CmPerLatUnit + WorldOriginLat;
}

bool UGlobalCoordinates::GetCRSCoordinatesFromUnrealLocation(FVector2D Location, FString ToCRS, FVector2D& OutCoordinates)
{
	// in Global CRS
	OutCoordinates[0] = Location.X / CmPerLongUnit + WorldOriginLong;
	OutCoordinates[1] = Location.Y / CmPerLatUnit + WorldOriginLat;
	
	// convert to ToCRS
	return GDALInterface::ConvertCoordinates(&OutCoordinates[0], &OutCoordinates[1], CRS, ToCRS);
}

bool UGlobalCoordinates::GetCRSCoordinatesFromUnrealLocation(FVector2D Location, OGRCoordinateTransformation *CoordinateTransformation, FVector2D& OutCoordinates)
{
	// in Global CRS
	OutCoordinates[0] = Location.X / CmPerLongUnit + WorldOriginLong;
	OutCoordinates[1] = Location.Y / CmPerLatUnit + WorldOriginLat;
	
	// convert to ToCRS
	return GDALInterface::Transform(CoordinateTransformation, &OutCoordinates[0], &OutCoordinates[1]);
}

void UGlobalCoordinates::GetCRSCoordinatesFromUnrealLocations(FVector4d Locations, FVector4d& OutCoordinates)
{
	// in Global CRS
	double xs[2] = { Locations[0] / CmPerLongUnit + WorldOriginLong,  Locations[1] / CmPerLongUnit + WorldOriginLong  };
	double ys[2] = { Locations[3] / CmPerLatUnit + WorldOriginLat, Locations[2] / CmPerLatUnit + WorldOriginLat };
	
	OutCoordinates[0] = xs[0];
	OutCoordinates[1] = xs[1];
	OutCoordinates[2] = ys[1];
	OutCoordinates[3] = ys[0];
}

bool UGlobalCoordinates::GetCRSCoordinatesFromUnrealLocations(FVector4d Locations, FString ToCRS, FVector4d& OutCoordinates)
{
	// in Global CRS
	double xs[2] = { Locations[0] / CmPerLongUnit + WorldOriginLong,  Locations[1] / CmPerLongUnit + WorldOriginLong  };
	double ys[2] = { Locations[3] / CmPerLatUnit + WorldOriginLat, Locations[2] / CmPerLatUnit + WorldOriginLat };

	if (!GDALInterface::ConvertCoordinates2(xs, ys, CRS, ToCRS)) return false;

	OutCoordinates[0] = xs[0];
	OutCoordinates[1] = xs[1];
	OutCoordinates[2] = ys[1];
	OutCoordinates[3] = ys[0];

	return true;
}

bool UGlobalCoordinates::GetCRSCoordinatesFromFBox(FBox Box, FString ToCRS, FVector4d& OutCoordinates)
{
	return GetCRSCoordinatesFromOriginExtent(Box.GetCenter(), Box.GetExtent(), ToCRS, OutCoordinates);
}

bool UGlobalCoordinates::GetCRSCoordinatesFromOriginExtent(FVector Origin, FVector Extent, FString ToCRS, FVector4d& OutCoordinates)
{
	FVector4d Locations;
	Locations[0] = Origin.X - Extent.X;
	Locations[1] = Origin.X + Extent.X;
	Locations[2] = Origin.Y + Extent.Y;
	Locations[3] = Origin.Y - Extent.Y;

	return GetCRSCoordinatesFromUnrealLocations(Locations, ToCRS, OutCoordinates);
}

FVector4d UGlobalCoordinates::GetCoordinatesFromSize(double Longitude, double Latitude, double Width, double Height)
{
	double LatitudeRadians = FMath::DegreesToRadians(Latitude);
	double ScaleFactor = FMath::Max(FMath::Cos(LatitudeRadians), 0.1);
	double LongDiff = Width / 40000000 * 360 / 2;
	double LatDiff  = Height * ScaleFactor / 40000000 * 360 / 2;
	return FVector4d(Longitude - LongDiff, Longitude + LongDiff, Latitude - LatDiff, Latitude + LatDiff);
}

#undef LOCTEXT_NAMESPACE