// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FBuildingFromSplineModule"

class FToolBarBuilder;
class FMenuBuilder;

class FBuildingFromSplineModule : public IModuleInterface
{
public:
	static const FName BuildingFromSplineTabName;

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

#undef LOCTEXT_NAMESPACE