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
	void AddRow(TSharedRef<SHorizontalBox> Row, bool bControlButtons, bool bSave);
	virtual TSharedRef<SWidget> MakeTable();

protected:
	int ButtonWidth = 35;
	virtual TSharedRef<SWidget> Header() = 0;
	virtual TSharedRef<SWidget> Footer() = 0;
	virtual void OnRemove(TSharedRef<SHorizontalBox> Line);
	TSharedRef<SBox> ButtonBox(TSharedRef<SButton> Button);
	EVisibility VisibleFrom(int Threshold) const;

private:
	int32 GetIndex(TSharedRef<SHorizontalBox> Row);
};