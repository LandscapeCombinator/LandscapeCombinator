// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "Utils/GDALUtils.h"
#include "Utils/Logging.h"

#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

bool GDALUtils::GetSpatialReference(OGRSpatialReference &InRs, FString File)
{
	GDALDataset* Dataset = (GDALDataset *) GDALOpen(TCHAR_TO_UTF8(*File), GA_ReadOnly);
	if (!Dataset)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("GetSpatialReferenceError", "Unable to open file {0} to read its spatial reference."),
			FText::FromString(File)
		));
		return false;
	}

	return GetSpatialReference(InRs, Dataset);
}

bool GDALUtils::GetSpatialReference(OGRSpatialReference& InRs, GDALDataset* Dataset)
{
	if (!Dataset)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("GetSpatialReferenceError", "Unable to get spatial reference from dataset (null pointer).")
		);
		return false;
	}

	const char* ProjectionRef = Dataset->GetProjectionRef();
	OGRErr Err = InRs.importFromWkt(ProjectionRef);

	if (Err != OGRERR_NONE)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("GetSpatialReferenceError2", "Unable to get spatial reference from dataset (error {0})."),
			FText::AsNumber(Err)
		));
		return false;
	}
	InRs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

	return true;
}

bool GDALUtils::GetSpatialReferenceFromEPSG(OGRSpatialReference& InRs, int EPSG)
{
	OGRErr Err = InRs.importFromEPSG(EPSG);
	if (Err != OGRERR_NONE)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("StartupModuleError1", "Could not create spatial reference from EPSG {0}: (Error {1})."),
				FText::AsNumber(EPSG),
				FText::AsNumber(Err)
			)
		);
		return false;
	}
	InRs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
	return true;
}

bool GDALUtils::GetCoordinates(FVector4d& Coordinates, GDALDataset* Dataset)
{
	if (!Dataset) return false;

	double AdfTransform[6];
	if (Dataset->GetGeoTransform(AdfTransform) != CE_None)
		return false;
		
	double LeftCoord    = AdfTransform[0];
	double RightCoord   = AdfTransform[0] + Dataset->GetRasterXSize()*AdfTransform[1];
	double TopCoord     = AdfTransform[3];
	double BottomCoord  = AdfTransform[3] + Dataset->GetRasterYSize()*AdfTransform[5];
	
	Coordinates[0] = LeftCoord;
	Coordinates[1] = RightCoord;
	Coordinates[2] = BottomCoord;
	Coordinates[3] = TopCoord;
	return true;
}

bool GDALUtils::GetMinMax(FVector2D &MinMax, TArray<FString> Files)
{
	MinMax[0] = DBL_MAX;
	MinMax[1] = -DBL_MAX;

	for (auto& File : Files) {

		GDALDataset *Dataset = (GDALDataset *)GDALOpen(TCHAR_TO_UTF8(*File), GA_ReadOnly);

		if (!Dataset) {
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(LOCTEXT("GetMinMaxError", "Could not open heightmap file '{0}' to compute min/max altitudes."),
					FText::FromString(File)
				)
			);
			return false;
		}
		
		double AdfMinMax[2];

		if (GDALComputeRasterMinMax(Dataset->GetRasterBand(1), false, AdfMinMax) != CE_None) {
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(LOCTEXT("GetMinMaxError2", "Could compute min/max altitudes of file '{0}'."),
					FText::FromString(File)
				)
			);
			return false;
		}

		MinMax[0] = FMath::Min(MinMax[0], AdfMinMax[0]);
		MinMax[1] = FMath::Max(MinMax[1], AdfMinMax[1]);
	}

	return true;
}

bool GDALUtils::Translate(FString &SourceFile, FString &TargetFile, int MinAltitude, int MaxAltitude, int Precision)
{
	FScopedSlowTask TranslateTask(1, LOCTEXT("TranslateTask", "Landscape Combiantor: GDAL translating file"));
	TranslateTask.MakeDialog();
	GDALDatasetH SourceDataset = GDALOpen(TCHAR_TO_UTF8(*SourceFile), GA_ReadOnly);

	if (!SourceDataset)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("ConvertGDALOpenError", "Could not open file {0}."),
			FText::FromString(SourceFile)
		));
		return false;
	}

	char** TranslateArgv = nullptr;
			
	TranslateArgv = CSLAddString(TranslateArgv, "-scale");
	TranslateArgv = CSLAddString(TranslateArgv, TCHAR_TO_UTF8(*FString::SanitizeFloat(MinAltitude)));
	TranslateArgv = CSLAddString(TranslateArgv, TCHAR_TO_UTF8(*FString::SanitizeFloat(MaxAltitude)));
	TranslateArgv = CSLAddString(TranslateArgv, "0");
	TranslateArgv = CSLAddString(TranslateArgv, "65535");
	TranslateArgv = CSLAddString(TranslateArgv, "-outsize");
	TranslateArgv = CSLAddString(TranslateArgv, TCHAR_TO_UTF8(*FString::Format(TEXT("{0}%"), { Precision })));
	TranslateArgv = CSLAddString(TranslateArgv, TCHAR_TO_UTF8(*FString::Format(TEXT("{0}%"), { Precision })));
	TranslateArgv = CSLAddString(TranslateArgv, "-ot");
	TranslateArgv = CSLAddString(TranslateArgv, "UInt16");
	TranslateArgv = CSLAddString(TranslateArgv, "-of");
	TranslateArgv = CSLAddString(TranslateArgv, "PNG");

	GDALTranslateOptions* Options = GDALTranslateOptionsNew(TranslateArgv, NULL);
	CSLDestroy(TranslateArgv);

	if (!Options)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("ConvertParseOptionsError", "Internal GDAL error while parsing GDALTranslate options for file {0}."),
			FText::FromString(SourceFile)
		));
		GDALClose(SourceDataset);
		return false;
	}

	UE_LOG(LogLandscapeCombinator, Log, TEXT("Translating using gdal_translate --config GDAL_PAM_ENABLED NO -scale %s %s 0 65535 -ot UInt16 -of PNG -outsize %d%% %d%% \"%s\" \"%s\""),
		*FString::SanitizeFloat(MinAltitude),
		*FString::SanitizeFloat(MaxAltitude),
		Precision, Precision,
		*SourceFile,
		*TargetFile
	);

	GDALDataset* DstDataset = (GDALDataset *) GDALTranslate(TCHAR_TO_UTF8(*TargetFile), SourceDataset, Options, nullptr);
	GDALClose(SourceDataset);
	GDALTranslateOptionsFree(Options);

	if (!DstDataset)
	{
		UE_LOG(LogLandscapeCombinator, Error, TEXT("Error while translating: %s to %s"), *SourceFile, *TargetFile);
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("ConvertGDALTranslateError", "Internal GDALTranslate error while converting dataset from file {0} to PNG."),
			FText::FromString(SourceFile)
		));
		return false;
	}
		
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Finished translating: %s to %s"), *SourceFile, *TargetFile);
	GDALClose(DstDataset);
	TranslateTask.EnterProgressFrame(1.0f);

	return true;
}

bool GDALUtils::Merge(TArray<FString> SourceFiles, FString& TargetFile)
{
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Merging %s into %s"), *FString::Join(SourceFiles, TEXT(", ")), *TargetFile);
	const int NumFiles = SourceFiles.Num();
	
	char** papszSrcDSNames = nullptr;
	for (int i = 0; i < NumFiles; i++)
	{
		papszSrcDSNames = CSLAddString(papszSrcDSNames, TCHAR_TO_UTF8(*SourceFiles[i]));
	}

	GDALDatasetH DatasetVRT = GDALBuildVRT(
		TCHAR_TO_UTF8(*TargetFile),
		SourceFiles.Num(),
		NULL,
		papszSrcDSNames,
		NULL,
		NULL
	);
	
	if (!DatasetVRT)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("GDALUtilsMergeError", "Could not merge the files {0}."),
			FText::FromString(FString::Join(SourceFiles, TEXT(", ")))
		));
		return false;
	}

    GDALClose(DatasetVRT);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Finished merging %s into %s"), *FString::Join(SourceFiles, TEXT(", ")), *TargetFile);
	return true;
}

bool GDALUtils::Warp(TArray<FString> SourceFiles, FString& TargetFile, OGRSpatialReference& OutRs, int NoData)
{
	check(SourceFiles.Num() > 0);

	if (SourceFiles.Num() == 1) return Warp(SourceFiles[0], TargetFile, OutRs, NoData);
	else
	{
		FString MergedFile = FPaths::Combine(FPaths::GetPath(TargetFile), FPaths::GetBaseFilename(TargetFile) + ".vrt");
		return Merge(SourceFiles, MergedFile) && Warp(MergedFile, TargetFile, OutRs, NoData);
	}
}

bool GDALUtils::Warp(FString& SourceFile, FString& TargetFile, OGRSpatialReference &OutRs, int NoData)
{
	FScopedSlowTask WarpTask(1, LOCTEXT("WarpTask", "Reprojecting File"));
	WarpTask.MakeDialog();

	GDALDatasetH SrcDataset = GDALOpen(TCHAR_TO_UTF8(*SourceFile), GA_ReadOnly);

	if (!SrcDataset)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("GDALWarpOpenError", "Could not open file {0}."),
			FText::FromString(SourceFile)
		));

		return false;
	}

	FVector4d Coordinates;
	if (!GDALUtils::GetCoordinates(Coordinates, (GDALDataset*) SrcDataset))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("GDALWarpOpenError", "Could not read coordinates from file {0}."),
			FText::FromString(SourceFile)
		));
		
		GDALClose(SrcDataset);
		return false;
	}
				
	double MinCoordWidth = Coordinates[0];
	double MaxCoordWidth = Coordinates[1];
	double MinCoordHeight = Coordinates[2];
	double MaxCoordHeight = Coordinates[3];

	double xs[4] = { MinCoordWidth,  MinCoordWidth,  MaxCoordWidth,  MaxCoordWidth };
	double ys[4] = { MinCoordHeight, MaxCoordHeight, MaxCoordHeight, MinCoordHeight };

	/* Compute xmin, xmax, ymin, ymax for cropping the output */
	OGRSpatialReference InRs;	
	if (!GDALUtils::GetSpatialReference(InRs, (GDALDataset *) SrcDataset) || !OGRCreateCoordinateTransformation(&InRs, &OutRs)->Transform(4, xs, ys))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("ReprojectionError", "Internal error while transforming coordinates for file {0}."),
			FText::FromString(SourceFile)
		));

		GDALClose(SrcDataset);
		return false;
	}

	double xmin = FMath::Max(xs[0], xs[1]);
	double xmax = FMath::Min(xs[2], xs[3]);
	double ymin = FMath::Max(ys[0], ys[3]);
	double ymax = FMath::Min(ys[1], ys[2]);

	char** WarpArgv = nullptr;
				
	WarpArgv = CSLAddString(WarpArgv, "-r");
	WarpArgv = CSLAddString(WarpArgv, "bilinear");
	WarpArgv = CSLAddString(WarpArgv, "-t_srs");
	WarpArgv = CSLAddString(WarpArgv, "EPSG:4326");
	WarpArgv = CSLAddString(WarpArgv, "-te");
	WarpArgv = CSLAddString(WarpArgv, TCHAR_TO_UTF8(*FString::SanitizeFloat(xmin)));
	WarpArgv = CSLAddString(WarpArgv, TCHAR_TO_UTF8(*FString::SanitizeFloat(ymin)));
	WarpArgv = CSLAddString(WarpArgv, TCHAR_TO_UTF8(*FString::SanitizeFloat(xmax)));
	WarpArgv = CSLAddString(WarpArgv, TCHAR_TO_UTF8(*FString::SanitizeFloat(ymax)));
	WarpArgv = CSLAddString(WarpArgv, "-dstnodata");
	WarpArgv = CSLAddString(WarpArgv, TCHAR_TO_UTF8(*FString::SanitizeFloat(NoData)));
	GDALWarpAppOptions* WarpOptions = GDALWarpAppOptionsNew(WarpArgv, NULL);
	CSLDestroy(WarpArgv);

	if (!WarpOptions)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("GDALWarpError", "Could not parse gdalwarp options for file {0}."),
			FText::FromString(SourceFile)
		));
		
		GDALClose(SrcDataset);
		return false;
	}

	UE_LOG(LogLandscapeCombinator, Log, TEXT("Reprojecting using gdalwarp --config GDAL_PAM_ENABLED NO -r bilinear -t_srs EPSG:4326 -te %s %s %s %s \"%s\" \"%s\""),
		*FString::SanitizeFloat(xmin),
		*FString::SanitizeFloat(ymin),
		*FString::SanitizeFloat(xmax),
		*FString::SanitizeFloat(ymax),
		*SourceFile,
		*TargetFile
	);

	int WarpError = 0;
	GDALDataset* WarpedDataset = (GDALDataset*) GDALWarp(TCHAR_TO_UTF8(*TargetFile), nullptr, 1, &SrcDataset, WarpOptions, &WarpError);
	GDALClose(SrcDataset);

	if (!WarpedDataset)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("GDALWarpError", "Internal GDALWarp error {0} while converting dataset from file {1} to EPSG 4326."),
			FText::AsNumber(WarpError),
			FText::FromString(SourceFile)
		));

		return false;
	}
	GDALClose(WarpedDataset);

	UE_LOG(LogLandscapeCombinator, Log, TEXT("Finished reprojecting using gdalwarp --config GDAL_PAM_ENABLED NO -r bilinear -t_srs EPSG:4326 -te %s %s %s %s \"%s\" \"%s\""),
		*FString::SanitizeFloat(xmin),
		*FString::SanitizeFloat(ymin),
		*FString::SanitizeFloat(xmax),
		*FString::SanitizeFloat(ymax),
		*SourceFile,
		*TargetFile
	);
	WarpTask.EnterProgressFrame(1.0f);

	return true;
}


#undef LOCTEXT_NAMESPACE
