// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "Coordinates/LevelCoordinates.h"
#include "Coordinates/DecalCoordinates.h"
#include "FileDownloader/Download.h"
#include "LCReporter/LCReporter.h"
#include "ConcurrencyHelpers/Concurrency.h"

#include "Engine/DecalActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h" 
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h" 
#include "Stats/Stats.h"
#include "Misc/Paths.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FCoordinatesModule"

ALevelCoordinates::ALevelCoordinates()
{
	PrimaryActorTick.bCanEverTick = false;

	GlobalCoordinates = CreateDefaultSubobject<UGlobalCoordinates>(TEXT("Global Coordinates"));
}

TObjectPtr<UGlobalCoordinates> ALevelCoordinates::GetGlobalCoordinates(UWorld* World, bool bShowDialog)
{
	TArray<AActor*> LevelCoordinatesCandidates0;

	Concurrency::RunOnGameThreadAndWait([World, &LevelCoordinatesCandidates0]() {
		UGameplayStatics::GetAllActorsOfClass(World, ALevelCoordinates::StaticClass(), LevelCoordinatesCandidates0);
	});

	TArray<AActor*> LevelCoordinatesCandidates = LevelCoordinatesCandidates0.FilterByPredicate([](AActor* Actor) { return IsValid(Actor) &&!Actor->IsHidden(); });

	if (LevelCoordinatesCandidates.Num() == 0)
	{
		if (bShowDialog)
		{
			ULCReporter::ShowError(
				LOCTEXT("NoLevelCoordinates", "Please add a visible (not Hidden in Game) LevelCoordinates actor to your level .")
			);
		}
		return nullptr;
	}

	if (LevelCoordinatesCandidates.Num() > 1)
	{
		if (bShowDialog)
		{
			ULCReporter::ShowError(
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

bool ALevelCoordinates::GetCRSCoordinatesFromUnrealLocation(UWorld* World, FVector2D Location, FVector2D& OutCoordinates)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return false;
	GlobalCoordinates->GetCRSCoordinatesFromUnrealLocation(Location, OutCoordinates);
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

bool ALevelCoordinates::GetLandscapeCRSBounds(ALandscape* Landscape, FString CRS, FVector4d &OutCoordinates)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(Landscape->GetWorld());
	if (!GlobalCoordinates) return false;
	return GlobalCoordinates->GetLandscapeCRSBounds(Landscape, CRS, OutCoordinates);
}

bool ALevelCoordinates::GetLandscapeCRSBounds(ALandscape* Landscape, FVector4d& OutCoordinates)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(Landscape->GetWorld());
	if (!GlobalCoordinates) return false;
	return GlobalCoordinates->GetLandscapeCRSBounds(Landscape, OutCoordinates);
}

bool ALevelCoordinates::GetActorCRSBounds(AActor* Actor, FString CRS, FVector4d& OutCoordinates)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(Actor->GetWorld());
	if (!GlobalCoordinates) return false;
	return GlobalCoordinates->GetActorCRSBounds(Actor, CRS, OutCoordinates);
}

bool ALevelCoordinates::GetActorCRSBounds(AActor* Actor, FVector4d& OutCoordinates)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(Actor->GetWorld());
	if (!GlobalCoordinates) return false;
	return GlobalCoordinates->GetActorCRSBounds(Actor, OutCoordinates);
}

void ALevelCoordinates::CreateWorldMap()
{
	if (!GlobalCoordinates)
	{
		ULCReporter::ShowError(
			LOCTEXT("ALevelCoordinates::CreateWorldMap::1", "This LevelCoordinates Actor doesn't have GlobalCoordinates.")
		);
	}
	if (bUseLocalFile)
	{
		CreateWorldMapFromFile(PathToGeoreferencedWorldMap);
	}
	else
	{
		double MinLong = -179.999989;
		double MaxLong = 179.999988;
		double MinLat = -89.000000;
		double MaxLat = 89.000000;

		FString URL = FString::Format(
			TEXT("https://basemap.nationalmap.gov:443/arcgis/services/USGSImageryOnly/MapServer/WmsServer?LAYERS=0&FORMAT=image/tiff&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetMap&CRS=CRS:84&STYLES=&BBOX={0},{1},{2},{3}&WIDTH={4}&HEIGHT={5}"),
			{ 
				MinLong, MinLat, MaxLong, MaxLat,
				Width, Height
			}
		);
	
		FString Intermediate = FPaths::ConvertRelativePathToFull(FPaths::EngineIntermediateDir());
		FString CoordinatesDir = FPaths::Combine(Intermediate, "Coordinates");
		FString DownloadedWorldMapPath = FPaths::Combine(CoordinatesDir, "DownloadedWorldMap.tif");
		FString TempWorldMapPath = FPaths::Combine(CoordinatesDir, "TempWorldMap.tif");
		FString WorldMapPath = FPaths::Combine(CoordinatesDir, "WorldMap.tif");
		Download::FromURL(URL, DownloadedWorldMapPath, true, [this, MinLong, MaxLong, MinLat, MaxLat, DownloadedWorldMapPath, TempWorldMapPath, WorldMapPath](bool bSuccess) {
			if (bSuccess)
			{
				bool bSuccessWriteCRS = GDALInterface::AddGeoreference(DownloadedWorldMapPath, WorldMapPath, "EPSG:4326", MinLong, MaxLong, MinLat, MaxLat);
				if (bSuccessWriteCRS)
				{
					CreateWorldMapFromFile(WorldMapPath);
				}
				else
				{
					ULCReporter::ShowError(
						LOCTEXT("ALevelCoordinates::CreateWorldMap", "Could not write coordinate system to world map.")
					);
				}
			}
			else
			{
				ULCReporter::ShowError(
					LOCTEXT("ALevelCoordinates::CreateWorldMap", "Could not download world map from USGS Imagery.")
				);
			}
		});
	}
}

void ALevelCoordinates::CreateWorldMapFromFile(FString Path)
{
	FVector4d Coordinates;
	UDecalCoordinates::CreateDecal(this->GetWorld(), Path, Coordinates);
	
	AStaticMeshActor* PlaneActor = GetWorld()->SpawnActor<AStaticMeshActor>();
#if WITH_EDITOR
	PlaneActor->SetActorLabel("WorldMapPlane");
#endif
	PlaneActor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane")));
	PlaneActor->SetActorLocation(FVector((Coordinates[1] + Coordinates[0]) / 2, (Coordinates[2] + Coordinates[3]) / 2, 0));

	// Z-scale = 1 has weird lighting issues when X and Y are large, so we use Z-scale = X-scale
	PlaneActor->SetActorScale3D(FVector((Coordinates[1] - Coordinates[0]) / 100, (Coordinates[2] - Coordinates[3]) / 100, (Coordinates[1] - Coordinates[0]) / 100));
}

#undef LOCTEXT_NAMESPACE
