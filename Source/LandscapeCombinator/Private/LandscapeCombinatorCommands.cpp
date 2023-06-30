// Copyright 2023 LandscapeCombinator. All Rights Reserved.

// File originally written from the Unreal Engine template for plugins

#include "LandscapeCombinatorCommands.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void FLandscapeCombinatorCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "LandscapeCombinator", "Landscape Combinator", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
