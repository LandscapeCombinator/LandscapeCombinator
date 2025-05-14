// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImageDownloader/Directories.h"

// Often used in the subclasses
#include "HAL/PlatformFile.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class IMAGEDOWNLOADER_API HMFetcher
{
public:
	HMFetcher() {};
	virtual ~HMFetcher() {};

	TArray<FString> OutputFiles;
	FString OutputCRS = "";
	bool bIsUserInitiated = false;

	virtual void SetIsUserInitiated(bool bIsUserInitiatedIn) { bIsUserInitiated = bIsUserInitiatedIn; }

	HMFetcher* AndThen(HMFetcher* OtherFetcher);
	HMFetcher* AndRun(TFunction<bool(HMFetcher*)> Lambda);

	bool Fetch(FString InputCRS, TArray<FString> InputFiles)
	{
		return SetDirectories() && OnFetch(InputCRS, InputFiles);
	}

protected:
	FString ImageDownloaderDir = "";
	FString DownloadDir = "";
	FString OutputDir = "";

	virtual FString GetOutputDir() { return ""; }

	bool SetDirectories()
	{
		ImageDownloaderDir = Directories::ImageDownloaderDir();
		if (ImageDownloaderDir.IsEmpty()) return false;

		DownloadDir = Directories::DownloadDir();
		if (DownloadDir.IsEmpty()) return false;

		OutputDir = GetOutputDir();
		if (OutputDir.IsEmpty()) return true; // This means this fetcher doesn't use an output directory

		// in case an output dir is set, we attempt clear it (if it exists) or create it
		return
			IPlatformFile::GetPlatformPhysical().DeleteDirectoryRecursively(*OutputDir) &&
			IPlatformFile::GetPlatformPhysical().CreateDirectory(*OutputDir);
	}

	virtual bool OnFetch(FString InputCRS, TArray<FString> InputFiles) = 0;
};

class IMAGEDOWNLOADER_API HMAndThenFetcher : public HMFetcher
{
public:
	HMAndThenFetcher(HMFetcher* F1, HMFetcher* F2)
	{
		Fetcher1 = F1;
		Fetcher2 = F2;
	}
	~HMAndThenFetcher() {
		delete Fetcher1;
		delete Fetcher2;
	}
	
	HMFetcher* Fetcher1;
	HMFetcher* Fetcher2;

	void SetIsUserInitiated(bool bIsUserInitiatedIn) override {
		HMFetcher::SetIsUserInitiated(bIsUserInitiatedIn);
		Fetcher1->SetIsUserInitiated(bIsUserInitiatedIn);
		Fetcher2->SetIsUserInitiated(bIsUserInitiatedIn);
	}
	
	bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;
};

class IMAGEDOWNLOADER_API HMAndRunFetcher : public HMFetcher
{
public:
	HMAndRunFetcher(HMFetcher* Fetcher0, TFunction<bool(HMFetcher*)> Lambda0)
	{
		Fetcher = Fetcher0;
		Lambda = Lambda0;
	}
	~HMAndRunFetcher() {
		delete Fetcher;
	}
	
	HMFetcher* Fetcher;
	TFunction<bool(HMFetcher*)> Lambda;

	void SetIsUserInitiated(bool bIsUserInitiatedIn) override {
		HMFetcher::SetIsUserInitiated(bIsUserInitiatedIn);
		Fetcher->SetIsUserInitiated(bIsUserInitiatedIn);
	}
	
	bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;
};

#undef LOCTEXT_NAMESPACE
