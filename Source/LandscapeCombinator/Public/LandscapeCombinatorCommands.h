// Copyright 2023 LandscapeCombinator. All Rights Reserved.

// File originally written from the Unreal Engine template for plugins

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "LandscapeCombinatorStyle.h"

class FLandscapeCombinatorCommands : public TCommands<FLandscapeCombinatorCommands>
{
public:

	FLandscapeCombinatorCommands()
		: TCommands<FLandscapeCombinatorCommands>(TEXT("LandscapeCombinator"), NSLOCTEXT("Contexts", "LandscapeCombinator", "LandscapeCombinator Plugin"), NAME_None, FLandscapeCombinatorStyle::GetStyleSetName())
	{
	}

	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};