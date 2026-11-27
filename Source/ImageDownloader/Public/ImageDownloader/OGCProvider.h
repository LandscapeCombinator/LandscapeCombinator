// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "OGCProvider.generated.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

/** The OGC web services that provide layers (WMS) or coverages (WCS) as images. */
UENUM()
enum class EOGCService : uint8
{
	WMS,
	WCS
};

/**
 * Capabilities of an OGC web service (WMS or WCS): its layers (WMS) or coverages (WCS) with their CRS and bounds,
 * and the creation of the requests that download an image. The two services share the arrays below and the
 * validation of the request; only the parsing of the capabilities and the form of the URL differ.
 */
USTRUCT()
struct FOGCProvider
{
	GENERATED_BODY()

public:
	/** Downloads and parses the capabilities of the service. A WCS needs a second request (DescribeCoverage) to get the bounds. */
	void SetFromURL(EOGCService InService, FString URL, TArray<FString> ExcludeCRS, TFunction<bool(FString)> NameFilter, TFunction<void(bool)> OnComplete);

	/** "WMS" or "WCS" */
	FString ServiceName() const;

	/** WMS only: adds TRANSPARENT=TRUE to the GetMap requests, so pixels without data are transparent (set by the downloader) */
	bool bTransparent = false;

	bool CreateURL(
		int Width, int Height, FString Name, FString CRS, bool XIsLong,
		double MinAllowedLong, double MaxAllowedLong, double MinAllowedLat, double MaxAllowedLat,
		double MinLong, double MaxLong, double MinLat, double MaxLat,
		FString &URL, bool &bGeoTiff, FString &FileExt
	);

	UPROPERTY();
	EOGCService Service = EOGCService::WMS;

	UPROPERTY();
	int MaxWidth = 0;

	UPROPERTY();
	int MaxHeight = 0;

	/** GetMap (WMS) or GetCoverage (WCS) URL, ending with ? or & */
	UPROPERTY();
	FString RequestURL;

	UPROPERTY();
	FString CapabilitiesURL;

	UPROPERTY();
	FString CapabilitiesFile;

	UPROPERTY();
	FString CapabilitiesContent;

	UPROPERTY();
	TArray<FString> Names;

	UPROPERTY();
	TArray<FString> Titles;

	UPROPERTY();
	TArray<FString> Abstracts;

	UPROPERTY();
	TArray<FString> CRSs;

	UPROPERTY();
	TArray<double> MinXs;

	UPROPERTY();
	TArray<double> MinYs;

	UPROPERTY();
	TArray<double> MaxXs;

	UPROPERTY();
	TArray<double> MaxYs;

	/** WCS only: image format of the GetCoverage requests (a GeoTIFF format) */
	UPROPERTY();
	FString ImageFormat;

	/** WCS only: labels of the axes of each coverage (used by the subset and scalesize parameters), for example x and y */
	UPROPERTY();
	TArray<FString> XAxisLabels;

	UPROPERTY();
	TArray<FString> YAxisLabels;

private:
	// WMS
	bool LoadWMSCapabilities(TArray<FString> ExcludeCRS, TFunction<bool(FString)> NameFilter);
	FString FindWMSRequestURL();
	void FindAndSetMaxSize();

	// WCS
	bool LoadWCSCapabilities(TFunction<bool(FString)> NameFilter);
	void DescribeWCSCoverages(int32 StartIndex, TArray<FString> ExcludeCRS, TFunction<void(bool)> OnComplete);
	bool LoadWCSDescriptions(const FString& File);
	bool CreateWCSURL(int Width, int Height, FString Name, double MinLong, double MaxLong, double MinLat, double MaxLat, FString &URL, bool &bGeoTiff, FString &FileExt);

	FString DescribeCoverageURL;
};

#undef LOCTEXT_NAMESPACE