// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SlateTable {
public:
	SlateTable();
	virtual ~SlateTable() {};

	TSharedPtr<SVerticalBox> Rows;
	virtual void Save() = 0;
	virtual void Load() = 0;
	void AddRow(TSharedRef<SHorizontalBox> Row, bool bControlButtons, bool bSave, float Fill);
	virtual TSharedRef<SWidget> MakeTable();

protected:
	TArray<float> ColumnsSizes;
	virtual TSharedRef<SWidget> Header() = 0;
	virtual TSharedRef<SWidget> Footer() = 0;
	virtual void OnRemove(TSharedRef<SHorizontalBox> Line);

private:
	int32 GetIndex(TSharedRef<SHorizontalBox> Row);
};