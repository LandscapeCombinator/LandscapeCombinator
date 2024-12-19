// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LogConcurrencyHelpers.h"

#include "Async/Async.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FConcurrencyHelpersModule"

using namespace ConcurrencyOperators;

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

void Concurrency::RunOnGameThread(TFunction<void()> Action)
{
	if (IsInGameThread())
	{
		Action();
		return;
	}
	
	AsyncTask(ENamedThreads::GameThread, [Action]() { Action(); });
}

void Concurrency::RunOnGameThreadAndWait(TFunction<void()> Action)
{
	if (IsInGameThread())
	{
		Action();
		return;
	}

	FEvent* SyncEvent = FPlatformProcess::GetSynchEventFromPool(false);
	if (!SyncEvent)
	{
		UE_LOG(LogConcurrencyHelpers, Error, TEXT("Failed to create sync event for RunOnGameThreadAndWait."));
		return;
	}

	AsyncTask(ENamedThreads::GameThread, [Action = MoveTemp(Action), SyncEvent]()
	{
		Action();
		SyncEvent->Trigger();
	});

	SyncEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(SyncEvent);
}

TFunction<void(TFunction<void(bool)>)> ConcurrencyOperators::operator >> (TFunction<void(TFunction<void(bool)>)> Action1, TFunction<void(TFunction<void(bool)>)> Action2)
{
	return [Action1, Action2](TFunction<void(bool)> OnComplete)
	{
		Action1([OnComplete, Action2](bool bSuccess)
		{
			if (bSuccess) Action2(OnComplete);
			else UE_LOG(LogConcurrencyHelpers, Error, TEXT("Something went wrong in the concurrency callbacks, pleqse check for previous errors0"));
		});
	};
}

TFunction<void(TFunction<void(bool)>)> Concurrency::Return(TFunction<void()> Action)
{
	return [Action](TFunction<void(bool)> OnComplete)
	{
		Action();
		if (OnComplete) OnComplete(true);
	};
}

TFunction<void(TFunction<void(bool)>)> Concurrency::I(TFunction<void(TFunction<void(bool)>)> Action)
{
	return [Action](TFunction<void(bool)> OnComplete) { Action(OnComplete); };
}

#undef LOCTEXT_NAMESPACE
