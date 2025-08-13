// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"
#include "ConcurrencyHelpers/LogConcurrencyHelpers.h"

#include "Async/Async.h"
#include "Misc/MessageDialog.h"
#include "Misc/EngineVersionComparison.h"

#define LOCTEXT_NAMESPACE "FConcurrencyHelpersModule"

void Concurrency::RunAsync(TFunction<void()> Action)
{
	Async(EAsyncExecution::Thread, [Action]() { Action(); });
	return;
}

void Concurrency::RunMany(int n, TFunction<void( int i, TFunction<void(bool)> )> Action, TFunction<void(bool)> OnComplete)
{
	TArray<int> Elements;
	Elements.Reserve(n);
	for (int i = 0; i < n; i++)
	{
		Elements.Add(i);
	}
	RunMany(Elements, Action, OnComplete);
}

bool Concurrency::RunManyAndWait(bool bEnableParallelDownload, int n, TFunction<bool( int i )> Action)
{
	if (bEnableParallelDownload)
	{
		TArray<int> Elements;
		Elements.Reserve(n);
		for (int i = 0; i < n; i++)
		{
			Elements.Add(i);
		}
		return RunManyAndWait(Elements, Action);
	}
	else
	{
		for (int i = 0; i < n; i++)
		{
			if (!Action(i)) return false;
		}
		return true;
	}
}

void Concurrency::RunOnGameThread(TFunction<void()> Action)
{
	if (IsInGameThread())
	{
		Action();
		return;
	}
	
	AsyncTask(ENamedThreads::GameThread, [Action]() { Action(); });
}

bool Concurrency::RunOnThreadAndWait(bool bRunOnGameThread, TFunction<bool()> Action)
{
	if (IsInGameThread())
	{
		if (bRunOnGameThread) return Action();
		else
		{
			LCReporter::ShowError(
				LOCTEXT("RunOnThreadAndWaitGameThread", "Internal error: Concurrency::RunOnThreadAndWait(false, _) cannot not be called from the game thread.")
			);
			return false;
		}
	}

	FEvent* SyncEvent = FPlatformProcess::GetSynchEventFromPool(false);
	if (!SyncEvent)
	{
		LCReporter::ShowError(
			LOCTEXT("RunOnThreadAndWaitEvent", "Failed to create sync event for RunOnGameThreadAndWait.")
		);
		return false;
	}

	bool bSuccess = false;
	
	if (bRunOnGameThread)
	{

#if UE_VERSION_OLDER_THAN(5, 6, 0)
		Async(EAsyncExecution::TaskGraphMainThread, [Action = MoveTemp(Action), &bSuccess, SyncEvent]()
#else
		// AsyncTask(ENamedThreads::GameThread, [Action = MoveTemp(Action), &bSuccess, SyncEvent]()
		Async(EAsyncExecution::TaskGraphMainTick, [Action = MoveTemp(Action), &bSuccess, SyncEvent]()
#endif
		{
			bSuccess = Action();
			SyncEvent->Trigger();
		});
	}
	else
	{
		Async(EAsyncExecution::Thread, [Action = MoveTemp(Action), &bSuccess, SyncEvent]()
		{
			bSuccess = Action();
			SyncEvent->Trigger();
		});
	}

	SyncEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(SyncEvent);
	return bSuccess;
}

bool Concurrency::RunOnGameThreadAndWait(TFunction<bool()> Action)
{
	if (IsInGameThread())
	{
		return Action();
	}

	return RunOnThreadAndWait(true, MoveTemp(Action));
}

bool Concurrency::RunAsyncAndWait(TFunction<bool()> Action)
{
	return RunOnThreadAndWait(false, MoveTemp(Action));
}

#undef LOCTEXT_NAMESPACE
