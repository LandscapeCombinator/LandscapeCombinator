// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "Coordinates/GlobalCoordinates.h"
#include "Coordinates/LevelCoordinates.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Landscape.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FCoordinatesModule"

void UGlobalCoordinates::GetUnrealCoordinatesFromCRS(double Longitude, double Latitude, FVector2D &XY)
{
	ensureMsgf(!IsSphere(), TEXT("Flat-model coordinate API called while BodyModel == Sphere -- use ToWorld/FromWorld/Up/Alignment instead."));
	XY[0] = (Longitude - WorldOriginLong) * CmPerLongUnit;
	XY[1] = (Latitude - WorldOriginLat) * CmPerLatUnit;
}

OGRCoordinateTransformation* UGlobalCoordinates::GetCRSTransformer(FString FromCRS)
{
	return GDALInterface::MakeTransform(FromCRS, CRS);
}

bool UGlobalCoordinates::GetUnrealCoordinatesFromCRS(double Longitude, double Latitude, FString FromCRS, FVector2D &XY)
{
	ensureMsgf(!IsSphere(), TEXT("Flat-model coordinate API called while BodyModel == Sphere -- use ToWorld/FromWorld/Up/Alignment instead."));
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
	ensureMsgf(!IsSphere(), TEXT("Flat-model coordinate API called while BodyModel == Sphere -- use ToWorld/FromWorld/Up/Alignment instead."));
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
	ensureMsgf(!IsSphere(), TEXT("Flat-model coordinate API called while BodyModel == Sphere -- use ToWorld/FromWorld/Up/Alignment instead."));
	// in Global CRS
	OutCoordinates[0] = Location.X / CmPerLongUnit + WorldOriginLong;
	OutCoordinates[1] = Location.Y / CmPerLatUnit + WorldOriginLat;
}

bool UGlobalCoordinates::GetCRSCoordinatesFromUnrealLocation(FVector2D Location, FString ToCRS, FVector2D& OutCoordinates)
{
	ensureMsgf(!IsSphere(), TEXT("Flat-model coordinate API called while BodyModel == Sphere -- use ToWorld/FromWorld/Up/Alignment instead."));
	// in Global CRS
	OutCoordinates[0] = Location.X / CmPerLongUnit + WorldOriginLong;
	OutCoordinates[1] = Location.Y / CmPerLatUnit + WorldOriginLat;
	
	// convert to ToCRS
	return GDALInterface::ConvertCoordinates(&OutCoordinates[0], &OutCoordinates[1], CRS, ToCRS);
}

bool UGlobalCoordinates::GetCRSCoordinatesFromUnrealLocation(FVector2D Location, OGRCoordinateTransformation *CoordinateTransformation, FVector2D& OutCoordinates)
{
	ensureMsgf(!IsSphere(), TEXT("Flat-model coordinate API called while BodyModel == Sphere -- use ToWorld/FromWorld/Up/Alignment instead."));
	// in Global CRS
	OutCoordinates[0] = Location.X / CmPerLongUnit + WorldOriginLong;
	OutCoordinates[1] = Location.Y / CmPerLatUnit + WorldOriginLat;
	
	// convert to ToCRS
	return GDALInterface::Transform(CoordinateTransformation, &OutCoordinates[0], &OutCoordinates[1]);
}

void UGlobalCoordinates::GetCRSCoordinatesFromUnrealLocations(FVector4d Locations, FVector4d& OutCoordinates)
{
	ensureMsgf(!IsSphere(), TEXT("Flat-model coordinate API called while BodyModel == Sphere -- use ToWorld/FromWorld/Up/Alignment instead."));
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
	ensureMsgf(!IsSphere(), TEXT("Flat-model coordinate API called while BodyModel == Sphere -- use ToWorld/FromWorld/Up/Alignment instead."));
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

bool UGlobalCoordinates::GetCRSCoordinatesFromFBox(FBox Box, FString ToCRS, FVector4d& OutCoordinates, bool bShowDialog)
{
	return GetCRSCoordinatesFromOriginExtent(Box.GetCenter(), Box.GetExtent(), ToCRS, OutCoordinates, bShowDialog);
}

bool UGlobalCoordinates::GetCRSCoordinatesFromOriginExtent(FVector Origin, FVector Extent, FString ToCRS, FVector4d& OutCoordinates, bool bShowDialog)
{
	if (IsSphere())
	{
		double MinX = TNumericLimits<double>::Max();
		double MaxX = TNumericLimits<double>::Lowest();
		double MinY = TNumericLimits<double>::Max();
		double MaxY = TNumericLimits<double>::Lowest();

		const FBox Box(Origin - Extent, Origin + Extent);
		TArray<FVector2D> Samples;
		for (int32 i = 0; i < 27; i++)
		{
			FVector2D Sample;
			double AltCm;
			FromWorld(Origin + FVector(((i % 3) - 1) * Extent.X, (((i / 3) % 3) - 1) * Extent.Y, ((i / 9) - 1) * Extent.Z), Sample.X, Sample.Y, AltCm);
			Samples.Add(Sample);
		}
		bool bPoleInBox = false;
		for (const double PoleLat : { 90.0, -90.0 })
		{
			const FVector Start = ToWorld(0, PoleLat, -BodyRadiusCm);
			const FVector End = ToWorld(0, PoleLat, BodyRadiusCm);
			if (!FMath::LineBoxIntersection(Box, Start, End, End - Start)) continue;
			Samples.Add(FVector2D(-180, PoleLat));
			Samples.Add(FVector2D(180, PoleLat));
			bPoleInBox = true;
		}
		if (!bPoleInBox)
		{
			double MinLong = TNumericLimits<double>::Max();
			double MaxLong = TNumericLimits<double>::Lowest();
			for (const FVector2D& Sample : Samples)
			{
				MinLong = FMath::Min(MinLong, Sample.X);
				MaxLong = FMath::Max(MaxLong, Sample.X);
			}
			if (MaxLong - MinLong > 180.0)
			{
				const FText Message = LOCTEXT("AntimeridianCrossing", "Regions crossing the antimeridian (longitude 180) are not supported. Please split the region in two.");
				if (bShowDialog) LCReporter::ShowError(Message);
				else UE_LOG(LogTemp, Warning, TEXT("%s"), *Message.ToString());
				return false;
			}
		}

		for (FVector2D Coordinates : Samples)
		{
			if (!GDALInterface::ConvertCoordinates(&Coordinates.X, &Coordinates.Y, CRS, ToCRS)) return false;

			MinX = FMath::Min(MinX, Coordinates.X);
			MaxX = FMath::Max(MaxX, Coordinates.X);
			MinY = FMath::Min(MinY, Coordinates.Y);
			MaxY = FMath::Max(MaxY, Coordinates.Y);
		}

		OutCoordinates = FVector4d(MinX, MaxX, MinY, MaxY);
		return true;
	}

	FVector4d Locations;
	Locations[0] = Origin.X - Extent.X;
	Locations[1] = Origin.X + Extent.X;
	Locations[2] = Origin.Y + Extent.Y;
	Locations[3] = Origin.Y - Extent.Y;

	return GetCRSCoordinatesFromUnrealLocations(Locations, ToCRS, OutCoordinates);
}

bool UGlobalCoordinates::GetSphereFootprint(const FBox& Box, double& OutLatMin, double& OutLatMax, double& OutLonMin, double& OutLonMax) const
{
	if (!IsSphere() || !Box.IsValid) return false;

	const FVector Center = Box.GetCenter();
	const double Radius = Box.GetExtent().Size();
	const double Distance = FVector(Center.X, Center.Y, Center.Z + BodyRadiusCm).Size();
	if (Distance <= Radius) return false;

	const double Theta = FMath::Asin(Radius / Distance);
	const double ThetaDeg = FMath::RadiansToDegrees(Theta);

	double Lon0, Lat0, AltCm;
	FromWorld(Center, Lon0, Lat0, AltCm);

	OutLatMin = FMath::Max(-90.0, Lat0 - ThetaDeg);
	OutLatMax = FMath::Min(90.0, Lat0 + ThetaDeg);

	const double S = FMath::Sin(Theta) / FMath::Max(FMath::Cos(FMath::DegreesToRadians(Lat0)), 1e-12);
	if (S >= 1.0)
	{
		OutLonMin = -180.0;
		OutLonMax = 180.0;
	}
	else
	{
		const double HalfWidth = FMath::RadiansToDegrees(FMath::Asin(S));
		OutLonMin = Lon0 - HalfWidth;
		OutLonMax = Lon0 + HalfWidth;
	}
	return true;
}

FVector UGlobalCoordinates::ToWorld(double Long, double Lat, double AltCm) const
{
	const double P = FMath::DegreesToRadians(Lat);
	const double P0 = FMath::DegreesToRadians(WorldOriginLat);
	const double DL = FMath::DegreesToRadians(Long - WorldOriginLong);

	const double E = FMath::Cos(P) * FMath::Sin(DL);
	const double N = FMath::Cos(P0) * FMath::Sin(P) - FMath::Sin(P0) * FMath::Cos(P) * FMath::Cos(DL);
	const double U = FMath::Sin(P0) * FMath::Sin(P) + FMath::Cos(P0) * FMath::Cos(P) * FMath::Cos(DL);

	const double R = BodyRadiusCm + AltCm;
	return FVector(R * E, -R * N, R * U - BodyRadiusCm);
}

void UGlobalCoordinates::FromWorld(const FVector& World, double& OutLong, double& OutLat, double& OutAltCm) const
{
	const double P0 = FMath::DegreesToRadians(WorldOriginLat);

	const FVector V(World.X, -World.Y, World.Z + BodyRadiusCm);
	const double R = V.Size();
	OutAltCm = R - BodyRadiusCm;

	const double E = V.X / R;
	const double N = V.Y / R;
	const double U = V.Z / R;

	OutLat = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(N * FMath::Cos(P0) + U * FMath::Sin(P0), -1.0, 1.0)));
	OutLong = WorldOriginLong + FMath::RadiansToDegrees(FMath::Atan2(E, U * FMath::Cos(P0) - N * FMath::Sin(P0)));
	OutLong = FMath::Fmod(FMath::Fmod(OutLong + 180.0, 360.0) + 360.0, 360.0) - 180.0;
}

FVector UGlobalCoordinates::Up(const FVector& World) const
{
	if (!IsSphere()) return FVector::UpVector;
	return FVector(World.X, World.Y, World.Z + BodyRadiusCm).GetSafeNormal();
}

FQuat UGlobalCoordinates::Alignment(const FVector& World) const
{
	if (!IsSphere()) return FQuat::Identity;
	return FQuat::FindBetweenNormals(FVector::UpVector, Up(World));
}

FVector4d UGlobalCoordinates::GetCoordinatesFromSize(double Longitude, double Latitude, double Width, double Height, double SphereRadiusCm)
{
	const double RadiusCm = SphereRadiusCm > 0 ? SphereRadiusCm : 40000000.0 * 100.0 / (2.0 * UE_PI);
	const double ScaleFactor = FMath::Max(FMath::Cos(FMath::DegreesToRadians(Latitude)), 0.1);
	const double LongDiff = FMath::RadiansToDegrees(Width * 100 / (RadiusCm * ScaleFactor) / 2);
	const double LatDiff  = FMath::RadiansToDegrees(Height * 100 / RadiusCm / 2);
	return FVector4d(Longitude - LongDiff, Longitude + LongDiff, Latitude - LatDiff, Latitude + LatDiff);
}

static void CheckSphereOrigin(UGlobalCoordinates* G, bool bDialog)
{
	if (!G->IsSphere()) return;
	G->CRS = "EPSG:4326";

	if (FMath::Abs(G->WorldOriginLat) <= 90 && FMath::Abs(G->WorldOriginLong) <= 180) return;

	const FText Msg = FText::Format(
		LOCTEXT("SphereOrigin", "Sphere mode: WorldOriginLat/WorldOriginLong must be in degrees (got {0}, {1})."),
		FText::AsNumber(G->WorldOriginLat), FText::AsNumber(G->WorldOriginLong)
	);
	if (bDialog) LCReporter::ShowError(Msg);
	else UE_LOG(LogTemp, Error, TEXT("%s"), *Msg.ToString());
}

void UGlobalCoordinates::PostLoad()
{
	Super::PostLoad();
	CheckSphereOrigin(this, false);
}

#if WITH_EDITOR
void UGlobalCoordinates::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	CheckSphereOrigin(this, true);
	if (ALevelCoordinates* LevelCoordinates = Cast<ALevelCoordinates>(GetOwner())) LevelCoordinates->UpdateBodyParameters();
}
#endif

void UGlobalCoordinates::ComputeSphereDecalPlacement(const UGlobalCoordinates* G, const FVector2D& BoundsMin, const FVector2D& BoundsMax, FVector& OutCenter, FQuat& OutRotation, FVector& OutSize)
{
	const double MidLong = (BoundsMin.X + BoundsMax.X) / 2;
	const double MidLat = (BoundsMin.Y + BoundsMax.Y) / 2;
	OutCenter = G->ToWorld(MidLong, MidLat);
	const FVector EastDir = G->ToWorld(MidLong + 0.001, MidLat) - OutCenter;
	OutRotation = FRotationMatrix::MakeFromZX(G->Up(OutCenter), EastDir).ToQuat() * FRotator(-90, 0, 0).Quaternion();
	FVector2D HalfSize = FVector2D::ZeroVector;
	for (int32 i = 0; i <= 4; i++)
	{
		for (int32 j = 0; j <= 4; j++)
		{
			const FVector Local = OutRotation.UnrotateVector(G->ToWorld(BoundsMin.X + (BoundsMax.X - BoundsMin.X) * i / 4, BoundsMin.Y + (BoundsMax.Y - BoundsMin.Y) * j / 4) - OutCenter);
			HalfSize.X = FMath::Max(HalfSize.X, FMath::Abs(Local.Y));
			HalfSize.Y = FMath::Max(HalfSize.Y, FMath::Abs(Local.Z));
		}
	}
	OutSize = FVector(G->BodyRadiusCm, HalfSize.X, HalfSize.Y);
}

void UGlobalCoordinates::SetSphereMappingParameters(UMaterialInstanceDynamic* MID, const FVector2D& BoundsMin, const FVector2D& BoundsMax) const
{
	if (!IsValid(MID) || !IsSphere()) return;

	const double RefLong = 0.5 * (BoundsMin.X + BoundsMax.X);
	const double RefLat = 0.5 * (BoundsMin.Y + BoundsMax.Y);

	const double P = FMath::DegreesToRadians(RefLat), P0 = FMath::DegreesToRadians(WorldOriginLat);
	const double DL = FMath::DegreesToRadians(RefLong - WorldOriginLong);
	const double sP = FMath::Sin(P), cP = FMath::Cos(P), sP0 = FMath::Sin(P0), cP0 = FMath::Cos(P0);
	const double sD = FMath::Sin(DL), cD = FMath::Cos(DL);

	const FVector East(cD, -sP0 * sD, -cP0 * sD);
	const FVector North(-sP * sD, -(cP0 * cP + sP0 * sP * cD), sP0 * cP - cP0 * sP * cD);
	const FVector Up(cP * sD, -(cP0 * sP - sP0 * cP * cD), sP0 * sP + cP0 * cP * cD);
	auto Color = [](const FVector& V) { return FLinearColor((float) V.X, (float) V.Y, (float) V.Z); };

	MID->SetDoubleVectorParameterValue(FName("RefOrigin"), ToWorld(RefLong, RefLat));
	MID->SetVectorParameterValue(FName("East"), Color(East));
	MID->SetVectorParameterValue(FName("North"), Color(North));
	MID->SetVectorParameterValue(FName("Up"), Color(Up));
	MID->SetVectorParameterValue(FName("RefLonLatR"), FLinearColor((float) RefLong, (float) RefLat, (float) BodyRadiusCm));
	MID->SetVectorParameterValue(FName("RelBounds"), FLinearColor(
		(float) (BoundsMin.X - RefLong), (float) (BoundsMax.X - RefLong),
		(float) (BoundsMin.Y - RefLat), (float) (BoundsMax.Y - RefLat)));
}

#undef LOCTEXT_NAMESPACE