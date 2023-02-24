#pragma once

#include "Road/RoadBuilderShortQuery.h"

class FRoadBuilderSelector : public FRoadBuilderShortQuery {
public:
	bool bMotorway;
	bool bTrunk;
	bool bPrimary;
	bool bSecondary;
	bool bTertiary;
	bool bUnclassified;
	bool bResidential;
	
	FRoadBuilderSelector() {}
	FRoadBuilderSelector(
		HeightMapTable* HMTable0,
		FString LandscapeLabel0,
		bool bMotorway0,
		bool bTrunk0,
		bool bPrimary0,
		bool bSecondary0,
		bool bTertiary0,
		bool bUnclassified0,
		bool bResidential0
	) :
		FRoadBuilderShortQuery(HMTable0, LandscapeLabel0, "")
	{
		SourceKind = ESourceKind::RoadSelector;
		bMotorway = bMotorway0;
		bTrunk = bTrunk0;
		bPrimary = bPrimary0;
		bSecondary = bSecondary0;
		bTertiary = bTertiary0;
		bUnclassified = bUnclassified0;
		bResidential = bResidential0;
	}
	virtual FString DetailsString() override;
	virtual void AddRoads(int WorldWidthKm, int WorldHeightKm, double ZScale, double WorldOriginX, double WorldOriginY) override;
	
	friend FArchive& operator<<(FArchive& Ar, FRoadBuilderSelector& RoadBuilder)
	{
		Ar << (FRoadBuilderShortQuery&) RoadBuilder;
		Ar << RoadBuilder.bMotorway;
		Ar << RoadBuilder.bTrunk;
		Ar << RoadBuilder.bPrimary;
		Ar << RoadBuilder.bSecondary;
		Ar << RoadBuilder.bTertiary;
		Ar << RoadBuilder.bUnclassified;
		Ar << RoadBuilder.bResidential;
		return Ar;
	}
};
