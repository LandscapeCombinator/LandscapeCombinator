// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/Directories.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"


void Directories::CouldNotInitializeDirectory(FString Dir)
{
	FMessageDialog::Open(EAppMsgType::Ok,
		FText::Format(
			LOCTEXT("DirectoryError", "Could not create or clear directory '{0}'."),
			FText::FromString(Dir)
		)
	);
}

FString Directories::LandscapeCombinatorDir()
{
	FString Intermediate = FPaths::ConvertRelativePathToFull(FPaths::EngineIntermediateDir());
	FString LandscapeCombinatorDir = FPaths::Combine(Intermediate, "LandscapeCombinator");
	if (!IPlatformFile::GetPlatformPhysical().CreateDirectory(*LandscapeCombinatorDir))
	{
		CouldNotInitializeDirectory(LandscapeCombinatorDir);
		return "";
	}

	return LandscapeCombinatorDir;
}

FString Directories::DownloadDir()
{
	FString LandscapeCombinatorDir = Directories::LandscapeCombinatorDir();
	if (LandscapeCombinatorDir.IsEmpty())
	{
		return "";
	}
	else
	{
		FString DownloadDir = FPaths::Combine(LandscapeCombinatorDir, "Download");
		if (!IPlatformFile::GetPlatformPhysical().CreateDirectory(*DownloadDir))
		{
			CouldNotInitializeDirectory(DownloadDir);
			return "";
		}
		else
		{
			return DownloadDir;
		}
	}
}

#undef LOCTEXT_NAMESPACE
