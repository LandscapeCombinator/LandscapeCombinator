// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

class FFileDownloaderStyle
{
public:
	static FSlateFontInfo RegularFont()		{ return MakeFont("Roboto-Regular", FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 12); };
	static FSlateFontInfo SubtitleFont()	{ return MakeFont("Roboto-Regular", FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 18); };
	static FSlateFontInfo TitleFont()		{ return MakeFont("Roboto-Regular", FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 32); };
	static FSlateFontInfo BoldFont()		{ return MakeFont("Roboto-Bold", FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 12); };

protected:
	static FSlateFontInfo MakeFont(FName FontName, FString FontPath, int FontSize) {
		return FSlateFontInfo(MakeShareable(new FCompositeFont(FontName, FontPath, EFontHinting::Default, EFontLoadingPolicy::LazyLoad)), FontSize);
	}
};
