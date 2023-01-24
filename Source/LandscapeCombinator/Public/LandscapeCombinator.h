// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HMInterface.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class FToolBarBuilder;
class FMenuBuilder;

class FLandscapeCombinatorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** This function will be bound to Command (by default it will bring up plugin window) */
	void PluginButtonClicked();
	
private:
	void RegisterMenus();
	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

private:
	TSharedPtr<class FUICommandList> PluginCommands;
	TSharedPtr<SVerticalBox> HeightMaps;
	TSharedPtr<SEditableTextBox> WorldWidthBlock;
	TSharedPtr<SEditableTextBox> WorldHeightBlock;
	TSharedPtr<SEditableTextBox> ZScaleBlock;
	
	FString HeightMapListProjectFileV0;
	FString HeightMapListPluginFileV1;
	FString HeightMapListProjectFileV1;

	struct HeightMapDetailsV0 {
		FString LandscapeName;
		FString KindText;
		FString Descr;
		int Precision;

		friend FArchive& operator<<(FArchive& Ar, HeightMapDetailsV0& Details) {
			Ar << Details.LandscapeName;
			Ar << Details.KindText;
			Ar << Details.Descr;
			Ar << Details.Precision;
			return Ar;
		}
	};

	struct HeightMapDetailsV1 {
		FString LandscapeName;
		FString KindText;
		FString Descr;
		int Precision;
		bool Reproject;

		friend FArchive& operator<<(FArchive& Ar, HeightMapDetailsV1& Details) {
			Ar << Details.LandscapeName;
			Ar << Details.KindText;
			Ar << Details.Descr;
			Ar << Details.Precision;
			Ar << Details.Reproject;
			return Ar;
		}
	};
	
	FReply LoadLandscapes();
	void AddHeightMapLine(FString LandscapeName, const FText& KindText, FString Descr, int Precision, bool bReproject, bool bSave);
	TSharedRef<SHorizontalBox> HeightMapLineAdder();

	void SaveHeightMaps();
	void LoadHeightMaps();
};

#undef LOCTEXT_NAMESPACE