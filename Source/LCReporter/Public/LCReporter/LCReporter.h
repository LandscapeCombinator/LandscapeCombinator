// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Styling/AppStyle.h"
#include "LCReporter.generated.h"

UCLASS()
class LCREPORTER_API ULCReporter : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

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

	static bool ShowMapboxFreeTierWarning();
	static bool ShowMapTilerFreeTierWarning();

	static void ShowOneError(const FText& Message, bool* bShowedDialog);
};
