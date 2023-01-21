// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeCombinatorCommands.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void FLandscapeCombinatorCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "LandscapeCombinator", "Landscape Combinator", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
