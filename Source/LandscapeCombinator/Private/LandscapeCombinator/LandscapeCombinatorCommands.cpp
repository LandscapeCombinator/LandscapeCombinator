// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeCombinatorCommands.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void FLandscapeCombinatorCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "LandscapeCombinator", "Open LandscapeCombinator Plugin", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
