// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/BasicImageDownloader.h"

#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

ABasicImageDownloader::ABasicImageDownloader()
{
	PrimaryActorTick.bCanEverTick = false;

	ImageDownloader = CreateDefaultSubobject<UImageDownloader>(TEXT("ImageDownloader"));
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

	ImageDownloader->DownloadImages(ALevelCoordinates::GetGlobalCoordinates(this->GetWorld(), false), nullptr);
}


#undef LOCTEXT_NAMESPACE
