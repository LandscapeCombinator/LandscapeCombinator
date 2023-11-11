// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "Coordinates/LevelCoordinates.h"

#include "Kismet/GameplayStatics.h" 
#include "Stats/Stats.h"

#define LOCTEXT_NAMESPACE "FCoordinatesModule"
ALevelCoordinates::ALevelCoordinates()
{
	PrimaryActorTick.bCanEverTick = false;

	GlobalCoordinates = CreateDefaultSubobject<UGlobalCoordinates>(TEXT("Global Coordinates"));
}

TObjectPtr<UGlobalCoordinates> ALevelCoordinates::GetGlobalCoordinates(UWorld* World, bool bShowDialog)
{
	TArray<AActor*> LevelCoordinatesCandidates0;

	UGameplayStatics::GetAllActorsOfClass(World, ALevelCoordinates::StaticClass(), LevelCoordinatesCandidates0);

	TArray<AActor*> LevelCoordinatesCandidates = LevelCoordinatesCandidates0.FilterByPredicate([](AActor* Actor) { return !Actor->IsHidden(); });

	if (LevelCoordinatesCandidates.Num() == 0)
	{
		if (bShowDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				LOCTEXT("NoLevelCoordinates", "Please add a visible (not Hidden in Game) LevelCoordinates actor to your level .")
			);
		}
		return nullptr;
	}

	if (LevelCoordinatesCandidates.Num() > 1)
	{
		if (bShowDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				LOCTEXT("MoreThanOneLevelCoordinates", "You must have only one visible (not Hidden in Game) LevelCoordinates actor in your level.")
			);
		}
		return nullptr;
	}

	return Cast<ALevelCoordinates>(LevelCoordinatesCandidates[0])->GlobalCoordinates;
}


OGRCoordinateTransformation *ALevelCoordinates::GetCRSTransformer(UWorld* World, FString CRS)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return nullptr;
	return GlobalCoordinates->GetCRSTransformer(CRS);
}

bool ALevelCoordinates::GetUnrealCoordinatesFromCRS(UWorld* World, double Longitude, double Latitude, FString CRS, FVector2D& OutXY)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return false;
	if (!GlobalCoordinates->GetUnrealCoordinatesFromCRS(Longitude, Latitude, CRS, OutXY)) return false;

	return true;
}

bool ALevelCoordinates::GetCRSCoordinatesFromUnrealLocation(UWorld* World, FVector2D Location, FString CRS, FVector2D &OutCoordinates)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return false;
	if (!GlobalCoordinates->GetCRSCoordinatesFromUnrealLocation(Location, CRS, OutCoordinates)) return false;

	return true;
}

bool ALevelCoordinates::GetCRSCoordinatesFromUnrealLocations(UWorld* World, FVector4d Locations, FString CRS, FVector4d &OutCoordinates)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return false;
	if (!GlobalCoordinates->GetCRSCoordinatesFromUnrealLocations(Locations, CRS, OutCoordinates)) return false;

	return true;
}

bool ALevelCoordinates::GetCRSCoordinatesFromUnrealLocations(UWorld* World, FVector4d Locations, FVector4d& OutCoordinates)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return false;

	GlobalCoordinates->GetCRSCoordinatesFromUnrealLocations(Locations, OutCoordinates);
	return true;
}

bool ALevelCoordinates::GetCRSCoordinatesFromFBox(UWorld* World, FBox Box, FString ToCRS, FVector4d& OutCoordinates)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return false;

	return GlobalCoordinates->GetCRSCoordinatesFromFBox(Box, ToCRS, OutCoordinates);
}

bool ALevelCoordinates::GetCRSCoordinatesFromOriginExtent(UWorld* World, FVector Origin, FVector Extent, FString ToCRS, FVector4d& OutCoordinates)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return false;
	
	return GlobalCoordinates->GetCRSCoordinatesFromOriginExtent(Origin, Extent, ToCRS, OutCoordinates);
}

bool ALevelCoordinates::GetLandscapeBounds(UWorld* World, ALandscape* Landscape, FString CRS, FVector4d &OutCoordinates)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return false;
	return GlobalCoordinates->GetLandscapeBounds(Landscape, CRS, OutCoordinates);
}

#undef LOCTEXT_NAMESPACE
