// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "LandscapeCombinator/LandscapeCombinatorStyle.h"

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FLandscapeCombinatorCommands : public TCommands<FLandscapeCombinatorCommands>
{
public:

	FLandscapeCombinatorCommands()
		: TCommands<FLandscapeCombinatorCommands>(TEXT("LandscapeCombinator"), NSLOCTEXT("Contexts", "LandscapeCombinator", "LandscapeCombinator Plugin"), NAME_None, FLandscapeCombinatorStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};
