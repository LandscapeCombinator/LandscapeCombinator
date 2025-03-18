// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/BasicImageDownloader.h"
#include "LCReporter/LCReporter.h"

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
		ULCReporter::ShowError(
			LOCTEXT("ABasicImageDownloader::DownloadImagesForLandscape", "ImageDownloader is not set, you may want to create one, or spawn a new BasicImageDownloader")
		);
		return;
	}

	ImageDownloader->DownloadImages(false, ALevelCoordinates::GetGlobalCoordinates(this->GetWorld(), false), nullptr);
}


#undef LOCTEXT_NAMESPACE
