#include "Road/FRoadBuilderAll.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

FString FRoadBuilderAll::DetailsString()
{
	return "All roads";
}

void FRoadBuilderAll::AddRoads(int WorldWidthKm, int WorldHeightKm, double ZScale, double WorldOriginX, double WorldOriginY)
{
	FVector4d Coordinates;
	
	if (!HMTable->Interfaces.Contains(LandscapeLabel))
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("LandscapeNotFound", "Could not find Landscape '{0}' in the elevation interfaces."),
				FText::FromString(LandscapeLabel)
			)
		);
		return;
	}

	HMInterface *Interface = HMTable->Interfaces[LandscapeLabel];

	if (!Interface->GetCoordinates4326(Coordinates)) return;

	double South = Coordinates[2];
	double West = Coordinates[0];
	double North = Coordinates[3];
	double East = Coordinates[1];
	OverpassQuery = FString::Format(
		TEXT("https://overpass-api.de/api/interpreter?data=%5Bbbox%3A{0}%2C{1}%2C{2}%2C{3}%5D%3Bway%5B%22highway%22%5D%3B%28%2E%5F%3B%3E%3B%29%3Bout%3B%0A"),
		{ South, West, North, East }
	);

	FRoadBuilderOverpass::AddRoads(WorldWidthKm, WorldHeightKm, ZScale, WorldOriginX, WorldOriginY);
}


#undef LOCTEXT_NAMESPACE