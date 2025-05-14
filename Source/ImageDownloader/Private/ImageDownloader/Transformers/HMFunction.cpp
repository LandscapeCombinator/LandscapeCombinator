// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMFunction.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "GDALInterface/GDALInterface.h"
#include "ConcurrencyHelpers/LCReporter.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

bool HMFunction::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	OutputCRS = InputCRS;
	OutputFiles.Append(InputFiles);

	for (auto &InputFile : InputFiles)
	{
		GDALDataset *Dataset = (GDALDataset *) GDALOpen(TCHAR_TO_UTF8(*InputFile), GA_Update);
		if (!Dataset)
		{
			LCReporter::ShowError(
				FText::Format(
					LOCTEXT("HMFunction::Fetch::1", "Image Downloader Error: Could not open heightmap file '{0}'.\nError: {1}"),
					FText::FromString(InputFile),
					FText::FromString(FString(CPLGetLastErrorMsg()))
				)
			);
			return false;
		}

		GDALRasterBand *Band = Dataset->GetRasterBand(1);

		if (!Band)
		{
			LCReporter::ShowError(FText::Format(
				LOCTEXT("HMFunction::Fetch::2", "Internal error: Could not get raster band of file {0}.\nError: {1}"),
				FText::FromString(InputFile),
				FText::FromString(FString(CPLGetLastErrorMsg()))
			));
			GDALClose(Dataset);
			return false;
		}

		int SizeX = Band->GetXSize();
		int SizeY = Band->GetYSize();
		float *Data = (float*) CPLMalloc(SizeX * SizeY * sizeof(float));

		if (!Data)
		{
			LCReporter::ShowError(
				LOCTEXT("HMFunction::Fetch::3", "Internal error: Could not allocate memory.")
			);
			GDALClose(Dataset);
			return false;
		}

		CPLErr Err;
		Err = Band->RasterIO(GF_Read, 0, 0, SizeX, SizeY, Data, SizeX, SizeY, GDT_Float32, 0, 0);

		if (Err != CE_None)
		{
			LCReporter::ShowError(FText::Format(
				LOCTEXT("HMFunction::Fetch::4", "Internal error: Could not read data from file {0}.\nError: {1}"),
				FText::FromString(InputFile),
				FText::FromString(FString(CPLGetLastErrorMsg()))
			));
			GDALClose(Dataset);
			return false;
		}

		for (int i = 0; i < SizeX; i++)
		{
			for (int j = 0; j < SizeY; j++)
			{
				Data[i + j * SizeX] = Function(Data[i + j * SizeX]);
			}
		}
		Err = Band->RasterIO(GF_Write, 0, 0, SizeX, SizeY, Data, SizeX, SizeY, GDT_Float32, 0, 0);
		CPLFree(Data);

		if (Err != CE_None)
		{
			LCReporter::ShowError(FText::Format(
				LOCTEXT("HMFunction::Fetch::5", "Internal error: Could not write data to dataset in file {0}.\nError: {1}"),
				FText::FromString(InputFile),
				FText::FromString(FString(CPLGetLastErrorMsg()))
			));
			GDALClose(Dataset);
			return false;
		}

		GDALClose(Dataset);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE