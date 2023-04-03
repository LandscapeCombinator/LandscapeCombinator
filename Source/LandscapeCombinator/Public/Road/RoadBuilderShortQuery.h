// Copyright LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Road/RoadBuilderOverpass.h"

class FRoadBuilderShortQuery : public FRoadBuilderOverpass
{
public:

	FString OverpassShortQuery;

	FRoadBuilderShortQuery() {}
	FRoadBuilderShortQuery(
		HeightMapTable* HMTable0,
		FString LandscapeLabel0,
		FString OverpassShortQuery0
	) :
		FRoadBuilderOverpass(HMTable0, LandscapeLabel0, "")
	{
		SourceKind = ESourceKind::OverpassShortQuery;
		OverpassShortQuery = OverpassShortQuery0;
	}


	virtual FString DetailsString() override;
	virtual void AddRoads(int WorldWidthKm, int WorldHeightKm, double ZScale, double WorldOriginX, double WorldOriginY) override;
	
	friend FArchive& operator<<(FArchive& Ar, FRoadBuilderShortQuery& RoadBuilder)
	{
		Ar << (FRoadBuilderOverpass&) RoadBuilder;
		Ar << RoadBuilder.OverpassShortQuery;
		return Ar;
	}
};

#undef LOCTEXT_NAMESPACE