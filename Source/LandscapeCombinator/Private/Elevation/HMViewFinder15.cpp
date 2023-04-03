// Copyright LandscapeCombinator. All Rights Reserved.

#include "Elevation/HMViewFinder15.h"
#include "Utils/Console.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

HMViewFinder15::HMViewFinder15(FString LandscapeLabel0, const FText &KindText0, FString Descr0, int Precision0) :
	HMInterfaceTiles(LandscapeLabel0, KindText0, Descr0, Precision0) {

	TileWidth = 14401;
	TileHeight = 10801;
	BaseURL = "http://www.viewfinderpanoramas.org/DEM/TIF15/";
}

bool HMViewFinder15::Initialize()
{
	if (!HMInterface::Initialize()) return false;

	for (auto &Tile : Files) {
		Tiles.Add(Tile);
		if (Tile.Len() != 4 || !Tile.StartsWith("15-") || Tile[3] < 'A' || Tile[3] > 'X') {
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("HMViewFinder15::Initialize", "Tile '{0}' is not a valid tile for Landscape {1}."),
					FText::FromString(Tile),
					FText::FromString(LandscapeLabel)
				)
			);
			return false;
		}
	}

	InitializeMinMaxTiles(); // must be called after `Tiles` has been filled

	for (auto &Tile : Tiles) {
		FString TifFile = FPaths::Combine(InterfaceDir, Tile, FString::Format(TEXT("{0}.tif"), { Tile }));
		FString ScaledFile = FPaths::Combine(ResultDir, FString::Format(TEXT("{0}.png"), { RenameWithPrecision(Tile) }));
		OriginalFiles.Add(TifFile);
		ScaledFiles.Add(ScaledFile);
	}

	if (!Console::Has7Z()) {
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


bool HMViewFinder15::GetCoordinatesSpatialReference(OGRSpatialReference &InRs) const
{
	return GetSpatialReferenceFromEPSG(InRs, 4326);
}

bool HMViewFinder15::GetDataSpatialReference(OGRSpatialReference &InRs) const
{
	return GetSpatialReferenceFromEPSG(InRs, 4326);
}

int HMViewFinder15::TileToX(FString Tile) const {
	char C = Tile[3];
	int T = C - 'A';
	return T % 6;
}

int HMViewFinder15::TileToY(FString Tile) const {
	char C = Tile[3];
	int T = C - 'A';
	return T / 6;
}

#undef LOCTEXT_NAMESPACE