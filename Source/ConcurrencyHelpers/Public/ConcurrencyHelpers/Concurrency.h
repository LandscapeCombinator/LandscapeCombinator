// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LogConcurrencyHelpers.h"

class CONCURRENCYHELPERS_API Concurrency
{
public:
	template<typename T>
	static void RunMany(TArray<T> Elements, TFunction<void( T Element, TFunction<void(bool)> )> Action, TFunction<void(bool)> OnComplete)
	{
		int32 NumberOfTasks = Elements.Num();
		std::atomic<int32> *SuccessfulTasks = new std::atomic<int>(0);
		std::atomic<int32> *FinishedTasks = new std::atomic<int>(0);
		UE_LOG(LogTemp, Log, TEXT("Starting %d tasks asynchronously"), NumberOfTasks);

		TFunction<void(bool)> OnCompleteAction = [FinishedTasks, SuccessfulTasks, NumberOfTasks, OnComplete](bool bWasSuccessful) {
			(*FinishedTasks)++;
			if (bWasSuccessful) (*SuccessfulTasks)++;

			if (*FinishedTasks == NumberOfTasks) {
				if (OnComplete) OnComplete(*SuccessfulTasks == NumberOfTasks);
			}
		};

		for (const T& Element : Elements) {
			Async(EAsyncExecution::Thread,
				[Element, Action, OnCompleteAction]() {
					Action(Element, OnCompleteAction);
				}
			);
		}

		return;
	}

	static void RunAsync(TFunction<void()> Action);
	static void RunMany(int n, TFunction<void( int i, TFunction<void(bool)> )> Action, TFunction<void(bool)> OnComplete);
	static void RunOne(TFunction<bool(void)> Action, TFunction<void(bool)> OnComplete);
};
