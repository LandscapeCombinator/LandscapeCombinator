// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMEnsureOneBand.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "GDALInterface/GDALInterface.h"
#include "LCReporter/LCReporter.h"

#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMEnsureOneBand::OnFetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	OutputCRS = InputCRS;
	OutputFiles.Append(InputFiles);

	FScopedSlowTask EnsureOneBandTask(InputFiles.Num(), LOCTEXT("EnsureOneBandTask", "Image Downloader: Ensuring each heightmap has only one band"));
	EnsureOneBandTask.MakeDialog(true);

	for (auto &InputFile : InputFiles)
	{
		if (EnsureOneBandTask.ShouldCancel())
		{
			if (OnComplete) OnComplete(false);
			return;
		}

		GDALDataset *Dataset = (GDALDataset *) GDALOpen(TCHAR_TO_UTF8(*InputFile), GA_ReadOnly);
		if (!Dataset)
		{
			ULCReporter::ShowError(
				FText::Format(
					LOCTEXT("HMEnsureOneBand::Fetch", "Image Downloader Error: Could not open heightmap file '{0}'.\nError: {1}"),
					FText::FromString(InputFile),
					FText::FromString(FString(CPLGetLastErrorMsg()))
				)
			);
			if (OnComplete) OnComplete(false);
			return;
		}

		int NumRasters = Dataset->GetRasterCount();
		GDALClose(Dataset);
		if (NumRasters != 1)
		{
			if (!ULCReporter::ShowMessage(
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
				if (OnComplete) OnComplete(false);
				return;
			}
		}
	}

	if (OnComplete) OnComplete(true);
	return;
}

#undef LOCTEXT_NAMESPACE