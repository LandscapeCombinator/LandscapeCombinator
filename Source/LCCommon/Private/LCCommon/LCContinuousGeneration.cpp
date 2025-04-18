// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LCCommon/LCContinuousGeneration.h"
#include "LCCommon/LCGenerator.h"
#include "LCCommon/LogLCCommon.h"
#include "LCReporter/LCReporter.h"
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
					UE_LOG(LogLCCommon, Warning, TEXT("Skipping generation as it already in progress"));
					return;
				}
				bIsCurrentlyGenerating = true;
				UE_LOG(LogLCCommon, Log, TEXT("Continuous Generation Calling Generate"));
				Generator->Generate(FName(), false, [this](bool bSuccess) { bIsCurrentlyGenerating = false; });
			}
		};
		UE_LOG(LogLCCommon, Log, TEXT("Starting Continuous Generation Timer (generate every %f seconds)"), ContinuousGenerationSeconds);
		Generate();
		GetWorld()->GetTimerManager().SetTimer(ContinuousGenerationTimer, Generate, ContinuousGenerationSeconds, true);
	}
	else
	{
		ULCReporter::ShowError(
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
