// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

namespace Concurrency {
	void RunAsync(TFunction<void()> Action);

	template<typename T>
	void RunMany(TArray<T> Elements, TFunction<void( T Element, TFunction<void(bool)> )> Action, TFunction<void(bool)> OnComplete);

	void RunMany(int n, TFunction<void( int i, TFunction<void(bool)> )> Action, TFunction<void(bool)> OnComplete);
	void RunOne(TFunction<bool(void)> Action, TFunction<void(bool)> OnComplete);
};
