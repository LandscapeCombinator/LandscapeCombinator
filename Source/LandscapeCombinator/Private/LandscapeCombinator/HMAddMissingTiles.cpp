// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/HMAddMissingTiles.h"
#include "LandscapeCombinator/HMTilesCounter.h"
#include "LandscapeCombinator/Directories.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"

#include "GDALInterface/GDALInterface.h"

#include "HAL/FileManagerGeneric.h"
#include "Internationalization/Regex.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void HMAddMissingTiles::Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	OutputEPSG = InputEPSG;
	if (InputFiles.Num() == 1)
	{
		OutputFiles.Add(InputFiles[0]);
		if (OnComplete) OnComplete(true);
		return;
	}

	FRegexPattern XYPattern(TEXT("(.*)_x\\d+_y\\d+\\.png"));
	FRegexMatcher XYMatcher(XYPattern, InputFiles[0]);

	if (!XYMatcher.FindNext())
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("MultipleFileImportError",
				"Landscape Combinator internal error: The Add Missing Tiles phase can only be used on files on the form Filename_x0_y0.png, "
				"but it was used on file: '{0}'."
			),
			FText::FromString(InputFiles[0])
		));

		if (OnComplete) OnComplete(false);
		return;
	}
	
	FString BaseName = XYMatcher.GetCaptureGroup(1);
	
	HMTilesCounter TilesCounter(InputFiles);
	TilesCounter.ComputeMinMaxTiles();
	
	GDALDriver *MEMDriver = GetGDALDriverManager()->GetDriverByName("MEM");
	GDALDriver *PNGDriver = GetGDALDriverManager()->GetDriverByName("PNG");

	if (!PNGDriver || !MEMDriver)
	{
		UE_LOG(LogLandscapeCombinator, Error, TEXT("Could not load GDAL drivers"));
		if (OnComplete) OnComplete(false);
	}

	FIntPoint Pixels;
	if (!GDALInterface::GetPixels(Pixels, InputFiles[0])) return;
	
	for (int i = 0; i <= TilesCounter.LastTileX; i++)
	{
		for (int j = 0; j <= TilesCounter.LastTileY; j++)
		{
			FString CurrentFile = FString::Format(TEXT("{0}_x{1}_y{2}.png"), { BaseName, i, j });
			OutputFiles.Add(CurrentFile);

			if (!FFileManagerGeneric::Get().FileExists(*CurrentFile))
			{
				UE_LOG(LogLandscapeCombinator, Error, TEXT("Creating missing file: %s"), *CurrentFile);
				GDALDataset *Dataset = MEMDriver->Create(
					"",
					Pixels[0], Pixels[1],
					1, // nBands
					GDT_UInt16,
					nullptr
				);

				if (!Dataset)
				{
					UE_LOG(LogLandscapeCombinator, Log, TEXT("Could not create missing file %s"), *CurrentFile);
					if (OnComplete) OnComplete(false);
					return;
				}

				GDALRasterBand *Band = Dataset->GetRasterBand(1);
				GUInt16 *Data = (GUInt16*) CPLMalloc(Pixels[0] * Pixels[1] * sizeof(GDT_UInt16));
				memset(Data, 0, Pixels[0] * Pixels[1] * sizeof(GDT_UInt16));

				Band->RasterIO(GF_Write, 0, 0, Pixels[0], Pixels[1], Data, Pixels[0], Pixels[1], GDT_UInt16, 0, 0);

				GDALDataset *PNGDataset = PNGDriver->CreateCopy(TCHAR_TO_UTF8(*CurrentFile), Dataset, 1, nullptr, nullptr, nullptr);
				GDALClose(Dataset);

				if (!PNGDataset)
				{
					UE_LOG(LogLandscapeCombinator, Log, TEXT("Could not create missing file %s"), *CurrentFile);
					if (OnComplete) OnComplete(false);
					return;
				}
				
				GDALClose(PNGDataset);
			}
		}
	}

	if (OnComplete) OnComplete(true);
}

#undef LOCTEXT_NAMESPACE