// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "GlobalCoordinates.h"

#include "Landscape.h"

#include "LevelCoordinates.generated.h"

#define LOCTEXT_NAMESPACE "FCoordinatesModule"

UCLASS(BlueprintType)
class COORDINATES_API ALevelCoordinates : public AActor
{
	GENERATED_BODY()

public:
	ALevelCoordinates();
	
	UPROPERTY(VisibleAnywhere, Category = "LevelCoordinates")
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates;

	static TObjectPtr<UGlobalCoordinates> GetGlobalCoordinates(UWorld *World, bool bShowDialog = true);
	
	static OGRCoordinateTransformation *GetCRSTransformer(UWorld *World, FString CRS);
	static bool GetUnrealCoordinatesFromCRS(UWorld *World, double Longitude, double Latitude, FString CRS, FVector2D &OutXY);
	static bool GetCRSCoordinatesFromUnrealLocation(UWorld* World, FVector2D Location, FVector2D& OutCoordinates);
	static bool GetCRSCoordinatesFromUnrealLocation(UWorld* World, FVector2D Location, FString CRS, FVector2D& OutCoordinates);
	static bool GetCRSCoordinatesFromUnrealLocations(UWorld* World, FVector4d Locations, FString CRS, FVector4d &OutCoordinates);
	static bool GetCRSCoordinatesFromUnrealLocations(UWorld* World, FVector4d Locations, FVector4d &OutCoordinates);
	static bool GetCRSCoordinatesFromFBox(UWorld* World, FBox Box, FString CRS, FVector4d &OutCoordinates);
	static bool GetCRSCoordinatesFromOriginExtent(UWorld* World, FVector Origin, FVector Extent, FString CRS, FVector4d &OutCoordinates);
	static bool GetLandscapeCRSBounds(ALandscape* Landscape, FString CRS, FVector4d &OutCoordinates);
	static bool GetActorCRSBounds(AActor* Actor, FString CRS, FVector4d &OutCoordinates);
	static bool GetLandscapeCRSBounds(ALandscape* Landscape, FVector4d &OutCoordinates);
	static bool GetActorCRSBounds(AActor* Actor, FVector4d &OutCoordinates);
	
	UPROPERTY(
		EditAnywhere, Category = "LevelCoordinates | WorldMap",
		meta = (DisplayPriority = "1")		
	)
	bool bUseLocalFile = false;

	UPROPERTY(
		EditAnywhere, Category = "LevelCoordinates | WorldMap",
		meta = (EditCondition = "bUseLocalFile", EditConditionHides, DisplayPriority = "2")
	)
	FString PathToGeoreferencedWorldMap = "";

	UPROPERTY(
		EditAnywhere, Category = "LevelCoordinates | WorldMap",
		meta = (EditCondition = "!bUseLocalFile", EditConditionHides, DisplayPriority = "2")
	)
	int Width = 2048;

	UPROPERTY(
		EditAnywhere, Category = "LevelCoordinates | WorldMap",
		meta = (EditCondition = "!bUseLocalFile", EditConditionHides, DisplayPriority = "3")
	)
	int Height = 2048;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "LevelCoordinates | WorldMap")
	void CreateWorldMap();

private:
	UFUNCTION(BlueprintCallable, Category = "LevelCoordinates | WorldMap")
	void CreateWorldMapFromFile(FString Path);
};

#undef LOCTEXT_NAMESPACE