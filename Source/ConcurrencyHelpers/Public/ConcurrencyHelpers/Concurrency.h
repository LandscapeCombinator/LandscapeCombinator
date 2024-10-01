// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Templates/Function.h"
#include "LogConcurrencyHelpers.h"

#include "Async/Async.h"

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
	static void RunSuccessively(TArray<T> Elements, TFunction<void(T Element, TFunction<void(bool)> OnCompleteOne)> Action, TFunction<void(bool)> OnCompleteAll)
	{
		TFunction<void(int)> Loop = [Action, Elements, OnCompleteAll, Loop](int32 Index)
		{
			if (Index >= Elements.Num())
			{
				if (OnCompleteAll) OnCompleteAll(true);
				return;
			}

			Action(Elements[Index], [Loop, OnCompleteAll, Index](bool bSuccess)
			{
				if (!bSuccess)
				{
					if (OnCompleteAll) OnCompleteAll(false);
					return;
				}

				Loop(Index + 1);
			});
		};

		Loop(0);
	}

	static void RunAsync(TFunction<void()> Action);
	static void RunMany(int n, TFunction<void( int i, TFunction<void(bool)> )> Action, TFunction<void(bool)> OnComplete);
	static void RunOne(TFunction<bool(void)> Action, TFunction<void(bool)> OnComplete);
	
	static TFunction<void(TFunction<void(bool)>)> Combine(TFunction<void(TFunction<void(bool)>)> Action1, TFunction<void(TFunction<void(bool)>)> Action2);
	static TFunction<void(TFunction<void(bool)>)> Pure(TFunction<void()> Action);
	static TFunction<void(TFunction<void(bool)>)> CombineLeft(TFunction<void(TFunction<void(bool)>)> Action1, TFunction<void()> Action2);
	static TFunction<void(TFunction<void(bool)>)> CombineRight(TFunction<void()> Action1, TFunction<void(TFunction<void(bool)>)> Action2);

	static void ShowDialog(const FText& Message, bool* bShowedDialog);
};
