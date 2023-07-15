// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "Elevation/HMViewFinder1or3.h"
#include "Utils/Concurrency.h"
#include "Utils/Console.h"
#include "Utils/Download.h"
#include "Utils/Logging.h"

#include "HAL/FileManagerGeneric.h"
#include "Internationalization/Regex.h"


#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

HMViewFinder1or3::HMViewFinder1or3(FString LandscapeLabel0, const FText &KindText0, FString Descr0, int Precision0) :
	HMViewFinder(LandscapeLabel0, KindText0, Descr0, Precision0)
{
}

bool HMViewFinder1or3::Initialize()
{
	if (!HMViewFinder::Initialize()) return false;

	if (!Console::Has7Z())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT(
				"MissingRequirement",
				"Please make sure 7z is installed on your computer and available in your PATH if you want to use Viewfinder Panoramas heightmaps."
			)
		);
		return false;
	}

	return true;
}

bool HMViewFinder1or3::GetCoordinatesSpatialReference(OGRSpatialReference &InRs) const
{
	return GetSpatialReferenceFromEPSG(InRs, 4326);
}

bool HMViewFinder1or3::GetDataSpatialReference(OGRSpatialReference &InRs) const
{
	return GetSpatialReferenceFromEPSG(InRs, 4326);
}

FReply HMViewFinder1or3::DownloadHeightMapsImpl(TFunction<void(bool)> OnComplete)
{
	HMViewFinder::DownloadHeightMapsImpl([OnComplete, this](bool bWasSuccessful) {
		if (!bWasSuccessful)
		{
			OnComplete(false);
			return;
		}
		for (auto& MegaTile0 : MegaTiles)
		{
			FString MegaTile = MegaTile0.TrimStartAndEnd();
			FString ArchiveFolder = FPaths::Combine(InterfaceDir, MegaTile);
			FString AllExt = FString("*");
			FString SubFoldersPath = FPaths::Combine(ArchiveFolder, AllExt);
			TArray<FString> SubFolders;
			FFileManagerGeneric::Get().FindFiles(SubFolders, *SubFoldersPath, false, true);

			for (auto& SubFolder : SubFolders)
			{

				FString HGTExt = FString("*.hgt");
				FString HGT = FPaths::Combine(ArchiveFolder, SubFolder, HGTExt);

				TArray<FString> SomeTiles;
				FFileManagerGeneric::Get().FindFiles(SomeTiles, *HGT, true, false);

				for (auto& Tile : SomeTiles)
				{
					FString TileFile = FPaths::Combine(ArchiveFolder, SubFolder, Tile);
					Tiles.Add(Tile);
					OriginalFiles.Add(TileFile);
				}
			}
		}

		HMInterfaceTiles::InitializeMinMaxTiles(); // must be called after `Tiles` has been filled


		for (auto& Tile : Tiles)
		{
			FString ScaledFile = FPaths::Combine(ResultDir, FString::Format(TEXT("{0}.png"), { RenameWithPrecision(Tile) }));
			ScaledFiles.Add(ScaledFile);
		}

		OnComplete(true);
		return;
	});
	return FReply::Handled();
}

int HMViewFinder1or3::TileToX(FString Tile) const
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

int HMViewFinder1or3::TileToY(FString Tile) const
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

#undef LOCTEXT_NAMESPACE
