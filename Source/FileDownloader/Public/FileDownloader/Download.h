// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FILEDOWNLOADER_API Download {
public:
	
	static bool SynchronousFromURLExpecting(FString URL, FString File, int32 ExpectedSize);
	static bool SynchronousFromURL(FString URL, FString File);

	static void FromURLExpecting(FString URL, FString File, bool bProgress, int64 ExpectedSize, TFunction<void(bool)> OnComplete);
	static void FromURL(FString URL, FString File, bool bProgress, TFunction<void(bool)> OnComplete);

	static void DownloadMany(TArray<FString> URLs, TArray<FString> Files, TFunction<void(TArray<FString>)> OnComplete);
	static void DownloadMany(TArray<FString> URLs, FString Directory, TFunction<void(TArray<FString>)> OnComplete);

	static void AddExpectedSize(FString URL, int32 ExpectedSize);
	static FString ExpectedSizeCacheFile();
	static void LoadExpectedSizeCache();
	static void SaveExpectedSizeCache();
};
