#include "HMElevationAPI.h"
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
#include "Misc/FileHelper.h"

#include <atomic>

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

HMElevationAPI::HMElevationAPI(FString LandscapeName0, const FText &KindText0, FString Descr0, int Precision0) :
		HMInterface(LandscapeName0, KindText0, Descr0, Precision0)
{
}

bool HMElevationAPI::Initialize() {
	HMInterface::Initialize();

	FRegexPattern BBoxPattern(TEXT("(.*),(.*),(.*),(.*)"));
	FRegexMatcher BBoxMatcher(BBoxPattern, Descr);

	if (!BBoxMatcher.FindNext()) {
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("ElevationAPIInitError", "Please use the format MinLong,MaxLong,MinLat,MaxLat for {0}"),
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
			LOCTEXT("ElevationAPIInitError2", "Please enter MinLong,MaxLong,MinLat,MaxLat using decimals for {0}"),
			FText::FromString(LandscapeName)
		));
		return false;
	}
	
	FString FileName = Descr.Replace(TEXT(",") , TEXT("-")).Replace(TEXT(" "), TEXT(""));
	FString PngFile = FPaths::Combine(InterfaceDir, FString::Format(TEXT("{0}.png"), { FileName }));
	FString ScaledFile  = FPaths::Combine(ResultDir, FString::Format(TEXT("{0}-{1}.png"), { LandscapeName, Precision }));
	
	JSONFile = FString::Format(TEXT("{0}.json"), { PngFile.LeftChop(4) });
	OriginalFiles.Add(PngFile);
	ScaledFiles.Add(ScaledFile);
	return true;
}

FReply HMElevationAPI::DownloadHeightMapsImpl(TFunction<void(bool)> OnComplete) const {

	FString URL1 = FString::Format(
		TEXT("https://api.elevationapi.com/api/model/3d/bbox/{0}?dataset=IGN_1m&textured=false&format=STL&zFactor=1&meshReduceFactor=0.001&onlyEstimateSize=false"),
		{ Descr }
	);

	Download::FromURL(URL1, JSONFile, [this, OnComplete](bool bWasSuccessful1) {
		if (bWasSuccessful1) {
			FString JSONContent;
			FFileHelper::LoadFileToString(JSONContent, *JSONFile);

			FRegexPattern FilePathPattern("filePath\":\"([^\"]*)\"");
			FRegexMatcher FilePathMatcher(FilePathPattern, JSONContent);
			FilePathMatcher.FindNext();
			FString HeightMapFileName = FilePathMatcher.GetCaptureGroup(1);
		
			FString URL2 = FString::Format(TEXT("https://api.elevationapi.com{0}"), { HeightMapFileName });

			Download::FromURL(URL2, OriginalFiles[0], OnComplete);
		}
		else {
			OnComplete(false);
		}
	});

	return FReply::Handled();
}

 bool HMElevationAPI::GetMinMax(FVector2D &MinMax) const
{
	FString JSONContent;
	if (!FFileHelper::LoadFileToString(JSONContent, *JSONFile)) {
		
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(LOCTEXT("ElevationAPIGetMinMaxError", "Could not open JSON file `{0}`. Please try the Download button again."),
				FText::FromString(JSONContent)
			)
		);
		return false;
	}

	FRegexPattern MinMaxPattern("minElevation\":(.*),\"maxElevation\":([^,]*),");
	FRegexMatcher MinMaxMatcher(MinMaxPattern, JSONContent);

	if (!MinMaxMatcher.FindNext()) {
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(LOCTEXT("ElevationAPIGetMinMaxError2", "Could not read minElevation and maxElevation from JSON file `{0}`. You can try deleting the file and pressing `Download` again."),
				FText::FromString(JSONContent)
			)
		);
		return false;
	}

	MinMax[0] = FCString::Atod(*MinMaxMatcher.GetCaptureGroup(1));
	MinMax[1] = FCString::Atod(*MinMaxMatcher.GetCaptureGroup(2));

	if (MinMax[0] == 0 || MinMax[1] == 0) {
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(LOCTEXT("ElevationAPIGetMinMaxError3", "Could not convert minElevation and maxElevation to double in JSON file `{0}`. You can try deleting the file and pressing `Download` again."),
				FText::FromString(JSONContent)
			)
		);
		return false;
	}

	return true;
}

bool HMElevationAPI::GetCoordinates(FVector4d &Coordinates) const
{
	Coordinates[0] = MinLong;
	Coordinates[1] = MaxLong;
	Coordinates[2] = MinLat;
	Coordinates[3] = MaxLat;
	return true;
}

bool HMElevationAPI::GetCoordinatesSpatialReference(OGRSpatialReference &InRs) const
{
	return GetSpatialReferenceFromEPSG(InRs, 4326);
}

bool HMElevationAPI::GetDataSpatialReference(OGRSpatialReference &InRs) const
{
	return GetSpatialReferenceFromEPSG(InRs, 2154);
}

#undef LOCTEXT_NAMESPACE