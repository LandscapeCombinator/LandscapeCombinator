// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "SlateTable.h"
#include "Elevation/HeightMapTable.h"
#include "Road/RoadBuilder.h"

class RoadTable : public SlateTable {
public:
	RoadTable(HeightMapTable* HMTable0);

	FString RoadListProjectFileV1;
	FString RoadListPluginFileV1;

	TArray<FRoadBuilder*> RoadBuilders;
	TMap<TSharedRef<SHorizontalBox>, FRoadBuilder*> RowToBuilder;

	HeightMapTable* HMTable;
	void Save() override;
	void Load() override;
	TSharedRef<SWidget> Header() override;
	TSharedRef<SWidget> Footer() override;
	void AddRoadRow(FRoadBuilder* RoadBuilder, bool bSave);

	virtual void OnRemove(TSharedRef<SHorizontalBox> Row) override;
};