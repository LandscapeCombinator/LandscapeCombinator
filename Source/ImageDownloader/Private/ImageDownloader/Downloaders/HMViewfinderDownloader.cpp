// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Downloaders/HMViewfinderDownloader.h"
#include "ConsoleHelpers/Console.h"
#include "ImageDownloader/Directories.h"

#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"
#include "FileDownloader/Download.h"
#include "HAL/FileManagerGeneric.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

HMViewfinderDownloader::HMViewfinderDownloader(FString MegaTilesString, FString BaseURL0, bool bIs15_0)
{
	OutputCRS = "EPSG:4326";

	TArray<FString> MegaTiles0;
	MegaTilesString.ParseIntoArray(MegaTiles0, TEXT(","), true);

	for (auto& MegaTile : MegaTiles0)
	{
		MegaTiles.Add(MegaTile.TrimStartAndEnd());
	}

	BaseURL = BaseURL0;
	bIs15 = bIs15_0;
}

bool HMViewfinderDownloader::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	if (!Console::ExecProcess(TEXT("7z"), TEXT(""), false))
	{
		LCReporter::ShowError(
			LOCTEXT(
				"MissingRequirement",
				"Please make sure 7z is installed on your computer and available in your PATH if you want to use Viewfinder Panoramas heightmaps."
			)
		);
		return false;
	}

	if (!ValidateTiles()) return false;

	TQueue<FString> OutputFilesQueue; // thread-safe

	bool bSuccess = Concurrency::RunManyAndWait<FString>(
		MegaTiles,
		[this, &OutputFilesQueue](FString MegaTile)
		{
			FString ZipFile = FPaths::Combine(DownloadDir, FString::Format(TEXT("{0}.zip"), { MegaTile }));
			FString ExtractionDir = FPaths::Combine(ImageDownloaderDir, MegaTile);
			FString URL = FString::Format(TEXT("{0}{1}.zip"), { BaseURL, MegaTile });

			if (!Download::SynchronousFromURL(URL, ZipFile, bIsUserInitiated)) return false;
			FString ExtractParams = FString::Format(TEXT("x -aos \"{0}\" -o\"{1}\""), { ZipFile, ExtractionDir });

			if (!Console::ExecProcess(TEXT("7z"), *ExtractParams)) return false;
			if (bIs15)
			{
				FString TifFile = FPaths::Combine(ExtractionDir, FString::Format(TEXT("{0}.tif"), { MegaTile }));
				OutputFilesQueue.Enqueue(TifFile);
			}
			else
			{
				TArray<FString> HGTFiles;
				FFileManagerGeneric::Get().FindFilesRecursive(HGTFiles, *ExtractionDir, TEXT("*.hgt"), true, false);
				for (auto &HGTFile: HGTFiles) OutputFilesQueue.Enqueue(HGTFile);
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