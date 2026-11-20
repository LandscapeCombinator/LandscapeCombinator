// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SWidget;
class UWorld;

class LANDSCAPECOMBINATOR_API FGeneratorsStatusOverlayManager
{
public:
	static TSharedRef<SWidget> CreateOverlayWidget(UWorld* World);

	static void ShowInGameViewport(UWorld* World);
	static void HideFromGameViewport();
	static void ToggleInGameViewport(UWorld* World);

	static bool IsVisibleInGameViewport() { return GameViewportOverlay.IsValid(); }

private:
	static TWeakPtr<SWidget> GameViewportOverlay;
};
