// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Landscape.h"
#include "LandscapeStreamingProxy.h"

#pragma warning(disable: 4668)
#include "gdal.h"
#include "gdal_priv.h"
#pragma warning(default: 4668)

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMInterface
{
public:
	HMInterface(FString LandscapeLabel0, const FText &KindText0, FString Descr0, int Precision0);
	virtual ~HMInterface() {};
	
	FString LandscapeLabel;
	ALandscape* Landscape;
	TArray<ALandscapeStreamingProxy*> LandscapeStreamingProxies;
	TArray<FString> OriginalFiles;
	TArray<FString> ScaledFiles;
	virtual bool Initialize();
	virtual FReply DownloadHeightMapsImpl(TFunction<void(bool)> OnComplete) = 0;
	FReply DownloadHeightMaps(TFunction<void(bool)> OnComplete);
	bool GetMinMaxZ(FVector2D& MinMaxZ);
	FReply ConvertHeightMaps(TFunction<void(bool)> OnComplete) const;
	FReply ImportHeightMaps();
	FReply CreateLandscape(int WorldWidthKm, int WorldHeightKm, double ZScale, double WorldOriginX, double WorldOriginY, TFunction<void(bool)> OnComplete);
	bool SetLandscapeFromLabel();
	void SetLandscape(ALandscape* Landscape0);
	void SetLandscapeStreamingProxies();
	FReply AdjustLandscape(int WorldWidthKm, int WorldHeightKm, double ZScale, double WorldOriginX, double WorldOriginY);
	FReply DigOtherLandscapes(int WorldWidthKm, int WorldHeightKm, double ZScale, double WorldOriginX, double WorldOriginY);
	bool GetCoordinates4326(FVector4d &Coordinates);
	FCollisionQueryParams CustomCollisionQueryParams(UWorld* World);
	double GetZ(UWorld* World, FCollisionQueryParams CollisionQueryParams, double x, double y);

protected:
	const FText& KindText;
	FString Descr;
	int32 Precision;
	FString LandscapeCombinatorDir;
	FString InterfaceDir;
	FString ResultDir;

	virtual bool GetInsidePixels(FIntPoint& InsidePixels) const;
	virtual bool GetMinMax(FVector2D &MinMax) const;
	virtual bool GetCoordinates(FVector4d &Coordinate) const;
	virtual bool GetCoordinatesSpatialReference(OGRSpatialReference &InRs) const = 0;
	virtual bool GetDataSpatialReference(OGRSpatialReference &InRs) const = 0;

private:
	static void CouldNotCreateDirectory(FString Dir);
};

#undef LOCTEXT_NAMESPACE
