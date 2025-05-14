// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Downloaders/HMLitto3DGuadeloupe.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"

#include "ConsoleHelpers/Console.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "HAL/FileManagerGeneric.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

bool HMLitto3DGuadeloupe::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	if (!Console::ExecProcess(TEXT("7z"), TEXT(""), false))
	{
		LCReporter::ShowError(
			LOCTEXT(
				"MissingRequirement",
				"Please make sure 7z is installed on your computer and available in your PATH if you want to use Litto 3D Guadeloupe heightmaps."
			)
		);
		return false;
	}

	FString AllArchives = FPaths::Combine(Folder, FString("*.7z"));
	TArray<FString> FileNames;
	FFileManagerGeneric::Get().FindFiles(FileNames, *AllArchives, true, false);

	int FileNamesNum = FileNames.Num();

	if (FileNamesNum == 0)
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("NoArchives", "Folder '{0}' does not contain any 7z file."),
			FText::FromString(Folder)
		));
		return false;
	}

	for (auto& FileName : FileNames)
	{
		FString ArchiveFile = FPaths::Combine(Folder, FileName);
		FString ExtractionDir = FPaths::Combine(ImageDownloaderDir, FPaths::GetBaseFilename(FileName));
		FString ExtractParams = FString::Format(TEXT("x -aos \"{0}\" -o\"{1}\""), { ArchiveFile, ExtractionDir });
		
		if (!bSkipExtraction && !Console::ExecProcess(TEXT("7z"), *ExtractParams))
		{
			UE_LOG(LogImageDownloader, Error, TEXT("Could not extract %s."), *ArchiveFile);
			return false;
		}

		TArray<FString> AscFiles;

		if (bUse5mData)
		{
			FFileManagerGeneric::Get().FindFilesRecursive(AscFiles, *ExtractionDir, TEXT("*_MNT5_*.asc"), true, false);
		}
		else
		{
			FFileManagerGeneric::Get().FindFilesRecursive(AscFiles, *ExtractionDir, TEXT("*_MNT_*.asc"), true, false);
		}

		OutputFiles.Append(AscFiles);
		
		if (AscFiles.Num() == 0)
		{
			LCReporter::ShowError(FText::Format(
				LOCTEXT("NoHeightMaps", "After extraction, folder '{0}' did not contain any heightmap files."),
				FText::FromString(ExtractionDir)
			));
			return false;
		}
	}
		
	if (OutputFiles.Num() == 0)
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("NoHeightMaps2", "After extraction, folder '{0}' did not contain any heightmap files."),
			FText::FromString(Folder)
		));
		return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
