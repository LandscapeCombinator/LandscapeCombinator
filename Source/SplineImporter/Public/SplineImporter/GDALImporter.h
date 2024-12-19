// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "LogSplineImporter.h"
#include "LCCommon/ActorSelection.h"
#include "SplineImporter/SplineCollection.h"
#include "Coordinates/LevelCoordinates.h"
#include "LCCommon/LCGenerator.h"

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
	OverpassShortQuery,
	OverpassQuery,
	LocalFile
};

UCLASS()
class SPLINEIMPORTER_API AGDALImporter : public AActor, public ILCGenerator
{
	GENERATED_BODY()

public:
	AGDALImporter();
	
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
	
	/* Use a volume, a landscape, or another rectangular actor to specify the area on which you want to import vector data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (DisplayPriority = "2")
	)
	FActorSelection BoundingActorSelection;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (DisplayPriority = "4")
	)
	/* Folder used to spawn the actors. This setting is unused when generating from a combination. */
	FName SpawnedActorsPath;

	UFUNCTION(CallInEditor, Category = "GDALImporter")
	void Import() { Generate(SpawnedActorsPath); }

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

	void LoadGDALDataset(TFunction<void(GDALDataset*)> OnComplete);
	void LoadGDALDatasetFromShortQuery(FString ShortQuery, TFunction<void(GDALDataset*)> OnComplete);

	virtual void SetOverpassShortQuery();
};

#undef LOCTEXT_NAMESPACE