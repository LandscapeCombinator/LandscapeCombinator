// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#define TIME(L, C) (Time::Time<int>(FString(L), [&]() { C; return 0; }))

namespace Time
{
	TMap<FString, double> TimeSpent;

	template<typename T>
	T Time(FString Label, TFunction<T()> Code);

	void DumpTable();
}

