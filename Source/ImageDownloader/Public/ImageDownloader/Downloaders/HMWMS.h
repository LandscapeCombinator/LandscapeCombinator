// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImageDownloader/HMFetcher.h"
#include "ImageDownloader/WMSProvider.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMWMS : public HMFetcher
{
public:
    HMWMS(
        FWMSProvider InWMS_Provider,
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
        double InResolutionPixelsPerUnit
    ) :
        WMS_Provider(InWMS_Provider),
        WMS_MaxTileWidth(InMaxTileWidth),
        WMS_MaxTileHeight(InMaxTileHeight),
        WMS_Name(InName),
        WMS_CRS(InCRS),
        WMS_X_IsLong(InXIsLong),
        WMS_MinAllowedLong(InMinAllowedLong),
        WMS_MaxAllowedLong(InMaxAllowedLong),
        WMS_MinAllowedLat(InMinAllowedLat),
        WMS_MaxAllowedLat(InMaxAllowedLat),
        WMS_MinLong(InMinLong),
        WMS_MaxLong(InMaxLong),
        WMS_MinLat(InMinLat),
        WMS_MaxLat(InMaxLat),
        bWMSSingleTile(InSingleTile),
        WMS_ResolutionPixelsPerUnit(InResolutionPixelsPerUnit)
    {
    }

    void Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

protected:
    FWMSProvider WMS_Provider;
    int WMS_MaxTileWidth;
    int WMS_MaxTileHeight;
    FString WMS_Name;
    FString WMS_CRS;
    bool WMS_X_IsLong;
    double WMS_MinAllowedLong;
    double WMS_MaxAllowedLong;
    double WMS_MinAllowedLat;
    double WMS_MaxAllowedLat;
    double WMS_MinLong;
    double WMS_MaxLong;
    double WMS_MinLat;
    double WMS_MaxLat;
    bool bWMSSingleTile;
    double WMS_ResolutionPixelsPerUnit;
};
