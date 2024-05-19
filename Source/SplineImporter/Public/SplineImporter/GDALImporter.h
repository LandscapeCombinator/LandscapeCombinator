// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "LogSplineImporter.h"
#include "SplineImporter/ActorSelection.h"
#include "SplineImporter/SplineCollection.h"
#include "Coordinates/LevelCoordinates.h"

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
class SPLINEIMPORTER_API AGDALImporter : public AActor
{
	GENERATED_BODY()

public:
	AGDALImporter();

	UPROPERTY(
		AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (DisplayPriority = "-12")
	)
	bool bSilentMode = false;

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
	
	UFUNCTION()
	bool IsOverpassPreset();
	
	UFUNCTION(CallInEditor, Category = "GDALImporter")
	void Import() { Import(nullptr); }

	virtual void Import(TFunction<void(bool)> OnComplete)
	{
		OnComplete(true);
	}
	
	UFUNCTION(BlueprintCallable, Category = "GDALImporter")
	virtual void Clear() {}


protected:
#if WITH_EDITOR
	void PostEditChangeProperty(struct FPropertyChangedEvent&);
#endif

	void LoadGDALDataset(TFunction<void(GDALDataset*)> OnComplete);
	void LoadGDALDatasetFromShortQuery(FString ShortQuery, TFunction<void(GDALDataset*)> OnComplete);

	virtual void SetOverpassShortQuery();
};

#undef LOCTEXT_NAMESPACE