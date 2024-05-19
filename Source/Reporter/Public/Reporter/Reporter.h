// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LogReporter.h"

class REPORTER_API Reporter
{
public:
	inline static int DialogsCount = 0;
	inline static bool bSilentMode = false;

	static void Reset();

	static void ErrorDialog();
	static void InfoDialog();
};
