// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "Coordinates/ActorCoordinates.h"
#include "Coordinates/LevelCoordinates.h"
#include "Coordinates/LogCoordinates.h"
#include "GDALInterface/GDALInterface.h"

#define LOCTEXT_NAMESPACE "FCoordinatesModule"

void UActorCoordinates::MoveActor()
{
	AActor *Owner = GetOwner();
	if (!Owner) return;

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

	FVector2D XY, Coordinates;
	XY.X = Owner->GetActorLocation().X;
	XY.Y = Owner->GetActorLocation().Y;
	if (!ALevelCoordinates::GetCRSCoordinatesFromUnrealLocation(Owner->GetWorld(), XY, CRS, Coordinates)) return;

	Longitude = Coordinates[0];
	Latitude = Coordinates[1];
}

#undef LOCTEXT_NAMESPACE