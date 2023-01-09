#pragma once

#include "CoreMinimal.h"

#pragma warning(disable: 4668)
#include "gdal.h"
#include "gdal_priv.h"
#pragma warning(default: 4668)

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMInterface
{

protected:
	FString LandscapeName;
	const FText& KindText;
	FString Descr;
	int32 Precision;
	FString LandscapeCombinatorDir;
	FString InterfaceDir;
	FString ResultDir;

	TArray<FString> OriginalFiles;
	TArray<FString> ScaledFiles;
	
	static bool GetSpatialReference(OGRSpatialReference &InRs, FString File);
	static void CouldNotCreateDirectory(FString Dir);

	virtual bool GetInsidePixels(FIntPoint& InsidePixels) const;
	virtual bool GetMinMax(FVector2D &MinMax) const;
	virtual bool GetCoordinates(FVector4d &Coordinates) const;
	virtual bool GetSpatialReference(OGRSpatialReference &InRs) const = 0;

public:
	HMInterface(FString LandscapeName0, const FText &KindText0, FString Descr0, int Precision0);
	virtual ~HMInterface() {};
	
	virtual bool Initialize();
	FReply DownloadHeightMaps() const;
	virtual FReply DownloadHeightMapsImpl() const = 0;
	FReply ConvertHeightMaps() const;
	FReply AdjustLandscape(int WorldWidthKm, int WorldHeigtKm) const;
};

#undef LOCTEXT_NAMESPACE
