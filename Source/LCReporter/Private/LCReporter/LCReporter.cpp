#include "LCReporter/LCReporter.h"
#include "Misc/MessageDialog.h"

#if WITH_EDITOR
#include "Dialogs/Dialogs.h"
#endif

#define LOCTEXT_NAMESPACE "FLCReporterModule"

void ULCReporter::ShowError(const FText& Message, const FText& Title)
{
	FMessageDialog::Open(
		EAppMsgCategory::Error,
		EAppMsgType::Ok,
		Message,
		Title
	);
}

bool ULCReporter::ShowMessage(
	const FText& Message, const FString& InIniSettingName, const FText& Title,
	bool bCancellable, const FSlateBrush* Image
)
{
#if WITH_EDITOR
	FSuppressableWarningDialog::FSetupInfo Info(Message, Title, InIniSettingName);

	if (Image) Info.Image = (FSlateBrush*) Image;

	if (bCancellable)
	{
		Info.ConfirmText = LOCTEXT("ULCReporter::ShowMessage_Yes", "Yes");
		Info.CancelText = LOCTEXT("ULCReporter::ShowMessage_No", "No");
	}
	else
	{
		Info.ConfirmText = LOCTEXT("ULCReporter::ShowMessage_OK", "OK");
		Info.CancelText = FText();
	}

	const FSuppressableWarningDialog WarningDialog(Info);
	const FSuppressableWarningDialog::EResult Result = WarningDialog.ShowModal();
	
	if (Result == FSuppressableWarningDialog::Suppressed) return true;
	else return Result == FSuppressableWarningDialog::Confirm;
#else
	return true;
#endif
}

bool ULCReporter::ShowMapboxFreeTierWarning()
{
	return ULCReporter::ShowMessage(
		LOCTEXT(
			"UImageDownloader::OnImageSourceChanged::MapboxWarning",
			"Please check your Mapbox account to make sure you remain within the free tier.\nRequests can be expensive once you go beyond the free tier.\n"
			"The 2x option counts for more API requests than usual.\nContinue?"
		),
		"SuppressFreeTierWarning",
		LOCTEXT("MapboxWarningTitle", "Mapbox Free Tier Warning")
	);
}

bool ULCReporter::ShowMapTilerFreeTierWarning()
{
	return ULCReporter::ShowMessage(
		LOCTEXT(
			"UImageDownloader::OnImageSourceChanged::MapTilerWarning",
			"Please check your MapTiler account to make sure you remain within the free tier.\nRequests can be expensive once you go beyond the free tier.\nContinue?"
		),
		"SuppressFreeTierWarning",
		LOCTEXT("MapTilerWarningTitle", "MapTiler Free Tier Warning")
	);
}

void ULCReporter::ShowOneError(const FText& Message, bool* bShowedDialog)
{
	// if we have a null pointer, or a pointer pointing to false, we show the dialog
	if (!bShowedDialog || !*bShowedDialog)
	{
		// if the pointer is non-null, we set its value to true
		if (bShowedDialog) *bShowedDialog = true;

		ShowError(Message);
	}
}

#undef LOCTEXT_NAMESPACE
