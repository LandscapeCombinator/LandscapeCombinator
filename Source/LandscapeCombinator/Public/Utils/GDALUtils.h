// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#pragma warning(disable: 4668)
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#pragma warning(default: 4668)

namespace GDALUtils
{
	bool GetSpatialReference(OGRSpatialReference &InRs, FString File);
	bool GetSpatialReference(OGRSpatialReference &InRs, GDALDataset* Dataset);
	bool GetSpatialReferenceFromEPSG(OGRSpatialReference &InRs, int EPSG);
	bool GetCoordinates(FVector4d& Coordinates, GDALDataset* Dataset);
	bool GetMinMax(FVector2D &MinMax, TArray<FString> Files);
	bool Translate(FString &SourceFile, FString &TargetFile, int MinAltitude, int MaxAltitude, int Precision);
	bool Warp(FString& SourceFile, FString& TargetFile, OGRSpatialReference &OutRs, int NoData);
	bool Warp(TArray<FString> SourceFiles, FString& TargetFolder, OGRSpatialReference& OutRs, int NoData);
	bool Merge(TArray<FString> SourceFiles, FString& TargetFile);
}