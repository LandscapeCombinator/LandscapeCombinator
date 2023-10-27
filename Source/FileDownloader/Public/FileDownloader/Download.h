// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FILEDOWNLOADER_API Download {
public:
	
	static bool SynchronousFromURLExpecting(FString URL, FString File, int32 ExpectedSize);
	static bool SynchronousFromURL(FString URL, FString File);

	static void FromURLExpecting(FString URL, FString File, int32 ExpectedSize, TFunction<void(bool)> OnComplete);
	static void FromURL(FString URL, FString File, TFunction<void(bool)> OnComplete);

	static void AddExpectedSize(FString URL, int32 ExpectedSize);
	static FString ExpectedSizeCacheFile();
	static void LoadExpectedSizeCache();
	static void SaveExpectedSizeCache();
};
