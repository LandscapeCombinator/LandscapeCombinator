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
enum ESourceKind: uint8 {
	OSM_Roads,
	OSM_Rivers,
	OSM_Buildings,
	OverpassShortQuery,
	OverpassQuery,
	LocalFile
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
	TEnumAsByte<ESourceKind> SplinesSource = ESourceKind::OSM_Roads;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Importer",
		meta = (EditCondition = "SplinesSource == ESourceKind::LocalFile", EditConditionHides, DisplayPriority = "-1")
	)
	FString LocalFile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Importer",
		meta = (EditCondition = "SplinesSource == ESourceKind::OverpassQuery", EditConditionHides, DisplayPriority = "-1")
	)
	FString OverpassQuery;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Importer",
		meta = (EditCondition = "SplinesSource == ESourceKind::OverpassShortQuery", EditConditionHides, DisplayPriority = "-1")
	)
	FString OverpassShortQuery;

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

	/* Tag to apply to the Spline Collections which are created. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Importer",
		meta = (EditCondition = "!bUseLandscapeSplines", EditConditionHides, DisplayPriority = "5")
	)
	FName SplineCollectionTag;

	/* Tag to apply to the Spline Components which are created. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Importer",
		meta = (EditCondition = "!bUseLandscapeSplines", EditConditionHides, DisplayPriority = "6")
	)
	FName SplineComponentsTag;
	
	/* Put all the Spline Components in the same Spline Collection actor. Untick if you prefer one actor per component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Importer",
		meta = (EditCondition = "!bUseLandscapeSplines", EditConditionHides, DisplayPriority = "7")
	)
	bool bUseSingleCollection = true;


	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Spline Importer",
		meta = (DisplayPriority = "-1")
	)
	void DeleteSplines();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Spline Importer",
		meta = (DisplayPriority = "-1")
	)
	void GenerateSplines();

private:
	UPROPERTY()
	TArray<AActor*> SplineCollections;

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
		AActor* Actor,
		ASplineCollection* SplineCollection,
		FCollisionQueryParams CollisionQueryParams,
		OGRCoordinateTransformation *OGRTransform,
		UGlobalCoordinates *GlobalCoordinates,
		FPointList &PointList
	);
};

#undef LOCTEXT_NAMESPACE