// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "Elevation/HMReprojected.h"
#include "Utils/Logging.h"
#include "Utils/GDALUtils.h"

#include "Internationalization/Regex.h"

#pragma warning(disable: 4668)
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#pragma warning(default: 4668)

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"


HMReprojected::HMReprojected(FString LandscapeLabel0, const FText& KindText0, FString Descr0, int Precision0, HMInterface* Target0) :
	HMInterface(LandscapeLabel0, KindText0, Descr0, Precision0)
{
	Target = Target0;
}

bool HMReprojected::Initialize()
{
	if (!HMInterface::Initialize()) return false;

	int32 NumFiles = Target->OriginalFiles.Num();
	check(NumFiles > 0);

	FString OriginalFile = Target->OriginalFiles[0];
	FString ReprojectedFile = FPaths::Combine(FPaths::GetPath(OriginalFile), FString::Format(TEXT("{0}-4326.tif"), { LandscapeLabel } ));
	FString ScaledFile = FPaths::Combine(ResultDir, FString::Format(TEXT("{0}-{1}.png"), { LandscapeLabel, Precision }));

	OriginalFiles.Add(ReprojectedFile);
	ScaledFiles.Add(ScaledFile);

	return true;
}

bool HMReprojected::GetDataSpatialReference(OGRSpatialReference &InRs) const
{
	return GDALUtils::GetSpatialReference(InRs, OriginalFiles[0]);
}

bool HMReprojected::GetCoordinatesSpatialReference(OGRSpatialReference &InRs) const
{
	return GDALUtils::GetSpatialReference(InRs, OriginalFiles[0]);
}

FReply HMReprojected::DownloadHeightMapsImpl(TFunction<void(bool)> OnComplete) const
{
	Target->DownloadHeightMapsImpl([this, OnComplete](bool bWasSuccessful) {
		if (bWasSuccessful)
		{
			int32 NumFiles = OriginalFiles.Num();
			check (NumFiles == 1);

			FString ReprojectedFile = OriginalFiles[0];

			OGRSpatialReference OutRs;
			if (
				GDALUtils::GetSpatialReferenceFromEPSG(OutRs, 4326) &&
				GDALUtils::Warp(Target->OriginalFiles, ReprojectedFile, OutRs, 0)
			)
			{
				if (OnComplete) OnComplete(true);
			}
			else
			{
				if (OnComplete) OnComplete(false);
			}
		}
		else
		{
			if (OnComplete) OnComplete(false);
		}
	});

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE