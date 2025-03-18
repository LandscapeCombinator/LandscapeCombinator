// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FBuildingFromSplineModule : public IModuleInterface
{

#if WITH_EDITOR
	void StartupModule() override;
#endif

};
