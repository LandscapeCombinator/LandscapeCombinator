// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMSwissALTI3DRenamer.h"

#include "Internationalization/Regex.h"

int HMSwissALTI3DRenamer::TileToX(FString Tile) const
{
	FRegexPattern Pattern(TEXT("_(\\d+)-\\d+_"));
	FRegexMatcher Matcher(Pattern, Tile);
	Matcher.FindNext();
	FString X = Matcher.GetCaptureGroup(1);
	return FCString::Atoi(*X);
}

int HMSwissALTI3DRenamer::TileToY(FString Tile) const
{
	FRegexPattern Pattern(TEXT("_\\d+-(\\d+)_"));
	FRegexMatcher Matcher(Pattern, Tile);
	Matcher.FindNext();
	FString Y = Matcher.GetCaptureGroup(1);
	return - FCString::Atoi(*Y);
}