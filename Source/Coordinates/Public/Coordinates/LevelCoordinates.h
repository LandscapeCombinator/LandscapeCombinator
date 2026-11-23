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

	// World-only lookup: works when the level has a single visible Level Coordinates (the usual Earth case).
	static TObjectPtr<UGlobalCoordinates> GetGlobalCoordinates(TWeakObjectPtr<UWorld> World, bool bShowDialog = true);

	// Context-aware lookup: uses the body bound to Context (or its attach/owner parents) if any,
	// otherwise behaves like the world-only lookup.
	static TObjectPtr<UGlobalCoordinates> GetGlobalCoordinatesFor(const AActor* Context, bool bShowDialog = true);

	// Returns the body explicitly bound to Actor (ULCBodyBinding component), or nullptr.
	UFUNCTION(BlueprintCallable, Category = "LevelCoordinates")
	static ALevelCoordinates* GetBoundBody(const AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "LevelCoordinates")
	static void BindBody(AActor* Actor, ALevelCoordinates* Body);

	// Makes Child use the same body as Parent (no-op if Parent has no explicit binding).
	static void InheritBody(AActor* Child, const AActor* Parent);
	
	static OGRCoordinateTransformation *GetCRSTransformer(UWorld *World, FString CRS, const AActor* Context = nullptr);
	static bool GetUnrealCoordinatesFromCRS(UWorld *World, double Longitude, double Latitude, FString CRS, FVector2D &OutXY, const AActor* Context = nullptr);
	static bool GetCRSCoordinatesFromUnrealLocation(UWorld* World, FVector2D Location, FVector2D& OutCoordinates, const AActor* Context = nullptr);
	static bool GetCRSCoordinatesFromUnrealLocation(UWorld* World, FVector2D Location, FString CRS, FVector2D& OutCoordinates, const AActor* Context = nullptr);
	static bool GetCRSCoordinatesFromUnrealLocations(UWorld* World, FVector4d Locations, FString CRS, FVector4d &OutCoordinates, const AActor* Context = nullptr);
	static bool GetCRSCoordinatesFromUnrealLocations(UWorld* World, FVector4d Locations, FVector4d &OutCoordinates, const AActor* Context = nullptr);
	static bool GetCRSCoordinatesFromFBox(UWorld* World, FBox Box, FString CRS, FVector4d &OutCoordinates, const AActor* Context = nullptr);
	static bool GetCRSCoordinatesFromOriginExtent(UWorld* World, FVector Origin, FVector Extent, FString CRS, FVector4d &OutCoordinates, const AActor* Context = nullptr);
	
	UPROPERTY(EditAnywhere, Category = "LevelCoordinates")
	bool bSphereViewportNavigation = true;

	UPROPERTY(EditAnywhere, Category = "LevelCoordinates", meta = (EditCondition = "IsSphereModel()", EditConditionHides))
	bool bBodyAtActorLocation = false;

	void SetSphereViewportNavigation(bool bEnabled);

	void UpdateBodyParameters();

	static FLinearColor MakeBodyVector(const UGlobalCoordinates* G);

	UFUNCTION()
	bool IsSphereModel() const;

	virtual void PostRegisterAllComponents() override;

	UFUNCTION(BlueprintCallable, Category = "LevelCoordinates")
	void SyncBodyLocation();

	bool IsNearestSphereBody(const FVector& Location) const;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	void RefreshBodyLocation();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(
		EditAnywhere, Category = "LevelCoordinates | WorldMap",
		meta = (EditCondition = "IsSphereModel()", EditConditionHides, DisplayPriority = "0")
	)
	bool bApplyWorldMapTexture = true;

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

	UPROPERTY(EditAnywhere, Category = "LevelCoordinates", meta = (EditCondition = "IsSphereModel()", EditConditionHides, DisplayPriority = "4"))
	double SphereRadiusOffsetCm = 0;

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
	static TObjectPtr<UGlobalCoordinates> GetGlobalCoordinatesImpl(TWeakObjectPtr<UWorld> World, TWeakObjectPtr<const AActor> Context, bool bShowDialog);

	UFUNCTION(BlueprintCallable, Category = "LevelCoordinates | WorldMap")
	void CreateWorldMapFromFile(FString Path);

	void MatchSkyAtmosphereToBody();
#if WITH_EDITOR
	bool SpawnSphere();
#endif
};

#undef LOCTEXT_NAMESPACE