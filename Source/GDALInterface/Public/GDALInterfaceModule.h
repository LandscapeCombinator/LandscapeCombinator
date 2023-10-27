// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FGDALInterfaceModule"

class FGDALInterfaceModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
};

#undef LOCTEXT_NAMESPACE