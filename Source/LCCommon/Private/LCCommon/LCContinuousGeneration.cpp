// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LCCommon/LCContinuousGeneration.h"
#include "LCCommon/LCGenerator.h"
#include "LCCommon/LogLCCommon.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"
#include "Engine/World.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void ULCContinuousGeneration::StartContinuousGeneration()
{
    AActor *Owner = GetOwner();
    if (IsValid(Owner) && IsValid(GetWorld()))
    {
        TWeakObjectPtr<ULCContinuousGeneration> WeakThis(this);
        TWeakObjectPtr<AActor> WeakOwner(Owner);

        auto Generate = [WeakThis, WeakOwner]()
        {
            if (!WeakThis.IsValid() || !WeakOwner.IsValid()) return;

            if (ILCGenerator *Generator = Cast<ILCGenerator>(WeakOwner.Get()))
            {
                if (WeakThis->bIsCurrentlyGenerating)
                {
                    UE_LOG(LogLCCommon, Warning, TEXT("Skipping generation as it already in progress. Consider increasing the continuous generation delay."));
                    return;
                }
                WeakThis->bIsCurrentlyGenerating = true;
                UE_LOG(LogLCCommon, Log, TEXT("Continuous Generation Calling Generate"));
                Generator->GenerateFromGameThread(FName(), false, [WeakThis](bool bSuccess) {
                    if (!WeakThis.IsValid()) return;
                    WeakThis->bIsCurrentlyGenerating = false;
                    if (!bSuccess && WeakThis->bStopOnError) WeakThis->StopContinuousGeneration();
                });
            }
        };

        auto StartTimer = [WeakThis, Generate]()
        {
            if (!WeakThis.IsValid()) return;
            UE_LOG(LogLCCommon, Log, TEXT("Starting Continuous Generation Timer (generate every %f seconds)"), WeakThis->ContinuousGenerationSeconds);
            WeakThis->GetWorld()->GetTimerManager().SetTimer(WeakThis->ContinuousGenerationTimer, Generate, WeakThis->ContinuousGenerationSeconds, true, 0);
        };

        if (StartupDelay <= 0) StartTimer();
        else GetWorld()->GetTimerManager().SetTimer(StartupDelayTimer, StartTimer, StartupDelay, false);
    }
    else
    {
        LCReporter::ShowError(
            LOCTEXT("NoOwner", "Position Based Generation cannot start: Invalid Owner or World")
        );
    }
}

void ULCContinuousGeneration::StopContinuousGeneration()
{
    TWeakObjectPtr<ULCContinuousGeneration> WeakThis(this);
    Concurrency::RunOnGameThread([WeakThis]() {
        if (WeakThis.IsValid() && IsValid(WeakThis->GetWorld()))
            WeakThis->GetWorld()->GetTimerManager().ClearTimer(WeakThis->ContinuousGenerationTimer);
    });
}

void ULCContinuousGeneration::BeginPlay()
{
	Super::BeginPlay();
	if (bStartContinuousGenerationOnBeginPlay) StartContinuousGeneration();
}

void ULCContinuousGeneration::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bStartContinuousGenerationOnBeginPlay) StopContinuousGeneration();
	Super::EndPlay(EndPlayReason);
}

#undef LOCTEXT_NAMESPACE
