#include "HMURL.h"
#include "Concurrency.h"
#include "Console.h"
#include "Download.h"

#include "Async/Async.h"
#include "LandscapeStreamingProxy.h"
#include "Engine/World.h"

#include "Kismet/GameplayStatics.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "Internationalization/Regex.h"

#include <atomic>

HMURL::HMURL(FString LandscapeName0, const FText &KindText0, FString Descr0, int Precision0) :
		HMInterface(LandscapeName0, KindText0, Descr0, Precision0)
{
}

bool HMURL::Initialize()
{
	if (!HMInterface::Initialize()) return false;
	
	FString OriginalFile = FPaths::Combine(LandscapeCombinatorDir, FString::Format(TEXT("{0}-{1}.tif"), { LandscapeName, Precision }));
	FString ScaledFile = FPaths::Combine(LandscapeCombinatorDir, FString::Format(TEXT("{0}-{1}.png"), { LandscapeName, Precision }));
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
