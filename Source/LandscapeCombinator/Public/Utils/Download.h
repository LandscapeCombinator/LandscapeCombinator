// Copyright LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Console.h"

namespace Download {
	TMap<FString, int> ExpectedSizeCache; 
	
	static bool SynchronousFromURLExpecting(FString URL, FString File, int32 ExpectedSize);
	static bool SynchronousFromURL(FString URL, FString File);

	static void FromURLExpecting(FString URL, FString File, int32 ExpectedSize, TFunction<void(bool)> OnComplete);
	static void FromURL(FString URL, FString File, TFunction<void(bool)> OnComplete);


	void AddExpectedSize(FString URL, int32 ExpectedSize);
	FString ExpectedSizeCacheFile();
	void LoadExpectedSizeCache();
	void SaveExpectedSizeCache();
};
