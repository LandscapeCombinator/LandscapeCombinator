// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeTexturer.h"
#include "LandscapeCombinator/LandscapeController.h"
#include "ImageDownloader/HMDebugFetcher.h"
#include "ImageDownloader/Transformers/HMCrop.h"
#include "LandscapeUtils/LandscapeUtils.h"
#include "Coordinates/DecalCoordinates.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

ALandscapeTexturer::ALandscapeTexturer()
{
	PrimaryActorTick.bCanEverTick = false;

	ImageDownloader = CreateDefaultSubobject<UImageDownloader>(TEXT("TextureDownloader"));
}

void ALandscapeTexturer::CreateDecal()
{
	if (ImageDownloader)
	{
		ImageDownloader->DownloadMergedImage([this](FString DownloadedImage)
		{
			UDecalCoordinates::CreateDecal(this->GetWorld(), DownloadedImage);
		});
	}
}


#undef LOCTEXT_NAMESPACE
