#include "HMViewFinder1or3.h"
#include "Concurrency.h"
#include "Console.h"
#include "Download.h"

#include "HAL/FileManagerGeneric.h"
#include "Internationalization/Regex.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

HMViewFinder1or3::HMViewFinder1or3(FString LandscapeName0, const FText &KindText0, FString Descr0, int Precision0) :
	HMInterfaceTiles(LandscapeName0, KindText0, Descr0, Precision0) {

}

bool HMViewFinder1or3::Initialize()
{
	if (!HMInterface::Initialize()) return false;

	if (!Console::Has7Z()) {
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT(
				"MissingRequirement",
				"Please make sure 7z is installed on your computer and available in your PATH if you want to use Viewfinder Panoramas heightmaps."
			)
		);
		return false;
	}

    for (auto &File : Files) {
		FString ArchiveFolder = FPaths::Combine(InterfaceDir, File);
		FString AllExt = FString("*");
		FString SubFoldersPath = FPaths::Combine(ArchiveFolder, AllExt);
		TArray<FString> SubFolders;
		FFileManagerGeneric::Get().FindFiles(SubFolders, *SubFoldersPath, false, true);

		for (auto &SubFolder : SubFolders) {

			FString HGTExt = FString("*.hgt");
			FString HGT = FPaths::Combine(ArchiveFolder, SubFolder, HGTExt);

			TArray<FString> SomeTiles;
			FFileManagerGeneric::Get().FindFiles(SomeTiles, *HGT, true, false);

			for (auto &Tile : SomeTiles) {
				FString TileFile = FPaths::Combine(ArchiveFolder, SubFolder, Tile);
				Tiles.Add(Tile);
				OriginalFiles.Add(TileFile);
			}
		}
	}

	InitializeMinMaxTiles(); // must be called after `Tiles` has been filled


	for (auto &Tile : Tiles) {
		FString ScaledFile = FPaths::Combine(ResultDir, FString::Format(TEXT("{0}.png"), { RenameWithPrecision(Tile) }));
		ScaledFiles.Add(ScaledFile);
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

int HMViewFinder1or3::TileToX(FString Tile) const {
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

int HMViewFinder1or3::TileToY(FString Tile) const {
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
