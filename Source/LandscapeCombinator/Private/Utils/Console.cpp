#include "Utils/Console.h"
#include "Utils/Logging.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

bool Console::ExecProcess(const TCHAR* URL, const TCHAR* Params, FString *StdOut, bool Debug) {
	FString StdErr;
	int32 ReturnCode;
	if (Debug) UE_LOG(LogLandscapeCombinator, Log, TEXT("Running: %s %s"), URL, Params);
	bool result = FPlatformProcess::ExecProcess(URL, Params, &ReturnCode, StdOut, &StdErr);
	if (!result || ReturnCode != 0) {
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("ExecProcessError", "Error while running command '{0} {1}'."),
				FText::FromString(URL),
				FText::FromString(Params)
			)
		);
		UE_LOG(LogLandscapeCombinator, Error, TEXT("Command '%s %s' returned an error:\n%s"), URL, Params, *StdErr);
	} else {
		if (Debug) UE_LOG(LogLandscapeCombinator, Log, TEXT("Command '%s %s' completed successfully"), URL, Params);
	}
	return result;
}

bool Console::Has7Z() {
	return ExecProcess(TEXT("7z"), TEXT(""), nullptr, false);
}

#undef LOCTEXT_NAMESPACE
