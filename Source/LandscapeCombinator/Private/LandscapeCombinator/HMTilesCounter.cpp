// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/HMTilesCounter.h"

#include "Internationalization/Regex.h"

void HMTilesCounter::ComputeMinMaxTiles()
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

int HMTilesCounter::TileToX(FString Tile) const
{
	FRegexPattern Pattern(TEXT("_x(\\d+)_y\\d+\\."));
	FRegexMatcher Matcher(Pattern, Tile);
	Matcher.FindNext();
	return FCString::Atoi(*Matcher.GetCaptureGroup(1));
}

int HMTilesCounter::TileToY(FString Tile) const
{
	FRegexPattern Pattern(TEXT("_x\\d+_y(\\d+)\\."));
	FRegexMatcher Matcher(Pattern, Tile);
	Matcher.FindNext();
	return FCString::Atoi(*Matcher.GetCaptureGroup(1));
}
