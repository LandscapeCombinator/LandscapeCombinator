// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "LCReporter.h"
#include "Templates/Function.h"
#include "Async/Async.h"
#include <atomic>

#define LOCTEXT_NAMESPACE "FConcurrencyHelpersModule"

class CONCURRENCYHELPERS_API Concurrency
{
public:

	static bool WaitForEvent(FEvent* SyncEvent);

	template<typename T>
	static void RunMany(TArray<T> Elements, TFunction<void( T Element, TFunction<void(bool)> )> Action, TFunction<void(bool)> OnComplete)
	{
		int32 NumberOfTasks = Elements.Num();
		if (NumberOfTasks == 0)
		{
			if (OnComplete) OnComplete(true);
			return;
		}

		std::atomic<int32> *SuccessfulTasks = new std::atomic<int>(0);
		std::atomic<int32> *FinishedTasks = new std::atomic<int>(0);
		UE_LOG(LogTemp, Log, TEXT("Starting %d tasks asynchronously"), NumberOfTasks);

		TFunction<void(bool)> OnCompleteAction = [FinishedTasks, SuccessfulTasks, NumberOfTasks, OnComplete](bool bWasSuccessful) {
			if (bWasSuccessful) (*SuccessfulTasks)++;
			int FinishedTasksLocal = ++(*FinishedTasks);

			if (FinishedTasksLocal == NumberOfTasks)
			{
				bool bSuccess = *SuccessfulTasks == NumberOfTasks;
				delete SuccessfulTasks;
				delete FinishedTasks;
				if (OnComplete) OnComplete(bSuccess);
			}
		};

		for (const T& Element : Elements)
		{
			// async and not the safe run async, so that we continue with "OnCompleteAction"
			Async(EAsyncExecution::Thread, [Element, Action, OnCompleteAction]() {
				if (IsEngineExitRequested())
				{
					OnCompleteAction(false);
					return;
				}
				Action(Element, OnCompleteAction);
			});
		}

		return;
	}

	template<typename T>
	static bool RunManyAndWait(TArray<T> Elements, TFunction<bool( T Element )> Action)
	{
		TArray<int> UnusedResults;
		return RunArrayAndWait<T, int>(Elements, UnusedResults, [Action](T Element, int &UnusedResult) {
			return Action(Element);
		});
	}

	template<typename A, typename B>
	static bool RunArrayAndWait(TArray<A> Elements, TArray<B> &OutResults, TFunction<bool( A Element, B &OutResult )> Function)
	{
		if (IsInGameThread())
		{
			LCReporter::ShowError(
				LOCTEXT("Concurrency::RunArrayAndWait", "Blocking function Concurrency::RunArrayAndWait must be run on a background thread.")
			);
			return false;
		}

		int32 NumberOfTasks = Elements.Num();
		if (NumberOfTasks == 0) return true;

		TSharedRef<TArray<A>> SharedElements = MakeShared<TArray<A>>(MoveTemp(Elements));
		TSharedRef<TArray<B>> SharedResults  = MakeShared<TArray<B>>();
		SharedResults->SetNum(NumberOfTasks);

		TSharedRef<std::atomic<int32>> SuccessfulTasks = MakeShared<std::atomic<int32>>(0);
		TSharedRef<std::atomic<int32>> RemainingTasks  = MakeShared<std::atomic<int32>>(NumberOfTasks);

		FEvent* SyncEvent = FPlatformProcess::GetSynchEventFromPool(false);
		if (!SyncEvent)
		{
			LCReporter::ShowError(
				LOCTEXT("RunArrayAndWaitEvent", "Failed to create sync event for RunArrayAndWait.")
			);
			return false;
		}

		UE_LOG(LogTemp, Log, TEXT("Starting %d tasks in parallel"), NumberOfTasks);

		for (int i = 0; i < NumberOfTasks; i++)
		{
			Async(EAsyncExecution::Thread,
				[Function, SuccessfulTasks, RemainingTasks, SyncEvent, SharedElements, SharedResults, i]()
				{
					if (!IsEngineExitRequested() && Function((*SharedElements)[i], (*SharedResults)[i]))
						(*SuccessfulTasks)++;

					if (--(*RemainingTasks) == 0)
						SyncEvent->Trigger();
				}
			);
		}

		bool bWaited = WaitForEvent(SyncEvent);
		FPlatformProcess::ReturnSynchEventToPool(SyncEvent);
		if (!bWaited) return false;

		OutResults = *SharedResults;
		return *SuccessfulTasks == NumberOfTasks;
	}
	
	template<typename T>
	static void RunSuccessivelyFrom(TSharedRef<TArray<T>> Elements, int Index, TFunction<void(T Element, TFunction<void(bool)> OnCompleteOne)> Action, TFunction<void(bool)> OnCompleteAll)
	{
		if (Index >= Elements->Num())
		{
			if (OnCompleteAll) OnCompleteAll(true);
			return;
		}

		Action((*Elements)[Index], [Elements, OnCompleteAll, Index, Action](bool bSuccess)
		{
			if (!bSuccess) { if (OnCompleteAll) OnCompleteAll(false); return; }

			Concurrency::RunAsync([Elements, OnCompleteAll, Index, Action]()
			{
				RunSuccessivelyFrom(Elements, Index + 1, Action, OnCompleteAll);
			});
		});
	}

	template<typename T>
	static void RunSuccessively(const TArray<T> &Elements, TFunction<void(T Element, TFunction<void(bool)> OnCompleteOne)> Action, TFunction<void(bool)> OnCompleteAll)
	{
		RunSuccessivelyFrom(MakeShared<TArray<T>>(Elements), 0, Action, OnCompleteAll);
	}

	static void RunAsync(TFunction<void()> Action);
	static void RunMany(int n, TFunction<void( int i, TFunction<void(bool)> )> Action, TFunction<void(bool)> OnComplete);
	static bool RunManyAndWait(bool bEnableParallelDownload, int n, TFunction<bool( int i )> Action);
	static void RunOnGameThread(TFunction<void()> Action);
	static void RunOnGameThreadThrottled(TFunction<void()> Action);
	static bool RunOnGameThreadThrottledAndWait(TFunction<bool()> Action);

	static bool RunOnThreadAndWait(bool bRunOnGameThread, TFunction<bool()> Action);
	static bool RunOnGameThreadAndWait(TFunction<bool()> Action);
	static bool RunAsyncAndWait(TFunction<bool()> Action);
};

#undef LOCTEXT_NAMESPACE
