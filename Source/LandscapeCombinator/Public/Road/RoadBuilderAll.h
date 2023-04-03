// Copyright LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Road/RoadBuilderShortQuery.h"

class FRoadBuilderAll : public FRoadBuilderShortQuery {
public:
	FRoadBuilderAll() {}
	FRoadBuilderAll(
		HeightMapTable* HMTable0,
		FString LandscapeLabel0
	) :
		FRoadBuilderShortQuery(HMTable0, LandscapeLabel0, "way[\"highway\"];")
	{
		SourceKind = ESourceKind::AllRoads;
	}
	virtual FString DetailsString() override;
	
	friend FArchive& operator<<(FArchive& Ar, FRoadBuilderAll& RoadBuilder)
	{
		Ar << (FRoadBuilderShortQuery&) RoadBuilder;
		return Ar;
	}
};

#undef LOCTEXT_NAMESPACE