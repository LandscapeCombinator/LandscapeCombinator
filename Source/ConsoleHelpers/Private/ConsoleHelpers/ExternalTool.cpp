// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ConsoleHelpers/ExternalTool.h"
#include "ConsoleHelpers/Console.h"

bool UExternalTool::Run(FString InputFile, FString OutputFile)
{	
	FString NewCommand = bUseWindowsCmd ? "cmd" : Command;
	FString Params;
	if (bUseWindowsCmd)
	{
		Params = FString::Format(TEXT("/c \"{0} \"{1}\" \"{2}\"\""), { Command, InputFile, OutputFile});
	}
	else
	{
		Params = FString::Format(TEXT("\"{0}\" \"{1}\""), { InputFile, OutputFile});
	}

	return Console::ExecProcess(*NewCommand, *Params);
}
