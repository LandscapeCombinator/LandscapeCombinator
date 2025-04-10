// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Templates/Function.h"
#include "LogConcurrencyHelpers.h"

#include "Async/Async.h"

namespace ConcurrencyOperators
{
	CONCURRENCYHELPERS_API TFunction<void(TFunction<void(bool)>)> operator >> (TFunction<void(TFunction<void(bool)>)> Action1, TFunction<void(TFunction<void(bool)>)> Action2);
}

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
			int FinishedTasksLocal = ++(*FinishedTasks);
			if (bWasSuccessful) (*SuccessfulTasks)++;

			if (FinishedTasksLocal == NumberOfTasks)
			{
				if (OnComplete) OnComplete(*SuccessfulTasks == NumberOfTasks);
			}
		};

		for (const T& Element : Elements)
		{
			Async(EAsyncExecution::Thread,
				[Element, Action, OnCompleteAction]() {
					Action(Element, OnCompleteAction);
				}
			);
		}

		return;
	}

	
	template<typename T>
	static void RunSuccessivelyFrom(const TArray<T> &Elements, int Index, TFunction<void(T Element, TFunction<void(bool)> OnCompleteOne)> Action, TFunction<void(bool)> OnCompleteAll)
	{
		if (Index >= Elements.Num())
		{
			if (OnCompleteAll) OnCompleteAll(true);
			return;
		}

		Action(Elements[Index], [Elements, OnCompleteAll, Index, Action](bool bSuccess)
		{
			if (!bSuccess)
			{
				if (OnCompleteAll) OnCompleteAll(false);
				return;
			}

			RunSuccessivelyFrom(Elements, Index + 1, Action, OnCompleteAll);
		});
	}
	
	template<typename T>
	static void RunSuccessively(const TArray<T> &Elements, TFunction<void(T Element, TFunction<void(bool)> OnCompleteOne)> Action, TFunction<void(bool)> OnCompleteAll)
	{
		RunSuccessivelyFrom(Elements, 0, Action, OnCompleteAll);
	}

	static void RunAsync(TFunction<void()> Action);
	static void RunMany(int n, TFunction<void( int i, TFunction<void(bool)> )> Action, TFunction<void(bool)> OnComplete);
	static void RunOnGameThread(TFunction<void()> Action);
	static void RunOnGameThreadAndWait(TFunction<void()> Action);
	static bool RunOnGameThreadAndReturn(TFunction<bool()> Action);
	
	static TFunction<void(TFunction<void(bool)>)> Return(TFunction<void()> Action);

	// to help the type-checker
	static TFunction<void(TFunction<void(bool)>)> I(TFunction<void(TFunction<void(bool)>)> Action);
};
