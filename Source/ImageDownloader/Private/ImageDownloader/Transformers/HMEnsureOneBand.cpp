// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMEnsureOneBand.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "GDALInterface/GDALInterface.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

bool HMEnsureOneBand::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	OutputCRS = InputCRS;
	OutputFiles.Append(InputFiles);
	for (auto &InputFile : InputFiles)
	{
		GDALDataset *Dataset = (GDALDataset *) GDALOpen(TCHAR_TO_UTF8(*InputFile), GA_ReadOnly);
		if (!Dataset)
		{
			LCReporter::ShowError(
				FText::Format(
					LOCTEXT("HMEnsureOneBand::Fetch", "Image Downloader Error: Could not open heightmap file '{0}'.\nError: {1}"),
					FText::FromString(InputFile),
					FText::FromString(FString(CPLGetLastErrorMsg()))
				)
			);
			return false;
		}

		int NumRasters = Dataset->GetRasterCount();
		GDALClose(Dataset);
		if (NumRasters != 1)
		{
			if (!LCReporter::ShowMessage(
				FText::Format(
					LOCTEXT(
						"HMEnsureOneBand::Fetch::Bands",
						"File '{0}' has {1} bands while it should have 1. This might not be a heightmap file.\nPress OK if you want to continue anyway, or Cancel."
					),
					FText::FromString(InputFile),
					FText::AsNumber(NumRasters)
				),
				"SuppressMultipleBands"
			))
			{
				return false;
			}
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE