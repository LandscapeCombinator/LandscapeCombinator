// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Downloaders/HMNapoli.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"

#include "FileDownloader/Download.h"
#include "GDALInterface/GDALInterface.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "HAL/FileManagerGeneric.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Internationalization/TextLocalizationResource.h" 

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

bool HMNapoli::OnFetch(FString InputCRS, TArray<FString> InputFiles)
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

	if (!Download::SynchronousFromURL(CSVURL, CSVFile, bIsUserInitiated)) return false;

	FString FileContent = "";
	if (!FFileHelper::LoadFileToString(FileContent, *CSVFile))
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("HMNapoli::Fetch", "Could not read file {0}."),
				FText::FromString(CSVFile)
			)
		);
		return false;
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
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("HMNapoli::Fetch", "Could not find valid URLs in file {0}."),
				FText::FromString(CSVFile)
			)
		);
		return false;
	}

	TArray<FString> Files = Download::DownloadManyAndWait(URLs, DownloadDir, bIsUserInitiated);
	return !Files.IsEmpty() && GDALInterface::Merge(Files, VRTFile);
}

#undef LOCTEXT_NAMESPACE
