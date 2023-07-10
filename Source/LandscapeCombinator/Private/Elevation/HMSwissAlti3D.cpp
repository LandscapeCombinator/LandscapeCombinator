// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "Elevation/HMSwissALTI3D.h"
#include "Utils/Concurrency.h"
#include "Utils/Console.h"
#include "Utils/Download.h"

#include "HAL/FileManagerGeneric.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"


#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

HMSwissALTI3D::HMSwissALTI3D(FString LandscapeLabel0, const FText &KindText0, FString Descr0, int Precision0) :
	HMInterfaceTiles(LandscapeLabel0, KindText0, Descr0, Precision0) {

}

bool HMSwissALTI3D::Initialize()
{
	if (!HMInterface::Initialize()) return false;

	FString FileContent = "";
	if (!FFileHelper::LoadFileToString(FileContent, *Descr))
	{
		return false;
	}

	if (FileContent.Contains(FString("_0.5_"))) // 0.5m data
	{
		TileWidth = 2000;
		TileHeight = 2000;
	}
	else // 2m data
	{
		TileWidth = 500;
		TileHeight = 500;
	}

	TArray<FString> Lines;
	FileContent.ParseIntoArray(Lines, TEXT("\n"), true);

	for (auto& Line : Lines)
	{
		Tiles.Add(Line);
		TArray<FString> Elements;
		int NumElements = Line.ParseIntoArray(Elements, TEXT("/"), true);
		FString FileName = Elements[NumElements - 1];
		FString BaseName = FPaths::GetCleanFilename(FileName);
		
		FString OriginalFile = FPaths::Combine(InterfaceDir, FileName);
		OriginalFiles.Add(OriginalFile);
	}

	InitializeMinMaxTiles(); // must be called after `Tiles` has been filled

	for (auto &Tile : Tiles)
	{
		FString ScaledFile = FPaths::Combine(ResultDir, FString::Format(TEXT("{0}.png"), { RenameWithPrecision(Tile) }));
		ScaledFiles.Add(ScaledFile);
	}

	return true;
}

bool HMSwissALTI3D::GetCoordinatesSpatialReference(OGRSpatialReference &InRs) const
{
	return GetSpatialReferenceFromEPSG(InRs, 2056);
}

bool HMSwissALTI3D::GetDataSpatialReference(OGRSpatialReference &InRs) const
{
	return GetSpatialReferenceFromEPSG(InRs, 2056);
}

int HMSwissALTI3D::TileToX(FString Tile) const
{
	FRegexPattern Pattern(TEXT("swissalti3d_\\d+_(\\d+)-\\d+/"));
	FRegexMatcher Matcher(Pattern, Tile);
	Matcher.FindNext();
	FString X = Matcher.GetCaptureGroup(1);
	return FCString::Atoi(*X);
}

int HMSwissALTI3D::TileToY(FString Tile) const
{
	FRegexPattern Pattern(TEXT("swissalti3d_\\d+_\\d+-(\\d+)/"));
	FRegexMatcher Matcher(Pattern, Tile);
	Matcher.FindNext();
	FString Y = Matcher.GetCaptureGroup(1);
	return 1300 - FCString::Atoi(*Y);
}

FReply HMSwissALTI3D::DownloadHeightMapsImpl(TFunction<void(bool)> OnComplete) const
{
	check(Tiles.Num() == OriginalFiles.Num());
	Concurrency::RunMany(
		Tiles.Num(),
		[this](int i, TFunction<void (bool)> OnCompleteElement)
		{
			Download::FromURL(Tiles[i], OriginalFiles[i], OnCompleteElement);
		},
		OnComplete
	);

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
