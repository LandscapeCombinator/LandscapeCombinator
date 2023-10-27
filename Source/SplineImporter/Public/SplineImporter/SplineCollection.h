// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "LogSplineImporter.h"
#include "Components/SplineComponent.h" 

#include "SplineCollection.generated.h"

#define LOCTEXT_NAMESPACE "FSplineImporterModule"

UCLASS()
class SPLINEIMPORTER_API ASplineCollection : public AActor
{
	GENERATED_BODY()

public:
	ASplineCollection();
	
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Spline Collection",
		meta = (DisplayPriority = "5")
	)
	/* Toggle linear/curvy splines for the spline components of this actor. */
	void ToggleLinear();

	UPROPERTY()
	TArray<TObjectPtr<USplineComponent>> SplineComponents;
};

#undef LOCTEXT_NAMESPACE