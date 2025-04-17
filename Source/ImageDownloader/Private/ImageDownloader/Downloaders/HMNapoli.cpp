// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Downloaders/HMNapoli.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "LCReporter/LCReporter.h"

#include "FileDownloader/Download.h"
#include "GDALInterface/GDALInterface.h"

#include "HAL/FileManagerGeneric.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Internationalization/TextLocalizationResource.h" 

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMNapoli::OnFetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	FString CSVURL = FString::Format(
		TEXT("https://sit.cittametropolitana.na.it/geoserver/ows?service=wfs&version=2.0.0&request=GetFeature&typeName=sit:quadro_unione_lidar_dtm&outputFormat=csv&CRS=EPSG:32633&STYLES=&BBOX={0},{1},{2},{3}"),
		{ MinLong, MinLat, MaxLong, MaxLat }
	);
	
	uint32 Hash = FTextLocalizationResource::HashString(CSVURL);
	FString CSVFile = FPaths::Combine(DownloadDir, FString::Format(TEXT("napoli_{0}.csv"), { Hash }));
	FString VRTFile = FPaths::Combine(ImageDownloaderDir, FString::Format(TEXT("napoli_{0}.vrt"), { Hash }));

	OutputFiles.Add(VRTFile);
	OutputCRS = "EPSG:32633";

	Download::FromURL(CSVURL, CSVFile, bIsUserInitiated, [this, OnComplete, CSVFile, VRTFile](bool bSuccess)
	{
		if (bSuccess)
		{
			FString FileContent = "";
			if (!FFileHelper::LoadFileToString(FileContent, *CSVFile))
			{
				ULCReporter::ShowError(
					FText::Format(
						LOCTEXT("HMNapoli::Fetch", "Could not read file {0}."),
						FText::FromString(CSVFile)
					)
				);
				if (OnComplete) OnComplete(false);
				return;
			}

			TArray<FString> Lines, URLs;
			FileContent.ParseIntoArray(Lines, TEXT("\n"), true);

			for (auto& Line : Lines)
			{
				int Index = Line.Find("http");
				if (Index == INDEX_NONE)
				{
					continue;
				}
				URLs.Add(Line.RightChop(Index).TrimStartAndEnd());
			}

			if (URLs.IsEmpty())
			{
				ULCReporter::ShowError(
					FText::Format(
						LOCTEXT("HMNapoli::Fetch", "Could not find valid URLs in file {0}."),
						FText::FromString(CSVFile)
					)
				);
				if (OnComplete) OnComplete(false);
				return;
			}

			Download::DownloadMany(URLs, DownloadDir, [OnComplete, VRTFile](TArray<FString> Files)
			{
				if (Files.IsEmpty())
				{
					if (OnComplete) OnComplete(false);
					return;
				}
				else
				{
					bool bMergeSuccess = GDALInterface::Merge(Files, VRTFile);
					if (OnComplete) OnComplete(bMergeSuccess);
					return;
				}
			});
		}
		else
		{
			OnComplete(false);
		}
	});
}

#undef LOCTEXT_NAMESPACE
