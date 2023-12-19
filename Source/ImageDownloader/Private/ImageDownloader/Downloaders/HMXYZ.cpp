// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Downloaders/HMXYZ.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"

#include "ConsoleHelpers/Console.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "FileDownloader/Download.h"
#include "GDALInterface/GDALInterface.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "HAL/FileManagerGeneric.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"


bool DecodeMapboxOneBand(FString InputFile, FString OutputFile, bool *bShowedDialog)
{
	/* Read the RGB bands from `InputFile` */

	GDALDataset *Dataset = (GDALDataset *) GDALOpen(TCHAR_TO_UTF8(*InputFile), GA_ReadOnly);

	if (!Dataset)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("HMDecodeMapbox::Fetch::6", "Could not read file {0} using GDAL."),
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
				LOCTEXT("HMDecodeMapbox::Fetch::0", "Expected one band from Mapbox heightmap {0}, but got {1} instead."),
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
				LOCTEXT("HMDecodeMapbox::Fetch::00", "Expected to have a Color Table in the Mapbox heightmap {0}."),
				FText::FromString(InputFile)
			));
			*bShowedDialog = true;
		}
        GDALClose(Dataset);
        return false;
    }

	
	int SizeX = Dataset->GetRasterXSize();
	int SizeY = Dataset->GetRasterYSize();
	
	uint8* CodeBand = (uint8*) malloc(SizeX * SizeY * (sizeof uint8));

	if (!CodeBand)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				LOCTEXT("HMDecodeMapbox::Fetch::7", "Not enough memory to allocate for decoding mapbox data.")
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
				LOCTEXT("HMDecodeMapbox::Fetch::7", "There was an error while reading heightmap data from file {0}."),
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
				LOCTEXT("HMDecodeMapbox::Fetch::3", "Could not load GDAL drivers.")
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
				LOCTEXT("HMDecodeMapbox::Fetch::12", "Not enough memory to allocate for decoding mapbox data."),
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
						LOCTEXT("HMDecodeMapbox::Fetch::12", "No Color Entry for X = {0}, Y = {1}, Index = {2}, i = {3}, ColorTable Entry Count: {4}"),
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
		GDT_UInt16,
		nullptr
	);
	

	if (!NewDataset)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("HMDecodeMapbox::Fetch::4", "There was an error while creating a GDAL Dataset."),
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
				LOCTEXT("HMDecodeMapbox::Fetch::5", "There was an error while writing heightmap data to file {0}. (Error: {1})"),
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
				LOCTEXT("HMDecodeMapbox::Fetch::5", "Could not write heightmap to file {0}."),
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

bool DecodeMapboxThreeBands(FString InputFile, FString OutputFile, bool *bShowedDialog)
{
	/* Read the RGB bands from `InputFile` */

	GDALDataset *Dataset = (GDALDataset *) GDALOpen(TCHAR_TO_UTF8(*InputFile), GA_ReadOnly);

	if (!Dataset)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("HMDecodeMapbox::Fetch::6", "Could not read file {0} using GDAL."),
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
				LOCTEXT("HMDecodeMapbox::Fetch::0", "Expected at least three bands from Mapbox heightmap {0}, but got {1} instead."),
				FText::FromString(InputFile),
				FText::AsNumber(Dataset->GetRasterCount())
			));
			*bShowedDialog = true;
		}
		return false;
	}
	
	int SizeX = Dataset->GetRasterXSize();
	int SizeY = Dataset->GetRasterYSize();
	
	uint8* RedBand = (uint8*) malloc(SizeX * SizeY * (sizeof uint8));
	uint8* GreenBand = (uint8*) malloc(SizeX * SizeY * (sizeof uint8));
	uint8* BlueBand = (uint8*) malloc(SizeX * SizeY * (sizeof uint8));

	if (!RedBand || !GreenBand || !BlueBand)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				LOCTEXT("HMDecodeMapbox::Fetch::7", "Not enough memory to allocate for decoding mapbox data.")
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
				LOCTEXT("HMDecodeMapbox::Fetch::7", "There was an error while reading heightmap data from file {0}."),
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
				LOCTEXT("HMDecodeMapbox::Fetch::3", "Could not load GDAL drivers.")
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
				LOCTEXT("HMDecodeMapbox::Fetch::12", "Not enough memory to allocate for decoding mapbox data."),
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
		GDT_UInt16,
		nullptr
	);
	

	if (!NewDataset)
	{
		if (!*bShowedDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("HMDecodeMapbox::Fetch::4", "There was an error while creating a GDAL Dataset."),
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
				LOCTEXT("HMDecodeMapbox::Fetch::5", "There was an error while writing heightmap data to file {0}. (Error: {1})"),
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
				LOCTEXT("HMDecodeMapbox::Fetch::5", "Could not write heightmap to file {0}."),
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


void HMXYZ::Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	FString XYZFolder = FPaths::Combine(Directories::ImageDownloaderDir(), Name + "-XYZ");

	if (!IPlatformFile::GetPlatformPhysical().DeleteDirectoryRecursively(*XYZFolder) || !IPlatformFile::GetPlatformPhysical().CreateDirectory(*XYZFolder))
	{
		Directories::CouldNotInitializeDirectory(XYZFolder);
		if (OnComplete) OnComplete(false);
		return;
	}

	if (!bGeoreferenceSlippyTiles && CRS.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("HMXYZ::Fetch::CRS", "Please provide a valid CRS for your XYZ tiles.")
		);
		if (OnComplete) OnComplete(false);
		return;
	}
	
	if (bGeoreferenceSlippyTiles)
	{
		OutputCRS = "EPSG:4326";
	}
	else
	{
		OutputCRS = CRS;
	}
	
	if (MinX > MaxX || MinY > MaxY)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("HMXYZ::Fetch::Tiles", "For XYZ tiles, MinX ({0}) must be <= than MaxX ({0}), and MinY ({0}) must be <= MaxY ({0})."),
			FText::AsNumber(MinX),
			FText::AsNumber(MaxX),
			FText::AsNumber(MinY),
			FText::AsNumber(MaxY)
		));
		if (OnComplete) OnComplete(false);
		return;
	}

	int NumTiles = (MaxX - MinX + 1) * (MaxY - MinY + 1);

	bool *bShowedDialog = new bool(false);

	FScopedSlowTask *Task = new FScopedSlowTask(NumTiles,
		FText::Format(
			LOCTEXT("HMXYZ::Fetch::Task", "Downloading and Georeferencing {0} Tiles"),
			FText::AsNumber(NumTiles)
		)
	);
	Task->MakeDialog();

	Concurrency::RunMany(
		NumTiles,

		[this, Task, XYZFolder, bShowedDialog](int i, TFunction<void(bool)> OnCompleteElement)
		{
			int X = i % (MaxX - MinX + 1) + MinX;
			int Y = i / (MaxX - MinX + 1) + MinY;

			FString ReplacedURL =
				URL.Replace(TEXT("{z}"), *FString::FromInt(Zoom))
				   .Replace(TEXT("{x}"), *FString::FromInt(X))
				   .Replace(TEXT("{y}"), *FString::FromInt(Y));
			FString DownloadFile = FPaths::Combine(Directories::DownloadDir(), FString::Format(TEXT("{0}-{1}-{2}-{3}.{4}"), { Layer, Zoom, X, Y, Format }));
			int XOffset = X - MinX;
			int YOffset = bMaxY_IsNorth ? MaxY - Y : Y - MinY;

			if (Format.Contains("."))
			{
				if (!Console::ExecProcess(TEXT("7z"), TEXT(""), false))
				{
					if (!(*bShowedDialog))
					{
						*bShowedDialog = true;
						FMessageDialog::Open(EAppMsgType::Ok,
							LOCTEXT(
								"MissingRequirement",
								"Please make sure 7z is installed on your computer and available in your PATH if you want to use a compressed format."
							)
						);
					}

					if (OnCompleteElement) OnCompleteElement(false);
					return;
				}
			}

			FString FileName = FString::Format(TEXT("{0}_x{1}_y{2}"), { Name, XOffset, YOffset });

			Download::FromURL(ReplacedURL, DownloadFile, false,
				[this, Task, bShowedDialog, OnCompleteElement, ReplacedURL, DownloadFile, FileName, XYZFolder, X, Y](bool bOneSuccess)
				{
					if (bOneSuccess)
					{
						FString DecodedFile = DownloadFile;

						if (bDecodeMapbox)
						{
							DecodedFile = FPaths::Combine(Directories::DownloadDir(), FString::Format(TEXT("MapboxTerrainDEMV1-{0}-{1}-{2}-decoded.tif"), { Zoom, X, Y }));
							if (!DecodeMapboxThreeBands(DownloadFile, DecodedFile, bShowedDialog))
							{
								if (!(*bShowedDialog))
								{
									*bShowedDialog = true;
									FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
										LOCTEXT("HMXYZ::Fetch::Decode", "Could not decode file {0}."),
										FText::FromString(DownloadFile)
									));
								}
								if (OnCompleteElement) OnCompleteElement(false);
								return;
							}
						}

						if (Format.Contains("."))
						{
							FString ExtractionDir = FPaths::Combine(Directories::DownloadDir(), FString::Format(TEXT("{0}-{1}-{2}-{3}"), { Layer, Zoom, X, Y }));
							FString ExtractParams = FString::Format(TEXT("x -aos \"{0}\" -o\"{1}\""), { DownloadFile, ExtractionDir });

							if (!Console::ExecProcess(TEXT("7z"), *ExtractParams))
							{
								if (OnCompleteElement) OnCompleteElement(false);
								return;
							}

							TArray<FString> TileFiles;
							FString ImageFormat = Format.Left(Format.Find(FString(".")));

							FFileManagerGeneric::Get().FindFilesRecursive(TileFiles, *ExtractionDir, *(FString("*.") + ImageFormat), true, false);

							if (TileFiles.Num() != 1)
							{
								if (!(*bShowedDialog))
								{
									*bShowedDialog = true;
									FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
										LOCTEXT("HMXYZ::Fetch::Extract", "Expected one {0} file inside the archive {1}, but found {2}."),
										FText::FromString(ImageFormat),
										FText::FromString(DownloadFile),
										FText::AsNumber(TileFiles.Num())
									));
								}
								if (OnCompleteElement) OnCompleteElement(false);
								return;
							}

							DecodedFile = TileFiles[0];
						}
						
						if (bGeoreferenceSlippyTiles)
						{
							double n = 1 << Zoom;
							double MinLong = X / n * 360.0 - 180;
							double MaxLong = (X + 1) / n * 360.0 - 180;
							double MinLatRad = FMath::Atan(FMath::Sinh(UE_PI * (1 - 2 * (Y + 1) / n)));
							double MaxLatRad = FMath::Atan(FMath::Sinh(UE_PI * (1 - 2 * Y / n)));
							double MinLat = FMath::RadiansToDegrees(MinLatRad);
							double MaxLat = FMath::RadiansToDegrees(MaxLatRad);
							FString OutputFile = FPaths::Combine(XYZFolder, FileName + ".tif");
							if (!GDALInterface::AddGeoreference(DecodedFile, OutputFile, "EPSG:4326", MinLong, MaxLong, MinLat, MaxLat))
							{
								if (OnCompleteElement) OnCompleteElement(false);
								return;
							}

							OutputFiles.Add(OutputFile);
						}
						else
						{
							FString OutputFile = FPaths::Combine(XYZFolder, FileName + FPaths::GetExtension(DecodedFile, true));
							if (IFileManager::Get().Copy(*OutputFile, *DecodedFile) != COPY_OK)
							{
								if (OnCompleteElement) OnCompleteElement(false);
								return;
							}

							OutputFiles.Add(OutputFile);
						}
					}

					Task->EnterProgressFrame(1);

					if (OnCompleteElement) OnCompleteElement(bOneSuccess);
				}
			);
		},

		[OnComplete, Task, bShowedDialog](bool bSuccess)
		{
			Task->Destroy();
			if (bShowedDialog) delete(bShowedDialog);
			if (OnComplete) OnComplete(bSuccess);
		}
	);
}

#undef LOCTEXT_NAMESPACE
