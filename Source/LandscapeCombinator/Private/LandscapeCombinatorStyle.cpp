// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeCombinatorStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/StyleColors.h"

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
	
	Style->Set("LandscapeCombinator.OpenPluginWindow", new IMAGE_BRUSH_SVG(TEXT("GreenLayers"), Icon20x20));
	Style->Set("LandscapeCombinator.Download", new IMAGE_BRUSH_SVG(TEXT("Download"), Icon20x20));
	Style->Set("LandscapeCombinator.Convert", new IMAGE_BRUSH_SVG(TEXT("Convert"), Icon20x20));
	Style->Set("LandscapeCombinator.Adjust", new IMAGE_BRUSH_SVG(TEXT("Adjust"), Icon20x20));
	Style->Set("LandscapeCombinator.Scale", new IMAGE_BRUSH_SVG(TEXT("Scale"), Icon20x20));
	Style->Set("LandscapeCombinator.MoveUp", new IMAGE_BRUSH_SVG(TEXT("MoveUp"), Icon20x20));
	Style->Set("LandscapeCombinator.MoveDown", new IMAGE_BRUSH_SVG(TEXT("MoveDown"), Icon20x20));
	Style->Set("LandscapeCombinator.Delete", new IMAGE_BRUSH_SVG(TEXT("Delete"), Icon20x20));
	Style->Set("LandscapeCombinator.Add", new IMAGE_BRUSH_SVG(TEXT("Add"), Icon20x20));

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
