// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "GDALInterface/GDALInterface.h"
#include "GDALInterface/LogGDALInterface.h"

#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "FGDALInterfaceModule"

bool GDALInterface::SetWellKnownGeogCRS(OGRSpatialReference& InRs, FString CRS)
{
	OGRErr Err = InRs.SetWellKnownGeogCS(TCHAR_TO_ANSI(*CRS));

	if (Err != OGRERR_NONE)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("SetWellKnownGeogCRS", "Unable to get spatial reference from string: {0}.\nError: {1}"),
			FText::FromString(CRS),
			FText::AsNumber(Err)
		));
		return false;
	}

	InRs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
	return true;
}


bool GDALInterface::SetCRSFromFile(OGRSpatialReference &InRs, FString File)
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

	return SetCRSFromDataset(InRs, Dataset);
}

bool GDALInterface::SetCRSFromDataset(OGRSpatialReference& InRs, GDALDataset* Dataset)
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
			LOCTEXT("GetSpatialReferenceError2", "Unable to get spatial reference from dataset (Error {0})."),
			FText::AsNumber(Err)
		));
		return false;
	}
	InRs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

	return true;
}

bool GDALInterface::SetCRSFromEPSG(OGRSpatialReference& InRs, int EPSG)
{
	OGRErr Err = InRs.importFromEPSG(EPSG);
	if (Err != OGRERR_NONE)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("StartupModuleError1", "Could not create spatial reference from EPSG {0} (Error {1})."),
				FText::AsNumber(EPSG),
				FText::AsNumber(Err)
			)
		);
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
		return false;
		
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

bool GDALInterface::GetCoordinates(FVector4d& Coordinates, TArray<FString> Files)
{
	double MinCoordWidth = DBL_MAX;
	double MaxCoordWidth = -DBL_MAX;
	double MinCoordHeight = DBL_MAX;
	double MaxCoordHeight = -DBL_MAX;

	if (Files.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("GetCoordinatesError", "GetCoordinates requires a non-empty array of files.")
		);
		return false;
	}

	for (auto& File : Files)
	{
		GDALDataset *Dataset = (GDALDataset *)GDALOpen(TCHAR_TO_UTF8(*File), GA_ReadOnly);
		if (!Dataset)
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("GetCoordinatesError", "Could not open heightmap file '{0}' to read the coordinates."),
					FText::FromString(File)
				)
			);
			return false;
		}

		FVector4d FileCoordinates;
		if (!GDALInterface::GetCoordinates(FileCoordinates, Dataset))
		{
			GDALClose(Dataset);
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("GetCoordinatesError", "Could not read coordinates from heightmap file '{0}'."),
					FText::FromString(File)
				)
			);
			return false;
		}
		GDALClose(Dataset);
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

bool GDALInterface::ConvertCoordinates(FVector4d& OriginalCoordinates, FVector4d& Coordinates, int InEPSG, int OutEPSG)
{
	double MinCoordWidth0 = OriginalCoordinates[0];
	double MaxCoordWidth0 = OriginalCoordinates[1];
	double MinCoordHeight0 = OriginalCoordinates[2];
	double MaxCoordHeight0 = OriginalCoordinates[3];
	double xs[2] = { MinCoordWidth0,  MaxCoordWidth0  };
	double ys[2] = { MaxCoordHeight0, MinCoordHeight0 };

	OGRSpatialReference InRs, OutRs;
	
	if (!SetCRSFromEPSG(InRs, InEPSG) || !SetCRSFromEPSG(OutRs, OutEPSG) || !OGRCreateCoordinateTransformation(&InRs, &OutRs)->Transform(2, xs, ys)) {
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("AdjustLandscapeTransformError", "Internal error while transforming coordinates.")
		);
		return false;
	}

	Coordinates[0] = xs[0];
	Coordinates[1] = xs[1];
	Coordinates[2] = ys[1];
	Coordinates[3] = ys[0];
	return true;
}

bool GDALInterface::GetPixels(FIntPoint& Pixels, FString File)
{
	GDALDataset *Dataset = (GDALDataset *)GDALOpen(TCHAR_TO_UTF8(*File), GA_ReadOnly);

	if (!Dataset) {
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(LOCTEXT("GetInsidePixelsError", "Could not open heightmap file '{0}' to read its size."),
				FText::FromString(File)
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
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(LOCTEXT("GetMinMaxError", "Could not open heightmap file '{0}' to compute min/max altitudes."),
					FText::FromString(File)
				)
			);
			return false;
		}
		
		double AdfMinMax[2];

		if (GDALComputeRasterMinMax(Dataset->GetRasterBand(1), false, AdfMinMax) != CE_None)
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(LOCTEXT("GetMinMaxError2", "Could not compute min/max altitudes of file '{0}'."),
					FText::FromString(File)
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

bool GDALInterface::ConvertToPNG(FString& SourceFile, FString& TargetFile, int MinAltitude, int MaxAltitude, int PrecisionPercent)
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

bool GDALInterface::ChangeResolution(FString& SourceFile, FString& TargetFile, int PrecisionPercent)
{
	TArray<FString> Args;
	Args.Add("-outsize");
	Args.Add(FString::Format(TEXT("{0}%"), { PrecisionPercent }));
	Args.Add(FString::Format(TEXT("{0}%"), { PrecisionPercent }));
	return Translate(SourceFile, TargetFile, Args);
}

bool GDALInterface::Translate(FString &SourceFile, FString &TargetFile, TArray<FString> Args)
{
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

	for (auto& Arg : Args)
	{
		TranslateArgv = CSLAddString(TranslateArgv, TCHAR_TO_UTF8(*Arg));
	}

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
		UE_LOG(LogGDALInterface, Error, TEXT("Error while translating: %s to %s"), *SourceFile, *TargetFile);
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("ConvertGDALTranslateError", "Internal GDALTranslate error while converting dataset from file {0} to PNG."),
			FText::FromString(SourceFile)
		));
		return false;
	}
		
	UE_LOG(LogGDALInterface, Log, TEXT("Finished translating: %s to %s"), *SourceFile, *TargetFile);
	GDALClose(DstDataset);

	return true;
}

bool GDALInterface::Merge(TArray<FString> SourceFiles, FString& TargetFile)
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
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("GDALInterfaceMergeError", "Could not merge the files {0}.\nError {1}."),
			FText::FromString(FString::Join(SourceFiles, TEXT(", "))),
			FText::AsNumber(pbUsageError)
		));
		return false;
	}

	GDALClose(DatasetVRT);
	UE_LOG(LogGDALInterface, Log, TEXT("Finished merging %s into %s"), *FString::Join(SourceFiles, TEXT(", ")), *TargetFile);
	return true;
}

bool GDALInterface::Warp(TArray<FString> SourceFiles, FString& TargetFile, int InEPSG, int OutEPSG, int NoData)
{
	check(SourceFiles.Num() > 0);

	if (SourceFiles.Num() == 1) return Warp(SourceFiles[0], TargetFile, InEPSG, OutEPSG, NoData);
	else
	{
		FString MergedFile = FPaths::Combine(FPaths::GetPath(TargetFile), FPaths::GetBaseFilename(TargetFile) + ".vrt");
		return Merge(SourceFiles, MergedFile) && Warp(MergedFile, TargetFile, InEPSG, OutEPSG, NoData);
	}
}

bool GDALInterface::Warp(FString& SourceFile, FString& TargetFile, int InEPSG, int OutEPSG, int NoData)
{
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
	if (!GDALInterface::GetCoordinates(Coordinates, (GDALDataset*) SrcDataset))
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
	OGRSpatialReference InRs, OutRs;	
	if (!GDALInterface::SetCRSFromEPSG(InRs, InEPSG) || !GDALInterface::SetCRSFromEPSG(OutRs, OutEPSG) || !OGRCreateCoordinateTransformation(&InRs, &OutRs)->Transform(4, xs, ys))
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
	WarpArgv = CSLAddString(WarpArgv, "-s_srs");
	WarpArgv = CSLAddString(WarpArgv, TCHAR_TO_UTF8(*FString::Format(TEXT("EPSG:{0}"), { InEPSG })));
	WarpArgv = CSLAddString(WarpArgv, "-t_srs");
	WarpArgv = CSLAddString(WarpArgv, TCHAR_TO_UTF8(*FString::Format(TEXT("EPSG:{0}"), { OutEPSG })));
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

	UE_LOG(LogGDALInterface, Log, TEXT("Reprojecting using gdalwarp --config GDAL_PAM_ENABLED NO -r bilinear -t_srs EPSG:4326 -te %s %s %s %s \"%s\" \"%s\""),
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
			LOCTEXT("GDALWarpError", "Internal GDALWarp error ({0}) while converting dataset from file {1} to EPSG 4326."),
			FText::AsNumber(WarpError),
			FText::FromString(SourceFile)
		));

		return false;
	}
	GDALClose(WarpedDataset);

	UE_LOG(LogGDALInterface, Log, TEXT("Finished reprojecting using gdalwarp --config GDAL_PAM_ENABLED NO -r bilinear -t_srs EPSG:4326 -te %s %s %s %s \"%s\" \"%s\""),
		*FString::SanitizeFloat(xmin),
		*FString::SanitizeFloat(ymin),
		*FString::SanitizeFloat(xmax),
		*FString::SanitizeFloat(ymax),
		*SourceFile,
		*TargetFile
	);

	return true;
}

void GDALInterface::AddPointLists(OGRMultiPolygon* MultiPolygon, TArray<TArray<OGRPoint>> &PointLists)
{
	for (OGRPolygon* &Polygon : MultiPolygon)
	{
		for (OGRLinearRing* &LinearRing : Polygon)
		{
			TArray<OGRPoint> NewList;
			for (OGRPoint &Point : LinearRing)
			{
				NewList.Add(Point);
			}
			PointLists.Add(NewList);
		}
	}
}

void GDALInterface::AddPointList(OGRLineString* LineString, TArray<TArray<OGRPoint>> &PointLists)
{
	TArray<OGRPoint> NewList;
	for (OGRPoint &Point : LineString)
	{
		NewList.Add(Point);
	}
	PointLists.Add(NewList);
}

TArray<TArray<OGRPoint>> GDALInterface::GetPointLists(GDALDataset *Dataset)
{
	TArray<TArray<OGRPoint>> PointLists;

	int FeatureCount = 0;
	for (auto Layer : Dataset->GetLayers())
	{
		FeatureCount += Layer->GetFeatureCount();
	}
	
	FScopedSlowTask FeaturesTask = FScopedSlowTask(0,
		FText::Format(
			LOCTEXT("PointsTask", "Reading Features from Dataset..."),
			FText::AsNumber(FeatureCount)
		)
	);
	FeaturesTask.MakeDialog(true);
	
	OGRFeature *Feature;
	OGRLayer *Layer;
	Feature = Dataset->GetNextFeature(&Layer, nullptr, nullptr, nullptr);
	while (Feature)
	{
		if (FeaturesTask.ShouldCancel())
		{
			return TArray<TArray<OGRPoint>>();
		}

		OGRGeometry* Geometry = Feature->GetGeometryRef();
		if (!Geometry) continue;

		OGRwkbGeometryType GeometryType = wkbFlatten(Geometry->getGeometryType());
			
		if (GeometryType == wkbMultiPolygon)
		{
			AddPointLists(Geometry->toMultiPolygon(), PointLists);
		}
		else if (GeometryType == wkbLineString)
		{
			AddPointList(Geometry->toLineString(), PointLists);
		}
		else if (GeometryType == wkbPoint)
		{
			// ignoring lone point
		}
		else
		{
			UE_LOG(LogGDALInterface, Warning, TEXT("Found an unsupported feature %d"), wkbFlatten(Geometry->getGeometryType()));
		}
		Feature = Dataset->GetNextFeature(&Layer, nullptr, nullptr, nullptr);
	}

	UE_LOG(LogGDALInterface, Log, TEXT("Found %d lists of points"), PointLists.Num());

	return PointLists;
}


#undef LOCTEXT_NAMESPACE
