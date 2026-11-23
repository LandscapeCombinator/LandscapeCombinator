// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImageDownloader/HMFetcher.h"
#include "ImageDownloader/OGCProvider.h"

class HMOGC : public HMFetcher
{
public:
	HMOGC(
		FOGCProvider InOGC_Provider,
		bool bEnableParallelDownload0,
		int InMaxTileWidth,
		int InMaxTileHeight,
		FString InName,
		FString InCRS,
		bool InXIsLong,
		double InMinAllowedLong,
		double InMaxAllowedLong,
		double InMinAllowedLat,
		double InMaxAllowedLat,
		double InMinLong,
		double InMaxLong,
		double InMinLat,
		double InMaxLat,
		bool InSingleTile,
		double &InResolutionPixelsPerUnit,
		bool InMaxResolution
	) :
		OGC_Provider(InOGC_Provider),
		bEnableParallelDownload(bEnableParallelDownload0),
		OGC_MaxTileWidth(InMaxTileWidth),
		OGC_MaxTileHeight(InMaxTileHeight),
		OGC_Name(InName),
		OGC_CRS(InCRS),
		OGC_X_IsLong(InXIsLong),
		OGC_MinAllowedLong(InMinAllowedLong),
		OGC_MaxAllowedLong(InMaxAllowedLong),
		OGC_MinAllowedLat(InMinAllowedLat),
		OGC_MaxAllowedLat(InMaxAllowedLat),
		OGC_MinLong(InMinLong),
		OGC_MaxLong(InMaxLong),
		OGC_MinLat(InMinLat),
		OGC_MaxLat(InMaxLat),
		bOGCSingleTile(InSingleTile),
		OGC_ResolutionPixelsPerUnit(InResolutionPixelsPerUnit),
		bOGC_MaxResolution(InMaxResolution)
	{
	}

	bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;

protected:
	FOGCProvider OGC_Provider;
	bool bEnableParallelDownload;
	int OGC_MaxTileWidth;
	int OGC_MaxTileHeight;
	FString OGC_Name;
	FString OGC_CRS;
	bool OGC_X_IsLong;
	double OGC_MinAllowedLong;
	double OGC_MaxAllowedLong;
	double OGC_MinAllowedLat;
	double OGC_MaxAllowedLat;
	double OGC_MinLong;
	double OGC_MaxLong;
	double OGC_MinLat;
	double OGC_MaxLat;
	bool bOGCSingleTile;
	double &OGC_ResolutionPixelsPerUnit;
	bool bOGC_MaxResolution;
};
