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

struct FPointList
{
	TArray<OGRPoint> Points;
	TMap<FString, FString> Fields;
};

class GDALINTERFACE_API GDALInterface
{
public:
	static bool SetWellKnownGeogCRS(OGRSpatialReference &InRs, FString CRS);
	static bool HasCRS(FString File);
	static bool SetCRSFromFile(OGRSpatialReference &InRs, FString File, bool bDialog = true);
	static bool SetCRSFromDataset(OGRSpatialReference &InRs, GDALDataset* Dataset, bool bDialog = true);
	static bool SetCRSFromEPSG(OGRSpatialReference &InRs, int EPSG);
	static bool SetCRSFromUserInput(OGRSpatialReference &InRs, FString CRS);
	static bool GetCoordinates(FVector4d& Coordinates, GDALDataset* Dataset);
	static bool GetCoordinates(FVector4d& Coordinates, TArray<FString> Files);
	static OGRCoordinateTransformation *MakeTransform(FString InCRS, FString OutCRS);
	static bool Transform(OGRCoordinateTransformation* CoordinateTransformation, double *Longitude, double *Latitude);
	static bool Transform2(OGRCoordinateTransformation* CoordinateTransformation, double *xs, double *ys);
	static bool ConvertCoordinates(double *Longitude, double *Latitude, FString InCRS, FString OutCRS);
	static bool ConvertCoordinates2(double *xs, double *ys, FString InCRS, FString OutCRS);
	static bool ConvertCoordinates(FVector4d& OriginalCoordinates, FVector4d& NewCoordinates, FString InCRS, FString OutCRS);
	static bool ConvertCoordinates(FVector4d& OriginalCoordinates, bool bCrop, FVector4d& NewCoordinates, FString InCRS, FString OutCRS);
	static bool GetPixels(FIntPoint &Pixels, FString File);
	static bool GetMinMax(FVector2D &MinMax, TArray<FString> Files);
	static bool ConvertToPNG(FString SourceFile, FString TargetFile, int MinAltitude, int MaxAltitude, int PrecisionPercent = 100);
	static bool ConvertToPNG(FString SourceFile, FString TargetFile);
	static bool ChangeResolution(FString SourceFile, FString TargetFile, int PrecisionPercent);
	static bool Translate(FString SourceFile, FString TargetFile, TArray<FString> Args);
	static bool Warp(FString SourceFile, FString TargetFile, FString InCRS, FString OutCRS, int NoData);
	static bool Warp(TArray<FString> SourceFiles, FString TargetFolder, FString InCRS, FString OutCRS, int NoData);
	static bool Warp(FString SourceFile, FString TargetFile, TArray<FString> Args);
	static bool Merge(TArray<FString> SourceFiles, FString TargetFile);
	static bool AddGeoreference(FString InputFile, FString OutputFile, FString CRS, double MinLong, double MaxLong, double MinLat, double MaxLat);

	static bool ReadColorsFromFile(FString File, int &OutWidth, int &OutHeight, TArray<FColor> &OutColors);
	static bool ReadHeightmapFromFile(FString File, int& OutWidth, int& OutHeight, TArray<float>& OutHeightmap);

	static TArray<FPointList> GetPointLists(GDALDataset *Dataset);
	static void AddPointList(OGRLineString* LineString, TArray<FPointList> &PointLists, TMap<FString, FString> &Fields);
	static void AddPointLists(OGRPolygon* Polygon, TArray<FPointList> &PointLists, TMap<FString, FString> &Fields);
	static void AddPointLists(OGRMultiPolygon* MultiPolygon, TArray<FPointList> &PointLists, TMap<FString, FString> &Fields);
	
	static void XYZTileToEPSG3857(double X, double Y, int Zoom, double &OutLong, double &OutLat);
	static void EPSG3857ToXYZTile(double Long, double Lat, int Zoom, int &OutX, int &OutY);
};