#include "Road/RoadBuilderShortQuery.h"
#include "Utils/Overpass.h"
#include "Utils/Logging.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

FString FRoadBuilderShortQuery::DetailsString()
{
	return OverpassShortQuery;
}

void FRoadBuilderShortQuery::AddRoads(int WorldWidthKm, int WorldHeightKm, double ZScale, double WorldOriginX, double WorldOriginY)
{
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Adding roads with short Overpass query: '%s'"), *OverpassShortQuery);

	if (!HMTable->Interfaces.Contains(LandscapeLabel))
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("LandscapeNotFound", "Could not find Landscape '{0}' in the elevation table."),
				FText::FromString(LandscapeLabel)
			)
		);
		return;
	}

	HMInterface *Interface = HMTable->Interfaces[LandscapeLabel];
	
	FVector4d Coordinates;
	if (!Interface->GetCoordinates4326(Coordinates)) return;

	double South = Coordinates[2];
	double West = Coordinates[0];
	double North = Coordinates[3];
	double East = Coordinates[1];
	OverpassQuery = Overpass::QueryFromShortQuery(South, West, North, East, OverpassShortQuery);
	FRoadBuilderOverpass::AddRoads(WorldWidthKm, WorldHeightKm, ZScale, WorldOriginX, WorldOriginY);
}


#undef LOCTEXT_NAMESPACE