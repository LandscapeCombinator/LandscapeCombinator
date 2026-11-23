// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Downloaders/HMOGC.h"
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

bool HMOGC::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	FString URL;
	bool bGeoTiff;
	FString FileExt;
	TArray<FString> DownloadURLs;
	TArray<FVector4d> Bounds;
	TArray<FString> FileNames;

	if (bOGC_MaxResolution)
		OGC_ResolutionPixelsPerUnit = FMath::Min(OGC_MaxTileWidth / (OGC_MaxLong - OGC_MinLong), OGC_MaxTileHeight / (OGC_MaxLat - OGC_MinLat));

	// Calculate the number of tiles needed based on resolution
	int NumTilesX = FMath::CeilToInt((OGC_MaxLong - OGC_MinLong) / (OGC_MaxTileWidth / OGC_ResolutionPixelsPerUnit));
	int NumTilesY = FMath::CeilToInt((OGC_MaxLat - OGC_MinLat) / (OGC_MaxTileHeight / OGC_ResolutionPixelsPerUnit));

	const uint32 Hash = FTextLocalizationResource::HashString(
		OGC_Name + OGC_CRS +
		FString::SanitizeFloat(OGC_MinLong) + FString::SanitizeFloat(OGC_MaxLong) +
		FString::SanitizeFloat(OGC_MinLat) + FString::SanitizeFloat(OGC_MaxLat) +
		FString::SanitizeFloat(OGC_ResolutionPixelsPerUnit) +
		FString::SanitizeFloat(OGC_MaxTileWidth) + FString::SanitizeFloat(OGC_MaxTileHeight)
	);

	if (bOGCSingleTile || (NumTilesX <= 1 && NumTilesY <= 1))
	{
		double IdealWidth = (OGC_MaxLong - OGC_MinLong) * OGC_ResolutionPixelsPerUnit;
		double IdealHeight = (OGC_MaxLat - OGC_MinLat) * OGC_ResolutionPixelsPerUnit;

		double Width = FMath::Min(OGC_MaxTileWidth, IdealWidth);
		double Height = FMath::Min(OGC_MaxTileHeight, IdealHeight);

		bool bResolutionWasCapped = (IdealWidth > OGC_MaxTileWidth) || (IdealHeight > OGC_MaxTileHeight);

		double ActualResolution = FMath::Min(Width / (OGC_MaxLong - OGC_MinLong), Height / (OGC_MaxLat - OGC_MinLat));

		if (bIsUserInitiated && bResolutionWasCapped && !bOGC_MaxResolution && !LCReporter::ShowMessage(
			FText::Format(
				LOCTEXT(
					"LowResolutionMessage",
					"Cannot download a single tile with a resolution of {0} pixels per unit because maximum tile size is {1}x{2} pixels.\n"
					"Continue with a lower resolution ({3} pixels per unit)?"
				),
				FText::AsNumber(OGC_ResolutionPixelsPerUnit),
				FText::AsNumber(OGC_MaxTileWidth),
				FText::AsNumber(OGC_MaxTileHeight),
				FText::AsNumber(ActualResolution)
			),
			"SuppressedLowResolution",
			LOCTEXT("LowResolutionTitle", "Cannot download tiles at given resolution")
		))
		{
			return false;
		}

		if (bResolutionWasCapped) OGC_ResolutionPixelsPerUnit = ActualResolution;

		if (OGC_Provider.CreateURL(Width, Height, OGC_Name, OGC_CRS, OGC_X_IsLong,
								OGC_MinAllowedLong, OGC_MaxAllowedLong, OGC_MinAllowedLat, OGC_MaxAllowedLat,
								OGC_MinLong, OGC_MaxLong, OGC_MinLat, OGC_MaxLat, URL, bGeoTiff, FileExt))
		{
			DownloadURLs.Add(URL);
			Bounds.Add(FVector4d(OGC_MinLong, OGC_MaxLong, OGC_MinLat, OGC_MaxLat));
			FileNames.Add(FString::Format(TEXT("{0}_{1}.{2}"), { OGC_Provider.ServiceName(), Hash, FileExt }));
		}
		else
		{
			return false;
		}
	}
	else
	{
		// Multiple tiles download
		double LongTileDiff = (OGC_MaxLong - OGC_MinLong) / NumTilesX;
		double LatTileDiff = (OGC_MaxLat - OGC_MinLat) / NumTilesY;

		double TileWidth = FMath::Min(OGC_MaxTileWidth, LongTileDiff * OGC_ResolutionPixelsPerUnit);
		double TileHeight = FMath::Min(OGC_MaxTileHeight, LatTileDiff * OGC_ResolutionPixelsPerUnit);

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
						"Your settings require downloading {0} tiles. This may take a while, continue?"
					),
					FText::AsNumber(TotalTiles)
				),
				"GenericWMSManyTiles",
				LOCTEXT("GenericWMSManyTilesTitle", "Several Tiles to Download")
			))
			{
				return false;
			}
		}

		for (int x = 0; x < NumTilesX; ++x)
		{
			for (int y = 0; y < NumTilesY; ++y)
			{
				double MinLong = OGC_MinLong + x * LongTileDiff;
				double MaxLong = FMath::Min(MinLong + LongTileDiff, OGC_MaxLong);
				double MaxLat = OGC_MaxLat - y * LatTileDiff;
				double MinLat = FMath::Max(MaxLat - LatTileDiff, OGC_MinLat);

				if (OGC_Provider.CreateURL(TileWidth, TileHeight, OGC_Name, OGC_CRS, OGC_X_IsLong,
										OGC_MinAllowedLong, OGC_MaxAllowedLong, OGC_MinAllowedLat, OGC_MaxAllowedLat,
										MinLong, MaxLong, MinLat, MaxLat, URL, bGeoTiff, FileExt))
				{
					DownloadURLs.Add(URL);
					Bounds.Add(FVector4d(MinLong, MaxLong, MinLat, MaxLat));
					FileNames.Add(FString::Format(TEXT("{0}_{1}_x{2}_y{3}.{4}"), { OGC_Provider.ServiceName(), Hash, x, y, FileExt }));
				}
				else
				{
					return false;
				}
			}
		}
	}

	OutputCRS = OGC_CRS;

	
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
				if (!GDALInterface::AddGeoreference(FileName, FileNameTiff, OGC_CRS, Bounds[i].X, Bounds[i].Y, Bounds[i].Z, Bounds[i].W)) return false;
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
