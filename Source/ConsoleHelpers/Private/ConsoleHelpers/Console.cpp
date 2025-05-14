// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ConsoleHelpers/Console.h"
#include "ConsoleHelpers/LogConsoleHelpers.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Misc/MessageDialog.h" 
#include "GenericPlatform/GenericPlatformProcess.h" 
#include "HAL/PlatformProcess.h"

#define LOCTEXT_NAMESPACE "FConsoleHelpersModule"

bool Console::ExecProcess(const TCHAR* URL, const TCHAR* Params, bool bDebug, bool bDialog)
{
	FString StdOut, StdErr;
	int32 ReturnCode;
	if (bDebug) UE_LOG(LogConsoleHelpers, Log, TEXT("Running %s with parameters %s"), URL, Params);

	if (!FPlatformProcess::ExecProcess(URL, Params, &ReturnCode, &StdOut, &StdErr) || ReturnCode != 0)
	{
		if (bDialog)
		{
			LCReporter::ShowError(
				FText::Format(
					LOCTEXT("ExecProcessError", "Error while running command '{0} {1}': (Error: {2})\nStdOut:\n{3}\nStdErr:\n{4}"),
					FText::FromString(URL),
					FText::FromString(Params),
					FText::AsNumber(ReturnCode, &FNumberFormattingOptions::DefaultNoGrouping()),
					FText::FromString(StdOut),
					FText::FromString(StdErr)
				)
			);
		}
		UE_LOG(LogConsoleHelpers, Error, TEXT("Command '%s %s' returned error code %d"), URL, Params, ReturnCode);
		UE_LOG(LogConsoleHelpers, Error, TEXT("StdOut:\n%s"), *StdOut);
		UE_LOG(LogConsoleHelpers, Error, TEXT("StdErr:\n%s"), *StdErr);
		return false;
	}
	else
	{
		if (bDebug) UE_LOG(LogConsoleHelpers, Log, TEXT("Command '%s %s' completed successfully"), URL, Params);
		return true;
	}
}

#undef LOCTEXT_NAMESPACE