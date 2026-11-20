// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/GeneratorsStatusOverlayManager.h"
#include "LandscapeCombinator/SGeneratorsStatus.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"

TWeakPtr<SWidget> FGeneratorsStatusOverlayManager::GameViewportOverlay;

TSharedRef<SWidget> FGeneratorsStatusOverlayManager::CreateOverlayWidget(UWorld* World)
{
	return SNew(SBox)
	.Visibility(EVisibility::SelfHitTestInvisible)
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Top)
	.Padding(16.f)
	[
		SNew(SDPIScaler)
		.Visibility(EVisibility::SelfHitTestInvisible)
		.DPIScale_Lambda([]() { return SGeneratorsStatus::GetUIScale(); })
		[
			SNew(SBox)
			.Visibility(EVisibility::Visible)
			.MinDesiredWidth(280.f)
			.MaxDesiredHeight(500.f)
			[
				SNew(SGeneratorsStatus)
				.World(World)
			]
		]
	];
}

void FGeneratorsStatusOverlayManager::ShowInGameViewport(UWorld* World)
{
	if (IsVisibleInGameViewport()) return;
	if (!GEngine || !GEngine->GameViewport) return;

	TSharedRef<SWidget> Overlay = CreateOverlayWidget(World);
	GEngine->GameViewport->AddViewportWidgetContent(Overlay);
	GameViewportOverlay = Overlay;
}

void FGeneratorsStatusOverlayManager::HideFromGameViewport()
{
	TSharedPtr<SWidget> Overlay = GameViewportOverlay.Pin();
	if (Overlay.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	}
	GameViewportOverlay.Reset();
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
