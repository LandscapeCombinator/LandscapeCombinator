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

	Width =  MaxLong - MinLong;
	Height = MaxLat - MinLat;

	if (Width < 0) {
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("RGEAltiInitErrorNegativeWidth", "The width of {0} is negative. Make sure you entered the coordinates as MinLong,MaxLong,MinLat,MaxLat."),
			FText::FromString(LandscapeName)
		));
		return false;
	}

	if (Width > 10000) {
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("RGEAltiInitErrorLargeWidth", "The width of {0} is higher than 10000, which is not supported by RGE Alti 1m WMS."),
			FText::FromString(LandscapeName)
		));
		return false;
	}

	if (Height < 0) {
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("RGEAltiInitErrorNegativeHeight", "The height of {0} is negative. Make sure you entered the coordinates as MinLong,MaxLong,MinLat,MaxLat."),
			FText::FromString(LandscapeName)
		));
		return false;
	}

	if (Height > 10000) {
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("RGEAltiInitErrorLargeHeight", "The height of {0} is higher than 10000, which is not supported by RGE Alti 1m WMS."),
			FText::FromString(LandscapeName)
		));
		return false;
	}


	FString FileName = Descr.Replace(TEXT(",") , TEXT("-")).Replace(TEXT(" "), TEXT(""));
	FString TifFile = FPaths::Combine(InterfaceDir, FString::Format(TEXT("{0}.tif"), { FileName }));
	FString ScaledFile  = FPaths::Combine(ResultDir, FString::Format(TEXT("{0}-{1}.png"), { LandscapeName, Precision }));

	OriginalFiles.Add(TifFile);
	ScaledFiles.Add(ScaledFile);
	return true;
}

bool HMRGEAlti::GetInsidePixels(FIntPoint &InsidePixels) const
{
	InsidePixels[0] = Width;
	InsidePixels[1] = Height;
	return true;
}

bool HMRGEAlti::GetCoordinatesSpatialReference(OGRSpatialReference &InRs) const
{
	return GetSpatialReferenceFromEPSG(InRs, 2154);
}

bool HMRGEAlti::GetDataSpatialReference(OGRSpatialReference &InRs) const
{
	return GetSpatialReferenceFromEPSG(InRs, 2154);
}

FReply HMRGEAlti::DownloadHeightMapsImpl(TFunction<void(bool)> OnComplete) const
{

	FString URL = FString::Format(
		TEXT("https://wxs.ign.fr/altimetrie/geoportail/r/wms?LAYERS=RGEALTI-MNT_PYR-ZIP_FXX_LAMB93_WMS&FORMAT=image/geotiff&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetMap&CRS=EPSG:2154&BBOX={0},{1},{2},{3}&WIDTH={4}&HEIGHT={5}&STYLES="),
		{ MinLong, MinLat, MaxLong, MaxLat, Width, Height }
	);

	Download::FromURL(URL, OriginalFiles[0], OnComplete);

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE