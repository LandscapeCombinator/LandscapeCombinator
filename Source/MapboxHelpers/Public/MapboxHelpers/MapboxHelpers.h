// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class MAPBOXHELPERS_API MapboxHelpers
{
public:
	static bool DecodeMapboxOneBand(FString InputFile, FString OutputFile, bool bUseTerrariumFormula, bool *bShowedDialog);
	static bool DecodeMapboxThreeBands(FString InputFile, FString OutputFile, bool bUseTerrariumFormula, bool *bShowedDialog);
};
