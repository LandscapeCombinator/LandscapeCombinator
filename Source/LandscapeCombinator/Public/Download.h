#pragma once

#include "Console.h"

class Download {
public:
	static void FromURLExpecting(FString URL, FString File, int32 ExpectedSize, TFunction<void()> OnComplete = nullptr);
	static void FromURL(FString URL, FString File, TFunction<void()> OnComplete = nullptr);
};
