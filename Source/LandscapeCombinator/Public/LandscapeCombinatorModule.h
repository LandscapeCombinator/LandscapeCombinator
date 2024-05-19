// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FLandscapeCombinatorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	

	void PluginButtonClicked();
	
private:
	//TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

	void RegisterMenus();
    
	TSharedPtr<class FUICommandList> PluginCommands;
};
