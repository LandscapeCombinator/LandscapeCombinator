// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGVolume.h"
#include "PCGComponent.h"
#include "Landscape.h"

#include "SplineImporter/ActorSelection.h"
#include "LandscapeUtils/LandscapeUtils.h"

#include "LandscapePCGVolume.generated.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

UCLASS(BlueprintType)
class LANDSCAPECOMBINATOR_API ALandscapePCGVolume : public APCGVolume
{
	GENERATED_BODY()

public:

	ALandscapePCGVolume(const FObjectInitializer& ObjectInitializer) : APCGVolume(ObjectInitializer)
	{
		PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
	}

	/* Landscape used to automatically set the size of the PCG volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapePCGVolume",
		meta = (DisplayPriority = "0")
	)
	FActorSelection LandscapeSelection;

	/* Landscape used to automatically set the size of the PCG volume. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "LandscapePCGVolume",
		meta = (DisplayPriority = "1")
	)
	FVector Position;

	/* Landscape used to automatically set the size of the PCG volume. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "LandscapePCGVolume",
		meta = (DisplayPriority = "1")
	)
	FVector Bounds;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "LandscapePCGVolume")
	void SetPositionAndBounds();
};

#undef LOCTEXT_NAMESPACE
