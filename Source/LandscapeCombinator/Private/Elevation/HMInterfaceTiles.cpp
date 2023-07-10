// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "Elevation/HMInterfaceTiles.h"

#include "Utils/Console.h"
#include "Utils/Concurrency.h"
#include "Utils/Download.h"

HMInterfaceTiles::HMInterfaceTiles(FString LandscapeLabel0, const FText &KindText0, FString Descr0, int Precision0) :
		HMInterface(LandscapeLabel0, KindText0, Descr0, Precision0) {

}

void HMInterfaceTiles::InitializeMinMaxTiles()
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

FString HMInterfaceTiles::Rename(FString Tile) const
{
	int X = this->TileToX(Tile);
	int Y = this->TileToY(Tile);
	return FString::Format(TEXT("{0}_x{1}_y{2}"), { LandscapeLabel, X - FirstTileX, Y - FirstTileY });
}

FString HMInterfaceTiles::RenameWithPrecision(FString Tile) const
{
	int X = this->TileToX(Tile);
	int Y = this->TileToY(Tile);

	return FString::Format(TEXT("{0}{3}_x{1}_y{2}"), { LandscapeLabel, X - FirstTileX, Y - FirstTileY, Precision });
}

bool HMInterfaceTiles::GetInsidePixels(FIntPoint &InsidePixels) const
{
	InsidePixels[0] = (LastTileX - FirstTileX + 1) * (TileWidth * Precision / 100);
	InsidePixels[1] = (LastTileY - FirstTileY + 1) * (TileHeight * Precision / 100);

	return true;
}
