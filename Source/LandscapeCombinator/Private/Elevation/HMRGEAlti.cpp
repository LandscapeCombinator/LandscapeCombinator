// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "Elevation/HMRGEAlti.h"
#include "Utils/Logging.h"
#include "Utils/Console.h"
#include "Utils/Concurrency.h"
#include "Utils/Download.h"

#include "Async/Async.h"
#include "LandscapeStreamingProxy.h"
#include "Engine/World.h"

#include "Kismet/GameplayStatics.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "Internationalization/Regex.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

HMRGEAlti::HMRGEAlti(FString LandscapeLabel0, const FText &KindText0, FString Descr0, int Precision0) :
		HMInterface(LandscapeLabel0, KindText0, Descr0, Precision0)
{
}

bool HMRGEAlti::Initialize() {
	if (!HMInterface::Initialize()) return false;

	FRegexPattern BBoxPattern(TEXT("([\\d\\s.]+),([\\d\\s.]+),([\\d\\s.]+),([\\d\\s.]+)(,([\\d\\s]+),([\\d\\s]+))?"));
	FRegexMatcher BBoxMatcher(BBoxPattern, Descr);

	if (!BBoxMatcher.FindNext()) {
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("RGEAltiInitError", "Please use the format MinLong,MaxLong,MinLat,MaxLat (and optionally ,Width,Height) for {0}."),
			FText::FromString(LandscapeLabel)
		));
		return false;
	}

	FString MinLongStr = BBoxMatcher.GetCaptureGroup(1).TrimStartAndEnd();
	FString MaxLongStr = BBoxMatcher.GetCaptureGroup(2).TrimStartAndEnd();
	FString MinLatStr = BBoxMatcher.GetCaptureGroup(3).TrimStartAndEnd();
	FString MaxLatStr = BBoxMatcher.GetCaptureGroup(4).TrimStartAndEnd();

	if (!MinLongStr.IsNumeric() || !MaxLongStr.IsNumeric() || !MinLatStr.IsNumeric() || !MaxLatStr.IsNumeric())
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("RGEAltiInitError2", "Please enter MinLong,MaxLong,MinLat,MaxLat (and optionally ,Width,Height) for {0}."),
			FText::FromString(LandscapeLabel)
		));
		return false;
	}

	MinLong = FCString::Atod(*MinLongStr);
	MaxLong = FCString::Atod(*MaxLongStr);
	MinLat  = FCString::Atod(*MinLatStr);
	MaxLat  = FCString::Atod(*MaxLatStr);

	if (BBoxMatcher.GetCaptureGroupEnding(4) != BBoxMatcher.GetMatchEnding() && !BBoxMatcher.GetCaptureGroup(5).IsEmpty())
	{
		FString WidthStr = BBoxMatcher.GetCaptureGroup(6).TrimStartAndEnd();
		FString HeightStr = BBoxMatcher.GetCaptureGroup(7).TrimStartAndEnd();

		if (!WidthStr.IsNumeric() || !HeightStr.IsNumeric())
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("RGEAltiInitError3", "Please enter MinLong,MaxLong,MinLat,MaxLat (and optionally ,Width,Height) for {0}. Width and Height must be integers."),
				FText::FromString(LandscapeLabel)
			));
			return false;
		}
		
		Width =  FCString::Atoi(*WidthStr);
		Height = FCString::Atoi(*HeightStr);
	}
	else
	{
		Width =  MaxLong - MinLong;
		Height = MaxLat - MinLat;
	}

	if (Width <= 0) {
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("RGEAltiInitErrorNegativeWidth", "The width of {0} is not positive. Make sure you entered the coordinates as MinLong,MaxLong,MinLat,MaxLat."),
			FText::FromString(LandscapeLabel)
		));
		return false;
	}

	if (Width > 10000) {
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("RGEAltiInitErrorLargeWidth", "The width of {0} is higher than 10000px, which is not supported by RGE Alti 1m WMS."),
			FText::FromString(LandscapeLabel)
		));
		return false;
	}

	if (Height <= 0) {
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("RGEAltiInitErrorNegativeHeight", "The height of {0} is not positive. Make sure you entered the coordinates as MinLong,MaxLong,MinLat,MaxLat."),
			FText::FromString(LandscapeLabel)
		));
		return false;
	}

	if (Height > 10000) {
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("RGEAltiInitErrorLargeHeight", "The height of {0} is higher than 10000px, which is not supported by RGE Alti 1m WMS."),
			FText::FromString(LandscapeLabel)
		));
		return false;
	}


	FString FileName = Descr.Replace(TEXT(",") , TEXT("-")).Replace(TEXT(" "), TEXT(""));
	FString TifFile = FPaths::Combine(InterfaceDir, FString::Format(TEXT("{0}.tif"), { FileName }));
	FString ScaledFile  = FPaths::Combine(ResultDir, FString::Format(TEXT("{0}-{1}.png"), { LandscapeLabel, Precision }));

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