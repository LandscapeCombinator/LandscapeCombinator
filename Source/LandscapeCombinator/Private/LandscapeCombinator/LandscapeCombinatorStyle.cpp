// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeCombinatorStyle.h"
#include "LandscapeCombinatorModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateStyleRegistry.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FLandscapeCombinatorStyle::StyleInstance = nullptr;

void FLandscapeCombinatorStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FLandscapeCombinatorStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FLandscapeCombinatorStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("LandscapeCombinatorStyle"));
	return StyleSetName;
}


const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);

TSharedRef< FSlateStyleSet > FLandscapeCombinatorStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("LandscapeCombinatorStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("LandscapeCombinator")->GetBaseDir() / TEXT("Resources"));

	Style->Set("LandscapeCombinator.PluginAction", new IMAGE_BRUSH_SVG(TEXT("logo-lc"), Icon20x20));
	return Style;
}

void FLandscapeCombinatorStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FLandscapeCombinatorStyle::Get()
{
	return *StyleInstance;
}
