// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "GDALInterface/GDALInterface.h"
#include "GDALInterface/LogGDALInterface.h"

#include "FileDownloader/Download.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "Internationalization/Regex.h"
#include "Internationalization/TextLocalizationResource.h" 
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Misc/Paths.h"
#include "Misc/MessageDialog.h"

#include "HAL/FileManager.h"

#define LOCTEXT_NAMESPACE "FGDALInterfaceModule"

bool GDALInterface::SetWellKnownGeogCRS(OGRSpatialReference& InRs, FString CRS)
{
	OGRErr Err = InRs.SetWellKnownGeogCS(TCHAR_TO_ANSI(*CRS));

	if (Err != OGRERR_NONE)
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("SetWellKnownGeogCRS", "Unable to set spatial reference from string: {0}.\nError: {1}"),
			FText::FromString(CRS),
			FText::AsNumber(Err, &FNumberFormattingOptions::DefaultNoGrouping())
		));
		return false;
	}

	InRs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
	return true;
}

bool GDALInterface::HasCRS(FString File)
{
	GDALDataset* Dataset = (GDALDataset *) GDALOpen(TCHAR_TO_UTF8(*File), GA_ReadOnly);

	if (!Dataset) return false;

	const char* ProjectionRef = Dataset->GetProjectionRef();
	OGRSpatialReference UnusedRs;
	bool bHasCRS = UnusedRs.importFromWkt(ProjectionRef) == OGRERR_NONE;
	GDALClose(Dataset);
	return bHasCRS;
}

bool GDALInterface::SetCRSFromFile(OGRSpatialReference &InRs, FString File, bool bDialog)
{
	GDALDataset* Dataset = (GDALDataset *) GDALOpen(TCHAR_TO_UTF8(*File), GA_ReadOnly);
	if (!Dataset)
	{
		if (bDialog)
		{
			LCReporter::ShowError(FText::Format(
				LOCTEXT("GetSpatialReferenceError", "Unable to open file {0} to read its spatial reference."),
				FText::FromString(File)
			));
		}
		return false;
	}

	bool bSuccess = SetCRSFromDataset(InRs, Dataset, bDialog);
	GDALClose(Dataset);
	return bSuccess;
}

bool GDALInterface::SetCRSFromDataset(OGRSpatialReference& InRs, GDALDataset* Dataset, bool bDialog)
{
	if (!Dataset)
	{
		if (bDialog)
		{
			LCReporter::ShowError(
				LOCTEXT("GetSpatialReferenceError", "Unable to get spatial reference from dataset (null pointer).")
			);
		}
		return false;
	}

	const char* ProjectionRef = Dataset->GetProjectionRef();
	OGRErr Err = InRs.importFromWkt(ProjectionRef);

	if (Err != OGRERR_NONE)
	{
		if (bDialog)
		{
			LCReporter::ShowError(FText::Format(
				LOCTEXT("GetSpatialReferenceError2", "Unable to get spatial reference from dataset (Error {0})."),
				FText::AsNumber(Err, &FNumberFormattingOptions::DefaultNoGrouping())
			));
		}
		return false;
	}
	InRs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

	return true;
}

bool GDALInterface::AddGeoreference(FString InputFile, FString OutputFile, FString CRS, double MinLong, double MaxLong, double MinLat, double MaxLat)
{
	FString TempFile = FPaths::Combine(FPaths::GetPath(InputFile), FPaths::GetBaseFilename(InputFile) + "-Temp.tif");

	FIntPoint Pixels;
	if (!GetPixels(Pixels, InputFile))
	{
		return false;
	}

	bool bTranslateSuccess =
		GDALInterface::Translate(InputFile, TempFile, {
			"-of",
			"GTiff",
			"-a_srs",
			CRS,
			"-a_ullr",
			FString::SanitizeFloat(MinLong),
			FString::SanitizeFloat(MaxLat),
			FString::SanitizeFloat(MaxLong),
			FString::SanitizeFloat(MinLat)
		});

	if (!bTranslateSuccess)
	{
		return false;
	}

	bool bWarpSuccess = GDALInterface::Warp(TempFile, OutputFile, {
		"-t_srs",
		CRS,
		"-ts",
		FString::FromInt(Pixels[0]),
		FString::FromInt(Pixels[1])
	});

	IFileManager::Get().Delete(*TempFile);

	return bWarpSuccess;
}

bool GDALInterface::SetCRSFromEPSG(OGRSpatialReference& InRs, int EPSG)
{
	OGRErr Err = InRs.importFromEPSG(EPSG);
	if (Err != OGRERR_NONE)
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("StartupModuleError1", "Could not create spatial reference from EPSG {0} (Error {1})."),
				FText::AsNumber(EPSG, &FNumberFormattingOptions::DefaultNoGrouping()),
				FText::AsNumber(Err, &FNumberFormattingOptions::DefaultNoGrouping())
			)
		);
		return false;
	}
	InRs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
	return true;
}

bool GDALInterface::SetCRSFromUserInput(OGRSpatialReference& InRs, FString CRS, bool bDialog)
{
	OGRErr Err = InRs.SetFromUserInput(TCHAR_TO_ANSI(*CRS));
	if (Err != OGRERR_NONE)
	{
		if (bDialog)
		{
			LCReporter::ShowError(
				FText::Format(
					LOCTEXT("StartupModuleError1", "Could not create spatial reference from user input '{0}' (Error {1})."),
					FText::FromString(CRS),
					FText::AsNumber(Err, &FNumberFormattingOptions::DefaultNoGrouping())
				)
			);
		}
		return false;
	}
	InRs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
	return true;
}

bool GDALInterface::GetCoordinates(FVector4d& Coordinates, GDALDataset* Dataset)
{
	if (!Dataset) return false;

	double AdfTransform[6];
	if (Dataset->GetGeoTransform(AdfTransform) != CE_None)
	{
		UE_LOG(LogGDALInterface, Error, TEXT("GetGeoTransform failed on GDAL Dataset"));
		return false;
	}
		
	double LeftCoord	= AdfTransform[0];
	double RightCoord   = AdfTransform[0] + Dataset->GetRasterXSize()*AdfTransform[1];
	double TopCoord	 = AdfTransform[3];
	double BottomCoord  = AdfTransform[3] + Dataset->GetRasterYSize()*AdfTransform[5];
	
	Coordinates[0] = LeftCoord;
	Coordinates[1] = RightCoord;
	Coordinates[2] = BottomCoord;
	Coordinates[3] = TopCoord;
	return true;
}

bool GDALInterface::GetCoordinates(FVector4d& Coordinates, FString File)
{
	GDALDataset *Dataset = (GDALDataset *)GDALOpen(TCHAR_TO_UTF8(*File), GA_ReadOnly);
	if (!Dataset)
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("GetCoordinatesError", "Could not open file '{0}' to read the coordinates.\n{1}"),
				FText::FromString(File),
				FText::FromString(FString(CPLGetLastErrorMsg()))
			)
		);
		return false;
	}

	bool bSuccess = GDALInterface::GetCoordinates(Coordinates, Dataset);
	if (!bSuccess)
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("GetCoordinatesError", "Could not read coordinates from file '{0}'.\n{1}"),
				FText::FromString(File),
				FText::FromString(FString(CPLGetLastErrorMsg()))
			)
		);
	}
	GDALClose(Dataset);
	return bSuccess;
}

bool GDALInterface::GetCoordinates(FVector4d& Coordinates, TArray<FString> Files)
{
	double MinCoordWidth = DBL_MAX;
	double MaxCoordWidth = -DBL_MAX;
	double MinCoordHeight = DBL_MAX;
	double MaxCoordHeight = -DBL_MAX;

	if (Files.IsEmpty())
	{
		LCReporter::ShowError(
			LOCTEXT("GetCoordinatesError", "GetCoordinates requires a non-empty array of files.")
		);
		return false;
	}

	for (auto& File : Files)
	{
		FVector4d FileCoordinates;
		if (!GDALInterface::GetCoordinates(FileCoordinates, File)) return false;
		MinCoordWidth  = FMath::Min(MinCoordWidth, FileCoordinates[0]);
		MaxCoordWidth  = FMath::Max(MaxCoordWidth, FileCoordinates[1]);
		MinCoordHeight = FMath::Min(MinCoordHeight, FileCoordinates[2]);
		MaxCoordHeight = FMath::Max(MaxCoordHeight, FileCoordinates[3]);
	}

	Coordinates[0] = MinCoordWidth;
	Coordinates[1] = MaxCoordWidth;
	Coordinates[2] = MinCoordHeight;
	Coordinates[3] = MaxCoordHeight;

	return true;
}


OGRCoordinateTransformation *GDALInterface::MakeTransform(FString InCRS, FString OutCRS)
{
	OGRSpatialReference InRs, OutRs;
	if (
		!GDALInterface::SetCRSFromUserInput(InRs, InCRS) ||
		!GDALInterface::SetCRSFromUserInput(OutRs, OutCRS)
	)
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("GDALInterface::MakeTransform::1", "Internal error while setting CRS.\n{0}"),
				FText::FromString(FString(CPLGetLastErrorMsg()))
			)
		);
		return nullptr;
	}

	OGRCoordinateTransformation *CoordinateTransformation = OGRCreateCoordinateTransformation(&InRs, &OutRs);
	if (!CoordinateTransformation )
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("GDALInterface::MakeTransform::3", "Internal error while creating coordinate transformation.\n{0}"),
				FText::FromString(FString(CPLGetLastErrorMsg()))
			)
		);
		return nullptr;
	}

	return CoordinateTransformation;
}

bool GDALInterface::Transform(OGRCoordinateTransformation* CoordinateTransformation, double *Longitude, double *Latitude)
{
	if (!CoordinateTransformation)
	{
		LCReporter::ShowError(LOCTEXT("GDALInterface::Transform", "Invalid Coordinate Transformation"));
		return false;
	}
		
	if (!CoordinateTransformation->Transform(1, Longitude, Latitude))
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("GDALInterface::Transform", "Internal error while transforming coordinates ({0}, {1}).\n{2}"),
				FText::AsNumber(*Longitude),
				FText::AsNumber(*Latitude),
				FText::FromString(FString(CPLGetLastErrorMsg()))
			)
		);
		return false;
	}
	return true;
}

bool GDALInterface::Transform2(OGRCoordinateTransformation* CoordinateTransformation, double *xs, double *ys)
{
	if (!CoordinateTransformation || !CoordinateTransformation->Transform(2, xs, ys))
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("GDALInterface::Transform2", "Internal error while transforming coordinates.\n{0}"),
				FText::FromString(FString(CPLGetLastErrorMsg()))
			)
		);
		return false;
	}
	return true;
}

bool GDALInterface::ConvertCoordinates(double *Longitude, double *Latitude, FString InCRS, FString OutCRS)
{
	return Transform(MakeTransform(InCRS, OutCRS), Longitude, Latitude);
}

bool GDALInterface::ConvertCoordinates2(double *xs, double *ys, FString InCRS, FString OutCRS)
{
	return Transform2(MakeTransform(InCRS, OutCRS), xs, ys);
}

bool GDALInterface::ConvertCoordinates(FVector4d& OriginalCoordinates, bool bCrop, FVector4d& NewCoordinates, FString InCRS, FString OutCRS)
{
	OGRSpatialReference InRs, OutRs;
	if (!SetCRSFromUserInput(InRs, InCRS) || !SetCRSFromUserInput(OutRs, OutCRS)) return false;

	return ConvertCoordinates(OriginalCoordinates, bCrop, NewCoordinates, InRs, OutRs);
}

bool GDALInterface::ConvertCoordinates(FVector4d& OriginalCoordinates, FVector4d& Coordinates, FString InCRS, FString OutCRS)
{
	double MinCoordWidth = OriginalCoordinates[0];
	double MaxCoordWidth = OriginalCoordinates[1];
	double MinCoordHeight = OriginalCoordinates[2];
	double MaxCoordHeight = OriginalCoordinates[3];
	double xs[2] = { MinCoordWidth,  MaxCoordWidth  };
	double ys[2] = { MaxCoordHeight, MinCoordHeight };

	OGRSpatialReference InRs, OutRs;
	if (!SetCRSFromUserInput(InRs, InCRS) || !SetCRSFromUserInput(OutRs, OutCRS) || !OGRCreateCoordinateTransformation(&InRs, &OutRs)->Transform(2, xs, ys)) {
		LCReporter::ShowError(
			LOCTEXT("GDALInterface::ConvertCoordinates", "Internal error while transforming coordinates.")
		);
		return false;
	}

	Coordinates[0] = xs[0];
	Coordinates[1] = xs[1];
	Coordinates[2] = ys[1];
	Coordinates[3] = ys[0];
	return true;
}

bool GDALInterface::ConvertCoordinates(FVector4d& OriginalCoordinates, bool bCrop, FVector4d& NewCoordinates, OGRSpatialReference InRs, OGRSpatialReference OutRs)
{
	double MinCoordWidth = OriginalCoordinates[0];
	double MaxCoordWidth = OriginalCoordinates[1];
	double MinCoordHeight = OriginalCoordinates[2];
	double MaxCoordHeight = OriginalCoordinates [3];

	double xs[4] = { MinCoordWidth,  MinCoordWidth,  MaxCoordWidth,  MaxCoordWidth };
	double ys[4] = { MinCoordHeight, MaxCoordHeight, MaxCoordHeight, MinCoordHeight };

	if (!OGRCreateCoordinateTransformation(&InRs, &OutRs)->Transform(4, xs, ys))
	{
		LCReporter::ShowError(LOCTEXT("GDALInterface::ConvertCoordinates", "Internal error while transforming coordinates."));
		return false;
	}

	if (bCrop)
	{
		NewCoordinates[0] = FMath::Max(xs[0], xs[1]);
		NewCoordinates[1] = FMath::Min(xs[2], xs[3]);
		NewCoordinates[2] = FMath::Max(ys[0], ys[3]);
		NewCoordinates[3] = FMath::Min(ys[1], ys[2]);
	}
	else
	{
		NewCoordinates[0] = FMath::Min(xs[0], xs[1]);
		NewCoordinates[1] = FMath::Max(xs[2], xs[3]);
		NewCoordinates[2] = FMath::Min(ys[0], ys[3]);
		NewCoordinates[3] = FMath::Max(ys[1], ys[2]);
	}

	return true;
}

bool GDALInterface::GetPixels(FIntPoint& Pixels, FString File)
{
	GDALDataset *Dataset = (GDALDataset *)GDALOpen(TCHAR_TO_UTF8(*File), GA_ReadOnly);

	if (!Dataset) {
		LCReporter::ShowError(
			FText::Format(LOCTEXT("GetInsidePixelsError", "Could not open heightmap file '{0}' to read its size.\n{1}"),
				FText::FromString(File),
				FText::FromString(FString(CPLGetLastErrorMsg()))
			)
		);
		return false;
	}

	Pixels[0] = Dataset->GetRasterXSize();
	Pixels[1] = Dataset->GetRasterYSize();
	GDALClose(Dataset);
	return true;
}

bool GDALInterface::GetMinMax(FVector2D &MinMax, TArray<FString> Files)
{
	MinMax[0] = DBL_MAX;
	MinMax[1] = -DBL_MAX;

	for (auto& File : Files) {

		GDALDataset *Dataset = (GDALDataset *)GDALOpen(TCHAR_TO_UTF8(*File), GA_ReadOnly);

		if (!Dataset)
		{
			LCReporter::ShowError(
				FText::Format(LOCTEXT("GetMinMaxError", "Could not open heightmap file '{0}' to compute min/max altitudes.\n{1}"),
					FText::FromString(File),
					FText::FromString(FString(CPLGetLastErrorMsg()))
				)
			);
			return false;
		}
		
		double AdfMinMax[2];

		if (GDALComputeRasterMinMax(Dataset->GetRasterBand(1), false, AdfMinMax) != CE_None)
		{
			LCReporter::ShowError(
				FText::Format(LOCTEXT("GetMinMaxError2", "Could not compute min/max altitudes of file '{0}'.\n{1}"),
					FText::FromString(File),
					FText::FromString(FString(CPLGetLastErrorMsg()))
				)
			);
			GDALClose(Dataset);
			return false;
		}
		GDALClose(Dataset);

		MinMax[0] = FMath::Min(MinMax[0], AdfMinMax[0]);
		MinMax[1] = FMath::Max(MinMax[1], AdfMinMax[1]);
	}

	return true;
}

FString GDALInterface::GetColorInterpretation(const FString &File)
{
	GDALDataset *Dataset = (GDALDataset *)GDALOpen(TCHAR_TO_UTF8(*File), GA_ReadOnly);
	if (!Dataset) {
		LCReporter::ShowError(
			FText::Format(LOCTEXT("GetColorInterpretationError", "Could not open file '{0}' to read color interpretation.\n{1}"),
				FText::FromString(File),
				FText::FromString(FString(CPLGetLastErrorMsg()))
			)
		);
		return "";
	}

	if (Dataset->GetRasterCount() < 1)
	{
		LCReporter::ShowError(
			FText::Format(LOCTEXT("GetColorInterpretationError2", "File '{0}' should have at least one band to read color interpretation"),
				FText::FromString(File)
			)
		);
		GDALClose(Dataset);
		return "";
	}

	GDALRasterBand* Band = Dataset->GetRasterBand(1);
	if (!Band) {
		LCReporter::ShowError(
			FText::Format(LOCTEXT("GetColorInterpretationError3", "Could not get band for file '{0}'"),
				FText::FromString(File),
				FText::FromString(FString(CPLGetLastErrorMsg()))
			)
		);
		GDALClose(Dataset);
		return "";
	}

	GDALColorInterp ColorInterp = Band->GetColorInterpretation();
	const char* ColorInterpName = GDALGetColorInterpretationName(ColorInterp);
	GDALClose(Dataset);

	return FString(ColorInterpName);
}

bool GDALInterface::ConvertToPNG(FString SourceFile, FString TargetFile, int MinAltitude, int MaxAltitude, int PrecisionPercent)
{
	TArray<FString> Args;
	Args.Add("-scale");
	Args.Add(FString::SanitizeFloat(MinAltitude));
	Args.Add(FString::SanitizeFloat(MaxAltitude));
	Args.Add("0");
	Args.Add("65535");
	Args.Add("-outsize");
	Args.Add(FString::Format(TEXT("{0}%"), { PrecisionPercent }));
	Args.Add(FString::Format(TEXT("{0}%"), { PrecisionPercent }));
	Args.Add("-ot");
	Args.Add("UInt16");
	Args.Add("-of");
	Args.Add("PNG");

	return Translate(SourceFile, TargetFile, Args);
}

bool GDALInterface::ConvertToPNG(FString SourceFile, FString TargetFile)
{
	TArray<FString> Args;
	Args.Add("-ot");
	Args.Add("UInt16");
	Args.Add("-of");
	Args.Add("PNG");

	return Translate(SourceFile, TargetFile, Args);
}

bool GDALInterface::ChangeResolution(FString SourceFile, FString TargetFile, int PrecisionPercent)
{
	TArray<FString> Args;
	Args.Add("-outsize");
	Args.Add(FString::Format(TEXT("{0}%"), { PrecisionPercent }));
	Args.Add(FString::Format(TEXT("{0}%"), { PrecisionPercent }));
	return Translate(SourceFile, TargetFile, Args);
}

bool GDALInterface::Translate(FString SourceFile, FString TargetFile, TArray<FString> Args)
{
	UE_LOG(LogGDALInterface, Log, TEXT("Opening SourceFile: '%s'"), *SourceFile);
	GDALDatasetH SourceDataset = GDALOpen(TCHAR_TO_ANSI(*SourceFile), (GDALAccess) 0);

	if (!SourceDataset)
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("ConvertGDALOpenError", "Could not open file {0}."),
			FText::FromString(SourceFile)
		));
		return false;
	}
	UE_LOG(LogGDALInterface, Log, TEXT("Opened SourceFile: '%s'"), *SourceFile);

	char** TranslateArgv = nullptr;

	for (auto& Arg : Args)
	{
		TranslateArgv = CSLAddString(TranslateArgv, TCHAR_TO_UTF8(*Arg));
	}

	GDALTranslateOptions* Options = GDALTranslateOptionsNew(TranslateArgv, NULL);
	CSLDestroy(TranslateArgv);

	if (!Options)
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("ConvertParseOptionsError", "Internal GDAL error while parsing GDALTranslate options for file {0}."),
			FText::FromString(SourceFile)
		));
		GDALClose(SourceDataset);
		return false;
	}

	UE_LOG(LogGDALInterface, Log, TEXT("Translating using gdal_translate --config GDAL_PAM_ENABLED NO %s \"%s\" \"%s\""),
		*FString::Join(Args, TEXT(" ")),
		*SourceFile,
		*TargetFile
	);

	GDALDataset* DstDataset = (GDALDataset *) GDALTranslate(TCHAR_TO_UTF8(*TargetFile), SourceDataset, Options, nullptr);
	GDALClose(SourceDataset);
	GDALTranslateOptionsFree(Options);

	if (!DstDataset)
	{
		FString Error = FString(CPLGetLastErrorMsg());
		UE_LOG(LogGDALInterface, Error, TEXT("Error while translating: %s to %s:\n%s"), *SourceFile, *TargetFile, *Error);
		LCReporter::ShowError(FText::Format(
			LOCTEXT("ConvertGDALTranslateError",
				"Internal GDALTranslate error while converting dataset from file {0} to PNG.\nIt is possible that the source image is not a heightmap.\n{1}"),
			FText::FromString(SourceFile),
			FText::FromString(Error)
		));
		return false;
	}
		
	UE_LOG(LogGDALInterface, Log, TEXT("Finished translating: %s to %s"), *SourceFile, *TargetFile);
	GDALClose(DstDataset);

	return true;
}

bool GDALInterface::Merge(TArray<FString> SourceFiles, FString TargetFile)
{
	UE_LOG(LogGDALInterface, Log, TEXT("Merging %s into %s"), *FString::Join(SourceFiles, TEXT(", ")), *TargetFile);
	const int NumFiles = SourceFiles.Num();
	
	char** papszSrcDSNames = nullptr;
	for (int i = 0; i < NumFiles; i++)
	{
		papszSrcDSNames = CSLAddString(papszSrcDSNames, TCHAR_TO_UTF8(*SourceFiles[i]));
	}

	int pbUsageError;
	GDALDatasetH DatasetVRT = GDALBuildVRT(
		TCHAR_TO_UTF8(*TargetFile),
		SourceFiles.Num(),
		NULL,
		papszSrcDSNames,
		NULL,
		&pbUsageError
	);
	
	if (!DatasetVRT)
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("GDALInterfaceMergeError", "Could not merge the files {0}.\n{1}."),
			FText::FromString(FString::Join(SourceFiles, TEXT(", "))),
			FText::FromString(FString(CPLGetLastErrorMsg()))
		));
		return false;
	}

	int bandCount = GDALGetRasterCount(DatasetVRT);
	for(int i = 1; i <= bandCount; i++)
	{
		GDALRasterBandH hBand = GDALGetRasterBand(DatasetVRT, i);
		if (hBand != NULL)
		{
			GDALSetRasterNoDataValue(hBand, 0);
		}
	}

	GDALClose(DatasetVRT);
	UE_LOG(LogGDALInterface, Log, TEXT("Finished merging %s into %s"), *FString::Join(SourceFiles, TEXT(", ")), *TargetFile);
	return true;
}

bool GDALInterface::ReadHeightmapFromFile(FString File, int& OutWidth, int& OutHeight, TArray<float>& OutHeightmap)
{
	GDALDataset* Dataset = (GDALDataset*)GDALOpen(TCHAR_TO_UTF8(*File), GA_ReadOnly);

	if (!Dataset)
	{
		LCReporter::ShowError(
			FText::Format(LOCTEXT("GDALInterface::ReadHeightsFromFile", "Could not open file '{0}' to read heightmap."),
				FText::FromString(File)
			)
		);
		return false;
	}

	OutWidth = Dataset->GetRasterXSize();
	OutHeight = Dataset->GetRasterYSize();
	int NumBands = Dataset->GetRasterCount();

	if (NumBands != 1)
	{
		UE_LOG(LogGDALInterface, Error, TEXT("Could not read heightmap, expecting one band and got %d."), NumBands);
		GDALClose(Dataset);
		return false;
	}

	UE_LOG(LogGDALInterface, Log, TEXT("Reading heightmap from image %s of size %d x %d"), *File, OutWidth, OutHeight);

	float* Buffer = new float[OutWidth * OutHeight];

	if (Dataset->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, OutWidth, OutHeight, Buffer, OutWidth, OutHeight, GDT_Float32, 0, 0) != CE_None)
	{
		delete[] Buffer;
		GDALClose(Dataset);
		UE_LOG(LogGDALInterface, Error, TEXT("Could not read heightmap in buffer."));
		return false;
	}

	GDALClose(Dataset);

	OutHeightmap.Reset();
	OutHeightmap.SetNum(OutWidth * OutHeight);

	for (int i = 0; i < OutWidth; i++)
	{
		for (int j = 0; j < OutHeight; j++)
		{
			const int k = i + j * OutWidth;
			OutHeightmap[k] = Buffer[k];
		}
	}

	delete[] Buffer;

	return true;
}

bool GDALInterface::ReadColorsFromFile(FString File, int &OutWidth, int &OutHeight, TArray<FColor> &OutColors)
{
	GDALDataset *Dataset = (GDALDataset *)GDALOpen(TCHAR_TO_UTF8(*File), GA_ReadOnly);
	if (!Dataset)
	{
		LCReporter::ShowError(
			FText::Format(LOCTEXT("GDALInterface::ReadTextureFromFile", "Could not open file '{0}' to create a texture."),
				FText::FromString(File)
			)
		);
		return false;
	}
	
	OutWidth = Dataset->GetRasterXSize();
	OutHeight = Dataset->GetRasterYSize();
	int NumBands = Dataset->GetRasterCount();
	
	UE_LOG(LogGDALInterface, Log, TEXT("Reading colors from image %s of size %d x %d with %d band(s)"), *File, OutWidth, OutHeight, NumBands);

	uint8_t* RedBuffer = nullptr;
	uint8_t* GreenBuffer = nullptr;
	uint8_t* BlueBuffer = nullptr;
	uint8_t* AlphaBuffer = nullptr;

	bool bError = false;

	if (NumBands >= 1)
	{
		RedBuffer = new uint8_t[OutWidth * OutHeight];
		if (Dataset->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, OutWidth, OutHeight, RedBuffer, OutWidth, OutHeight, GDT_Byte, 0, 0) != CE_None)
		{
			UE_LOG(LogGDALInterface, Error, TEXT("Could not read red buffer."));
			bError = true;
		}
	}

	if (!bError && NumBands >= 2)
	{
		GreenBuffer = new uint8_t[OutWidth * OutHeight];
		if (Dataset->GetRasterBand(2)->RasterIO(GF_Read, 0, 0, OutWidth, OutHeight, GreenBuffer, OutWidth, OutHeight, GDT_Byte, 0, 0) != CE_None)
		{
			UE_LOG(LogGDALInterface, Error, TEXT("Could not read green buffer."));
			bError = true;
		}
	}

	if (!bError && NumBands >= 3)
	{
		BlueBuffer = new uint8_t[OutWidth * OutHeight];
		if (Dataset->GetRasterBand(3)->RasterIO(GF_Read, 0, 0, OutWidth, OutHeight, BlueBuffer, OutWidth, OutHeight, GDT_Byte, 0, 0) != CE_None)
		{
			UE_LOG(LogGDALInterface, Error, TEXT("Could not read blue buffer."));
			bError = true;
		}
	}

	if (!bError && NumBands >= 4)
	{
		AlphaBuffer = new uint8_t[OutWidth * OutHeight];
		if (Dataset->GetRasterBand(4)->RasterIO(GF_Read, 0, 0, OutWidth, OutHeight, AlphaBuffer, OutWidth, OutHeight, GDT_Byte, 0, 0) != CE_None)
		{
			UE_LOG(LogGDALInterface, Error, TEXT("Could not read alpha buffer."));
			bError = true;
		}
	}
	
	GDALClose(Dataset);

	if (bError)
	{
		if (RedBuffer) delete[] RedBuffer;
		if (GreenBuffer) delete[] GreenBuffer;
		if (BlueBuffer) delete[] BlueBuffer;
		if (AlphaBuffer) delete[] AlphaBuffer;
		return false;
	}

	OutColors.Reset();
	OutColors.SetNum(OutWidth * OutHeight);

	for (int i = 0; i < OutWidth; i++)
	{
		for (int j = 0; j < OutHeight; j++)
		{
			FColor Color = FColor::Black;
			int k = i + j * OutWidth;
			if (NumBands >= 1) Color.R = RedBuffer[k];
			if (NumBands >= 2) Color.G = GreenBuffer[k];
			if (NumBands >= 3) Color.B = BlueBuffer[k];
			if (NumBands >= 4) Color.A = AlphaBuffer[k];
			OutColors[k] = Color;
		}
	}

	if (RedBuffer) delete[] RedBuffer;
	if (GreenBuffer) delete[] GreenBuffer;
	if (BlueBuffer) delete[] BlueBuffer;
	if (AlphaBuffer) delete[] AlphaBuffer;
	return true;
}

bool GDALInterface::Warp(TArray<FString> SourceFiles, FString TargetFile, FString InCRS, FString OutCRS, bool bCrop, int NoData)
{
	check(SourceFiles.Num() > 0);

	if (SourceFiles.Num() == 1) return Warp(SourceFiles[0], TargetFile, InCRS, OutCRS, bCrop, NoData);
	else
	{
		FString MergedFile = FPaths::Combine(FPaths::GetPath(TargetFile), FPaths::GetBaseFilename(TargetFile) + ".vrt");
		return Merge(SourceFiles, MergedFile) && Warp(MergedFile, TargetFile, InCRS, OutCRS, bCrop, NoData);
	}
}

bool GDALInterface::Warp(FString SourceFile, FString TargetFile, TArray<FString> Args)
{
	UE_LOG(LogGDALInterface, Log, TEXT("Reprojecting using gdalwarp --config GDAL_PAM_ENABLED NO %s \"%s\" \"%s\""),
		*FString::Join(Args, TEXT(" ")),
		*SourceFile,
		*TargetFile
	);

	GDALDatasetH SrcDataset = GDALOpen(TCHAR_TO_UTF8(*SourceFile), GA_ReadOnly);

	if (!SrcDataset)
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("GDALWarpOpenError", "Could not open file {0}.\n{1}"),
			FText::FromString(SourceFile),
			FText::FromString(FString(CPLGetLastErrorMsg()))
		));

		return false;
	}
		
	
	char** WarpArgv = nullptr;

	for (auto& Arg : Args)
	{
		WarpArgv = CSLAddString(WarpArgv, TCHAR_TO_UTF8(*Arg));
	}

	GDALWarpAppOptions* Options = GDALWarpAppOptionsNew(WarpArgv, NULL);
	CSLDestroy(WarpArgv);

	if (!Options)
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("GDALWarpError", "Could not parse gdalwarp options for file {0}.\nError: {1}"),
			FText::FromString(SourceFile),
			FText::FromString(FString(CPLGetLastErrorMsg()))
		));
		
		GDALClose(SrcDataset);
		return false;
	}

	int WarpError = 0;
	GDALDataset* WarpedDataset = (GDALDataset*) GDALWarp(TCHAR_TO_UTF8(*TargetFile), nullptr, 1, &SrcDataset, Options, &WarpError);
	GDALClose(SrcDataset);

	if (!WarpedDataset)
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("GDALWarpError", "Internal GDALWarp error ({0}):\n{1}"),
			FText::AsNumber(WarpError, &FNumberFormattingOptions::DefaultNoGrouping()),
			FText::FromString(FString(CPLGetLastErrorMsg()))
		));

		return false;
	}
	GDALClose(WarpedDataset);

	UE_LOG(LogGDALInterface, Log, TEXT("Finished reprojecting using gdalwarp --config GDAL_PAM_ENABLED NO %s \"%s\" \"%s\""),
		*FString::Join(Args, TEXT(" ")),
		*SourceFile,
		*TargetFile
	);

	return true;
}

bool GDALInterface::Warp(FString SourceFile, FString TargetFile, FString InCRS, FString OutCRS, bool bCrop, int NoData)
{
	TArray<FString> Args;
	Args.Add("-r");
	Args.Add("bilinear");
	if (!InCRS.IsEmpty())
	{
		Args.Add("-s_srs");
		Args.Add(TCHAR_TO_UTF8(*InCRS));
	}
	Args.Add("-t_srs");
	Args.Add(TCHAR_TO_UTF8(*OutCRS));
	Args.Add("-dstnodata");
	Args.Add(TCHAR_TO_UTF8(*FString::SanitizeFloat(NoData)));

	if (bCrop)
	{
		FVector4d Coordinates;
		FVector4d ConvertedCoordinates;
		if (!GDALInterface::GetCoordinates(Coordinates, { SourceFile })) return false;

		OGRSpatialReference InRs, OutRs;

		if (!SetCRSFromUserInput(OutRs, OutCRS)) return false;
		if (!SetCRSFromUserInput(InRs, InCRS, false) && !SetCRSFromFile(InRs, SourceFile, true)) return false;
		if (!GDALInterface::ConvertCoordinates(Coordinates, true, ConvertedCoordinates, InRs, OutRs)) return false;

		Args.Add("-te");
		Args.Add(TCHAR_TO_UTF8(*FString::SanitizeFloat(FMath::Min(ConvertedCoordinates[0], ConvertedCoordinates[1]))));
		Args.Add(TCHAR_TO_UTF8(*FString::SanitizeFloat(FMath::Min(ConvertedCoordinates[2], ConvertedCoordinates[3]))));
		Args.Add(TCHAR_TO_UTF8(*FString::SanitizeFloat(FMath::Max(ConvertedCoordinates[0], ConvertedCoordinates[1]))));
		Args.Add(TCHAR_TO_UTF8(*FString::SanitizeFloat(FMath::Max(ConvertedCoordinates[2], ConvertedCoordinates[3]))));
	}

	return Warp(SourceFile, TargetFile, Args);
}

TMap<FString, FString> GDALInterface::FieldsFromFeature(OGRFeature* Feature)
{
	TMap<FString, FString> Result;

	int n = Feature->GetFieldCount();

	for (int i = 0; i < n; i++)
	{
		FString Key = FString(Feature->GetFieldDefnRef(i)->GetNameRef());
		FString Value = FString(Feature->GetFieldAsString(i));
		Result.Add(Key, Value);
	}

	return Result;
}

void GDALInterface::AddPointLists(OGRMultiPolygon* MultiPolygon, TArray<FPointList> &PointLists, TMap<FString, FString> &Fields)
{
	for (OGRPolygon* &Polygon : MultiPolygon)
	{
		for (OGRLinearRing* &LinearRing : Polygon)
		{
			FPointList NewList;
			NewList.Fields = Fields;
			for (OGRPoint &Point : LinearRing)
			{
				NewList.Points.Add(Point);
			}
			PointLists.Add(NewList);
		}
	}
}

const double Radius = 6378137.0;
const double HalfPerimeter = PI * Radius;
const double Perimeter = 2 * HalfPerimeter;

void GDALInterface::XYZTileToEPSG3857(double X, double Y, int Zoom, double& OutLong, double& OutLat)
{
	double N = 1 << Zoom;
	OutLong = X * Perimeter / N - HalfPerimeter;
	OutLat  = HalfPerimeter - Y * Perimeter / N;
}

void GDALInterface::EPSG3857ToXYZTile(double Long, double Lat, int Zoom, int& OutX, int& OutY)
{
	double N = 1 << Zoom;
	OutX = (Long + HalfPerimeter) * N / Perimeter;
	OutY = (HalfPerimeter - Lat) * N / Perimeter;
}

void GDALInterface::AddPointLists(OGRPolygon* Polygon, TArray<FPointList> &PointLists, TMap<FString, FString> &Fields)
{
	for (OGRLinearRing* &LinearRing : Polygon)
	{
		FPointList NewList;
		NewList.Fields = Fields;
		for (OGRPoint &Point : LinearRing)
		{
			NewList.Points.Add(Point);
		}
		PointLists.Add(NewList);
	}
}

void GDALInterface::AddPointList(OGRLineString* LineString, TArray<FPointList> &PointLists, TMap<FString, FString> &Fields)
{
	FPointList NewList;
	NewList.Fields = Fields;
	for (OGRPoint &Point : LineString)
	{
		NewList.Points.Add(Point);
	}
	PointLists.Add(NewList);
}

TArray<FPointList> GDALInterface::GetPointLists(GDALDataset *Dataset)
{
	TArray<FPointList> PointLists;
	
	OGRFeature *Feature;
	OGRLayer *Layer;
	Feature = Dataset->GetNextFeature(&Layer, nullptr, nullptr, nullptr);
	int NumFeatures = 0;
	while (Feature)
	{
		NumFeatures += 1;

		TMap<FString, FString> Fields = FieldsFromFeature(Feature);

		OGRGeometry* Geometry = Feature->GetGeometryRef();
		if (!Geometry) continue;

		OGRwkbGeometryType GeometryType = wkbFlatten(Geometry->getGeometryType());
			
		if (GeometryType == wkbMultiPolygon)
		{
			AddPointLists(Geometry->toMultiPolygon(), PointLists, Fields);
		}
		else if (GeometryType == wkbPolygon)
		{
			AddPointLists(Geometry->toPolygon(), PointLists, Fields);
		}
		else if (GeometryType == wkbLineString)
		{
			AddPointList(Geometry->toLineString(), PointLists, Fields);
		}
		else if (GeometryType == wkbPoint)
		{
			// ignoring lone point
		}
		else
		{
			UE_LOG(LogGDALInterface, Warning, TEXT("Found an unsupported feature %d"), wkbFlatten(Geometry->getGeometryType()));
		}
		OGRFeature::DestroyFeature(Feature);
		Feature = Dataset->GetNextFeature(&Layer, nullptr, nullptr, nullptr);
	}
	
	UE_LOG(LogGDALInterface, Log, TEXT("Explored %d features"), NumFeatures);
	UE_LOG(LogGDALInterface, Log, TEXT("Found %d lists of points"), PointLists.Num());

	return PointLists;
}

GDALDataset* GDALInterface::LoadGDALVectorDatasetFromFile(const FString &File)
{
	GDALDataset* Dataset = (GDALDataset*) GDALOpenEx(TCHAR_TO_UTF8(*File), GDAL_OF_VECTOR, NULL, NULL, NULL);

	if (!Dataset)
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("FileNotFound", "Could not load vector file '{0}'.\n{1}"),
				FText::FromString(File),
				FText::FromString(FString(CPLGetLastErrorMsg()))
			)
		);
	}

	return Dataset;
}

GDALDataset* GDALInterface::LoadGDALVectorDatasetFromQuery(FString Query, bool bIsUserInitiated)
{
	UE_LOG(LogGDALInterface, Log, TEXT("Loading dataset from Overpass query: '%s'"), *Query);
	UE_LOG(LogGDALInterface, Log, TEXT("Decoded URL: '%s'"), *(FGenericPlatformHttp::UrlDecode(Query)));
	FString IntermediateDir = FPaths::ConvertRelativePathToFull(FPaths::EngineIntermediateDir());
	FString LandscapeCombinatorDir = FPaths::Combine(IntermediateDir, "LandscapeCombinator");
	FString DownloadDir = FPaths::Combine(LandscapeCombinatorDir, "Download");
	uint32 Hash = FTextLocalizationResource::HashString(Query);
	FString XmlFilePath = FPaths::Combine(DownloadDir, FString::Format(TEXT("overpass_query_{0}.xml"), { Hash }));

	if (!Download::SynchronousFromURL(Query.Replace(TEXT(" "), TEXT("+")), XmlFilePath, bIsUserInitiated))
	{
		
		LCReporter::ShowError(FText::Format(
			LOCTEXT("GetSpatialReferenceError", "Unable to get the result for the Overpass query: {0}."),
			FText::FromString(Query)
		));
		return nullptr;
	}
	
	return LoadGDALVectorDatasetFromFile(XmlFilePath);
}

bool GDALInterface::ExportMesh(const FDynamicMesh3 &Mesh, const FString &File)
{
	CPLSetConfigOption("DXF_WRITE_HATCH", "NO");
	GDALDriver *DXFDriver = GetGDALDriverManager()->GetDriverByName("DXF");
	if (!DXFDriver) {
		UE_LOG(LogGDALInterface, Error, TEXT("DXF driver not available"));
		return false;
	}

	GDALDataset *Dataset = DXFDriver->Create(TCHAR_TO_UTF8(*File), 0, 0, 0, GDT_Unknown, nullptr);
	if (!Dataset) {
		UE_LOG(LogGDALInterface, Error, TEXT("Failed to create DXF file: %s"), *File);
		return false;
	}

	OGRLayer *Layer = Dataset->CreateLayer("faces", nullptr, OGRwkbGeometryType::wkbPolygon25D);
	if (!Layer) {
		UE_LOG(LogGDALInterface, Error, TEXT("Failed to create DXF layer"));
		GDALClose(Dataset);
		return false;
	}

	OGRFeatureDefn *LayerDefn = Layer->GetLayerDefn();
	if (!LayerDefn) {
		UE_LOG(LogGDALInterface, Error, TEXT("Failed to get layer definition"));
		GDALClose(Dataset);
		return false;
	}

	int NumTriangles = Mesh.TriangleCount();
	for (int32 Index = 0; Index < NumTriangles; ++Index)
	{
		FIndex3i Tri = Mesh.GetTriangle(Index);
		OGRPolygon Triangle;
		OGRLinearRing Ring;

		for (int i = 0; i < 3; ++i) {
			FVector3d Vertex = Mesh.GetVertex(Tri[i]);
			Ring.addPoint(Vertex.X, Vertex.Y, Vertex.Z);
		}
		
		Triangle.addRing(&Ring);

		OGRFeature *Feature = OGRFeature::CreateFeature(LayerDefn);

		if (!Feature)
		{
			UE_LOG(LogGDALInterface, Error, TEXT("Failed to create feature for triangle %d"), Index);
			GDALClose(Dataset);
			return false;	
		}

		Feature->SetGeometry(&Triangle);

		if (Layer->CreateFeature(Feature) != OGRERR_NONE)
		{
			UE_LOG(LogGDALInterface, Error, TEXT("Failed to add feature to layer for triangle %d"), Index);
			OGRFeature::DestroyFeature(Feature);
			GDALClose(Dataset);
			return false;
		}

		OGRFeature::DestroyFeature(Feature);
	}

	GDALClose(Dataset);
	return true;
}

bool GDALInterface::ExportPolygons(const TArray<TArray<FVector>> &PointLists, const FString &File)
{
	CPLSetConfigOption("DXF_WRITE_HATCH", "NO");
	GDALDriver *DXFDriver = GetGDALDriverManager()->GetDriverByName("DXF");
	if (!DXFDriver) {
		UE_LOG(LogGDALInterface, Error, TEXT("DXF driver not available"));
		return false;
	}

	GDALDataset *Dataset = DXFDriver->Create(TCHAR_TO_UTF8(*File), 0, 0, 0, GDT_Unknown, nullptr);
	if (!Dataset) {
		UE_LOG(LogGDALInterface, Error, TEXT("Failed to create DXF file: %s"), *File);
		return false;
	}

	OGRLayer *Layer = Dataset->CreateLayer("polygons", nullptr, OGRwkbGeometryType::wkbPolygon25D);
	if (!Layer) {
		UE_LOG(LogGDALInterface, Error, TEXT("Failed to create DXF layer"));
		GDALClose(Dataset);
		return false;
	}

	OGRFeatureDefn *LayerDefn = Layer->GetLayerDefn();
	if (!LayerDefn) {
		UE_LOG(LogGDALInterface, Error, TEXT("Failed to get layer definition"));
		GDALClose(Dataset);
		return false;
	}

	for (const TArray<FVector> &Points: PointLists)
	{
		OGRPolygon Polygon;
		OGRLinearRing Ring;
		int NumPoints = Points.Num();
		for (int i = 0; i < NumPoints; ++i) {
			Ring.addPoint(Points[i].X, Points[i].Y, Points[i].Z);
		}
		
		Polygon.addRing(&Ring);

		OGRFeature *Feature = OGRFeature::CreateFeature(LayerDefn);

		if (!Feature)
		{
			UE_LOG(LogGDALInterface, Error, TEXT("Failed to create feature"));
			GDALClose(Dataset);
			return false;	
		}

		Feature->SetGeometry(&Polygon);

		if (Layer->CreateFeature(Feature) != OGRERR_NONE)
		{
			UE_LOG(LogGDALInterface, Error, TEXT("Failed to add feature to layer"));
			OGRFeature::DestroyFeature(Feature);
			GDALClose(Dataset);
			return false;
		}

		OGRFeature::DestroyFeature(Feature);
	}

	GDALClose(Dataset);
	return true;
}

bool GDALInterface::WriteHeightmapDataToTIF(const FString& File, int32 SizeX, int32 SizeY, uint16* HeightmapData)
{
	GDALDriver *MEMDriver = GetGDALDriverManager()->GetDriverByName("MEM");
	GDALDriver *TIFDriver = GetGDALDriverManager()->GetDriverByName("GTiff");

	if (!TIFDriver || !MEMDriver)
	{
		LCReporter::ShowError(
			LOCTEXT("GDALInterface::WriteHeightmapDataToTIF::3", "Could not load GDAL drivers.")
		);
		return false;
	}

	GDALDataset *Dataset = MEMDriver->Create(
		"",
		SizeX, SizeY,
		1, // nBands
		GDT_UInt16,
		nullptr
	);

	if (!Dataset)
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("GDALInterface::WriteHeightmapDataToTIF::4", "There was an error while creating a GDAL Dataset."),
			FText::FromString(File)
		));
		return false;
	}

	if (Dataset->GetRasterCount() != 1)
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("GDALInterface::WriteHeightmapDataToTIF::4", "There was an error while writing heightmap data to file {0}."),
			FText::FromString(File)
		));
		GDALClose(Dataset);
		return false;
	}


	CPLErr WriteErr = Dataset->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, SizeX, SizeY, HeightmapData, SizeX, SizeY, GDT_UInt16, 0, 0);

	if (WriteErr != CE_None)
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("GDALInterface::WriteHeightmapDataToTIF::5", "There was an error while writing heightmap data to file {0}. (Error: {1})"),
			FText::FromString(File),
			FText::AsNumber(WriteErr, &FNumberFormattingOptions::DefaultNoGrouping())
		));
		GDALClose(Dataset);
		return false;
	}

	GDALDataset *TIFDataset = TIFDriver->CreateCopy(TCHAR_TO_UTF8(*File), Dataset, 1, nullptr, nullptr, nullptr);
	GDALClose(Dataset);

	if (!TIFDataset)
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("GDALInterface::WriteHeightmapDataToTIF::5", "Could not write heightmap to file {0}."),
			FText::FromString(File),
			FText::AsNumber(WriteErr, &FNumberFormattingOptions::DefaultNoGrouping())
		));
		return false;
	}
	
	GDALClose(TIFDataset);
	return true;
}


#undef LOCTEXT_NAMESPACE
