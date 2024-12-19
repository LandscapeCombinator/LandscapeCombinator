// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FLandscapeCombinatorModule : public IModuleInterface
{

#if WITH_EDITOR

public:

#if WITH_EDITOR

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	

	void PluginButtonClicked();

#endif
	
private:

#if WITH_EDITOR

	void RegisterMenus();

#endif
	
	TSharedPtr<class FUICommandList> PluginCommands;

#endif

};
