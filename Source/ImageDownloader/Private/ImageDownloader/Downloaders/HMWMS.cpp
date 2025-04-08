// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Downloaders/HMWMS.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "LCReporter/LCReporter.h"
#include "ConcurrencyHelpers/Concurrency.h"

#include "FileDownloader/Download.h"
#include "GDALInterface/GDALInterface.h"

#include "HAL/FileManagerGeneric.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Internationalization/TextLocalizationResource.h" 

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMWMS::Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
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
		if (WMS_Provider.CreateURL(WMS_MaxTileWidth, WMS_MaxTileHeight, WMS_Name, WMS_CRS, WMS_X_IsLong,
								WMS_MinAllowedLong, WMS_MaxAllowedLong, WMS_MinAllowedLat, WMS_MaxAllowedLat,
								WMS_MinLong, WMS_MaxLong, WMS_MinLat, WMS_MaxLat, URL, bGeoTiff, FileExt))
		{
			DownloadURLs.Add(URL);
			Bounds.Add(FVector4d(WMS_MinLong, WMS_MaxLong, WMS_MinLat, WMS_MaxLat));
			FileNames.Add(FString::Format(TEXT("WMS_{0}.{1}"), { Hash, FileExt }));
		}
		else
		{
			if (OnComplete) OnComplete(false);
			return;
		}
	}
	else
	{
		// Multiple tiles download
		double TileWidth = (WMS_MaxLong - WMS_MinLong) / NumTilesX;
		double TileHeight = (WMS_MaxLat - WMS_MinLat) / NumTilesY;

		int TotalTiles = NumTilesX * NumTilesY;
		if (TotalTiles <= 0)
		{
			ULCReporter::ShowError(LOCTEXT("GenericWMSNoTiles", "No tiles to download."));
			if (OnComplete) OnComplete(false);
			return;
		}

		if (TotalTiles >= 100)
		{
			if (!ULCReporter::ShowMessage(
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
				if (OnComplete) OnComplete(false);
				return;
			}
		}

		for (int x = 0; x < NumTilesX; ++x)
		{
			for (int y = 0; y < NumTilesY; ++y)
			{
				double MinLong = WMS_MinLong + x * TileWidth;
				double MaxLong = FMath::Min(MinLong + TileWidth, WMS_MaxLong);
				double MaxLat = WMS_MaxLat - y * TileHeight;
				double MinLat = FMath::Max(MaxLat - TileHeight, WMS_MinLat);

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
					if (OnComplete) OnComplete(false);
					return;
				}
			}
		}
	}

	FString DownloadDir = Directories::DownloadDir();
	Concurrency::RunMany(
		DownloadURLs.Num(),
		[this, DownloadURLs, Bounds, FileNames, DownloadDir, bGeoTiff](int i, TFunction<void(bool)> OnCompleteElement)
		{
			const FString FileName = FPaths::Combine(DownloadDir, FileNames[i]);
			Download::FromURL(
				DownloadURLs[i], FileName, true,
				[this, DownloadDir, Bounds, FileNames, i, FileName, bGeoTiff, OnCompleteElement](bool bSuccess)
				{
					if (bSuccess)
					{
						if (!bGeoTiff || GDALInterface::HasCRS(FileName))
						{
							const FString FileNameTiff = FPaths::GetBaseFilename(FileName) + TEXT(".tif");
							GDALInterface::AddGeoreference(FileName, FileNameTiff, WMS_CRS, Bounds[i].X, Bounds[i].Y, Bounds[i].Z, Bounds[i].W);
							OutputFiles.Add(FileNameTiff);
						}
						else
						{
							OutputFiles.Add(FileName);
						}
						OnCompleteElement(true);
					}
					else
					{
						OnCompleteElement(false);
					}
				}
			);
		},
		OnComplete
	);
}

#undef LOCTEXT_NAMESPACE
