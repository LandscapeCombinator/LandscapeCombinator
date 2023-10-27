// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class CONSOLEHELPERS_API Console
{
public:
	static bool ExecProcess(const TCHAR* URL, const TCHAR* Params, bool Debug = true);
};
