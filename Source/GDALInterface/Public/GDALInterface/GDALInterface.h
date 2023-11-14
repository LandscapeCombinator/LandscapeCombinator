// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#pragma warning(disable: 4668)
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#include <ogr_api.h>
#include <ogrsf_frmts.h>
#pragma warning(default: 4668)

struct FOSMInfo
{
	float Height;
};

struct FPointList
{
	TArray<OGRPoint> Points;
	FOSMInfo Info;
};

class GDALINTERFACE_API GDALInterface
{
public:
	static bool SetWellKnownGeogCRS(OGRSpatialReference &InRs, FString CRS);
	static bool SetCRSFromFile(OGRSpatialReference &InRs, FString File);
	static bool SetCRSFromDataset(OGRSpatialReference &InRs, GDALDataset* Dataset);
	static bool SetCRSFromEPSG(OGRSpatialReference &InRs, int EPSG);
	static bool SetCRSFromUserInput(OGRSpatialReference &InRs, FString CRS);
	static bool GetCoordinates(FVector4d& Coordinates, GDALDataset* Dataset);
	static bool GetCoordinates(FVector4d& Coordinates, TArray<FString> Files);
	static bool ConvertCoordinates(FVector4d& OriginalCoordinates, FVector4d& NewCoordinates, FString InCRS, FString OutCRS);
	static bool ConvertCoordinates(FVector4d& OriginalCoordinates, bool bCrop, FVector4d& NewCoordinates, FString InCRS, FString OutCRS);
	static bool GetPixels(FIntPoint &Pixels, FString File);
	static bool GetMinMax(FVector2D &MinMax, TArray<FString> Files);
	static bool ConvertToPNG(FString& SourceFile, FString& TargetFile, int MinAltitude, int MaxAltitude, int PrecisionPercent = 100);
	static bool ConvertToPNG(FString& SourceFile, FString& TargetFile);
	static bool ChangeResolution(FString& SourceFile, FString& TargetFile, int PrecisionPercent);
	static bool Translate(FString &SourceFile, FString &TargetFile, TArray<FString> Args);
	static bool Warp(FString& SourceFile, FString& TargetFile, FString InCRS, FString OutCRS, int NoData);
	static bool Warp(TArray<FString> SourceFiles, FString& TargetFolder, FString InCRS, FString OutCRS, int NoData);
	static bool Warp(FString &SourceFile, FString &TargetFile, TArray<FString> Args);
	static bool Merge(TArray<FString> SourceFiles, FString& TargetFile);

	static TArray<FPointList> GetPointLists(GDALDataset *Dataset);
	static void AddPointList(OGRLineString* LineString, TArray<FPointList> &PointLists, FOSMInfo Info);
	static void AddPointLists(OGRMultiPolygon* MultiPolygon, TArray<FPointList> &PointLists, FOSMInfo Info);
};