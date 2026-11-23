// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "Coordinates/ActorCoordinates.h"
#include "Coordinates/LevelCoordinates.h"
#include "Coordinates/LogCoordinates.h"
#include "GDALInterface/GDALInterface.h"

#define LOCTEXT_NAMESPACE "FCoordinatesModule"

void UActorCoordinates::MoveActor()
{
	AActor *Owner = GetOwner();
	if (!Owner) return;

	UGlobalCoordinates* GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinatesFor(Owner, false);
	if (UGlobalCoordinates::IsSphere(GlobalCoordinates))
	{
		double Long = Longitude;
		double Lat = Latitude;
		double OldLong, OldLat, AltCm;
		if (!GDALInterface::ConvertCoordinates(&Long, &Lat, CRS, "EPSG:4326")) return;
		const FVector OldLocation = Owner->GetActorLocation();
		GlobalCoordinates->FromWorld(OldLocation, OldLong, OldLat, AltCm);
		const FVector NewLocation = GlobalCoordinates->ToWorld(Long, Lat, AltCm);
		const FQuat DeltaAlignment = GlobalCoordinates->Alignment(NewLocation) * GlobalCoordinates->Alignment(OldLocation).Inverse();
		Owner->SetActorLocationAndRotation(NewLocation, DeltaAlignment * Owner->GetActorQuat());
		return;
	}

	FVector2D XY;
	if (!ALevelCoordinates::GetUnrealCoordinatesFromCRS(Owner->GetWorld(), Longitude, Latitude, CRS, XY)) return;

	double X = XY[0];
	double Y = XY[1];
	double Z = Owner->GetActorLocation().Z;

	Owner->SetActorLocation(FVector(X, Y, Z));
}

void UActorCoordinates::SetCoordinates()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	UGlobalCoordinates* GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinatesFor(Owner, false);
	if (UGlobalCoordinates::IsSphere(GlobalCoordinates))
	{
		double Long, Lat, AltCm;
		GlobalCoordinates->FromWorld(Owner->GetActorLocation(), Long, Lat, AltCm);
		if (!GDALInterface::ConvertCoordinates(&Long, &Lat, "EPSG:4326", CRS)) return;
		Longitude = Long;
		Latitude = Lat;
		return;
	}

	FVector2D XY, Coordinates;
	XY.X = Owner->GetActorLocation().X;
	XY.Y = Owner->GetActorLocation().Y;
	if (!ALevelCoordinates::GetCRSCoordinatesFromUnrealLocation(Owner->GetWorld(), XY, CRS, Coordinates)) return;

	Longitude = Coordinates[0];
	Latitude = Coordinates[1];
}

#undef LOCTEXT_NAMESPACE