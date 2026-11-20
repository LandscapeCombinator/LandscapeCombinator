// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "Coordinates/LCGameMode.h"
#include "Coordinates/LCPawn.h"

ALCGameMode::ALCGameMode(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DefaultPawnClass = ALCPawn::StaticClass();
}
