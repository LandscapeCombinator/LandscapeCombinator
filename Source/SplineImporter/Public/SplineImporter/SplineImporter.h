// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "LogSplineImporter.h"
#include "SplineImporter/GDALImporter.h"
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
enum class ESplineOwnerKind : uint8
{
	SingleSplineCollection,
	ManySplineCollections,
	CustomActor
};

UENUM(BlueprintType)
enum class ESplineDirection : uint8
{
	Any,
	Clockwise,
	CounterClockwise
};

UCLASS()
class SPLINEIMPORTER_API ASplineImporter : public AGDALImporter
{
	GENERATED_BODY()

public:
	ASplineImporter() : AGDALImporter() {};

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (DisplayPriority = "-100")
	)
	bool bDebugLineTraces = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "Source == EVectorSource::LocalFile", EditConditionHides, DisplayPriority = "0")
	)
	bool bSkipCoordinateConversion = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "Source == EVectorSource::LocalFile && bSkipCoordinateConversion", EditConditionHides, DisplayPriority = "0")
	)
	double ScaleX = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "Source == EVectorSource::LocalFile && bSkipCoordinateConversion", EditConditionHides, DisplayPriority = "1")
	)
	double ScaleY = 1;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (DisplayPriority = "1000")
	)
	bool bDeleteOldSplinesWhenCreatingSplines = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (DisplayPriority = "1001")
	)
	FActorSelection ActorsOrLandscapesToPlaceSplinesSelection;

	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (DisplayPriority = "1002")
	)
	/* Check this only if `ActorToPlaceSplines` is a landscape and if you want to use landscape splines instead of normal spline components.
	 * For roads, it is recommended you use landscape splines (checked).
	 * For buildings, it is recommended you use normal spline components (unchecked). */
	bool bUseLandscapeSplines = false;

	/* Increasing this value (before generating splines) makes your landscape splines less curvy. This does not apply to regular splines. */
	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "bUseLandscapeSplines", EditConditionHides, DisplayPriority = "1004")
	)
	double LandscapeSplinesStraightness = 1;

	
	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "!bUseLandscapeSplines", EditConditionHides, DisplayPriority = "1005")
	)
	/* Whether to put the created spline components in a single Spline Collection actor, or use one Spline Collection per actor, or use a custom actor. */
	ESplineOwnerKind SplineOwnerKind = ESplineOwnerKind::SingleSplineCollection;
	
	/* Tag to apply to the Spline Owners which are created. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (DisplayPriority = "1005")
	)
	FName SplinesTag;

	/* Tag to apply to the Spline Components which are created. */
	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "!bUseLandscapeSplines", EditConditionHides, DisplayPriority = "1006")
	)
	FName SplineComponentsTag;
	
	/* Spawn an actor of this type for each imported spline, and copy the imported spline points to the spawned actor's spline component. */
	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "!bUseLandscapeSplines && SplineOwnerKind == ESplineOwnerKind::CustomActor", EditConditionHides, DisplayPriority = "1010")
	)
	TSubclassOf<AActor> ActorToSpawn;

	/* Splines that are closed loops can be forced to be Clockwise or CounterClockwise (when seen from top). */
	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "!bUseLandscapeSplines", EditConditionHides, DisplayPriority = "1200")
	)
	ESplineDirection SplineDirection = ESplineDirection::Any;

	/* An offset which is added to all spline points that are generated. */
	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (DisplayPriority = "1300")
	)
	FVector SplinePointsOffset;

	bool OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated) override;
	
	UPROPERTY(
		EditAnywhere, DuplicateTransient, Category = "GDALImporter",
		meta = (EditCondition = "false", EditConditionHides)
	)
	TArray<TSoftObjectPtr<AActor>> SplineOwners;

	virtual TArray<UObject*> GetGeneratedObjects() const {
		TArray<UObject*> GeneratedObjects;
		for (const TSoftObjectPtr<AActor>& SplineOwner : SplineOwners)
		{
			if (SplineOwner.IsValid())
			{
				GeneratedObjects.Add(SplineOwner.Get());
			}
		}
		return GeneratedObjects;
	}

	virtual bool Cleanup_Implementation(bool bSkipPrompt) override;

protected:
	bool GetUECoordinates(
		double Longitude, double Latitude,
		OGRCoordinateTransformation *OGRTransform, UGlobalCoordinates *GlobalCoordinates,
		double &OutX, double &OutY
	);

	virtual void SetOverpassShortQuery() override;

	bool GenerateRegularSplines(
		bool bIsUserInitiated,
		FName SpawnedActorsPathOverride,
		FCollisionQueryParams CollisionQueryParams,
		OGRCoordinateTransformation *OGRTransform,
		UGlobalCoordinates *GlobalCoordinates,
		TArray<FPointList> &PointLists
	);

	bool AddRegularSpline(
		AActor* SplineOwner,
		FCollisionQueryParams CollisionQueryParams,
		OGRCoordinateTransformation *OGRTransform,
		UGlobalCoordinates *GlobalCoordinates,
		FPointList &PointList
	);

#if WITH_EDITOR
	bool GenerateLandscapeSplines(
		bool bIsUserInitiated,
		ALandscape* Landscape,
		FCollisionQueryParams CollisionQueryParams,
		OGRCoordinateTransformation* OGRTransform,
		UGlobalCoordinates* GlobalCoordinates,
		TArray<FPointList>& PointLists
	);

	void AddLandscapeSplinesPoints(
		FCollisionQueryParams CollisionQueryParams,
		OGRCoordinateTransformation* OGRTransform,
		UGlobalCoordinates* GlobalCoordinates,
		ULandscapeSplinesComponent* LandscapeSplinesComponent,
		FPointList& PointList,
		TMap<FVector2D, ULandscapeSplineControlPoint*>& Points
	);

	void AddLandscapeSplines(
		ULandscapeSplinesComponent* LandscapeSplinesComponent,
		FPointList& PointList,
		TMap<FVector2D, ULandscapeSplineControlPoint*>& Points
	);

	virtual AActor* Duplicate(FName FromName, FName ToName) override;
#endif

};

#undef LOCTEXT_NAMESPACE