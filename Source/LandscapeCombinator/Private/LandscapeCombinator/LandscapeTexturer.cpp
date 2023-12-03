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

void ALandscapeTexturer::DownloadImages()
{
	DownloadImages(nullptr);
}

void ALandscapeTexturer::DownloadImages(TFunction<void(bool)> OnComplete)
{
	if (!ImageDownloader)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("ALandscapeTexturer::DownloadImages", "ImageDownloader is not set, you may want to create one, or spawn a new LandscapeTexturer")
		);
		return;
	}
	
	Fetcher = ImageDownloader->CreateFetcher(GetActorLabel(), false, false, false, nullptr);

	if (!Fetcher)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("ALandscapeTexturer::DownloadImages::NoFetcher", "Could not make image fetcher.")
		);
		return;
	}

	Fetcher->Fetch("", {}, [this, OnComplete](bool bSuccess)
	{
		if (bSuccess)
		{
			if (OnComplete)
			{
				OnComplete(true);
				return;
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
					LOCTEXT("ALandscapeTexturer::DownloadImages::Success", "LandscapeTexturer successfully prepared the following file(s):\n{0}"),
					FText::FromString(FString::Join(Fetcher->OutputFiles, TEXT("\n")))
				));
				return;
			}
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				LOCTEXT("ALandscapeTexturer::DownloadImages::Failure", "There was an error while downloading or preparing the files.")
			);
			if (OnComplete) OnComplete(false);
			return;
		}
	});
}

void ALandscapeTexturer::CreateDecal()
{
	DownloadImages([this](bool bSuccess) {
		if (bSuccess)
		{
			for (int i = 0; i < Fetcher->OutputFiles.Num(); i++)
			{
				UDecalCoordinates::CreateDecal(this->GetWorld(), Fetcher->OutputFiles[i]);
			}
		}
	});
}


#undef LOCTEXT_NAMESPACE
