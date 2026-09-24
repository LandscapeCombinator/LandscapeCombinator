// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
class SLevelViewport;
class UUserWidget;
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

	void ToggleGeneratorStatusOverlay();
	void ShowGeneratorStatusOverlay();
	void HideGeneratorStatusOverlay();

	void OnPIEStateChanged(bool bIsSimulating);

	TSharedPtr<class FUICommandList> PluginCommands;

	TWeakObjectPtr<UUserWidget> GeneratorStatusOverlayInstance;
	TWeakPtr<SLevelViewport> GeneratorStatusOverlayViewport;
	bool bGeneratorStatusOverlayVisible = false;

	FDelegateHandle BeginPIEHandle;
	FDelegateHandle EndPIEHandle;

    FDelegateHandle ModifyCookDelegateHandle;
    void GetPackagesToAlwaysCook(TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& OutPackagesToCook, TArray<FName>& OutPackagesToNeverCook);

#endif

};