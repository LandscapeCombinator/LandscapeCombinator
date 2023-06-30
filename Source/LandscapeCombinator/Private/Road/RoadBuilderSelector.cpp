// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "Road/RoadBuilderSelector.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

FString FRoadBuilderSelector::DetailsString()
{
	TArray<FString> Result;
	if (bMotorway)		Result.Add("Motorway");
	if (bTrunk)			Result.Add("Trunk");
	if (bPrimary)		Result.Add("Primary");
	if (bSecondary)		Result.Add("Secondary");
	if (bTertiary)		Result.Add("Tertiary");
	if (bUnclassified)	Result.Add("Unclassified");
	if (bResidential)	Result.Add("Residential");
	return FString::Join(Result, *FString(" | "));
}

void FRoadBuilderSelector::AddRoads(int WorldWidthKm, int WorldHeightKm, double ZScale, double WorldOriginX, double WorldOriginY)
{
	OverpassShortQuery = FString::Format(TEXT("way[highway~\"({0})\"];"), { DetailsString().ToLower().Replace(TEXT(" "), TEXT("")) });
	FRoadBuilderShortQuery::AddRoads(WorldWidthKm, WorldHeightKm, ZScale, WorldOriginX, WorldOriginY);
}

#undef LOCTEXT_NAMESPACE