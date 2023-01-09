#include "HMInterfaceTiles.h"
#include "Download.h"

HMInterfaceTiles::HMInterfaceTiles(FString LandscapeName0, const FText &KindText0, FString Descr0, int Precision0) :
		HMInterface(LandscapeName0, KindText0, Descr0, Precision0) {

	TArray<FString> Files0;
	Descr.ParseIntoArray(Files0, TEXT(","), true);
	for (auto& File : Files0) {
		Files.Add(File.TrimStartAndEnd());
	}
}

void HMInterfaceTiles::InitializeMinMaxTiles()
{
	FirstTileX = MAX_int32;
	LastTileX = 0;
	FirstTileY = MAX_int32;
	LastTileY = 0;

	for (auto& Tile : Tiles) {
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
	return FString::Format(TEXT("{0}_x{1}_y{2}"), { LandscapeName, X - FirstTileX, Y - FirstTileY });
}

FString HMInterfaceTiles::RenameWithPrecision(FString Tile) const
{
	int X = this->TileToX(Tile);
	int Y = this->TileToY(Tile);

	return FString::Format(TEXT("{0}{3}_x{1}_y{2}"), { LandscapeName, X - FirstTileX, Y - FirstTileY, Precision });
}

bool HMInterfaceTiles::GetInsidePixels(FIntPoint &InsidePixels) const
{
	InsidePixels[0] = (LastTileX - FirstTileX + 1) * (TileWidth * Precision / 100);
	InsidePixels[1] = (LastTileY - FirstTileY + 1) * (TileHeight * Precision / 100);

	return true;
}


FReply HMInterfaceTiles::DownloadHeightMapsImpl() const
{
	for (auto &File : Files) {
		FString ZipFile = FPaths::Combine(InterfaceDir, FString::Format(TEXT("{0}.zip"), { File }));
		FString ExtractionDir = FPaths::Combine(InterfaceDir, File);
		FString URL = FString::Format(TEXT("{0}{1}.zip"), { BaseURL, File });

		Download::FromURL(URL, ZipFile, [ExtractionDir, ZipFile]() {
			FString ExtractParams = FString::Format(TEXT("x -y \"{0}\" -o\"{1}\""), { ZipFile, ExtractionDir });
			Console::ExecProcess(TEXT("7z"), *ExtractParams, nullptr);
		});
	}

	return FReply::Handled();
}