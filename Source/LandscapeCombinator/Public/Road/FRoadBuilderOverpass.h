#pragma once

#include "Elevation/HMInterface.h"
#include "Road/FRoadBuilder.h"

class FRoadBuilderOverpass : public FRoadBuilder {
public:
	FString OverpassQuery;
	
	FRoadBuilderOverpass() {}
	FRoadBuilderOverpass(HeightMapTable* HMTable0, FString LandscapeLabel0, FString LayerName0, float RoadWidth0, FString OverpassQuery0) :
		FRoadBuilder(HMTable0, LandscapeLabel0, LayerName0, "", RoadWidth0)
	{
		OverpassQuery = OverpassQuery0;
		SourceKind = ESourceKind::OverpassAPI;
	}

	virtual FString DetailsString() override;
	//virtual FRoadBuilderDetails Details() override;
	virtual void AddRoads(int WorldWidthKm, int WorldHeightKm, double ZScale, double WorldOriginX, double WorldOriginY) override;

	friend FArchive& operator<<(FArchive& Ar, FRoadBuilderOverpass& RoadBuilder)
	{
		Ar << (FRoadBuilder&) RoadBuilder;
		Ar << RoadBuilder.OverpassQuery;
		return Ar;
	}

};
