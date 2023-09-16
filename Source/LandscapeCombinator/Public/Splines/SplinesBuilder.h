// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Utils/Logging.h"
#include "GlobalSettings.h"

#include "Landscape.h"
#include "Components/SplineComponent.h" 

#pragma warning(disable: 4668)
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#include <ogr_api.h>
#include <ogrsf_frmts.h>
#pragma warning(default: 4668)

#include "SplinesBuilder.generated.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

using namespace GlobalSettings;

UENUM(BlueprintType)
enum ESourceKind: uint8 {
	Roads,
	Rivers,
	Buildings,
	OverpassShortQuery,
	OverpassQuery,
	LocalFile
};

UCLASS()
class LANDSCAPECOMBINATOR_API ASplinesBuilder : public AActor
{
	GENERATED_BODY()

public:
	ASplinesBuilder();

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Spline Generation",
		meta = (DisplayPriority = "0")
	)
	TEnumAsByte<ESourceKind> SplinesSource = ESourceKind::Roads;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Generation",
		meta = (DisplayPriority = "1")
	)
	bool bUseLandscapeSplines = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Generation",
		meta = (DisplayPriority = "2")
	)
	FString TargetLandscapeLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Generation",
		meta = (EditCondition = "SplinesSource == ESourceKind::LocalFile", EditConditionHides, DisplayPriority = "3")
	)
	FString LocalFile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Generation",
		meta = (EditCondition = "SplinesSource == ESourceKind::OverpassQuery", EditConditionHides, DisplayPriority = "3")
	)
	FString OverpassQuery;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Generation",
		meta = (EditCondition = "SplinesSource == ESourceKind::OverpassShortQuery", EditConditionHides, DisplayPriority = "3")
	)
	FString OverpassShortQuery;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Spline Generation",
		meta = (DisplayPriority = "4")
	)
	void GenerateSplines();
	
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Spline Generation",
		meta = (DisplayPriority = "5")
	)
	void ToggleLinear();

private:
	UPROPERTY()
	TArray<TObjectPtr<USplineComponent>> SplineComponents;

	GDALDataset* LoadGDALDatasetFromFile(FString File);
	void LoadGDALDataset(TFunction<void(GDALDataset*)> OnComplete);
	void LoadGDALDatasetFromQuery(FString Query, TFunction<void(GDALDataset*)> OnComplete);
	void LoadGDALDatasetFromShortQuery(FString ShortQuery, TFunction<void(GDALDataset*)> OnComplete);
	void GenerateSplinesFromDataset(ALandscape *Landscape, GDALDataset* Dataset);

	void AddPointLists(OGRMultiPolygon* MultiPolygon, TArray<TArray<OGRPoint>> &PointLists);
	void AddPointList(OGRLineString* LineString, TArray<TArray<OGRPoint>> &PointLists);

	void GenerateLandscapeSplines(
		ALandscape *Landscape,
		FCollisionQueryParams CollisionQueryParams,
		TArray<TArray<OGRPoint>> &PointLists,
		double WorldWidthCm,
		double WorldHeightCm,
		double WorldOriginX,
		double WorldOriginY
	);

	void AddLandscapeSplinesPoints(
		ALandscape* Landscape,
		FCollisionQueryParams CollisionQueryParams,
		ULandscapeSplinesComponent* LandscapeSplinesComponent,
		TArray<OGRPoint> &PointList,
		TMap<FVector2d, ULandscapeSplineControlPoint*> &Points,
		double WorldWidthCm,
		double WorldHeightCm,
		double WorldOriginX,
		double WorldOriginY
	);

	void AddLandscapeSplines(
		ALandscape* Landscape,
		FCollisionQueryParams CollisionQueryParams,
		ULandscapeSplinesComponent* LandscapeSplinesComponent,
		TArray<OGRPoint> &PointList,
		TMap<FVector2d, ULandscapeSplineControlPoint*> &Points,
		double WorldWidthCm,
		double WorldHeightCm,
		double WorldOriginX,
		double WorldOriginY
	);

	void GenerateRegularSplines(
		ALandscape *Landscape,
		FCollisionQueryParams CollisionQueryParams,
		TArray<TArray<OGRPoint>> &PointLists,
		double WorldWidthCm,
		double WorldHeightCm,
		double WorldOriginX,
		double WorldOriginY
	);

	void AddRegularSpline(
		ALandscape* Landscape,
		FCollisionQueryParams CollisionQueryParams,
		TArray<OGRPoint> &PointList,
		double WorldWidthCm,
		double WorldHeightCm,
		double WorldOriginX,
		double WorldOriginY
	);
};

#undef LOCTEXT_NAMESPACE