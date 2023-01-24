#pragma once

#include "CoreMinimal.h"

#pragma warning(disable: 4668)
#include "gdal.h"
#include "gdal_priv.h"
#pragma warning(default: 4668)

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMInterface
{
public:
	HMInterface(FString LandscapeName0, const FText &KindText0, FString Descr0, int Precision0);
	virtual ~HMInterface() {};
	
	FString LandscapeName;
	TArray<FString> OriginalFiles;
	virtual bool Initialize();
	FReply DownloadHeightMaps(TFunction<void(bool)> OnComplete) const;
	virtual FReply DownloadHeightMapsImpl(TFunction<void(bool)> OnComplete) const = 0;
	FReply ConvertHeightMaps() const;
	FReply AdjustLandscape(int WorldWidthKm, int WorldHeigtKm, double ZScale, bool AdjustPosition) const;

protected:
	const FText& KindText;
	FString Descr;
	int32 Precision;
	FString LandscapeCombinatorDir;
	FString InterfaceDir;
	FString ResultDir;
	
	TArray<FString> ScaledFiles;
	
	static void CouldNotCreateDirectory(FString Dir);
	static bool GetSpatialReference(OGRSpatialReference &InRs, FString File);
	static bool GetSpatialReferenceFromEPSG(OGRSpatialReference &InRs, int EPSG);
	static bool GetMinMax(FVector2D &MinMax, TArray<FString> Files);

	virtual bool GetInsidePixels(FIntPoint& InsidePixels) const;
	virtual bool GetMinMax(FVector2D &MinMax) const;
	virtual bool GetCoordinates(FVector4d &Coordinate) const;
	virtual bool GetCoordinatesSpatialReference(OGRSpatialReference &InRs) const = 0;
	virtual bool GetDataSpatialReference(OGRSpatialReference &InRs) const = 0;
};

#undef LOCTEXT_NAMESPACE
