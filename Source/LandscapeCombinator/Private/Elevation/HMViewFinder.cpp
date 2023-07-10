// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "Elevation/HMViewFinder.h"
#include "Utils/Concurrency.h"
#include "Utils/Console.h"
#include "Utils/Download.h"

#include "HAL/FileManagerGeneric.h"
#include "Internationalization/Regex.h"


#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

HMViewFinder::HMViewFinder(FString LandscapeLabel0, const FText &KindText0, FString Descr0, int Precision0) :
	HMInterfaceTiles(LandscapeLabel0, KindText0, Descr0, Precision0)
{
	TArray<FString> MegaTiles0;
	Descr.ParseIntoArray(MegaTiles0, TEXT(","), true);
	for (auto& MegaTile : MegaTiles0)
	{
		MegaTiles.Add(MegaTile.TrimStartAndEnd());
	}
}

bool HMViewFinder::Initialize()
{
	return true;
}

bool HMViewFinder::GetCoordinatesSpatialReference(OGRSpatialReference &InRs) const
{
	return GetSpatialReferenceFromEPSG(InRs, 4326);
}

bool HMViewFinder::GetDataSpatialReference(OGRSpatialReference &InRs) const
{
	return GetSpatialReferenceFromEPSG(InRs, 4326);
}

int HMViewFinder::TileToX(FString Tile) const
{
	FRegexPattern Pattern(TEXT("([WE])(\\d\\d\\d)"));
	FRegexMatcher Matcher(Pattern, Tile);
	Matcher.FindNext();
	FString Direction = Matcher.GetCaptureGroup(1);
	FString Degrees = Matcher.GetCaptureGroup(2);
	if (Direction == "W")
		return 180 - FCString::Atoi(*Degrees);
	else
		return 180 + FCString::Atoi(*Degrees);
}

int HMViewFinder::TileToY(FString Tile) const
{
	FRegexPattern Pattern(TEXT("([NS])(\\d\\d)"));
	FRegexMatcher Matcher(Pattern, Tile);
	Matcher.FindNext();
	FString Direction = Matcher.GetCaptureGroup(1);
	FString Degrees = Matcher.GetCaptureGroup(2);
	if (Direction == "N")
		return 83 - FCString::Atoi(*Degrees);
	else
		return 83 + FCString::Atoi(*Degrees);
}

FReply HMViewFinder::DownloadHeightMapsImpl(TFunction<void(bool)> OnComplete) const
{
	Concurrency::RunMany<FString>(
		MegaTiles,
		[this](FString File, TFunction<void(bool)> OnCompleteElement)
		{
			FString ZipFile = FPaths::Combine(InterfaceDir, FString::Format(TEXT("{0}.zip"), { File }));
			FString ExtractionDir = FPaths::Combine(InterfaceDir, File);
			FString URL = FString::Format(TEXT("{0}{1}.zip"), { BaseURL, File });

			Download::FromURL(URL, ZipFile, [ExtractionDir, ZipFile, OnCompleteElement](bool bWasSuccessful)
			{
				if (bWasSuccessful)
				{
					FString ExtractParams = FString::Format(TEXT("x -y \"{0}\" -o\"{1}\""), { ZipFile, ExtractionDir });
					bool bWasExtracted = Console::ExecProcess(TEXT("7z"), *ExtractParams, nullptr);
					OnCompleteElement(bWasExtracted);
				}
				else
				{
					OnCompleteElement(false);
				}
			});
		},
		OnComplete
	);

	return FReply::Handled();
}


#undef LOCTEXT_NAMESPACE
