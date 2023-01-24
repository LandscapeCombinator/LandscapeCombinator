#pragma once

#include "Console.h"

class Download {
public:
	static void FromURLExpecting(FString URL, FString File, int32 ExpectedSize, TFunction<void(bool)> OnComplete);
	static void FromURL(FString URL, FString File, TFunction<void(bool)> OnComplete);
};
