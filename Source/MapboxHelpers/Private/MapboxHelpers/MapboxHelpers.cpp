// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "MapboxHelpers/MapboxHelpers.h"
#include "MapboxHelpers/LogMapboxHelpers.h"
#include "GDALInterface/GDALInterface.h"

#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FMapboxHelpersModule"

bool MapboxHelpers::DecodeMapboxOneBand(FString InputFile, FString OutputFile, bool *bShowedDialog)
{
	/* Read the RGB bands from `InputFile` */

	GDALDataset *Dataset = (GDALDataset *) GDALOpen(TCHAR_TO_UTF8(*InputFile), GA_ReadOnly);

	if (!Dataset)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("MapboxHelpers::6", "Could not read file {0} using GDAL."),
				FText::FromString(OutputFile)
			));
			*bShowedDialog = true;
		}
		return false;
	}


	if (Dataset->GetRasterCount() != 1)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("MapboxHelpers::0", "Expected one band from heightmap {0}, but got {1} instead."),
				FText::FromString(InputFile),
				FText::AsNumber(Dataset->GetRasterCount())
			));
			*bShowedDialog = true;
		}
		return false;
	}


	GDALColorTable* ColorTable = Dataset->GetRasterBand(1)->GetColorTable();
    if (!ColorTable)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("MapboxHelpers::00", "Expected to have a Color Table in the heightmap {0}."),
				FText::FromString(InputFile)
			));
			*bShowedDialog = true;
		}
        GDALClose(Dataset);
        return false;
    }

	
	int SizeX = Dataset->GetRasterXSize();
	int SizeY = Dataset->GetRasterYSize();
	
	uint8* CodeBand = (uint8*) malloc(SizeX * SizeY * (sizeof (uint8)));

	if (!CodeBand)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				LOCTEXT("MapboxHelpers::7", "Not enough memory to allocate for decoding data.")
			);
			*bShowedDialog = true;
		}
		GDALClose(Dataset);
		if (CodeBand) free(CodeBand);
		return false;
	}

	
	CPLErr ReadErr = Dataset->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, SizeX, SizeY, CodeBand, SizeX, SizeY, GDT_Byte, 0, 0);

	if (ReadErr != CE_None)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("MapboxHelpers::7", "There was an error while reading heightmap data from file {0}."),
				FText::FromString(OutputFile)
			));
			*bShowedDialog = true;
		}
		free(CodeBand);
		GDALClose(Dataset);
		return false;
	}
	


	/* Prepare GDAL Drivers */

	GDALDriver *MEMDriver = GetGDALDriverManager()->GetDriverByName("MEM");
	GDALDriver *TIFDriver = GetGDALDriverManager()->GetDriverByName("GTiff");

	if (!TIFDriver || !MEMDriver)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				LOCTEXT("MapboxHelpers::3", "Could not load GDAL drivers.")
			);
			*bShowedDialog = true;
			GDALClose(Dataset);
		}
		free(CodeBand);
		return false;
	}
	


	/* Decode and write the HeightmapData to `OutputFile` */

	float* HeightmapData = (float*) malloc(SizeX * SizeY * sizeof(float));

	if (!HeightmapData)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("MapboxHelpers::12", "Not enough memory to allocate for decoding data."),
				FText::FromString(OutputFile)
			));
			*bShowedDialog = true;
		}
		free(CodeBand);
		return false;
	}


	for (int X = 0; X < SizeX; X++)
	{
		for (int Y = 0; Y < SizeY; Y++)
		{
			int i = X + Y * SizeX;
			const GDALColorEntry *ColorEntry = ColorTable->GetColorEntry(CodeBand[i]);
			if (!ColorEntry)
			{
				if (!*bShowedDialog)
				{
					FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
						LOCTEXT("MapboxHelpers::12", "No Color Entry for X = {0}, Y = {1}, Index = {2}, i = {3}, ColorTable Entry Count: {4}"),
						FText::AsNumber(X),
						FText::AsNumber(Y),
						FText::AsNumber(CodeBand[i]),
						FText::AsNumber(i),
						FText::AsNumber(ColorTable->GetColorEntryCount())
					));
					*bShowedDialog = true;
				}
				free(CodeBand);
				GDALClose(Dataset);
				return false;
			}
			float R = ColorEntry->c1;
			float G = ColorEntry->c2;
			float B = ColorEntry->c3;
			HeightmapData[i] = -10000 + ((R * 256 * 256 + G * 256 + B) * 0.1);
		}
	}
	free(CodeBand);
	GDALClose(Dataset);
	


	GDALDataset *NewDataset = MEMDriver->Create(
		"",
		SizeX, SizeY,
		1, // nBands
		GDT_Float32,
		nullptr
	);
	

	if (!NewDataset)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("MapboxHelpers::4", "There was an error while creating a GDAL Dataset."),
				FText::FromString(InputFile)
			));
			*bShowedDialog = true;
		}
		free(HeightmapData);
		return false;
	}
	CPLErr WriteErr = NewDataset->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, SizeX, SizeY, HeightmapData, SizeX, SizeY, GDT_Float32, 0, 0);


	if (WriteErr != CE_None)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("MapboxHelpers::5", "There was an error while writing heightmap data to file {0}. (Error: {1})"),
				FText::FromString(OutputFile),
				FText::AsNumber(WriteErr, &FNumberFormattingOptions::DefaultNoGrouping())
			));
			*bShowedDialog = true;
		}
		GDALClose(NewDataset);
		free(HeightmapData);
		return false;
	}


	GDALDataset *TIFDataset = TIFDriver->CreateCopy(TCHAR_TO_UTF8(*OutputFile), NewDataset, 1, nullptr, nullptr, nullptr);
	GDALClose(NewDataset);

	if (!TIFDataset)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("MapboxHelpers::5", "Could not write heightmap to file {0}."),
				FText::FromString(OutputFile),
				FText::AsNumber(WriteErr, &FNumberFormattingOptions::DefaultNoGrouping())
			));
			*bShowedDialog = true;
		}
		free(HeightmapData);
		return false;
	}

	
	GDALClose(TIFDataset);

	return true;
}

bool MapboxHelpers::DecodeMapboxThreeBands(FString InputFile, FString OutputFile, bool *bShowedDialog)
{
	/* Read the RGB bands from `InputFile` */

	GDALDataset *Dataset = (GDALDataset *) GDALOpen(TCHAR_TO_UTF8(*InputFile), GA_ReadOnly);

	if (!Dataset)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("MapboxHelpers::6", "Could not read file {0} using GDAL."),
				FText::FromString(OutputFile)
			));
			*bShowedDialog = true;
		}
		return false;
	}

	if (Dataset->GetRasterCount() < 3)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("MapboxHelpers::0", "Expected at least three bands from heightmap {0}, but got {1} instead."),
				FText::FromString(InputFile),
				FText::AsNumber(Dataset->GetRasterCount())
			));
			*bShowedDialog = true;
		}
		return false;
	}
	
	int SizeX = Dataset->GetRasterXSize();
	int SizeY = Dataset->GetRasterYSize();
	
	uint8* RedBand = (uint8*) malloc(SizeX * SizeY * (sizeof (uint8)));
	uint8* GreenBand = (uint8*) malloc(SizeX * SizeY * (sizeof (uint8)));
	uint8* BlueBand = (uint8*) malloc(SizeX * SizeY * (sizeof (uint8)));

	if (!RedBand || !GreenBand || !BlueBand)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				LOCTEXT("MapboxHelpers::7", "Not enough memory to allocate for decoding data.")
			);
			*bShowedDialog = true;
		}
		GDALClose(Dataset);
		if (RedBand) free(RedBand);
		if (GreenBand) free(GreenBand);
		if (BlueBand) free(BlueBand);
		return false;
	}
	
	CPLErr ReadErr1 = Dataset->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, SizeX, SizeY, RedBand, SizeX, SizeY, GDT_Byte, 0, 0);
	CPLErr ReadErr2 = Dataset->GetRasterBand(2)->RasterIO(GF_Read, 0, 0, SizeX, SizeY, GreenBand, SizeX, SizeY, GDT_Byte, 0, 0);
	CPLErr ReadErr3 = Dataset->GetRasterBand(3)->RasterIO(GF_Read, 0, 0, SizeX, SizeY, BlueBand, SizeX, SizeY, GDT_Byte, 0, 0);

	if (ReadErr1 != CE_None || ReadErr2 != CE_None || ReadErr3 != CE_None)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("MapboxHelpers::7", "There was an error while reading heightmap data from file {0}."),
				FText::FromString(OutputFile)
			));
			*bShowedDialog = true;
		}
		free(RedBand);
		free(GreenBand);
		free(BlueBand);
		GDALClose(Dataset);
		return false;
	}
	


	/* Prepare GDAL Drivers */

	GDALDriver *MEMDriver = GetGDALDriverManager()->GetDriverByName("MEM");
	GDALDriver *TIFDriver = GetGDALDriverManager()->GetDriverByName("GTiff");

	if (!TIFDriver || !MEMDriver)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				LOCTEXT("MapboxHelpers::3", "Could not load GDAL drivers.")
			);
			*bShowedDialog = true;
			GDALClose(Dataset);
		}
		free(RedBand);
		free(GreenBand);
		free(BlueBand);
		return false;
	}
	


	/* Decode and write the HeightmapData to `OutputFile` */

	float* HeightmapData = (float*) malloc(SizeX * SizeY * sizeof(float));

	if (!HeightmapData)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("MapboxHelpers::12", "Not enough memory to allocate for decoding data."),
				FText::FromString(OutputFile)
			));
			*bShowedDialog = true;
		}
		free(RedBand);
		free(GreenBand);
		free(BlueBand);
		return false;
	}


	for (int X = 0; X < SizeX; X++)
	{
		for (int Y = 0; Y < SizeY; Y++)
		{
			int i = X + Y * SizeX;
			float R = RedBand[i];
			float G = GreenBand[i];
			float B = BlueBand[i];
			HeightmapData[i] = -10000 + ((R * 256 * 256 + G * 256 + B) * 0.1);
		}
	}
	free(RedBand);
	free(GreenBand);
	free(BlueBand);
	GDALClose(Dataset);
	


	GDALDataset *NewDataset = MEMDriver->Create(
		"",
		SizeX, SizeY,
		1, // nBands
		GDT_Float32,
		nullptr
	);
	

	if (!NewDataset)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("MapboxHelpers::4", "There was an error while creating a GDAL Dataset."),
				FText::FromString(InputFile)
			));
			*bShowedDialog = true;
		}
		free(HeightmapData);
		return false;
	}
	CPLErr WriteErr = NewDataset->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, SizeX, SizeY, HeightmapData, SizeX, SizeY, GDT_Float32, 0, 0);


	if (WriteErr != CE_None)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("MapboxHelpers::5", "There was an error while writing heightmap data to file {0}. (Error: {1})"),
				FText::FromString(OutputFile),
				FText::AsNumber(WriteErr, &FNumberFormattingOptions::DefaultNoGrouping())
			));
			*bShowedDialog = true;
		}
		GDALClose(NewDataset);
		free(HeightmapData);
		return false;
	}


	GDALDataset *TIFDataset = TIFDriver->CreateCopy(TCHAR_TO_UTF8(*OutputFile), NewDataset, 1, nullptr, nullptr, nullptr);
	GDALClose(NewDataset);

	if (!TIFDataset)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("MapboxHelpers::5", "Could not write heightmap to file {0}."),
				FText::FromString(OutputFile),
				FText::AsNumber(WriteErr, &FNumberFormattingOptions::DefaultNoGrouping())
			));
			*bShowedDialog = true;
		}
		free(HeightmapData);
		return false;
	}
	
	GDALClose(TIFDataset);
	return true;
}

#undef LOCTEXT_NAMESPACE