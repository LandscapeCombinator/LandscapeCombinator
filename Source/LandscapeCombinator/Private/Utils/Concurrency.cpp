// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "Utils/Concurrency.h"
#include "Utils/Logging.h"

#include "CoreMinimal.h"
#include "Async/Async.h"

namespace Concurrency {

	void RunAsync(TFunction<void()> Action)
	{
		Async(EAsyncExecution::Thread, [Action]() { Action(); });
		return;
	}

	void RunMany(TArray<FString> Elements, TFunction<void( FString Element, TFunction<void(bool)> )> Action, TFunction<void(bool)> OnComplete)
	{
		int32 NumberOfTasks = Elements.Num();
		std::atomic<int32> *SuccessfulTasks = new std::atomic<int>(0);
		std::atomic<int32> *FinishedTasks = new std::atomic<int>(0);
		UE_LOG(LogLandscapeCombinator, Log, TEXT("Starting %d tasks asynchronously"), NumberOfTasks);

		TFunction<void(bool)> OnCompleteAction = [FinishedTasks, SuccessfulTasks, NumberOfTasks, OnComplete](bool bWasSuccessful) {
			(*FinishedTasks)++;
			if (bWasSuccessful) (*SuccessfulTasks)++;

			if (*FinishedTasks == NumberOfTasks) {
				if (OnComplete) OnComplete(*SuccessfulTasks == NumberOfTasks);
			}
		};

		for (const FString& Element : Elements) {
			Async(EAsyncExecution::Thread,
				[Element, Action, OnCompleteAction]() {
					Action(Element, OnCompleteAction);
				}
			);
		}

		return;
	}

	void RunOne(TFunction<bool(void)> Action, TFunction<void(bool)> OnComplete)
	{
		TFuture<bool> bSuccessFuture = Async(EAsyncExecution::Thread, Action);
		bSuccessFuture.Next(OnComplete);
	}

}