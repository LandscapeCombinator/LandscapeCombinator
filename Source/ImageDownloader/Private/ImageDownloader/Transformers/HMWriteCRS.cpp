// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMWriteCRS.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"

#include "GDALInterface/GDALInterface.h"

#include "Misc/CString.h" 
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMWriteCRS::Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	if (InputFiles.Num() != 1)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("HMWriteCRS::Fetch", "Phase WriteCRS only works when there is a single input file. Found %d input files."),
				FText::AsNumber(InputFiles.Num())
			)
		);
		if (OnComplete) OnComplete(false);
		return;
	}

	OutputCRS = InputCRS;
	FString TempFolder = FPaths::Combine(Directories::ImageDownloaderDir(), Name + "-GeoTIFF_TEMP");
	FString WriteFolder = FPaths::Combine(Directories::ImageDownloaderDir(), Name + "-GeoTIFF");

	if (!IPlatformFile::GetPlatformPhysical().DeleteDirectoryRecursively(*WriteFolder) || !IPlatformFile::GetPlatformPhysical().CreateDirectory(*WriteFolder))
	{
		Directories::CouldNotInitializeDirectory(WriteFolder);
		if (OnComplete) OnComplete(false);
		return;
	}

	if (!IPlatformFile::GetPlatformPhysical().DeleteDirectoryRecursively(*TempFolder) || !IPlatformFile::GetPlatformPhysical().CreateDirectory(*TempFolder))
	{
		Directories::CouldNotInitializeDirectory(TempFolder);
		if (OnComplete) OnComplete(false);
		return;
	}
	
	FString InputFile = InputFiles[0];
	FString TempFile = FPaths::Combine(TempFolder, FPaths::GetBaseFilename(InputFile) + ".tif");
	FString OutputFile = FPaths::Combine(WriteFolder, FPaths::GetBaseFilename(InputFile) + ".tif");
	OutputFiles.Add(OutputFile);
	bool bSuccess =
		GDALInterface::Translate(InputFile, TempFile, {
			"-of",
			"GTiff",
			"-a_srs",
			InputCRS,
			"-gcp",
			"0", "0",
			FString::SanitizeFloat(MinLong), FString::SanitizeFloat(MaxLat),
			"-gcp",
			FString::FromInt(Width-1), "0",
			FString::SanitizeFloat(MaxLong), FString::SanitizeFloat(MaxLat),
			"-gcp",
			FString::FromInt(Width-1), FString::FromInt(Height-1),
			FString::SanitizeFloat(MaxLong), FString::SanitizeFloat(MinLat),
			"-gcp",
			"0", FString::FromInt(Height-1),
			FString::SanitizeFloat(MinLong), FString::SanitizeFloat(MinLat),
		}) &&
		GDALInterface::Warp(TempFile, OutputFile, "", InputCRS, false, 0);

	if (OnComplete) OnComplete(bSuccess);
	return;
}

#undef LOCTEXT_NAMESPACE