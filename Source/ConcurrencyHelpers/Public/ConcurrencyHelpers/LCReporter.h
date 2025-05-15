// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class CONCURRENCYHELPERS_API LCReporter
{
public:
	static void ShowError(const FText& Message, const FText& Title = FText());

	static bool ShowMessage(
		const FText& Message, const FString& InIniSettingName, const FText& Title = FText(),
		bool bCancellable = true, const FSlateBrush *Image = nullptr
	);

	static void ShowInfo(const FText& Message, const FString& InIniSettingName, const FText& Title = FText())
	{
		ShowMessage(Message, InIniSettingName, Title, false, FAppStyle::GetBrush("Icons.InfoWithColor.Large"));
	}

	static void ShowOneError(const FText& Message, bool* bShowedDialog);
};

#undef LOCTEXT_NAMESPACE
