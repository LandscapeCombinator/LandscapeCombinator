// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Downloaders/HMViewfinderDownloader.h"
#include "ConsoleHelpers/Console.h"
#include "ImageDownloader/Directories.h"
#include "LCReporter/LCReporter.h"

#include "ConcurrencyHelpers/Concurrency.h"
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

void HMViewfinderDownloader::OnFetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	if (!Console::ExecProcess(TEXT("7z"), TEXT(""), false))
	{
		ULCReporter::ShowError(
			LOCTEXT(
				"MissingRequirement",
				"Please make sure 7z is installed on your computer and available in your PATH if you want to use Viewfinder Panoramas heightmaps."
			)
		);
		if (OnComplete) OnComplete(false);
		return;
	}

	if (!ValidateTiles())
	{
		if (OnComplete) OnComplete(false);
		return;
	}

	Concurrency::RunMany<FString>(
		MegaTiles,
		[this](FString MegaTile, TFunction<void(bool)> OnCompleteElement)
		{
			FString ZipFile = FPaths::Combine(DownloadDir, FString::Format(TEXT("{0}.zip"), { MegaTile }));
			FString ExtractionDir = FPaths::Combine(ImageDownloaderDir, MegaTile);
			FString URL = FString::Format(TEXT("{0}{1}.zip"), { BaseURL, MegaTile });

			Download::FromURL(URL, ZipFile, bIsUserInitiated, [this, ExtractionDir, ZipFile, MegaTile, OnCompleteElement](bool bWasSuccessful)
			{
				if (bWasSuccessful)
				{
					FString ExtractParams = FString::Format(TEXT("x -aos \"{0}\" -o\"{1}\""), { ZipFile, ExtractionDir });

					if (Console::ExecProcess(TEXT("7z"), *ExtractParams))
					{
						if (bIs15)
						{
							FString TifFile = FPaths::Combine(ExtractionDir, FString::Format(TEXT("{0}.tif"), { MegaTile }));
							OutputFiles.Add(TifFile);
						}
						else
						{
							TArray<FString> HGTFiles;
							FFileManagerGeneric::Get().FindFilesRecursive(HGTFiles, *ExtractionDir, TEXT("*.hgt"), true, false);
							OutputFiles.Append(HGTFiles);
						}
						OnCompleteElement(true);
					}
					else
					{
						OnCompleteElement(false);
					}
				}
				else
				{
					OnCompleteElement(false);
				}
			});
		},
		OnComplete
	);
}

#undef LOCTEXT_NAMESPACE