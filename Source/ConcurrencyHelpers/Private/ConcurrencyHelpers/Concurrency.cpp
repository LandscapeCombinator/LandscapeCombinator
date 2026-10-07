// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"
#include "ConcurrencyHelpers/LogConcurrencyHelpers.h"
#include "ConcurrencyHelpers/ConcurrencySettings.h"

#include "Async/Async.h"
#include "Misc/MessageDialog.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/ScopeLock.h"
#include "Misc/CoreDelegates.h"
#include "Containers/Ticker.h"
#include "Containers/Queue.h"
#include "HAL/PlatformTime.h"

#define LOCTEXT_NAMESPACE "FConcurrencyHelpersModule"

namespace
{
	TQueue<TFunction<void()>, EQueueMode::Mpsc> GThrottledActions;
	FTSTicker::FDelegateHandle GThrottleTickerHandle;

	bool ProcessThrottledActions(float DeltaTime)
	{
		if (IsEngineExitRequested()) return false;

		const double BudgetSeconds = GetDefault<UConcurrencySettings>()->GameThreadWorkBudgetPerTickSeconds;
		const double StartTime = FPlatformTime::Seconds();

		int32 NumRun = 0;
		TFunction<void()> Action;
		while (GThrottledActions.Dequeue(Action))
		{
			Action();
			NumRun++;
			if (FPlatformTime::Seconds() - StartTime >= BudgetSeconds) break;
		}

		if (NumRun > 0)
		{
			UE_LOG(LogConcurrencyHelpers, Verbose,
				TEXT("RunOnGameThreadThrottled: Ran %d task(s) in %fs (budget %fs)"),
				NumRun, FPlatformTime::Seconds() - StartTime, BudgetSeconds);
		}

		return true;
	}

	TFunction<void()> MakeExitSafe(TFunction<void()> Action)
	{
		return [Action = MoveTemp(Action)]()
		{
			if (IsEngineExitRequested()) return;
			Action();
		};
	}

	TFunction<bool()> MakeExitSafeBool(TFunction<bool()> Action)
	{
		return [Action = MoveTemp(Action)]() -> bool
		{
			if (IsEngineExitRequested()) return false;
			return Action();
		};
	}
}

bool Concurrency::WaitForEvent(FEvent* SyncEvent)
{
	while (!SyncEvent->Wait(100))
		if (IsEngineExitRequested()) return false;
	return true;
}

void Concurrency::RunAsync(TFunction<void()> Action)
{
	Async(EAsyncExecution::Thread, MakeExitSafe(MoveTemp(Action)));
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

	if (IsEngineExitRequested()) return;

	#if UE_VERSION_OLDER_THAN(5, 6, 0)
		Async(EAsyncExecution::TaskGraphMainThread, MakeExitSafe(MoveTemp(Action)));
	#else
		Async(EAsyncExecution::TaskGraphMainTick, MakeExitSafe(MoveTemp(Action)));
	#endif
}

void Concurrency::RunOnGameThreadThrottled(TFunction<void()> Action)
{
	if (IsEngineExitRequested()) return;

	{
		static FCriticalSection ThrottleInitLock;
		FScopeLock Lock(&ThrottleInitLock);

		if (!GThrottleTickerHandle.IsValid())
		{
			GThrottleTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateStatic(&ProcessThrottledActions), 0.0f);

			FCoreDelegates::OnEnginePreExit.AddLambda([]()
			{
				FScopeLock ExitLock(&ThrottleInitLock);
				FTSTicker::GetCoreTicker().RemoveTicker(GThrottleTickerHandle);
				GThrottleTickerHandle.Reset();
				GThrottledActions.Empty();
			});
		}
	}

	GThrottledActions.Enqueue(MakeExitSafe(MoveTemp(Action)));
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

	if (IsEngineExitRequested()) return false;
	if (IsCancelRequested()) return false;

	FEvent* SyncEvent = FPlatformProcess::GetSynchEventFromPool(false);
	if (!SyncEvent)
	{
		LCReporter::ShowError(
			LOCTEXT("RunOnThreadAndWaitEvent", "Failed to create sync event for RunOnGameThreadAndWait.")
		);
		return false;
	}

	TSharedRef<bool, ESPMode::ThreadSafe> bSuccess = MakeShared<bool, ESPMode::ThreadSafe>(false);
	TFunction<bool()> SafeAction = MakeExitSafeBool(MoveTemp(Action));

	if (bRunOnGameThread)
	{

#if UE_VERSION_OLDER_THAN(5, 6, 0)
		Async(EAsyncExecution::TaskGraphMainThread, [SafeAction = MoveTemp(SafeAction), bSuccess, SyncEvent]()
#else
		Async(EAsyncExecution::TaskGraphMainTick, [SafeAction = MoveTemp(SafeAction), bSuccess, SyncEvent]()
#endif
		{
			*bSuccess = SafeAction();
			SyncEvent->Trigger();
		});
	}
	else
	{
		Async(EAsyncExecution::Thread, [SafeAction = MoveTemp(SafeAction), bSuccess, SyncEvent]()
		{
			*bSuccess = SafeAction();
			SyncEvent->Trigger();
		});
	}

	bool bWaited = WaitForEvent(SyncEvent);
	FPlatformProcess::ReturnSynchEventToPool(SyncEvent);
    if (!bWaited) return false;
    return *bSuccess;
}

bool Concurrency::RunOnGameThreadThrottledAndWait(TFunction<bool()> Action)
{
	if (IsInGameThread()) return Action();
	if (IsEngineExitRequested()) return false;
	if (IsCancelRequested()) return false;

	FEvent* SyncEvent = FPlatformProcess::GetSynchEventFromPool(false);
	if (!SyncEvent)
	{
		LCReporter::ShowError(
			LOCTEXT("RunOnGameThreadThrottledAndWaitEvent", "Failed to create sync event for RunOnGameThreadThrottledAndWait.")
		);
		return false;
	}

	TSharedRef<bool, ESPMode::ThreadSafe> bSuccess = MakeShared<bool, ESPMode::ThreadSafe>(false);
	TFunction<bool()> SafeAction = MakeExitSafeBool(MoveTemp(Action));

	RunOnGameThreadThrottled([SafeAction = MoveTemp(SafeAction), bSuccess, SyncEvent]()
	{
		*bSuccess = SafeAction();
		SyncEvent->Trigger();
	});

	bool bWaited = WaitForEvent(SyncEvent);
	FPlatformProcess::ReturnSynchEventToPool(SyncEvent);
    if (!bWaited) return false;
    return *bSuccess;
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
