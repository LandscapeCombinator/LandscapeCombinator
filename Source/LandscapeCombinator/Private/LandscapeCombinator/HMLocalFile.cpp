// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/HMLocalFile.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"


#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void HMLocalFile::Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	if (FPaths::FileExists(File))
	{
		OutputFiles.Add(File);
		if (OnComplete) OnComplete(true);
		return;
	}
	else
	{
		if (OnComplete) OnComplete(false);
		return;
	}
}

#undef LOCTEXT_NAMESPACE