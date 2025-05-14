// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/BasicImageDownloader.h"
#include "ConcurrencyHelpers/LCReporter.h"

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
		LCReporter::ShowError(
			LOCTEXT("ABasicImageDownloader::DownloadImagesForLandscape", "ImageDownloader is not set, you may want to create one, or spawn a new BasicImageDownloader")
		);
		return;
	}

	TArray<FString> UnusedImages;
	FString UnusedCRS;
	ImageDownloader->DownloadImages(true, false, ALevelCoordinates::GetGlobalCoordinates(this->GetWorld(), false), UnusedImages, UnusedCRS);

}


#undef LOCTEXT_NAMESPACE
