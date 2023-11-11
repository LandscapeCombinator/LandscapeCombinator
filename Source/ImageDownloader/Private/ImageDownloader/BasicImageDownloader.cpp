// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/BasicImageDownloader.h"

#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

ABasicImageDownloader::ABasicImageDownloader()
{
	PrimaryActorTick.bCanEverTick = false;

	ImageDownloader = CreateDefaultSubobject<UImageDownloader>(TEXT("TextureDownloader"));
}

void ABasicImageDownloader::DownloadImages()
{
	if (!ImageDownloader)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("ABasicImageDownloader::DownloadImagesForLandscape", "ImageDownloader is not set, you may want to create one, or spawn a new BasicImageDownloader")
		);
		return;
	}

	HMFetcher* Fetcher = ImageDownloader->CreateFetcher(Name, false, false, FVector4d::Zero(), { 0, 0 }, nullptr);

	if (!Fetcher)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("ABasicImageDownloader::DownloadImagesForLandscape::NoFetcher", "Could not make image fetcher.")
		);
		return;
	}

	Fetcher->Fetch("", {}, [Fetcher](bool bSuccess)
	{
		if (bSuccess)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("ABasicImageDownloader::DownloadImagesForLandscape::Success", "BasicImageDownloader successfully prepared the following file(s):\n{0}"),
				FText::FromString(FString::Join(Fetcher->OutputFiles, TEXT("\n")))
			));
			return;
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				LOCTEXT("ABasicImageDownloader::DownloadImagesForLandscape::Failure", "There was an error while downloading or preparing the files.")
			);
			return;
		}
	});
}


#undef LOCTEXT_NAMESPACE
