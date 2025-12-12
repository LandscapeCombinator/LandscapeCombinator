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
		auto Generate = [this, Owner]()
		{
			if (ILCGenerator *Generator = Cast<ILCGenerator>(Owner))
			{
				if (bIsCurrentlyGenerating)
				{
					UE_LOG(LogLCCommon, Warning, TEXT("Skipping generation as it already in progress. Consider increasing the continuous generation delay."));
					return;
				}
				bIsCurrentlyGenerating = true;
				UE_LOG(LogLCCommon, Log, TEXT("Continuous Generation Calling Generate"));
				Generator->GenerateFromGameThread(FName(), false, [this](bool bSuccess) {
					bIsCurrentlyGenerating = false;
					if (!bSuccess && bStopOnError) StopContinuousGeneration();
				});
			}
		};

		UE_LOG(LogLCCommon, Log, TEXT("Starting Continuous Generation Timer (generate every %f seconds)"), ContinuousGenerationSeconds);

		GetWorld()->GetTimerManager().SetTimer(ContinuousGenerationTimer, Generate, ContinuousGenerationSeconds, true, 0);
	}
	else
	{
		LCReporter::ShowError(
			LOCTEXT(
				"NoOwner" ,
				"Position Based Generation cannot start: Invalid Owner or World"
			)
		);
	}
}

void ULCContinuousGeneration::StopContinuousGeneration()
{
	Concurrency::RunOnGameThread([this]() {
		if (IsValid(GetWorld()))
			GetWorld()->GetTimerManager().ClearTimer(ContinuousGenerationTimer);
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
