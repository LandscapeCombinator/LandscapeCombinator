// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/HMViewfinder15Renamer.h"

int HMViewfinder15Renamer::TileToX(FString Tile) const
{
	char C = Tile[Tile.Len() - 5];
	int T = C - 'A';
	return T % 6;
}

int HMViewfinder15Renamer::TileToY(FString Tile) const
{
	char C = Tile[Tile.Len() - 5];
	int T = C - 'A';
	return T / 6;
}
