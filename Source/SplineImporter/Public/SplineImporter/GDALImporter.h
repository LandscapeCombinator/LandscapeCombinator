// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "LogSplineImporter.h"
#include "LCCommon/ActorSelection.h"
#include "SplineImporter/SplineCollection.h"
#include "Coordinates/LevelCoordinates.h"
#include "LCCommon/LCGenerator.h"
#include "ConcurrencyHelpers/Concurrency.h"

#include "Landscape.h"
#include "Components/SplineComponent.h" 

#pragma warning(disable: 4668)
#include "gdal.h"
#include "gdal_priv.h"
#pragma warning(default: 4668)

#include "GDALImporter.generated.h"

#define LOCTEXT_NAMESPACE "FSplineImporterModule"

UENUM(BlueprintType)
enum class EVectorSource: uint8 {
	OSM_Roads,
	OSM_Rivers,
	OSM_Buildings,
	OSM_Forests,
	OSM_Beaches,
	OSM_Parks,
	OSM_SkiSlopes,
	OSM_Grass,
	OverpassShortQuery,
	OverpassQuery,
	LocalFile
};

UENUM(BlueprintType)
enum class EBoundingMethod: uint8 {
	BoundingActor,
	TileNumbers
};

UCLASS()
class SPLINEIMPORTER_API AGDALImporter : public AActor, public ILCGenerator
{
	GENERATED_BODY()

public:
	AGDALImporter();
	
	virtual bool ConfigureForTiles(int Zoom, int MinX, int MaxX, int MinY, int MaxY) override
	{
		BoundingMethod = EBoundingMethod::TileNumbers;
		BoundingZoneZoom = Zoom;
		BoundingZoneMinX = MinX;
		BoundingZoneMaxX = MaxX;
		BoundingZoneMinY = MinY;
		BoundingZoneMaxY = MaxY;
		return true;
	}

	UPROPERTY(VisibleAnywhere, Category = "GDALImporter", meta = (DisplayPriority = "-1000"))
	TObjectPtr<ULCPositionBasedGeneration> PositionBasedGeneration = nullptr;
	
	virtual TArray<UObject*> GetGeneratedObjects() const { return TArray<UObject*>(); };

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (DisplayPriority = "-10")
	)
	EVectorSource Source = EVectorSource::OSM_Roads;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "Source == EVectorSource::LocalFile", EditConditionHides, DisplayPriority = "-1")
	)
	FString LocalFile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "Source == EVectorSource::OverpassQuery", EditConditionHides, DisplayPriority = "-1")
	)
	FString OverpassQuery;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "Source == EVectorSource::OverpassShortQuery", EditConditionHides, DisplayPriority = "-1")
	)
	FString OverpassShortQuery;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "IsOverpassPreset()", EditConditionHides, DisplayPriority = "-1")
	)
	FString OverpassShortQueryPreset;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter", meta = (DisplayPriority = "10"))
	EBoundingMethod BoundingMethod = EBoundingMethod::BoundingActor;

	/* Use a volume, a landscape, or another rectangular actor to specify the area on which you want to import vector data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "BoundingMethod == EBoundingMethod::BoundingActor", EditConditionHides, DisplayPriority = "11")
	)
	FActorSelection BoundingActorSelection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "BoundingMethod == EBoundingMethod::TileNumbers", EditConditionHides, DisplayPriority = "11")
	)
	int BoundingZoneZoom = 14;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "BoundingMethod == EBoundingMethod::TileNumbers", EditConditionHides, DisplayPriority = "12")
	)
	int BoundingZoneMinX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "BoundingMethod == EBoundingMethod::TileNumbers", EditConditionHides, DisplayPriority = "13")
	)
	int BoundingZoneMaxX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "BoundingMethod == EBoundingMethod::TileNumbers", EditConditionHides, DisplayPriority = "14")
	)
	int BoundingZoneMinY = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "BoundingMethod == EBoundingMethod::TileNumbers", EditConditionHides, DisplayPriority = "15")
	)
	int BoundingZoneMaxY = 0;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (DisplayPriority = "4")
	)
	/* Folder used to spawn the actors. This setting is unused when generating from a combination. */
	FName SpawnedActorsPath;

	UFUNCTION(CallInEditor, Category = "GDALImporter")
	void Import() {
		Concurrency::RunAsync([this]() { Generate(SpawnedActorsPath, true); });
	}

	UFUNCTION(CallInEditor, Category = "GDALImporter")
	void Delete() { Execute_Cleanup(this, true); }
	
	UFUNCTION()
	bool IsOverpassPreset();

#if WITH_EDITOR
	virtual AActor* Duplicate(FName FromName, FName ToName) { return nullptr; };
#endif

protected:

#if WITH_EDITOR
	void PostEditChangeProperty(struct FPropertyChangedEvent&);
#endif

	GDALDataset* LoadGDALDataset(bool bIsUserInitiated);
	GDALDataset* LoadGDALDatasetFromShortQuery(FString ShortQuery, bool bIsUserInitiated);

	virtual void SetOverpassShortQuery();
};

#undef LOCTEXT_NAMESPACE