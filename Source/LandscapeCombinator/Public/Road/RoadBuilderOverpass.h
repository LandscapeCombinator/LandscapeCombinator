// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Elevation/HMInterface.h"
#include "Road/RoadBuilder.h"

class FRoadBuilderOverpass : public FRoadBuilder {
public:
	FString OverpassQuery;
	
	FRoadBuilderOverpass() {}
	FRoadBuilderOverpass(HeightMapTable* HMTable0, FString LandscapeLabel0, FString OverpassQuery0) :
		FRoadBuilder(HMTable0, LandscapeLabel0, "")
	{
		OverpassQuery = OverpassQuery0;
		SourceKind = ESourceKind::OverpassAPI;
	}

	virtual FString DetailsString() override;
	virtual void AddRoads(int WorldWidthKm, int WorldHeightKm, double ZScale, double WorldOriginX, double WorldOriginY) override;

	friend FArchive& operator<<(FArchive& Ar, FRoadBuilderOverpass& RoadBuilder)
	{
		Ar << (FRoadBuilder&) RoadBuilder;
		Ar << RoadBuilder.OverpassQuery;
		return Ar;
	}

};
