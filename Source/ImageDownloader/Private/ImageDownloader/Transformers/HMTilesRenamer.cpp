// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMTilesRenamer.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"

#include "HAL/FileManagerGeneric.h"
#include "Misc/ScopedSlowTask.h"

void HMTilesRenamer::OnFetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	OutputCRS = InputCRS;

	if (InputFiles.Num() == 1)
	{
		OutputFiles.Add(InputFiles[0]);
		if (OnComplete) OnComplete(true);
		return;
	}

	Tiles = InputFiles;
	ComputeMinMaxTiles();
	
	for (auto& InputFile : InputFiles)
	{
		FString OutputFile = FPaths::Combine(OutputDir, Rename(InputFile));
		UE_LOG(LogImageDownloader, Log, TEXT("Copying %s to %s"), *InputFile, *OutputFile);
		FFileManagerGeneric::Get().Copy(*OutputFile, *InputFile);

		FString AuxFile = InputFile + ".aux.xml";
		if (FFileManagerGeneric::Get().FileExists(*AuxFile))
		{
			FFileManagerGeneric::Get().Copy(*(OutputFile + ".aux.xml"), *AuxFile);
		}
		OutputFiles.Add(OutputFile);
	}

	if (OnComplete) OnComplete(true);
}

FString HMTilesRenamer::Rename(FString Tile) const
{
	int X = this->TileToX(Tile);
	int Y = this->TileToY(Tile);
	return FString::Format(TEXT("{0}_x{1}_y{2}.{3}"), { Name, X - FirstTileX, Y - FirstTileY, FPaths::GetExtension(Tile) });
}
