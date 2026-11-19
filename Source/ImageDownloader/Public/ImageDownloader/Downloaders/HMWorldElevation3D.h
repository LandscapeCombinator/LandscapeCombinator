#pragma once

#include "CoreMinimal.h"
#include "ImageDownloader/HMFetcher.h"

/** Fetches Terrain3D elevation tiles, decodes LERC, and assigns Web Mercator georeferencing. */
class HMWorldElevation3D : public HMFetcher
{
public:
	HMWorldElevation3D(const bool bInAllowInvalidTiles, const bool bInEnableParallelDownload,
		FString InName, const int InZoom, const int InMinX, const int InMaxX, const int InMinY, const int InMaxY)
		: bAllowInvalidTiles(bInAllowInvalidTiles), bEnableParallelDownload(bInEnableParallelDownload),
		  Name(MoveTemp(InName)), Zoom(InZoom), MinX(InMinX), MinY(InMinY), MaxX(InMaxX), MaxY(InMaxY) {}

protected:
	virtual FString GetOutputDir() override { return FPaths::Combine(ImageDownloaderDir, Name + TEXT("-XYZ")); }
	virtual bool OnFetch(FString InputCrs, TArray<FString> InputFiles) override;

	bool DecodeTiles(const FString& DownloadFile, int X, int Y, FString& DecodedFile, bool* BShowedDialog);
	bool GeoreferenceTiles(const FString& DecodedFile, int X, int Y, FString& OutputFile);

private:
	bool bAllowInvalidTiles;
	bool bEnableParallelDownload;
	FString Name;
	int Zoom;
	int MinX;
	int MinY;
	int MaxX;
	int MaxY;
};
