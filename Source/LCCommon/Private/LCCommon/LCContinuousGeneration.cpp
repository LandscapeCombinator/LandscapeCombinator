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
	if (AActor *Owner = GetOwner())
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
					if (!bSuccess) StopContinuousGeneration();
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
				"Position Based Generation needs an owner to start continuous generation"
			)
		);
	}
}

void ULCContinuousGeneration::StopContinuousGeneration()
{
	GetWorld()->GetTimerManager().ClearTimer(ContinuousGenerationTimer);
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
