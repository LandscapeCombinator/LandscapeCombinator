// Copyright 2023 LandscapeCombinator. All Rights Reserved.

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

UCLASS()
class SPLINEIMPORTER_API ASplineImporter : public AGDALImporter
{
	GENERATED_BODY()

public:
	ASplineImporter() : AGDALImporter() {};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (DisplayPriority = "2")
	)
	FActorSelection ActorOrLandscapeToPlaceSplinesSelection;

	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (DisplayPriority = "3")
	)
	/* Check this only if `ActorToPlaceSplines` is a landscape and if you want to use landscape splines instead of normal spline components.
	 * For roads, it is recommended you use landscape splines (checked).
	 * For buildings, it is recommended you use normal spline components (unchecked). */
	bool bUseLandscapeSplines = false;

	/* Increasing this value (before generating splines) makes your landscape splines less curvy. This does not apply to regular splines. */
	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "bUseLandscapeSplines", EditConditionHides, DisplayPriority = "4")
	)
	double LandscapeSplinesStraightness = 1;

	
	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "!bUseLandscapeSplines", EditConditionHides, DisplayPriority = "5")
	)
	/* Whether to put the created spline components in a single Spline Collection actor, or use one Spline Collection per actor, or use a custom actor. */
	ESplineOwnerKind SplineOwnerKind = ESplineOwnerKind::SingleSplineCollection;
	
	/* Tag to apply to the Spline Owners which are created. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (DisplayPriority = "5")
	)
	FName SplinesTag;

	/* Tag to apply to the Spline Components which are created. */
	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "!bUseLandscapeSplines", EditConditionHides, DisplayPriority = "6")
	)
	FName SplineComponentsTag;
	
	/* Spawn an actor of this type for each imported spline, and copy the imported spline points to the spawned actor's spline component. */
	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "!bUseLandscapeSplines && SplineOwnerKind == ESplineOwnerKind::CustomActor", EditConditionHides, DisplayPriority = "10")
	)
	TSubclassOf<AActor> ActorToSpawn;
	
	/* The name of the property in `ActorToSpawn` where to copy the spline points. */
	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (EditCondition = "!bUseLandscapeSplines && SplineOwnerKind == ESplineOwnerKind::CustomActor", EditConditionHides, DisplayPriority = "11")
	)
	FName SplineComponentName;
	
	
	/* An offset which is added to all spline points that are generated. */
	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (DisplayPriority = "100")
	)
	FVector SplinePointsOffset;

	void Import(TFunction<void(bool)> OnComplete) override;

	void Clear() override;

protected:
	virtual void SetOverpassShortQuery() override;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, DuplicateTransient, Category = "GDALImporter",
		meta = (EditCondition = "false", EditConditionHides)
	)
	TArray<TObjectPtr<AActor>> SplineOwners;

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

#if WITH_EDITOR
	void GenerateLandscapeSplines(
		ALandscape* Landscape,
		FCollisionQueryParams CollisionQueryParams,
		OGRCoordinateTransformation* OGRTransform,
		UGlobalCoordinates* GlobalCoordinates,
		TArray<FPointList>& PointLists
	);

	void AddLandscapeSplinesPoints(
		ALandscape* Landscape,
		FCollisionQueryParams CollisionQueryParams,
		OGRCoordinateTransformation* OGRTransform,
		UGlobalCoordinates* GlobalCoordinates,
		ULandscapeSplinesComponent* LandscapeSplinesComponent,
		FPointList& PointList,
		TMap<FVector2D, ULandscapeSplineControlPoint*>& Points
	);

	void AddLandscapeSplines(
		ALandscape* Landscape,
		FCollisionQueryParams CollisionQueryParams,
		OGRCoordinateTransformation* OGRTransform,
		UGlobalCoordinates* GlobalCoordinates,
		ULandscapeSplinesComponent* LandscapeSplinesComponent,
		FPointList& PointList,
		TMap<FVector2D, ULandscapeSplineControlPoint*>& Points
	);
#endif

};

#undef LOCTEXT_NAMESPACE