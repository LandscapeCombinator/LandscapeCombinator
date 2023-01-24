#include "HMLocalFile.h"
#include "Concurrency.h"
#include "Console.h"

#include "Async/Async.h"
#include "LandscapeStreamingProxy.h"
#include "Engine/World.h"

#include "Kismet/GameplayStatics.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "Internationalization/Regex.h"

#include <atomic>

HMLocalFile::HMLocalFile(FString LandscapeName0, const FText &KindText0, FString Descr0, int Precision0) :
		HMInterface(LandscapeName0, KindText0, Descr0, Precision0)
{
}

bool HMLocalFile::Initialize() {
	if (!HMInterface::Initialize()) return false;

	FString ScaledFile = FPaths::Combine(ResultDir, FString::Format(TEXT("{0}-{1}.png"), { LandscapeName, Precision }));
	OriginalFiles.Add(Descr);
	ScaledFiles.Add(ScaledFile);
	return true;
}

bool HMLocalFile::GetDataSpatialReference(OGRSpatialReference &InRs) const
{
	return HMInterface::GetSpatialReference(InRs, OriginalFiles[0]);
}

bool HMLocalFile::GetCoordinatesSpatialReference(OGRSpatialReference &InRs) const
{
	return HMInterface::GetSpatialReference(InRs, OriginalFiles[0]);
}

FReply HMLocalFile::DownloadHeightMapsImpl(TFunction<void(bool)> OnComplete) const
{
	OnComplete(true);
	return FReply::Handled();
}
