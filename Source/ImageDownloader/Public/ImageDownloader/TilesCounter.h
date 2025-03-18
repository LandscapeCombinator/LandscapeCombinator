// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class IMAGEDOWNLOADER_API TilesCounter
{
public:
	TilesCounter() {};
	TilesCounter(TArray<FString> Tiles0) : Tiles(Tiles0) {};
	virtual ~TilesCounter() {};
	void ComputeMinMaxTiles();
	int FirstTileX;
	int FirstTileY;
	int LastTileX;
	int LastTileY;

protected:
	TArray<FString> Tiles;
	virtual int TileToX(FString Tile) const;
	virtual int TileToY(FString Tile) const;
};

#undef LOCTEXT_NAMESPACE
