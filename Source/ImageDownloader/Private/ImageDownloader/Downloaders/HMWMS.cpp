// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Downloaders/HMWMS.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "FileDownloader/Download.h"
#include "GDALInterface/GDALInterface.h"

#include "HAL/FileManagerGeneric.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Internationalization/TextLocalizationResource.h"
#include "Containers/Queue.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

bool HMWMS::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	FString URL;
	bool bGeoTiff;
	FString FileExt;
	TArray<FString> DownloadURLs;
	TArray<FVector4d> Bounds;
	TArray<FString> FileNames;

	// Calculate the number of tiles needed based on resolution
	int NumTilesX = FMath::CeilToInt((WMS_MaxLong - WMS_MinLong) / (WMS_MaxTileWidth / WMS_ResolutionPixelsPerUnit));
	int NumTilesY = FMath::CeilToInt((WMS_MaxLat - WMS_MinLat) / (WMS_MaxTileHeight / WMS_ResolutionPixelsPerUnit));

	const uint32 Hash = FTextLocalizationResource::HashString(
		WMS_Name + WMS_CRS +
		FString::SanitizeFloat(WMS_MinLong) + FString::SanitizeFloat(WMS_MaxLong) +
		FString::SanitizeFloat(WMS_MinLat) + FString::SanitizeFloat(WMS_MaxLat) +
		FString::SanitizeFloat(WMS_ResolutionPixelsPerUnit) +
		FString::SanitizeFloat(WMS_MaxTileWidth) + FString::SanitizeFloat(WMS_MaxTileHeight)
	);

	if (bWMSSingleTile || (NumTilesX <= 1 && NumTilesY <= 1))
	{
		// Single tile download
		double Width = FMath::Min(WMS_MaxTileWidth, (WMS_MaxLong - WMS_MinLong) * WMS_ResolutionPixelsPerUnit);
		double Height = FMath::Min(WMS_MaxTileHeight, (WMS_MaxLat - WMS_MinLat) * WMS_ResolutionPixelsPerUnit);

		double ActualResolution = FMath::Min(Width / (WMS_MaxLong - WMS_MinLong), Height / (WMS_MaxLat - WMS_MinLat));
		if (bIsUserInitiated && ActualResolution < WMS_ResolutionPixelsPerUnit && !LCReporter::ShowMessage(
				FText::Format(
					LOCTEXT(
						"LowResolutionMessage",
						"Cannot download a single tile with a resolution of {0} pixels per unit because maximum tile size is {1}x{2} pixels.\n"
						"Continue with a lower resolution ({3} pixels per unit)?"
					),
					FText::AsNumber(WMS_ResolutionPixelsPerUnit),
					FText::AsNumber(WMS_MaxTileWidth),
					FText::AsNumber(WMS_MaxTileHeight),
					FText::AsNumber(ActualResolution)
				),
				"SuppressedLowResolution",
				LOCTEXT("LowResolutionTitle", "Cannot download tiles at given resolution")
			)
		)
		{
			return false;
		}

		if (WMS_Provider.CreateURL(Width, Height, WMS_Name, WMS_CRS, WMS_X_IsLong,
								WMS_MinAllowedLong, WMS_MaxAllowedLong, WMS_MinAllowedLat, WMS_MaxAllowedLat,
								WMS_MinLong, WMS_MaxLong, WMS_MinLat, WMS_MaxLat, URL, bGeoTiff, FileExt))
		{
			DownloadURLs.Add(URL);
			Bounds.Add(FVector4d(WMS_MinLong, WMS_MaxLong, WMS_MinLat, WMS_MaxLat));
			FileNames.Add(FString::Format(TEXT("WMS_{0}.{1}"), { Hash, FileExt }));
		}
		else
		{
			return false;
		}
	}
	else
	{
		// Multiple tiles download
		double LongTileDiff = (WMS_MaxLong - WMS_MinLong) / NumTilesX;
		double LatTileDiff = (WMS_MaxLat - WMS_MinLat) / NumTilesY;

		double TileWidth = FMath::Min(WMS_MaxTileWidth, LongTileDiff * WMS_ResolutionPixelsPerUnit);
		double TileHeight = FMath::Min(WMS_MaxTileHeight, LatTileDiff * WMS_ResolutionPixelsPerUnit);

		int TotalTiles = NumTilesX * NumTilesY;
		if (TotalTiles <= 0)
		{
			LCReporter::ShowError(LOCTEXT("GenericWMSNoTiles", "No tiles to download."));
			return false;
		}

		if (TotalTiles >= 100)
		{
			if (!LCReporter::ShowMessage(
				FText::Format(
					LOCTEXT(
						"GenericWMSManyTilesMessage",
						"Your WMS settings require downloading {0} tiles. This may take a while, continue?"
					),
					FText::AsNumber(TotalTiles)
				),
				"GenericWMSManyTiles",
				LOCTEXT("GenericWMSManyTilesTitle", "Several WMS Tiles to Download")
			))
			{
				return false;
			}
		}

		for (int x = 0; x < NumTilesX; ++x)
		{
			for (int y = 0; y < NumTilesY; ++y)
			{
				double MinLong = WMS_MinLong + x * LongTileDiff;
				double MaxLong = FMath::Min(MinLong + LongTileDiff, WMS_MaxLong);
				double MaxLat = WMS_MaxLat - y * LatTileDiff;
				double MinLat = FMath::Max(MaxLat - LatTileDiff, WMS_MinLat);

				if (WMS_Provider.CreateURL(TileWidth, TileHeight, WMS_Name, WMS_CRS, WMS_X_IsLong,
										WMS_MinAllowedLong, WMS_MaxAllowedLong, WMS_MinAllowedLat, WMS_MaxAllowedLat,
										MinLong, MaxLong, MinLat, MaxLat, URL, bGeoTiff, FileExt))
				{
					DownloadURLs.Add(URL);
					Bounds.Add(FVector4d(MinLong, MaxLong, MinLat, MaxLat));
					FileNames.Add(FString::Format(TEXT("WMS_{0}_x{1}_y{2}.{3}"), { Hash, x, y, FileExt }));
				}
				else
				{
					return false;
				}
			}
		}
	}

	OutputCRS = WMS_CRS;

	
	TQueue<FString> OutputFilesQueue; // thread-safe

	bool bSuccess = Concurrency::RunManyAndWait(
		bEnableParallelDownload,
		DownloadURLs.Num(),
		[this, DownloadURLs, Bounds, FileNames, bGeoTiff, &OutputFilesQueue](int i)
		{
			const FString FileName = FPaths::Combine(DownloadDir, FileNames[i]);
			if (!Download::SynchronousFromURL(DownloadURLs[i], FileName, bIsUserInitiated)) return false;

			if (!bGeoTiff || !GDALInterface::HasCRS(FileName))
			{
				const FString FileNameTiff = FPaths::GetBaseFilename(FileName) + TEXT(".tif");
				if (!GDALInterface::AddGeoreference(FileName, FileNameTiff, WMS_CRS, Bounds[i].X, Bounds[i].Y, Bounds[i].Z, Bounds[i].W)) return false;
				OutputFilesQueue.Enqueue(FileNameTiff);
			}
			else
			{
				OutputFilesQueue.Enqueue(FileName);
			}
			return true;
		}
	);

	if (bSuccess)
	{
		FString Element;
		while (OutputFilesQueue.Dequeue(Element)) OutputFiles.Add(Element);
	}

	return bSuccess;
}

#undef LOCTEXT_NAMESPACE
