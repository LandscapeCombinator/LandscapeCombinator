// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "LogSplineImporter.h"
#include "SplineImporter/SplineCollection.h"
#include "Coordinates/LevelCoordinates.h"

#include "Landscape.h"
#include "Components/SplineComponent.h" 

#pragma warning(disable: 4668)
#include "gdal.h"
#include "gdal_priv.h"
#pragma warning(default: 4668)

#include "SplineImporter.generated.h"

#define LOCTEXT_NAMESPACE "FSplineImporterModule"

UENUM(BlueprintType)
enum class ESplinesSource: uint8 {
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

UENUM(BlueprintType)
enum class ESplineOwnerKind : uint8
{
	SingleSplineCollection,
	ManySplineCollections,
	CustomActor
};

UCLASS()
class SPLINEIMPORTER_API ASplineImporter : public AActor
{
	GENERATED_BODY()

public:
	ASplineImporter();

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Spline Importer",
		meta = (DisplayPriority = "-10")
	)
	ESplinesSource SplinesSource = ESplinesSource::OSM_Roads;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Importer",
		meta = (EditCondition = "SplinesSource == ESplinesSource::LocalFile", EditConditionHides, DisplayPriority = "-1")
	)
	FString LocalFile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Importer",
		meta = (EditCondition = "SplinesSource == ESplinesSource::OverpassQuery", EditConditionHides, DisplayPriority = "-1")
	)
	FString OverpassQuery;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Importer",
		meta = (EditCondition = "SplinesSource == ESplinesSource::OverpassShortQuery", EditConditionHides, DisplayPriority = "-1")
	)
	FString OverpassShortQuery;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Spline Importer",
		meta = (EditCondition = "IsOverpassPreset()", EditConditionHides, DisplayPriority = "-1")
	)
	FString OverpassShortQueryPreset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Importer",
		meta = (DisplayPriority = "0")
	)
	AActor *ActorOrLandscapeToPlaceSplines = nullptr;

	/* Check this if you don't want to use import splines on the whole area of `ActorToPlaceSplines`.
	 * Then, set the `BoundingActor` to a cube or another rectangular actor covering the area that you want. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Importer",
		meta = (DisplayPriority = "1")
	)
	bool bRestrictArea = false;

	/* Use a cube or another rectangular actor to specify the area on which you want to import splines.
	 * Splines will be imported on `ActorToPlaceSplines`. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Importer",
		meta = (EditCondition = "bRestrictArea", EditConditionHides, DisplayPriority = "2")
	)
	AActor *BoundingActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Importer",
		meta = (DisplayPriority = "3")
	)
	/* Check this only if `ActorToPlaceSplines` is a landscape and if you want to use landscape splines instead of normal spline components.
	 * For roads, it is recommended you use landscape splines (checked).
	 * For buildings, it is recommended you use normal spline components (unchecked). */
	bool bUseLandscapeSplines = false;

	/* Increasing this value (before generating splines) makes your landscape splines less curvy. This does not apply to regular splines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Importer",
		meta = (EditCondition = "bUseLandscapeSplines", EditConditionHides, DisplayPriority = "4")
	)
	double LandscapeSplinesStraightness = 1;

	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Importer",
		meta = (EditCondition = "!bUseLandscapeSplines", EditConditionHides, DisplayPriority = "5")
	)
	/* Whether to put the created spline components in a single Spline Collection actor, or use one Spline Collection per actor, or use a custom actor. */
	ESplineOwnerKind SplineOwnerKind = ESplineOwnerKind::SingleSplineCollection;

	/* Tag to apply to the Spline Owners which are created. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Importer",
		meta = (EditCondition = "!bUseLandscapeSplines", EditConditionHides, DisplayPriority = "5")
	)
	FName SplineOwnerTag;

	/* Tag to apply to the Spline Components which are created. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Importer",
		meta = (EditCondition = "!bUseLandscapeSplines", EditConditionHides, DisplayPriority = "6")
	)
	FName SplineComponentsTag;
	
	/* Spawn an actor of this type for each imported spline, and copy the imported spline points to the spawned actor's spline component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Importer",
		meta = (EditCondition = "!bUseLandscapeSplines && SplineOwnerKind == ESplineOwnerKind::CustomActor", EditConditionHides, DisplayPriority = "10")
	)
	TSubclassOf<AActor> ActorToSpawn;
	
	/* The name of the property in `ActorToSpawn` where to copy the spline points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Importer",
		meta = (EditCondition = "!bUseLandscapeSplines && SplineOwnerKind == ESplineOwnerKind::CustomActor", EditConditionHides, DisplayPriority = "11")
	)
	FName SplineComponentName;
	
	
	/* An offset which is added to all spline points that are generated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Importer",
		meta = (DisplayPriority = "100")
	)
	FVector SplinePointsOffset;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Spline Importer",
		meta = (DisplayPriority = "-2")
	)
	void GenerateSplines();	

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Spline Importer",
		meta = (DisplayPriority = "-1")
	)
	void DeleteSplines();

	void SetOverpassShortQuery();

	UFUNCTION()
	bool IsOverpassPreset();


#if WITH_EDITOR
	void PostEditChangeProperty(struct FPropertyChangedEvent&);
#endif

private:
	UPROPERTY(DuplicateTransient)
	TArray<AActor*> SplineOwners;

	GDALDataset* LoadGDALDatasetFromFile(FString File);
	void LoadGDALDataset(TFunction<void(GDALDataset*)> OnComplete);
	void LoadGDALDatasetFromQuery(FString Query, TFunction<void(GDALDataset*)> OnComplete);
	void LoadGDALDatasetFromShortQuery(FString ShortQuery, TFunction<void(GDALDataset*)> OnComplete);

	void GenerateLandscapeSplines(
		ALandscape *Landscape,
		FCollisionQueryParams CollisionQueryParams,
		OGRCoordinateTransformation *OGRTransform,
		UGlobalCoordinates *GlobalCoordinates,
		TArray<FPointList> &PointLists
	);

	void AddLandscapeSplinesPoints(
		ALandscape* Landscape,
		FCollisionQueryParams CollisionQueryParams,
		OGRCoordinateTransformation *OGRTransform,
		UGlobalCoordinates *GlobalCoordinates,
		ULandscapeSplinesComponent* LandscapeSplinesComponent,
		FPointList &PointList,
		TMap<FVector2D, ULandscapeSplineControlPoint*> &Points
	);

	void AddLandscapeSplines(
		ALandscape* Landscape,
		FCollisionQueryParams CollisionQueryParams,
		OGRCoordinateTransformation *OGRTransform,
		UGlobalCoordinates *GlobalCoordinates,
		ULandscapeSplinesComponent* LandscapeSplinesComponent,
		FPointList &PointList,
		TMap<FVector2D, ULandscapeSplineControlPoint*> &Points
	);

	void GenerateRegularSplines(
		AActor *Actor,
		FCollisionQueryParams CollisionQueryParams,
		OGRCoordinateTransformation *OGRTransform,
		UGlobalCoordinates *GlobalCoordinates,
		TArray<FPointList> &PointLists
	);

	void AddRegularSpline(
		AActor* SplineOwner,
		FCollisionQueryParams CollisionQueryParams,
		OGRCoordinateTransformation *OGRTransform,
		UGlobalCoordinates *GlobalCoordinates,
		FPointList &PointList
	);
};

#undef LOCTEXT_NAMESPACE