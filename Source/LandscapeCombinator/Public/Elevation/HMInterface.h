// Copyright LandscapeCombinator. All Rights Reserved.

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
	virtual bool Initialize();
	FReply DownloadHeightMaps(TFunction<void(bool)> OnComplete) const;
	bool GetMinMaxZ(FVector2D& MinMaxZ);
	virtual FReply DownloadHeightMapsImpl(TFunction<void(bool)> OnComplete) const = 0;
	FReply ConvertHeightMaps(TFunction<void(bool)> OnComplete) const;
	FReply ImportHeightMaps();
	FReply CreateLandscape(int WorldWidthKm, int WorldHeightKm, double ZScale, double WorldOriginX, double WorldOriginY, TFunction<void(bool)> OnComplete);
	bool SetLandscapeFromLabel();
	void SetLandscape(ALandscape* Landscape0);
	void SetLandscapeStreamingProxies();
	FReply AdjustLandscape(int WorldWidthKm, int WorldHeightKm, double ZScale, double WorldOriginX, double WorldOriginY);
	bool GetCoordinates4326(FVector4d &Coordinates);
	FCollisionQueryParams CustomCollisionQueryParams(UWorld* World);
	double GetZ(UWorld* World, FCollisionQueryParams CollisionQueryParams, double x, double y);
	
	
	static void CouldNotCreateDirectory(FString Dir);
	static bool GetSpatialReference(OGRSpatialReference &InRs, FString File);
	static bool GetSpatialReference(OGRSpatialReference &InRs, GDALDataset* Dataset);
	static bool GetSpatialReferenceFromEPSG(OGRSpatialReference &InRs, int EPSG);
	static bool GetCoordinates(FVector4d& Coordinates, GDALDataset* Dataset);
	static bool GetMinMax(FVector2D &MinMax, TArray<FString> Files);

protected:
	const FText& KindText;
	FString Descr;
	int32 Precision;
	FString LandscapeCombinatorDir;
	FString InterfaceDir;
	FString ResultDir;
	
	TArray<FString> ScaledFiles;

	virtual bool GetInsidePixels(FIntPoint& InsidePixels) const;
	virtual bool GetMinMax(FVector2D &MinMax) const;
	virtual bool GetCoordinates(FVector4d &Coordinate) const;
	virtual bool GetCoordinatesSpatialReference(OGRSpatialReference &InRs) const = 0;
	virtual bool GetDataSpatialReference(OGRSpatialReference &InRs) const = 0;

private:
	ALandscape* GetLandscapeFromLabel();
};

#undef LOCTEXT_NAMESPACE
