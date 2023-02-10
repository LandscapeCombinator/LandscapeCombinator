#pragma once

#include "Elevation/HeightMapTable.h"

enum ESourceKind {
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
	FString LayerName;
	FString XmlFilePath;
	float RoadWidth;
	
	FRoadBuilder() {}
	FRoadBuilder(HeightMapTable* HMTable0, FString LandscapeLabel0, FString LayerName0, FString XmlFilePath0, float RoadWidth0)
	{
		HMTable = HMTable0;
		LandscapeLabel = LandscapeLabel0;
		LayerName = LayerName0;
		XmlFilePath = XmlFilePath0;
		RoadWidth = RoadWidth0;
		SourceKind = ESourceKind::LocalFile;
	}
	virtual ~FRoadBuilder() {};

	virtual FString DetailsString();
	virtual void AddRoads(int WorldWidthKm, int WorldHeightKm, double ZScale, double WorldOriginX, double WorldOriginY);

	friend FArchive& operator<<(FArchive& Ar, FRoadBuilder& RoadBuilder)
	{
		int Source = RoadBuilder.SourceKind;
		Ar << Source;
		Ar << RoadBuilder.LandscapeLabel;
		Ar << RoadBuilder.LayerName;
		Ar << RoadBuilder.XmlFilePath;
		Ar << RoadBuilder.RoadWidth;
		return Ar;
	}
};