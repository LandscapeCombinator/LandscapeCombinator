// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeTexturer.h"
#include "LandscapeCombinator/LandscapeController.h"

#include "ImageDownloader/HMDebugFetcher.h"
#include "ImageDownloader/Transformers/HMCrop.h"

#include "LandscapeUtils/LandscapeUtils.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

ALandscapeTexturer::ALandscapeTexturer()
{
	PrimaryActorTick.bCanEverTick = false;

	ImageDownloader = CreateDefaultSubobject<UImageDownloader>(TEXT("TextureDownloader"));
}

void ALandscapeTexturer::SetCoordinatesFromLandscape()
{
	if (!TargetLandscape)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("ALandscapeTexturer::SetCoordinatesFromLandscape", "TargetLandscape is not set")
		);
		return;
	}
	
	if (!ImageDownloader)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("ALandscapeTexturer::SetCoordinatesFromLandscape", "ImageDownloader is not set, you may want to create one, or spawn a new LandscapeTexturer")
		);
		return;
	}

	TObjectPtr<ULandscapeController> LandscapeController = nullptr;

	for (auto& Component : TargetLandscape->GetComponents())
	{
		if (Component->IsA<ULandscapeController>())
		{
			LandscapeController = Cast<ULandscapeController>(Component);
			break;
		}
	}
	
	bool bFoundCoordinates = false;
	FVector4d ImageDownloaderCoordinates;
	if (LandscapeController)
	{
		bFoundCoordinates = GDALInterface::ConvertCoordinates(LandscapeController->Coordinates, false, ImageDownloaderCoordinates, LandscapeController->CRS, ImageDownloader->WMS_CRS);
	}

	if (!bFoundCoordinates)
	{
		bFoundCoordinates = ALevelCoordinates::GetLandscapeBounds(TargetLandscape->GetWorld(), TargetLandscape, ImageDownloader->WMS_CRS, ImageDownloaderCoordinates);
	}

	if (!bFoundCoordinates)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("ALandscapeTexturer::SetCoordinatesFromLandscape::NoController", "TargetLandscape {0} does not have a LandscapeController component"),
			FText::FromString(TargetLandscape->GetActorLabel())
		));
		return;
	}

	ImageDownloader->WMS_MinLong = ImageDownloaderCoordinates[0];
	ImageDownloader->WMS_MaxLong = ImageDownloaderCoordinates[1];
	ImageDownloader->WMS_MinLat = ImageDownloaderCoordinates[2];
	ImageDownloader->WMS_MaxLat = ImageDownloaderCoordinates[3];
}

void ALandscapeTexturer::PrepareImagesForLandscape()
{
	if (!ImageDownloader)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("ALandscapeTexturer::PrepareImagesForLandscape", "ImageDownloader is not set, you may want to create one, or spawn a new LandscapeTexturer")
		);
		return;
	}

	FString Name = TargetLandscape ? TargetLandscape->GetActorLabel() : "Unnamed";

	FVector4d Coordinates(0, 0, 0, 0);
	FIntPoint ImageSize(0, 0);

	if (TargetLandscape)
	{		
		FVector2D MinMaxX, MinMaxY, UnusedMinMaxZ;
		if (!LandscapeUtils::GetLandscapeBounds(TargetLandscape, MinMaxX, MinMaxY, UnusedMinMaxZ))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("ALandscapeTexturer::PrepareImagesForLandscape::NoBounds", "Could not compute bounds of Landscape {0}"),
				FText::FromString(TargetLandscape->GetActorLabel())
			));
			return;
		}

		FVector4d Locations;
		Locations[0] = MinMaxX[0];
		Locations[1] = MinMaxX[1];
		Locations[2] = MinMaxY[1];
		Locations[3] = MinMaxY[0];

		if (!ALevelCoordinates::GetCRSCoordinatesFromUnrealLocations(TargetLandscape->GetWorld(), Locations, Coordinates))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("ALandscapeTexturer::PrepareImagesForLandscape::NoCoordinates", "Could not compute coordinates of Landscape {0}"),
				FText::FromString(TargetLandscape->GetActorLabel())
			));
			return;
		}

		ImageSize.X = TargetLandscape->ComputeComponentCounts().X * TargetLandscape->ComponentSizeQuads + 1;
		ImageSize.Y = TargetLandscape->ComputeComponentCounts().Y * TargetLandscape->ComponentSizeQuads + 1;
	}
	
	HMFetcher* Fetcher = ImageDownloader->CreateFetcher(Name, false, false, Coordinates, ImageSize, nullptr);

	if (!Fetcher)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("ALandscapeTexturer::PrepareImagesForLandscape::NoFetcher", "Could not make image fetcher.")
		);
		return;
	}

	Fetcher->Fetch("", {}, [Fetcher](bool bSuccess)
	{
		if (bSuccess)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("ALandscapeTexturer::PrepareImagesForLandscape::Success", "LandscapeTexturer successfully prepared the following file(s):\n{0}"),
				FText::FromString(FString::Join(Fetcher->OutputFiles, TEXT("\n")))
			));
			return;
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				LOCTEXT("ALandscapeTexturer::PrepareImagesForLandscape::Failure", "There was an error while downloading or preparing the files.")
			);
			return;
		}
	});
}


#undef LOCTEXT_NAMESPACE
