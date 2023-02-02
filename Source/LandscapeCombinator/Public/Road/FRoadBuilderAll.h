#pragma once

#include "Road/FRoadBuilderOverpass.h"

class FRoadBuilderAll : public FRoadBuilderOverpass {
public:
	FRoadBuilderAll() {}
	FRoadBuilderAll(
		HeightMapTable* HMTable0,
		FString LandscapeLabel0,
		FString LayerName0,
		float RoadWidth0
	) :
		FRoadBuilderOverpass(HMTable0, LandscapeLabel0, LayerName0, RoadWidth0, "")
	{
		SourceKind = ESourceKind::AllRoads;
	}
	virtual FString DetailsString() override;
	virtual void AddRoads(int WorldWidthKm, int WorldHeightKm, double ZScale, double WorldOriginX, double WorldOriginY) override;
	
	friend FArchive& operator<<(FArchive& Ar, FRoadBuilderAll& RoadBuilder)
	{
		Ar << (FRoadBuilder&) RoadBuilder;
		return Ar;
	}
};

#undef LOCTEXT_NAMESPACE