// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "LCReporter.h"
#include "Templates/Function.h"
#include "Async/Async.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

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
				bool bSuccess = *SuccessfulTasks == NumberOfTasks;
				delete SuccessfulTasks;
				delete FinishedTasks;
				if (OnComplete) OnComplete(bSuccess);
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
		std::atomic<int32> *SuccessfulTasks = new std::atomic<int>(0);
		std::atomic<int32> *FinishedTasks = new std::atomic<int>(0);
		UE_LOG(LogTemp, Log, TEXT("Starting %d tasks in parallel"), NumberOfTasks);

		OutResults.Empty(NumberOfTasks);
		OutResults.SetNum(NumberOfTasks);

		for (int i = 0; i < NumberOfTasks; i++)
		{
			Async(EAsyncExecution::Thread,
				[&Function, FinishedTasks, SuccessfulTasks, NumberOfTasks, &Elements, &OutResults, i]() {

					if (Function(Elements[i], OutResults[i])) (*SuccessfulTasks)++;
					(*FinishedTasks)++;
				}
			);
		}

		UE_LOG(LogTemp, Log, TEXT("Actively waiting for all %d tasks to complete"), NumberOfTasks);
		while (*FinishedTasks < NumberOfTasks) FPlatformProcess::Sleep(0.05);
		UE_LOG(LogTemp, Log, TEXT("Finished waiting"));

		bool bSuccess = *SuccessfulTasks == *FinishedTasks;
		delete SuccessfulTasks;
		delete FinishedTasks;

		return bSuccess;
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
	static bool RunManyAndWait(int n, TFunction<bool( int i )> Action);
	static void RunOnGameThread(TFunction<void()> Action);

	static bool RunOnThreadAndWait(bool bRunOnGameThread, TFunction<bool()> Action);
	static bool RunOnGameThreadAndWait(TFunction<bool()> Action);
	static bool RunAsyncAndWait(TFunction<bool()> Action);
	
	static TFunction<void(TFunction<void(bool)>)> Return(TFunction<void()> Action);

	// to help the type-checker
	static TFunction<void(TFunction<void(bool)>)> I(TFunction<void(TFunction<void(bool)>)> Action);
};

#undef LOCTEXT_NAMESPACE
