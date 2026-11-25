// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "MapboxHelpers/MapboxHelpers.h"
#include "MapboxHelpers/LogMapboxHelpers.h"
#include "GDALInterface/GDALInterface.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Misc/MessageDialog.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"
#include "HAL/PlatformProcess.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#elif PLATFORM_LINUX
#include <dlfcn.h>
#endif

#define LOCTEXT_NAMESPACE "FMapboxHelpersModule"

bool MapboxHelpers::DecodeMapboxOneBand(FString InputFile, FString OutputFile, bool bUseTerrariumFormula, bool *bShowedDialog)
{
	/* Read the RGB bands from `InputFile` */

	GDALDataset *Dataset = (GDALDataset *) GDALOpen(TCHAR_TO_UTF8(*InputFile), GA_ReadOnly);

	if (!Dataset)
	{
		LCReporter::ShowOneError(
			FText::Format(
				LOCTEXT("MapboxHelpers::6", "Could not read file {0} using GDAL:\n{1}"),
				FText::FromString(InputFile),
				FText::FromString(CPLGetLastErrorMsg())
			),
			bShowedDialog
		);
		return false;
	}


	if (Dataset->GetRasterCount() != 1)
	{
		LCReporter::ShowOneError(
			FText::Format(
				LOCTEXT("MapboxHelpers::0", "Expected one band from Mapbox heightmap {0}, but got {1} instead."),
				FText::FromString(InputFile),
				FText::AsNumber(Dataset->GetRasterCount())
			),
			bShowedDialog
		);
		return false;
	}


	GDALColorTable* ColorTable = Dataset->GetRasterBand(1)->GetColorTable();
	if (!ColorTable)
	{
		LCReporter::ShowOneError(
			FText::Format(
				LOCTEXT("MapboxHelpers::00", "Expected to have a Color Table in the heightmap {0}."),
				FText::FromString(InputFile)
			),
			bShowedDialog
		);
		GDALClose(Dataset);
		return false;
	}

	
	int SizeX = Dataset->GetRasterXSize();
	int SizeY = Dataset->GetRasterYSize();
	
	uint8* CodeBand = (uint8*) malloc(SizeX * SizeY * (sizeof (uint8)));

	if (!CodeBand)
	{
		LCReporter::ShowOneError(LOCTEXT("MapboxHelpers::7", "Not enough memory to allocate for decoding data."), bShowedDialog);
		GDALClose(Dataset);
		if (CodeBand) free(CodeBand);
		return false;
	}

	
	CPLErr ReadErr = Dataset->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, SizeX, SizeY, CodeBand, SizeX, SizeY, GDT_Byte, 0, 0);

	if (ReadErr != CE_None)
	{
		LCReporter::ShowOneError(
			FText::Format(
				LOCTEXT("MapboxHelpers::7", "There was an error while reading heightmap data from file {0}."),
				FText::FromString(OutputFile)
			),
			bShowedDialog
		);
		free(CodeBand);
		GDALClose(Dataset);
		return false;
	}
	


	/* Prepare GDAL Drivers */

	GDALDriver *MEMDriver = GetGDALDriverManager()->GetDriverByName("MEM");
	GDALDriver *TIFDriver = GetGDALDriverManager()->GetDriverByName("GTiff");

	if (!TIFDriver || !MEMDriver)
	{
		LCReporter::ShowOneError(LOCTEXT("MapboxHelpers::3", "Could not load GDAL drivers."), bShowedDialog);
		GDALClose(Dataset);
		free(CodeBand);
		return false;
	}
	


	/* Decode and write the HeightmapData to `OutputFile` */

	float* HeightmapData = (float*) malloc(SizeX * SizeY * sizeof(float));

	if (!HeightmapData)
	{
		LCReporter::ShowOneError(
			FText::Format(
				LOCTEXT("MapboxHelpers::12", "Not enough memory to allocate for decoding data."),
				FText::FromString(OutputFile)
			),
			bShowedDialog
		);
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
				LCReporter::ShowOneError(
					FText::Format(
						LOCTEXT("MapboxHelpers::12", "No Color Entry for X = {0}, Y = {1}, Index = {2}, i = {3}, ColorTable Entry Count: {4}"),
						FText::AsNumber(X),
						FText::AsNumber(Y),
						FText::AsNumber(CodeBand[i]),
						FText::AsNumber(i),
						FText::AsNumber(ColorTable->GetColorEntryCount())
					),
					bShowedDialog
				);
				free(CodeBand);
				GDALClose(Dataset);
				return false;
			}
			float R = ColorEntry->c1;
			float G = ColorEntry->c2;
			float B = ColorEntry->c3;

			if (bUseTerrariumFormula)
				HeightmapData[i] = (R * 256 + G + B / 256) - 32768;
			else
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
		LCReporter::ShowOneError(
			FText::Format(
				LOCTEXT("MapboxHelpers::4", "There was an error while creating a GDAL Dataset."),
				FText::FromString(InputFile)
			),
			bShowedDialog
		);
		free(HeightmapData);
		return false;
	}
	CPLErr WriteErr = NewDataset->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, SizeX, SizeY, HeightmapData, SizeX, SizeY, GDT_Float32, 0, 0);


	if (WriteErr != CE_None)
	{
		LCReporter::ShowOneError(
			FText::Format(
				LOCTEXT("MapboxHelpers::5", "There was an error while writing heightmap data to file {0}. (Error: {1})"),
				FText::FromString(OutputFile),
				FText::AsNumber(WriteErr, &FNumberFormattingOptions::DefaultNoGrouping())
			),
			bShowedDialog
		);
		GDALClose(NewDataset);
		free(HeightmapData);
		return false;
	}


	GDALDataset *TIFDataset = TIFDriver->CreateCopy(TCHAR_TO_UTF8(*OutputFile), NewDataset, 1, nullptr, nullptr, nullptr);
	GDALClose(NewDataset);

	if (!TIFDataset)
	{
		LCReporter::ShowOneError(
			FText::Format(
				LOCTEXT("MapboxHelpers::5", "Could not write heightmap to file {0}."),
				FText::FromString(OutputFile),
				FText::AsNumber(WriteErr, &FNumberFormattingOptions::DefaultNoGrouping())
			),
			bShowedDialog
		);
		free(HeightmapData);
		return false;
	}

	
	GDALClose(TIFDataset);

	return true;
}

bool MapboxHelpers::DecodeMapboxThreeBands(FString InputFile, FString OutputFile, bool bUseTerrariumFormula, bool *bShowedDialog)
{
	/* Read the RGB bands from `InputFile` */

	GDALDataset *Dataset = (GDALDataset *) GDALOpen(TCHAR_TO_UTF8(*InputFile), GA_ReadOnly);

	if (!Dataset)
	{
		LCReporter::ShowOneError(
			FText::Format(
				LOCTEXT("MapboxHelpers::6", "Could not read (three bands) file {0} using GDAL.\n{1}"),
				FText::FromString(InputFile),
				FText::FromString(CPLGetLastErrorMsg())
			),
			bShowedDialog
		);
		return false;
	}

	if (Dataset->GetRasterCount() < 3)
	{
		LCReporter::ShowOneError(
			FText::Format(
				LOCTEXT("MapboxHelpers::0", "Expected at least three bands from heightmap {0}, but got {1} instead."),
				FText::FromString(InputFile),
				FText::AsNumber(Dataset->GetRasterCount())
			),
			bShowedDialog
		);
		return false;
	}
	
	int SizeX = Dataset->GetRasterXSize();
	int SizeY = Dataset->GetRasterYSize();
	
	uint8* RedBand = (uint8*) malloc(SizeX * SizeY * (sizeof (uint8)));
	uint8* GreenBand = (uint8*) malloc(SizeX * SizeY * (sizeof (uint8)));
	uint8* BlueBand = (uint8*) malloc(SizeX * SizeY * (sizeof (uint8)));

	if (!RedBand || !GreenBand || !BlueBand)
	{
		LCReporter::ShowOneError(
			LOCTEXT("MapboxHelpers::7", "Not enough memory to allocate for decoding data."),
			bShowedDialog
		);
		GDALClose(Dataset);
		if (RedBand) free(RedBand);
		if (GreenBand) free(GreenBand);
		if (BlueBand) free(BlueBand);
		return false;
	}
	
	CPLErr ReadErr1 = Dataset->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, SizeX, SizeY, RedBand, SizeX, SizeY, GDT_Byte, 0, 0);
	CPLErr ReadErr2 = Dataset->GetRasterBand(2)->RasterIO(GF_Read, 0, 0, SizeX, SizeY, GreenBand, SizeX, SizeY, GDT_Byte, 0, 0);
	CPLErr ReadErr3 = Dataset->GetRasterBand(3)->RasterIO(GF_Read, 0, 0, SizeX, SizeY, BlueBand, SizeX, SizeY, GDT_Byte, 0, 0);
	GDALClose(Dataset);

	if (ReadErr1 != CE_None || ReadErr2 != CE_None || ReadErr3 != CE_None)
	{
		LCReporter::ShowOneError(
			FText::Format(
				LOCTEXT("MapboxHelpers::7", "There was an error while reading heightmap data from file {0}."),
				FText::FromString(OutputFile)
			),
			bShowedDialog
		);
		free(RedBand);
		free(GreenBand);
		free(BlueBand);
		return false;
	}
	


	/* Prepare GDAL Drivers */

	GDALDriver *MEMDriver = GetGDALDriverManager()->GetDriverByName("MEM");
	GDALDriver *TIFDriver = GetGDALDriverManager()->GetDriverByName("GTiff");

	if (!TIFDriver || !MEMDriver)
	{
		LCReporter::ShowOneError(LOCTEXT("MapboxHelpers::3", "Could not load GDAL drivers."), bShowedDialog);
		free(RedBand);
		free(GreenBand);
		free(BlueBand);
		return false;
	}
	


	/* Decode and write the HeightmapData to `OutputFile` */

	float* HeightmapData = (float*) malloc(SizeX * SizeY * sizeof(float));

	if (!HeightmapData)
	{
		LCReporter::ShowOneError(
			FText::Format(
				LOCTEXT("MapboxHelpers::12", "Not enough memory to allocate for decoding data."),
				FText::FromString(OutputFile)
			),
			bShowedDialog
		);
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
			if (bUseTerrariumFormula)
			{
				HeightmapData[i] = (R * 256 + G + B / 256) - 32768;			
			}
			else
			{
				HeightmapData[i] = -10000 + ((R * 256 * 256 + G * 256 + B) * 0.1);
			}
		}
	}
	free(RedBand);
	free(GreenBand);
	free(BlueBand);
	
	GDALDataset *NewDataset = MEMDriver->Create(
		"",
		SizeX, SizeY,
		1, // nBands
		GDT_Float32,
		nullptr
	);
	

	if (!NewDataset)
	{
		LCReporter::ShowOneError(
			FText::Format(
				LOCTEXT("MapboxHelpers::4", "There was an error while creating a GDAL Dataset."),
				FText::FromString(InputFile)
			),
			bShowedDialog
		);
		free(HeightmapData);
		return false;
	}
	CPLErr WriteErr = NewDataset->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, SizeX, SizeY, HeightmapData, SizeX, SizeY, GDT_Float32, 0, 0);


	if (WriteErr != CE_None)
	{
		LCReporter::ShowOneError(
			FText::Format(
				LOCTEXT("MapboxHelpers::5", "There was an error while writing heightmap data to file {0}. (Error: {1})"),
				FText::FromString(OutputFile),
				FText::AsNumber(WriteErr, &FNumberFormattingOptions::DefaultNoGrouping())
			),
			bShowedDialog
		);
		GDALClose(NewDataset);
		free(HeightmapData);
		return false;
	}

	GDALDataset *TIFDataset = TIFDriver->CreateCopy(TCHAR_TO_UTF8(*OutputFile), NewDataset, 1, nullptr, nullptr, nullptr);
	GDALClose(NewDataset);

	if (!TIFDataset)
	{
		LCReporter::ShowOneError(
			FText::Format(
				LOCTEXT("MapboxHelpers::5", "Could not write heightmap to file {0}."),
				FText::FromString(OutputFile),
				FText::AsNumber(WriteErr, &FNumberFormattingOptions::DefaultNoGrouping())
			),
			bShowedDialog
		);
		free(HeightmapData);
		return false;
	}
	
	GDALClose(TIFDataset);
	return true;
}

bool MapboxHelpers::DecodeLERC(const FString& InputFile, const FString& OutputFile, bool bAllowInvalidTiles, bool* BShowedDialog)
{
	const auto fail = [&](const FString& Message)
	{
		UE_LOG(LogMapboxHelpers, Error, TEXT("%s: %s"), *InputFile, *Message);
		if (!bAllowInvalidTiles) LCReporter::ShowOneError(FText::FromString(Message), BShowedDialog);
		return false;
	};
	TArray<uint8> Blob;
	if (!FFileHelper::LoadFileToArray(Blob, *InputFile) || Blob.IsEmpty())
	{
		return fail(TEXT("Could not read the LERC tile."));
	}
	using FGetBlobInfo = unsigned int (*)(const unsigned char*, unsigned int, unsigned int*, double*, int, int);
	using FDecode = unsigned int (*)(const unsigned char*, unsigned int, int, unsigned char*, int, int, int, int, unsigned int, void*);
#if PLATFORM_WINDOWS
	const HMODULE Library = ::GetModuleHandleW(L"Lerc.dll");
	if (!Library) return fail(TEXT("Lerc.dll is not loaded by the plugin."));
	const FGetBlobInfo GetBlobInfo = reinterpret_cast<FGetBlobInfo>(FPlatformProcess::GetDllExport(Library, TEXT("lerc_getBlobInfo")));
	const FDecode Decode = reinterpret_cast<FDecode>(FPlatformProcess::GetDllExport(Library, TEXT("lerc_decode")));
#elif PLATFORM_LINUX
	const FGetBlobInfo GetBlobInfo = reinterpret_cast<FGetBlobInfo>(dlsym(RTLD_DEFAULT, "lerc_getBlobInfo"));
	const FDecode Decode = reinterpret_cast<FDecode>(dlsym(RTLD_DEFAULT, "lerc_decode"));
#else
	return fail(TEXT("LERC decoding is not supported on this platform."));
#endif
	if (!GetBlobInfo || !Decode) return fail(TEXT("The loaded LERC library is missing decoder entry points."));

	unsigned int Info[11] = {};
	double Ranges[3] = {};
	if (GetBlobInfo(Blob.GetData(), Blob.Num(), Info, Ranges, 11, 3) != 0)
	{
		return fail(TEXT("Invalid or truncated LERC tile."));
	}
	// Terrain3D stores 257 vertex samples over 256 cells, in one Float32 band.
	if (Info[1] != 6 || Info[2] != 1 || Info[3] != 257 || Info[4] != 257 || Info[5] != 1 || Info[8] > 1 || Info[10] != 0)
	{
		return fail(TEXT("Expected a single-band 257 x 257 Float32 Terrain3D tile."));
	}
	if (Info[6] == 0) return fail(TEXT("The elevation tile contains only NoData."));
	constexpr int Size = 257;
	TArray<float> Heights;
	Heights.SetNumUninitialized(Size * Size);
	TArray<uint8> Valid;
	Valid.Init(1, Size * Size);
	if (Decode(Blob.GetData(), Blob.Num(), Info[8], Info[8] ? Valid.GetData() : nullptr,
		1, Size, Size, 1, 6, Heights.GetData()) != 0)
	{
		return fail(TEXT("LERC elevation decoding failed."));
	}
	constexpr float NoData = -3.402823466e+38F;
	int ValidCount = 0;
	for (int Index = 0; Index < Heights.Num(); ++Index)
	{
		if (!Valid[Index] || !FMath::IsFinite(Heights[Index])) Heights[Index] = NoData;
		else ++ValidCount;
	}
	if (ValidCount == 0) return fail(TEXT("The tile contains no finite valid elevation values."));

	GDALDriver* Driver = GetGDALDriverManager()->GetDriverByName("GTiff");
	GDALDataset* Dataset = Driver ? Driver->Create(TCHAR_TO_UTF8(*OutputFile), Size, Size, 1, GDT_Float32, nullptr) : nullptr;
	if (!Dataset) return fail(TEXT("Could not create the decoded Float32 TIFF."));
	ON_SCOPE_EXIT { GDALClose(Dataset); };
	GDALRasterBand* Band = Dataset->GetRasterBand(1);
	// A separate NoData value keeps valid sea-level zero samples intact.
	if (Band->SetNoDataValue(NoData) != CE_None ||
		Band->RasterIO(GF_Write, 0, 0, Size, Size, Heights.GetData(), Size, Size, GDT_Float32, 0, 0) != CE_None ||
		Dataset->FlushCache() != CE_None)
	{
		return fail(TEXT("Could not write the decoded elevation TIFF."));
	}
	return true;
}

#undef LOCTEXT_NAMESPACE