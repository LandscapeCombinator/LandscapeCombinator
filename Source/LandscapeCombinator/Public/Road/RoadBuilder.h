#pragma once

#include "Elevation/HeightMapTable.h"
#include "Utils/Logging.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

enum ESourceKind: uint8 {
	AllRoads,
	RoadSelector,
	OverpassShortQuery,
	OverpassAPI,
	LocalFile
};

class FRoadBuilder {
public:
	HeightMapTable* HMTable;
	ESourceKind SourceKind;
	FString LandscapeLabel;
	FString XmlFilePath;
	
	FRoadBuilder() {}
	FRoadBuilder(HeightMapTable* HMTable0, FString LandscapeLabel0, FString XmlFilePath0)
	{
		HMTable = HMTable0;
		LandscapeLabel = LandscapeLabel0;
		XmlFilePath = XmlFilePath0;
		SourceKind = ESourceKind::LocalFile;
	}
	virtual ~FRoadBuilder() {};

	virtual FString DetailsString();
	virtual void AddRoads(int WorldWidthKm, int WorldHeightKm, double ZScale, double WorldOriginX, double WorldOriginY);

	friend FArchive& operator<<(FArchive& Ar, FRoadBuilder& RoadBuilder)
	{
		int Source = RoadBuilder.SourceKind;
		Ar << Source;
		RoadBuilder.SourceKind = (ESourceKind) Source; // loading
		Ar << RoadBuilder.LandscapeLabel;
		Ar << RoadBuilder.XmlFilePath;
		return Ar;
	}
};

#undef LOCTEXT_NAMESPACE