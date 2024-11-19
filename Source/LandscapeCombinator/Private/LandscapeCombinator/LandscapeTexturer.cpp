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
	ImageDownloader->bRemap = false;
}

void ALandscapeTexturer::CreateDecal(TObjectPtr<UGlobalCoordinates> GlobalCoordinates)
{
	if (ImageDownloader)
	{
		ImageDownloader->DownloadMergedImage(false, GlobalCoordinates, [this](FString DownloadedImage, FString ImageCRS)
		{
			UDecalCoordinates::CreateDecal(this->GetWorld(), DownloadedImage);
		});
	}
}


#undef LOCTEXT_NAMESPACE
