// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class Console {
public:
	static bool ExecProcess(const TCHAR* URL, const TCHAR* Params, FString *StdOut, bool Debug = true);
	static bool Has7Z();
};
