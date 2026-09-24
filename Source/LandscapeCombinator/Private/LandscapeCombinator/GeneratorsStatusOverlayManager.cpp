// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/GeneratorsStatusOverlayManager.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"

const TCHAR* FGeneratorsStatusOverlayManager::OverlayClassPath =
	TEXT("/LandscapeCombinator/UI/W_GeneratorsStatus.W_GeneratorsStatus_C");

TWeakObjectPtr<UUserWidget> FGeneratorsStatusOverlayManager::GameViewportOverlayInstance;

UUserWidget* FGeneratorsStatusOverlayManager::CreateOverlayWidget(UWorld* World)
{
	if (!IsValid(World)) return nullptr;

	UClass* OverlayClass = LoadClass<UUserWidget>(nullptr, OverlayClassPath);
	if (!IsValid(OverlayClass)) return nullptr;

	return CreateWidget<UUserWidget>(World, OverlayClass);
}

void FGeneratorsStatusOverlayManager::ShowInGameViewport(UWorld* World)
{
	if (IsVisibleInGameViewport()) return;

	UUserWidget* Widget = CreateOverlayWidget(World);
	if (!IsValid(Widget)) return;

	Widget->AddToViewport();
	GameViewportOverlayInstance = Widget;
}

void FGeneratorsStatusOverlayManager::HideFromGameViewport()
{
	if (UUserWidget* Widget = GameViewportOverlayInstance.Get())
		Widget->RemoveFromParent();

	GameViewportOverlayInstance.Reset();
}

void FGeneratorsStatusOverlayManager::ToggleInGameViewport(UWorld* World)
{
	IsVisibleInGameViewport() ? HideFromGameViewport() : ShowInGameViewport(World);
}

static FAutoConsoleCommandWithWorld CVarOverlay(
	TEXT("LC.Overlay"),
	TEXT("Toggles the Landscape Combinator generators status overlay."),
	FConsoleCommandWithWorldDelegate::CreateStatic(
		[](UWorld* World)
		{
			FGeneratorsStatusOverlayManager::ToggleInGameViewport(World);
		}
	)
);
