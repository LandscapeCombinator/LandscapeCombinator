// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "GlobalCoordinates.h"

#include "Landscape.h"

#include "Engine/World.h"
#include "Containers/Ticker.h"
#include "Materials/Material.h"
#include "LevelCoordinates.generated.h"

#define LOCTEXT_NAMESPACE "FCoordinatesModule"

class FBodyViewportExtension;

UCLASS(BlueprintType)
class COORDINATES_API ALevelCoordinates : public AActor
{
	GENERATED_BODY()

public:
	ALevelCoordinates();
	
	UPROPERTY(VisibleAnywhere, Category = "LevelCoordinates")
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates;

	static TObjectPtr<UGlobalCoordinates> GetGlobalCoordinates(TWeakObjectPtr<UWorld> World, bool bShowDialog = true);
	
	static OGRCoordinateTransformation *GetCRSTransformer(UWorld *World, FString CRS);
	static bool GetUnrealCoordinatesFromCRS(UWorld *World, double Longitude, double Latitude, FString CRS, FVector2D &OutXY);
	static bool GetCRSCoordinatesFromUnrealLocation(UWorld* World, FVector2D Location, FVector2D& OutCoordinates);
	static bool GetCRSCoordinatesFromUnrealLocation(UWorld* World, FVector2D Location, FString CRS, FVector2D& OutCoordinates);
	static bool GetCRSCoordinatesFromUnrealLocations(UWorld* World, FVector4d Locations, FString CRS, FVector4d &OutCoordinates);
	static bool GetCRSCoordinatesFromUnrealLocations(UWorld* World, FVector4d Locations, FVector4d &OutCoordinates);
	static bool GetCRSCoordinatesFromFBox(UWorld* World, FBox Box, FString CRS, FVector4d &OutCoordinates);
	static bool GetCRSCoordinatesFromOriginExtent(UWorld* World, FVector Origin, FVector Extent, FString CRS, FVector4d &OutCoordinates);
	
	UPROPERTY(EditAnywhere, Category = "LevelCoordinates")
	bool bSphereViewportNavigation = true;

	void SetSphereViewportNavigation(bool bEnabled);

	void UpdateBodyParameters();

	UFUNCTION()
	bool IsSphereModel() const;

	virtual void PostRegisterAllComponents() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

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

	UPROPERTY(EditAnywhere, Category = "LevelCoordinates", meta = (EditCondition = "!IsSphereModel()", EditConditionHides, DisplayPriority = "5"))
	TObjectPtr<UMaterialInterface> WorldMapMaterial =
		Cast<UMaterial>(
			StaticLoadObject(
				UMaterial::StaticClass(),
				nullptr,
				*FString("/Script/Engine.Material'/LandscapeCombinator/Materials/M_GeoDecal.M_GeoDecal'")
			)
		);

	UPROPERTY(EditAnywhere, Category = "LevelCoordinates", meta = (EditCondition = "IsSphereModel()", EditConditionHides, DisplayPriority = "5"))
	TObjectPtr<UMaterialInterface> WorldMapSphereMaterial =
		Cast<UMaterial>(
			StaticLoadObject(
				UMaterial::StaticClass(),
				nullptr,
				*FString("/Script/Engine.Material'/LandscapeCombinator/Materials/M_GeoSphere.M_GeoSphere'")
			)
		);

	UPROPERTY(EditAnywhere, Category = "LevelCoordinates", meta = (ClampMin = "4", ClampMax = "512", DisplayPriority = "4"))
	int SphereSteps = 64;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "LevelCoordinates")
	void CreateWorldMap();

protected:
	FTSTicker::FDelegateHandle ViewportTickHandle;
	TMap<void*, FVector> ViewportLastLocations;
	TMap<void*, FRotator> ViewportLastRotators;
	TSharedPtr<FBodyViewportExtension, ESPMode::ThreadSafe> ViewportExtension;
	bool TickViewport(float DeltaTime);
	void UpdateViewportTicker();

private:
	UFUNCTION(BlueprintCallable, Category = "LevelCoordinates | WorldMap")
	void CreateWorldMapFromFile(FString Path);

	void MatchSkyAtmosphereToBody();
#if WITH_EDITOR
	bool SpawnSphere();
#endif
};

#undef LOCTEXT_NAMESPACE