// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
class SLevelViewport;
class SWidget;
class SDockTab;
class FSpawnTabArgs;
#endif

class FLandscapeCombinatorModule : public IModuleInterface
{

public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

#if WITH_EDITOR

	void PluginButtonClicked();

private:

	void RegisterMenus();

	void ShowGeneratorStatusOverlay();
	void HideGeneratorStatusOverlay();

	void OpenGeneratorsTab();
	TSharedRef<SDockTab> SpawnGeneratorsTab(const FSpawnTabArgs& Args);

	TSharedPtr<class FUICommandList> PluginCommands;

	TSharedPtr<SWidget> GeneratorStatusOverlay;
	TWeakPtr<SLevelViewport> GeneratorStatusOverlayViewport;

	FDelegateHandle ModifyCookDelegateHandle;
	void GetPackagesToAlwaysCook(TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& OutPackagesToCook, TArray<FName>& OutPackagesToNeverCook);

#endif

};