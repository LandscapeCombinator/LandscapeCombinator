#include "HMRGEAlti.h"
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

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

HMRGEAlti::HMRGEAlti(FString LandscapeName0, const FText &KindText0, FString Descr0, int Precision0) :
		HMInterface(LandscapeName0, KindText0, Descr0, Precision0)
{
}

bool HMRGEAlti::Initialize() {
	if (!HMInterface::Initialize()) return false;

	FRegexPattern BBoxPattern(TEXT("(.*),(.*),(.*),(.*)"));
	FRegexMatcher BBoxMatcher(BBoxPattern, Descr);
	if (!BBoxMatcher.FindNext()) {
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("RGEAltiInitError", "Please use the format MinLong,MaxLong,MinLat,MaxLat for {0}."),
			FText::FromString(LandscapeName)
		));
		return false;
	}
	MinLong = FCString::Atod(*BBoxMatcher.GetCaptureGroup(1));
	MaxLong = FCString::Atod(*BBoxMatcher.GetCaptureGroup(2));
	MinLat  = FCString::Atod(*BBoxMatcher.GetCaptureGroup(3));
	MaxLat  = FCString::Atod(*BBoxMatcher.GetCaptureGroup(4));
	
	if (MinLong == 0 || MaxLong == 0 || MinLat == 0 || MaxLat == 0) {
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("RGEAltiInitError2", "Please enter MinLong,MaxLong,MinLat,MaxLat using decimals for {0}."),
			FText::FromString(LandscapeName)
		));
		return false;
	}
	FString FileName = Descr.Replace(TEXT(",") , TEXT("-"));
	FString TifFile = FPaths::Combine(InterfaceDir, FString::Format(TEXT("{0}.tif"), { FileName }));
	FString ScaledFile  = FPaths::Combine(ResultDir, FString::Format(TEXT("{0}{1}.png"), { LandscapeName, Precision }));

	OriginalFiles.Add(TifFile);
	ScaledFiles.Add(ScaledFile);
	return true;
}

bool HMRGEAlti::GetInsidePixels(FIntPoint &InsidePixels) const
{
	InsidePixels[0] = MaxLong - MinLong;
	InsidePixels[1] = MaxLat - MinLat;
	return true;
}

bool HMRGEAlti::GetSpatialReference(OGRSpatialReference &InRs) const
{
	InRs = SR2154;
	return true;
}

FReply HMRGEAlti::DownloadHeightMapsImpl() const
{		
	FIntPoint InsidePixels;
	GetInsidePixels(InsidePixels); // always returns true
	int InsidePixelWidth = InsidePixels[0];
	int InsidePixelHeight = InsidePixels[1];

	FString URL = FString::Format(
		TEXT("https://wxs.ign.fr/altimetrie/geoportail/r/wms?LAYERS=RGEALTI-MNT_PYR-ZIP_FXX_LAMB93_WMS&FORMAT=image/geotiff&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetMap&CRS=EPSG:2154&BBOX={0},{1},{2},{3}&WIDTH={4}&HEIGHT={5}&STYLES="),
		{ MinLong, MinLat, MaxLong, MaxLat, InsidePixelWidth, InsidePixelHeight }
	);

	Download::FromURL(URL, OriginalFiles[0]);

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE