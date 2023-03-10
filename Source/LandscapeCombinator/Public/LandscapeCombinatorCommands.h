// Copyright Epic Games, Inc. All Rights Reserved.

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