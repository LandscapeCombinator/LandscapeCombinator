// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"

#include "ExternalTool.generated.h"

#define LOCTEXT_NAMESPACE "FConsoleHelpersModule"

UCLASS(BlueprintType)
class CONSOLEHELPERS_API UExternalTool : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ExternalTool",
		meta = (EditConditionHides, DisplayPriority = "11")
	)
	/* For Windows users, check this if you want to run the command using 'cmd.exe /c Command'.
	 * You must enable this option when the Command is a batch script. */
	bool bUseWindowsCmd = true;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ExternalTool",
		meta = (EditConditionHides, DisplayPriority = "12")
	)
	/* Enter the name of the binary, which should be in your PATH, and which will be used on your heightmap.
	 * Your processing command must take exactly two arguments: the input file and the output file. */
	FString Command;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ExternalTool",
		meta = (EditConditionHides, DisplayPriority = "13")
	)
	/* Check this if your preprocessing command produces output files whose extension is different than the input files' extension. */
	bool bChangeExtension;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ExternalTool",
		meta = (EditCondition = "bChangeExtension", EditConditionHides, DisplayPriority = "13")
	)
	/* The extension of the output files that your preprocessing script produces. */
	FString NewExtension;


	bool Run(FString InputFile, FString OutputFile);
};

#undef LOCTEXT_NAMESPACE
