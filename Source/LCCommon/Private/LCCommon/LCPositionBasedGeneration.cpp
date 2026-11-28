// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "LCCommon/LCPositionBasedGeneration.h"

void ULCPositionBasedGeneration::ClearGeneratedTilesCache()
{
	GeneratedTiles.Empty();
	PendingTiles.Empty();
}

TArray<UObject*> ULCPositionBasedGeneration::EvictTiles(const TSet<FTile>& WantedTiles, int NumNewTiles)
{
	CurrentPass++;
	for (const FTile& Tile : WantedTiles)
	{
		if (FTileRecord* Record = GeneratedTiles.Find(Tile)) Record->LastUsedPass = CurrentPass;
	}

	TArray<UObject*> Objects;
	if (MaxTiles <= 0) return Objects;

	TArray<FTile> Candidates;
	for (const TPair<FTile, FTileRecord>& Pair : GeneratedTiles)
	{
		if (!WantedTiles.Contains(Pair.Key)) Candidates.Add(Pair.Key);
	}
	Candidates.Sort([this](const FTile& A, const FTile& B) {
		return GeneratedTiles[A].LastUsedPass < GeneratedTiles[B].LastUsedPass;
	});

	const int NumToEvict = FMath::Min(Candidates.Num(), GeneratedTiles.Num() + NumNewTiles - MaxTiles);
	if (NumToEvict <= 0) return Objects;

	Modify();
	TArray<TSoftObjectPtr<UObject>> Destroyed;
	for (int I = 0; I < NumToEvict; ++I)
	{
		Destroyed.Append(GeneratedTiles[Candidates[I]].Objects);
		GeneratedTiles.Remove(Candidates[I]);
	}
	for (const TSoftObjectPtr<UObject>& Object : Destroyed)
	{
		if (UObject* Resolved = Object.Get()) Objects.Add(Resolved);
	}
	if (!Destroyed.IsEmpty()) OnTilesDestroyed.Broadcast(Destroyed);
	return Objects;
}
