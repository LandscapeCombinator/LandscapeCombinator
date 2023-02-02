#pragma once

#include "Console.h"

namespace Download {
	static void FromURLExpecting(FString URL, FString File, int32 ExpectedSize, TFunction<void(bool)> OnComplete);
	static void FromURL(FString URL, FString File, TFunction<void(bool)> OnComplete);
};
