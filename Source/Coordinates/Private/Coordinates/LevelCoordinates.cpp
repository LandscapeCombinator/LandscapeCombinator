// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "Coordinates/LevelCoordinates.h"

#include "Kismet/GameplayStatics.h" 
#include "Stats/Stats.h"

#define LOCTEXT_NAMESPACE "FCoordinatesModule"

DECLARE_STATS_GROUP(TEXT("Coordinates"), STATGROUP_Coordinates, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("GetUnrealCoordinatesFromEPSG"), STAT_GetUnrealCoordinatesFromEPSG, STATGROUP_Coordinates);

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


OGRCoordinateTransformation *ALevelCoordinates::GetEPSGTransformer(UWorld* World, int EPSG)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return nullptr;
	return GlobalCoordinates->GetEPSGTransformer(EPSG);
}

bool ALevelCoordinates::GetUnrealCoordinatesFromEPSG(UWorld* World, double Longitude, double Latitude, int EPSG, FVector2D& OutXY)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return false;
	if (!GlobalCoordinates->GetUnrealCoordinatesFromEPSG(Longitude, Latitude, EPSG, OutXY)) return false;

	return true;
}

bool ALevelCoordinates::GetEPSGCoordinatesFromUnrealLocation(UWorld* World, FVector2D Location, int EPSG, FVector2D &OutCoordinates)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return false;
	if (!GlobalCoordinates->GetEPSGCoordinatesFromUnrealLocation(Location, EPSG, OutCoordinates)) return false;

	return true;
}

bool ALevelCoordinates::GetEPSGCoordinatesFromUnrealLocations(UWorld* World, FVector4d Locations, int EPSG, FVector4d &OutCoordinates)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return false;
	if (!GlobalCoordinates->GetEPSGCoordinatesFromUnrealLocations(Locations, EPSG, OutCoordinates)) return false;

	return true;
}

bool ALevelCoordinates::GetEPSGCoordinatesFromFBox(UWorld* World, FBox Box, int ToEPSG, FVector4d& OutCoordinates)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return false;
	if (!GlobalCoordinates->GetEPSGCoordinatesFromFBox(Box, ToEPSG, OutCoordinates)) return false;

	return true;
}

bool ALevelCoordinates::GetEPSGCoordinatesFromOriginExtent(UWorld* World, FVector Origin, FVector Extent, int ToEPSG, FVector4d& OutCoordinates)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return false;
	if (!GlobalCoordinates->GetEPSGCoordinatesFromOriginExtent(Origin, Extent, ToEPSG, OutCoordinates)) return false;

	return true;
}

#undef LOCTEXT_NAMESPACE
