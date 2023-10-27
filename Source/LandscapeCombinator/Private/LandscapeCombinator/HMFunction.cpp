// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/HMFunction.h"
#include "LandscapeCombinator/Directories.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"
#include "GDALInterface/GDALInterface.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void HMFunction::Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	OutputEPSG = InputEPSG;
	OutputFiles.Append(InputFiles);

	for (auto &InputFile : InputFiles)
	{
		GDALDataset *Dataset = (GDALDataset *) GDALOpen(TCHAR_TO_UTF8(*InputFile), GA_Update);
		if (!Dataset)
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("GetCoordinatesError", "Landscape Combinator Error: Could not open heightmap file '{0}'."),
					FText::FromString(InputFile)
				)
			);
			if (OnComplete) OnComplete(false);
			return;
		}

		GDALRasterBand *Band = Dataset->GetRasterBand(1);

		if (!Band)
		{
			UE_LOG(LogLandscapeCombinator, Log, TEXT("Could not get raster band of file %s"), *InputFile);
			if (OnComplete) OnComplete(false);
			return;
		}

		int SizeX = Band->GetXSize();
		int SizeY = Band->GetYSize();
		float *Data = (float*) CPLMalloc(SizeX * SizeY * sizeof(float));

		if (!Data)
		{
			UE_LOG(LogLandscapeCombinator, Log, TEXT("Could not allocate memory"));
			if (OnComplete) OnComplete(false);
			return;
		}

		CPLErr Err;
		Err = Band->RasterIO(GF_Read, 0, 0, SizeX, SizeY, Data, SizeX, SizeY, GDT_Float32, 0, 0);

		if (Err != CE_None)
		{
			UE_LOG(LogLandscapeCombinator, Log, TEXT("Could not read data from file %s"), *InputFile);
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
			UE_LOG(LogLandscapeCombinator, Log, TEXT("Could not write data to dataset"), *InputFile);
			if (OnComplete) OnComplete(false);
			return;
		}

		GDALClose(Dataset);
	}

	if (OnComplete) OnComplete(true);
	return;
}

#undef LOCTEXT_NAMESPACE