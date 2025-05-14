// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ConcurrencyHelpers/LCReporter.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LogConcurrencyHelpers.h"

#include "Async/Async.h"
#include "Misc/MessageDialog.h"

#if WITH_EDITOR
#include "Dialogs/Dialogs.h"
#endif

#define LOCTEXT_NAMESPACE "FConcurrencyHelpersModule"

void LCReporter::ShowError(const FText& Message, const FText& Title)
{
	FMessageDialog::Open(
		EAppMsgCategory::Error,
		EAppMsgType::Ok,
		Message,
		Title
	);
}

bool LCReporter::ShowMessage(
	const FText& Message, const FString& InIniSettingName, const FText& Title,
	bool bCancellable, const FSlateBrush* Image
)
{
#if WITH_EDITOR

	return Concurrency::RunOnGameThreadAndWait([&]() {

		FSuppressableWarningDialog::FSetupInfo Info(Message, Title, InIniSettingName);

		if (Image) Info.Image = (FSlateBrush*) Image;

		if (bCancellable)
		{
			Info.ConfirmText = LOCTEXT("LCReporter::ShowMessage_Yes", "Yes");
			Info.CancelText = LOCTEXT("LCReporter::ShowMessage_No", "No");
		}
		else
		{
			Info.ConfirmText = LOCTEXT("LCReporter::ShowMessage_OK", "OK");
			Info.CancelText = FText();
		}

		const FSuppressableWarningDialog WarningDialog(Info);
		const FSuppressableWarningDialog::EResult Result = WarningDialog.ShowModal();
		
		if (Result == FSuppressableWarningDialog::Suppressed) return true;
		else return Result == FSuppressableWarningDialog::Confirm;
	});
#else
	return true;
#endif
}

void LCReporter::ShowOneError(const FText& Message, bool* bShowedDialog)
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
