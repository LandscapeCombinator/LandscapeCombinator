// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "LCContinuousGeneration.generated.h"

UCLASS(BlueprintType)
class LCCOMMON_API ULCContinuousGeneration : public UActorComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "ContinuousGeneration", meta=(DisplayPriority=100))
    double ContinuousGenerationSeconds = 10;

    UPROPERTY(EditAnywhere, Category = "ContinuousGeneration", meta=(DisplayPriority=101))
    bool bStartContinuousGenerationOnBeginPlay = false;

    UPROPERTY(EditAnywhere, Category = "ContinuousGeneration", meta=(DisplayPriority=102))
    bool bStopOnError = true;

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "ContinuousGeneration")
    void StartContinuousGeneration();

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "ContinuousGeneration")
    void StopContinuousGeneration();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    bool bIsCurrentlyGenerating = false;
    FTimerHandle ContinuousGenerationTimer;

};
