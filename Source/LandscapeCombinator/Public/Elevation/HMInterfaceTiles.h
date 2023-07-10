// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HMInterface.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMInterfaceTiles : public HMInterface
{

protected:
	int FirstTileX;
	int FirstTileY;
	int LastTileX;
	int LastTileY;
	int TileWidth;
	int TileHeight;
	TArray<FString> Tiles;
	
	virtual bool Initialize() = 0;
	virtual int TileToX(FString Tile) const = 0;
	virtual int TileToY(FString Tile) const = 0;
	FString Rename(FString Tile) const;
	FString RenameWithPrecision(FString Tile) const;

	void InitializeMinMaxTiles();
	bool GetInsidePixels(FIntPoint &InsidePixels) const;

public:
	HMInterfaceTiles(FString LandscapeLabel0, const FText &KindText0, FString Descr0, int Precision0);


};

#undef LOCTEXT_NAMESPACE