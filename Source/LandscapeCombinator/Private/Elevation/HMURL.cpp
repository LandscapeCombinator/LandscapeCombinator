// Copyright LandscapeCombinator. All Rights Reserved.

#include "Elevation/HMURL.h"
#include "Utils/Logging.h"
#include "Utils/Console.h"
#include "Utils/Concurrency.h"
#include "Utils/Download.h"

#include "Async/Async.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "Internationalization/Regex.h"


HMURL::HMURL(FString LandscapeLabel0, const FText &KindText0, FString Descr0, int Precision0) :
		HMInterface(LandscapeLabel0, KindText0, Descr0, Precision0)
{
}

bool HMURL::Initialize()
{
	if (!HMInterface::Initialize()) return false;
	
	FString OriginalFile = FPaths::Combine(InterfaceDir, FString::Format(TEXT("{0}-{1}.tif"), { LandscapeLabel, Precision }));
	FString ScaledFile = FPaths::Combine(InterfaceDir, FString::Format(TEXT("{0}-{1}.png"), { LandscapeLabel, Precision }));
	OriginalFiles.Add(OriginalFile);
	ScaledFiles.Add(ScaledFile);
	return true;
}

bool HMURL::GetDataSpatialReference(OGRSpatialReference &InRs) const
{
	return HMInterface::GetSpatialReference(InRs, Descr);
}

bool HMURL::GetCoordinatesSpatialReference(OGRSpatialReference &InRs) const
{
	return HMInterface::GetSpatialReference(InRs, Descr);
}

FReply HMURL::DownloadHeightMapsImpl(TFunction<void(bool)> OnComplete) const
{
	Download::FromURL(Descr, OriginalFiles[0], OnComplete);
	return FReply::Handled();
}
