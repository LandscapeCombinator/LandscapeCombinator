#pragma once

class Concurrency {
public:
	static void RunAsync(TFunction<void()> Action);
	static void RunMany(TArray<FString> Elements, TFunction<void( FString Element, TFunction<void(bool)> )> Action, TFunction<void(bool)> OnComplete);
};
