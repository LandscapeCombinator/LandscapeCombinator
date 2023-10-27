// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/HMTilesRenamer.h"
#include "LandscapeCombinator/Directories.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"

#include "HAL/FileManagerGeneric.h"
#include "Misc/ScopedSlowTask.h"

void HMTilesRenamer::Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	OutputEPSG = InputEPSG;

	if (InputFiles.Num() == 1)
	{
		OutputFiles.Add(InputFiles[0]);
		if (OnComplete) OnComplete(true);
		return;
	}

	Tiles = InputFiles;
	ComputeMinMaxTiles();

	FString RenamerFolder = FPaths::Combine(Directories::LandscapeCombinatorDir(), LandscapeLabel + "-Renamer");

	if (!IPlatformFile::GetPlatformPhysical().DeleteDirectoryRecursively(*RenamerFolder) || !IPlatformFile::GetPlatformPhysical().CreateDirectory(*RenamerFolder))
	{
		Directories::CouldNotInitializeDirectory(RenamerFolder);
		if (OnComplete) OnComplete(false);
		return;
	}
	
	for (auto& InputFile : InputFiles)
	{
		FString OutputFile = FPaths::Combine(RenamerFolder, Rename(InputFile));
		UE_LOG(LogLandscapeCombinator, Log, TEXT("Copying %s to %s"), *InputFile, *OutputFile);
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
	return FString::Format(TEXT("{0}_x{1}_y{2}.{3}"), { LandscapeLabel, X - FirstTileX, Y - FirstTileY, FPaths::GetExtension(Tile) });
}
