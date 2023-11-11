// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMFunction.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "GDALInterface/GDALInterface.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMFunction::Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	OutputCRS = InputCRS;
	OutputFiles.Append(InputFiles);

	for (auto &InputFile : InputFiles)
	{
		GDALDataset *Dataset = (GDALDataset *) GDALOpen(TCHAR_TO_UTF8(*InputFile), GA_Update);
		if (!Dataset)
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("GetCoordinatesError", "Image Downloader Error: Could not open heightmap file '{0}'.\nError: {1}"),
					FText::FromString(InputFile),
					FText::FromString(FString(CPLGetLastErrorMsg()))
				)
			);
			if (OnComplete) OnComplete(false);
			return;
		}

		GDALRasterBand *Band = Dataset->GetRasterBand(1);

		if (!Band)
		{
			UE_LOG(LogImageDownloader, Log, TEXT("Could not get raster band of file %s"), *InputFile);
			GDALClose(Dataset);
			if (OnComplete) OnComplete(false);
			return;
		}

		int SizeX = Band->GetXSize();
		int SizeY = Band->GetYSize();
		float *Data = (float*) CPLMalloc(SizeX * SizeY * sizeof(float));

		if (!Data)
		{
			UE_LOG(LogImageDownloader, Log, TEXT("Could not allocate memory"));
			GDALClose(Dataset);
			if (OnComplete) OnComplete(false);
			return;
		}

		CPLErr Err;
		Err = Band->RasterIO(GF_Read, 0, 0, SizeX, SizeY, Data, SizeX, SizeY, GDT_Float32, 0, 0);

		if (Err != CE_None)
		{
			UE_LOG(LogImageDownloader, Log, TEXT("Could not read data from file %s"), *InputFile);
			GDALClose(Dataset);
			if (OnComplete) OnComplete(false);
			return;
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
			UE_LOG(LogImageDownloader, Log, TEXT("Could not write data to dataset"), *InputFile);
			GDALClose(Dataset);
			if (OnComplete) OnComplete(false);
			return;
		}

		GDALClose(Dataset);
	}

	if (OnComplete) OnComplete(true);
	return;
}

#undef LOCTEXT_NAMESPACE