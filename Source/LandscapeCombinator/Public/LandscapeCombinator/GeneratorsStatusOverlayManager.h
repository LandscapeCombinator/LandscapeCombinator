// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

class LANDSCAPECOMBINATOR_API FGeneratorsStatusOverlayManager
{
public:
	// Shared by both editor and runtime attach paths.
	static UUserWidget* CreateOverlayWidget(UWorld* World);

	// Runtime path
	static void ShowInGameViewport(UWorld* World);
	static void HideFromGameViewport();
	static void ToggleInGameViewport(UWorld* World);

	static bool IsVisibleInGameViewport() { return GameViewportOverlayInstance.IsValid(); }

private:
	static const TCHAR* OverlayClassPath;
	static TWeakObjectPtr<UUserWidget> GameViewportOverlayInstance;
};
