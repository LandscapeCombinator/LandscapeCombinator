// Copyright LandscapeCombinator. All Rights Reserved.

#pragma once

namespace Concurrency {
	void RunAsync(TFunction<void()> Action);
	void RunMany(TArray<FString> Elements, TFunction<void( FString Element, TFunction<void(bool)> )> Action, TFunction<void(bool)> OnComplete);
	void RunOne(TFunction<bool(void)> Action, TFunction<void(bool)> OnComplete);
};
