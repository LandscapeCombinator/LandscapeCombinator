// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGVolume.h"
#include "PCGComponent.h"
#include "Landscape.h"

#include "LCCommon/ActorSelection.h"
#include "LandscapeUtils/LandscapeUtils.h"
#include "LCCommon/LCGenerator.h"

#include "LandscapePCGVolume.generated.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

UCLASS(BlueprintType)
class LANDSCAPECOMBINATOR_API ALandscapePCGVolume : public APCGVolume, public ILCGenerator
{
	GENERATED_BODY()

public:

	virtual TArray<UObject*> GetGeneratedObjects() const override { return TArray<UObject*>(); }

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

	virtual bool OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated) override;

	virtual bool Cleanup_Implementation(bool bSkipPrompt) override;

#if WITH_EDITOR
	virtual AActor* Duplicate(FName FromName, FName ToName) override;
#endif
};

#undef LOCTEXT_NAMESPACE
