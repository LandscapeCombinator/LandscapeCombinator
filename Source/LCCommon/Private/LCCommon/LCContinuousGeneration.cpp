// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

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
		bRunning = true;
		UE_LOG(LogLCCommon, Log, TEXT("Starting Continuous Generation (pause of %f seconds between generations)"), PauseBetweenGenerationsSeconds);

		if (StartupDelay <= 0) GenerateAndRearm();
		else GetWorld()->GetTimerManager().SetTimer(StartupDelayTimer, FTimerDelegate::CreateUObject(this, &ULCContinuousGeneration::GenerateAndRearm), StartupDelay, false);
	}
	else
	{
		LCReporter::ShowError(
			LOCTEXT("NoOwner", "Position Based Generation cannot start: Invalid Owner or World")
		);
	}
}

void ULCContinuousGeneration::GenerateAndRearm()
{
	ILCGenerator *Generator = Cast<ILCGenerator>(GetOwner());
	if (!bRunning || !Generator) return;

	UE_LOG(LogLCCommon, Log, TEXT("Continuous Generation Calling Generate"));
	TWeakObjectPtr<ULCContinuousGeneration> WeakThis(this);
	Generator->GenerateFromGameThread(FName(), false, [WeakThis](bool bSuccess) {
		const bool bCancelled = Concurrency::IsCancelRequested();
		Concurrency::RunOnGameThread([WeakThis, bSuccess, bCancelled]() {
			if (!WeakThis.IsValid() || !WeakThis->bRunning || !IsValid(WeakThis->GetWorld())) return;

			if (bCancelled || (!bSuccess && !WeakThis->bContinueOnError))
			{
				WeakThis->StopContinuousGeneration();
				return;
			}

			WeakThis->GetWorld()->GetTimerManager().SetTimer(
				WeakThis->ContinuousGenerationTimer,
				FTimerDelegate::CreateUObject(WeakThis.Get(), &ULCContinuousGeneration::GenerateAndRearm),
				WeakThis->PauseBetweenGenerationsSeconds, false
			);
		});
	});
}

void ULCContinuousGeneration::StopContinuousGeneration()
{
	TWeakObjectPtr<ULCContinuousGeneration> WeakThis(this);
	Concurrency::RunOnGameThread([WeakThis]() {
		if (!WeakThis.IsValid()) return;
		WeakThis->bRunning = false;
		if (IsValid(WeakThis->GetWorld()))
		{
			WeakThis->GetWorld()->GetTimerManager().ClearTimer(WeakThis->ContinuousGenerationTimer);
			WeakThis->GetWorld()->GetTimerManager().ClearTimer(WeakThis->StartupDelayTimer);
		}
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
