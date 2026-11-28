// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "LCContinuousGeneration.generated.h"

UCLASS(BlueprintType, meta=(BlueprintSpawnableComponent))
class LCCOMMON_API ULCContinuousGeneration : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(AdvancedDisplay, EditAnywhere, Category = "ContinuousGeneration", meta=(DisplayPriority=100))
	double PauseBetweenGenerationsSeconds = 0.1;

	UPROPERTY(AdvancedDisplay, EditAnywhere, Category = "ContinuousGeneration", meta=(DisplayPriority=101))
	bool bStartContinuousGenerationOnBeginPlay = false;

	// delay to let the world start, start continuous generation after n seconds
	UPROPERTY(AdvancedDisplay, EditAnywhere, Category = "ContinuousGeneration", meta=(DisplayPriority=102))
	double StartupDelay = 2;

	UPROPERTY(AdvancedDisplay, EditAnywhere, Category = "ContinuousGeneration", meta=(DisplayPriority=103))
	bool bContinueOnError = true;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "ContinuousGeneration")
	void StartContinuousGeneration();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "ContinuousGeneration")
	void StopContinuousGeneration();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void GenerateAndRearm();

	bool bRunning = false;
	FTimerHandle ContinuousGenerationTimer;
	FTimerHandle StartupDelayTimer;
};
