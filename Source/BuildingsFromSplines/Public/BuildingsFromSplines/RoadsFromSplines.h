// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "BuildingsFromSplines/LogBuildingsFromSplines.h"
#include "LCCommon/LCGenerator.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Components/SplineComponent.h" 
#include "Components/SplineMeshComponent.h"

#include "RoadsFromSplines.generated.h"

#define LOCTEXT_NAMESPACE "FBuildingsFromSplinesModule"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSplineMeshCreatedDelegate, USplineComponent*, FromSplineComponent, USplineMeshComponent*, SplineMeshComponent);

UCLASS(Blueprintable, PrioritizeCategories = "RoadsFromSplines")
class BUILDINGSFROMSPLINES_API ARoadsFromSplines : public AActor, public ILCGenerator
{
	GENERATED_BODY()

public:
	ARoadsFromSplines();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines")
	TObjectPtr<USceneComponent> EmptySceneComponent;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines",
		meta = (DisplayPriority = "-200")
	)
	TObjectPtr<UStaticMesh> RoadMesh;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines",
		meta = (DisplayPriority = "-199")
	)
	TEnumAsByte<ESplineMeshAxis::Type> RoadMeshAxis = ESplineMeshAxis::X;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines",
		meta = (DisplayPriority = "-99")
	)
	double WidthScale = 1;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines",
		meta = (DisplayPriority = "-98")
	)
	double HeightScale = 1;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines",
		meta = (DisplayPriority = "-97")
	)
	double ZOffset = 0;

	/* Delete previous roads before generating new ones */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines",
		meta = (DisplayPriority = "0")
	)
	bool bDeleteOldRoadsWhenCreatingRoads = true;

	/* The tag of the actors containing spline components to search for. If None, all actors will be used. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines",
		meta = (DisplayPriority = "-3")
	)
	FName SplinesTag;
	
	/* The tag of the Spline Components to use to generate roads. If None, all spline components will be used. */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines",
		meta = (DisplayPriority = "-2")
	)
	FName SplineComponentsTag;

	UFUNCTION(BlueprintCallable, Category = "RoadsFromSplines",
		meta = (DisplayPriority = "100")
	)
	bool GenerateRoads(bool bIsUserInitiated);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "RoadsFromSplines",
		meta = (DisplayName = "Generate Roads", DisplayPriority = "101")
	)
	void GenerateRoadsEditor() { GenerateRoads(true); }

	virtual bool OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated) override;

	virtual TArray<UObject*> GetGeneratedObjects() const {
		TArray<UObject*> GeneratedObjects;
		for (auto &SplineMeshComponent: SplineMeshComponents)
			if (IsValid(SplineMeshComponent)) GeneratedObjects.Add(SplineMeshComponent);

		return GeneratedObjects;
	}

	virtual bool Cleanup_Implementation(bool bSkipPrompt) override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "RoadsFromSplines",
		meta = (DisplayPriority = "4", DisplayName = "Clear Roads")
	)
	void ClearRoads();

#if WITH_EDITOR
	virtual AActor* Duplicate(FName FromName, FName ToName) override;
#endif

	// UFUNCTION(BlueprintImplementableEvent, Category = "RoadsFromSplines", meta = (DisplayPriority = "102"))
	// UMaterialInterface* GetMaterialOverride(USplineComponent *SplineComponent);

	UFUNCTION(CallInEditor, BlueprintImplementableEvent, Category = "RoadsFromSplines", meta = (DisplayPriority = "102"))
	void OnSplineMeshCreated(USplineComponent *FromSplineComponent, USplineMeshComponent *SplineMeshCreated);

	// UFUNCTION(BlueprintImplementableEvent, Category = "RoadsFromSplines", meta = (DisplayPriority = "102"))
	// int GetResult();

protected:
	UPROPERTY(DuplicateTransient)
	TArray<TObjectPtr<USplineMeshComponent>> SplineMeshComponents;
};

#undef LOCTEXT_NAMESPACE
