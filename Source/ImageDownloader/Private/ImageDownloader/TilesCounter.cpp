// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/TilesCounter.h"

#include "Internationalization/Regex.h"

void TilesCounter::ComputeMinMaxTiles()
{
	FirstTileX = MAX_int32;
	LastTileX = 0;
	FirstTileY = MAX_int32;
	LastTileY = 0;

	for (auto& Tile : Tiles)
	{
		FirstTileX = FMath::Min(FirstTileX, TileToX(Tile));
		LastTileX  = FMath::Max(LastTileX,  TileToX(Tile));
		FirstTileY = FMath::Min(FirstTileY, TileToY(Tile));
		LastTileY  = FMath::Max(LastTileY,  TileToY(Tile));
	}
}

int TilesCounter::TileToX(FString Tile) const
{
	FRegexPattern Pattern(TEXT("_x(\\d+)_y\\d+\\."));
	FRegexMatcher Matcher(Pattern, Tile);
	Matcher.FindNext();
	return FCString::Atoi(*Matcher.GetCaptureGroup(1));
}

int TilesCounter::TileToY(FString Tile) const
{
	FRegexPattern Pattern(TEXT("_x\\d+_y(\\d+)\\."));
	FRegexMatcher Matcher(Pattern, Tile);
	Matcher.FindNext();
	return FCString::Atoi(*Matcher.GetCaptureGroup(1));
}
