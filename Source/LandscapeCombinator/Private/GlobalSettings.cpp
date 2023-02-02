#include "GlobalSettings.h"
#include "LandscapeCombinatorStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Utils/Logging.h"
#include "Internationalization/Regex.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

namespace GlobalSettings
{

	void Save(const FText&, ETextCommit::Type)
	{
		FString GlobalSettingsProjectFileV1 = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()) / "GlobalSettingsV1";
		WorldParametersV1 GlobalParams;
		if (!GetWorldParameters(GlobalParams)) 

		IPlatformFile::GetPlatformPhysical().CreateDirectory(*FPaths::ProjectSavedDir());
		UE_LOG(LogLandscapeCombinator, Log, TEXT("Saving global params to '%s'"), *GlobalSettingsProjectFileV1);
		FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*GlobalSettingsProjectFileV1);
		if (FileWriter)
		{
			UE_LOG(LogLandscapeCombinator, Log, TEXT("saving '%d'"), GlobalParams.WorldWidthKm);
			*FileWriter << GlobalParams;
			if (FileWriter->Close()) return;
		}

		UE_LOG(LogLandscapeCombinator, Error, TEXT("Failed to save global params to '%s'"), *GlobalSettingsProjectFileV1);
	}

	void Load()
	{
		FString GlobalSettingsPluginFileV1  = FPaths::Combine(IPluginManager::Get().FindPlugin("LandscapeCombinator")->GetBaseDir(), "GlobalSettingsV1");
		FString GlobalSettingsProjectFileV1 = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()) / "GlobalSettingsV1";

		FArchive* FileReader = IFileManager::Get().CreateFileReader(*GlobalSettingsProjectFileV1);
		if (FileReader)
		{
			UE_LOG(LogLandscapeCombinator, Log, TEXT("Loading global settings from %s"), *GlobalSettingsProjectFileV1);
		}
		else
		{
			FileReader = IFileManager::Get().CreateFileReader(*GlobalSettingsPluginFileV1);
			if (FileReader)
				UE_LOG(LogLandscapeCombinator, Log, TEXT("Loading global settings from %s"), *GlobalSettingsPluginFileV1);
		}

		if (FileReader)
		{
			WorldParametersV1 GlobalParams;
			*FileReader << GlobalParams;
			

			if (FileReader->Close()) {
				// FString::Format has easier rounding and formatting defaults than FText::AsNumber
				WorldWidthBlock->SetText(FText::FromString(FString::Format(TEXT("{0}"), { GlobalParams.WorldWidthKm })));
				WorldHeightBlock->SetText(FText::FromString(FString::Format(TEXT("{0}"), { GlobalParams.WorldHeightKm })));
				
				ZScaleBlock->SetText(FText::FromString(FString::Format(TEXT("{0}"), { GlobalParams.ZScale })));
				WorldOriginXBlock->SetText(FText::FromString(FString::Format(TEXT("{0}"), { GlobalParams.WorldOriginX })));
				WorldOriginYBlock->SetText(FText::FromString(FString::Format(TEXT("{0}"), { GlobalParams.WorldOriginY })));
				return;
			}
		}

		UE_LOG(LogLandscapeCombinator, Error, TEXT("Failed to load the road builders list"));
	}

	TSharedRef<SWidget> GlobalSettingsTable()
	{
		SAssignNew(WorldWidthBlock, SEditableTextBox)
			.SelectAllTextWhenFocused(true)
			.Text(FText::FromString("40000"))
			.Font(FLandscapeCombinatorStyle::RegularFont())
			.OnTextCommitted_Lambda(&Save)
		;
		SAssignNew(WorldHeightBlock, SEditableTextBox)
			.SelectAllTextWhenFocused(true)
			.Text(FText::FromString("20000"))
			.Font(FLandscapeCombinatorStyle::RegularFont())
			.OnTextCommitted_Lambda(&Save)
		;
		SAssignNew(ZScaleBlock, SEditableTextBox)
			.SelectAllTextWhenFocused(true)
			.Text(FText::FromString("1"))
			.Font(FLandscapeCombinatorStyle::RegularFont())
			.OnTextCommitted_Lambda(&Save)
		;
		SAssignNew(WorldOriginXBlock, SEditableTextBox)
			.SelectAllTextWhenFocused(true)
			.Text(FText::FromString("0"))
			.Font(FLandscapeCombinatorStyle::RegularFont())
			.OnTextCommitted_Lambda(&Save)
		;
		SAssignNew(WorldOriginYBlock, SEditableTextBox)
			.SelectAllTextWhenFocused(true)
			.Text(FText::FromString("0"))
			.Font(FLandscapeCombinatorStyle::RegularFont())
			.OnTextCommitted_Lambda(&Save)
		;

		Load();

		return
			SNew(SHorizontalBox)
				+SHorizontalBox::Slot().FillWidth(0.15)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot().Padding(FMargin(0, 0, 0, 10)).AutoHeight()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot().FillWidth(0.66) [ SNew(STextBlock).Text(LOCTEXT("WorldWidth", "World Width (km)")).Font(FLandscapeCombinatorStyle::RegularFont()) ]
						+SHorizontalBox::Slot().FillWidth(0.33) [ WorldWidthBlock.ToSharedRef() ]
					]
					+SVerticalBox::Slot().Padding(FMargin(0, 0, 0, 10)).AutoHeight()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot().FillWidth(0.66) [ SNew(STextBlock).Text(LOCTEXT("WorldHeight", "World Height (km)")).Font(FLandscapeCombinatorStyle::RegularFont()) ]
						+SHorizontalBox::Slot().FillWidth(0.33) [ WorldHeightBlock.ToSharedRef() ]
					]
					+SVerticalBox::Slot().Padding(FMargin(0, 0, 0, 10)).AutoHeight()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot().FillWidth(0.66) [ SNew(STextBlock).Text(LOCTEXT("ZScale", "Z Scale of the world")).Font(FLandscapeCombinatorStyle::RegularFont()) ]
						+SHorizontalBox::Slot().FillWidth(0.33) [ ZScaleBlock.ToSharedRef() ]
					]
				]
				+SHorizontalBox::Slot().FillWidth(0.05)
				+SHorizontalBox::Slot().FillWidth(0.15)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot().Padding(FMargin(0, 0, 0, 10)).AutoHeight()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot().FillWidth(0.66) [ SNew(STextBlock).Text(LOCTEXT("WorldOriginXBlock", "World Origin Longitude (EPSG:4326)")).Font(FLandscapeCombinatorStyle::RegularFont()) ]
						+SHorizontalBox::Slot().FillWidth(0.33) [ WorldOriginXBlock.ToSharedRef() ]
					]
					+SVerticalBox::Slot().Padding(FMargin(0, 0, 0, 10)).AutoHeight()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot().FillWidth(0.66) [ SNew(STextBlock).Text(LOCTEXT("WorldOriginYBlock", "World Origin Lattitude (EPSG:4326)")).Font(FLandscapeCombinatorStyle::RegularFont()) ]
						+SHorizontalBox::Slot().FillWidth(0.33) [ WorldOriginYBlock.ToSharedRef() ]
					]
				]
				+SHorizontalBox::Slot().FillWidth(0.55)
		;
	}

	bool GetWorldParameters(WorldParametersV1& Params)
	{
		int WorldWidthKm = FCString::Atoi(*WorldWidthBlock->GetText().ToString());
		int WorldHeightKm = FCString::Atoi(*WorldHeightBlock->GetText().ToString());
		double ZScale = FCString::Atod(*ZScaleBlock->GetText().ToString().Replace(TEXT(" "), TEXT("")));

		if (WorldWidthKm <= 0) {
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("PositiveWidth", "World Width must be greater than 0 km"));
			return false;
		}

		if (WorldHeightKm <= 0) {
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("PositiveHeight", "World Height must be greater than 0 km"));
			return false;
		}

		double WorldOriginX = FCString::Atod(*WorldOriginXBlock->GetText().ToString().Replace(TEXT(" "), TEXT("")));
		double WorldOriginY = FCString::Atod(*WorldOriginYBlock->GetText().ToString().Replace(TEXT(" "), TEXT("")));
		
		if (WorldOriginX < -180 || WorldOriginX > 180) {
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("WorldOriginX", "World Origin X must be between -180 and 180 (EPSG 4326 coordinates)"));
			return false;
		}
		
		if (WorldOriginY < -90 || WorldOriginY > 90) {
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("WorldOriginY", "World Origin Y must be between -90 and 90 (EPSG 4326 coordinates)"));
			return false;
		}
		
		Params.WorldWidthKm = WorldWidthKm;
		Params.WorldHeightKm = WorldHeightKm;
		Params.ZScale = ZScale;
		Params.WorldOriginX = WorldOriginX;
		Params.WorldOriginY = WorldOriginY;
		return true;
	};

}


#undef LOCTEXT_NAMESPACE