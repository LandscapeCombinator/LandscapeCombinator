// Copyright 2023 LandscapeCombinator. All Rights Reserved.

// File originally written from the Unreal Engine template for plugins

#pragma once

#include "CoreMinimal.h"
#include "Elevation/HMInterface.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class FToolBarBuilder;
class FMenuBuilder;

class FLandscapeCombinatorModule : public IModuleInterface
{
public:

	static const FName LandscapeCombinatorTabName;

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** This function will be bound to Command (by default it will bring up plugin window) */
	void PluginButtonClicked();
	
private:
	void RegisterMenus();
	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

private:
	TSharedPtr<class FUICommandList> PluginCommands;
	FReply LoadLandscapes();
};

#undef LOCTEXT_NAMESPACE