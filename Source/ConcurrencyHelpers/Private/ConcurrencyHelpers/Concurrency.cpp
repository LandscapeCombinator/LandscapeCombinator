// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LogConcurrencyHelpers.h"

#include "Async/Async.h"

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

void Concurrency::RunOne(TFunction<bool(void)> Action, TFunction<void(bool)> OnComplete)
{
	TFuture<bool> bSuccessFuture = Async(EAsyncExecution::Thread, Action);
	bSuccessFuture.Next(OnComplete);
}


#undef LOCTEXT_NAMESPACE