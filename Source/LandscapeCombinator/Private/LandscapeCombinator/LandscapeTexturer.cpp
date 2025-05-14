// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeTexturer.h"
#include "LandscapeCombinator/LandscapeController.h"
#include "ImageDownloader/HMDebugFetcher.h"
#include "ImageDownloader/Transformers/HMCrop.h"
#include "LandscapeUtils/LandscapeUtils.h"
#include "Coordinates/DecalCoordinates.h"
#include "LCCommon/LCBlueprintLibrary.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"
#include "Components/DecalComponent.h"


#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

ALandscapeTexturer::ALandscapeTexturer()
{
	PrimaryActorTick.bCanEverTick = false;

	ImageDownloader = CreateDefaultSubobject<UImageDownloader>(TEXT("TextureDownloader"));
	ImageDownloader->bRemap = false;
	PositionBasedGeneration = CreateDefaultSubobject<ULCPositionBasedGeneration >(TEXT("PositionBasedGeneration"));
}

bool ALandscapeTexturer::CreateDecals(TObjectPtr<UGlobalCoordinates> GlobalCoordinates, FName SpawnedActorsPathOverride, bool bIsUserInitiated)
{
	Modify();

	if (bDeleteOldDecalsWhenCreatingDecals)
	{
		if (!Concurrency::RunOnGameThreadAndWait([this, bIsUserInitiated]() { return Execute_Cleanup(this, !bIsUserInitiated); }))
		{
			return false;
		}
	}

	if (!IsValid(ImageDownloader))
	{
		if (bIsUserInitiated)
		{
			LCReporter::ShowError(
				LOCTEXT(
					"ALandscapeTexturer::CreateDecal::NoImageDownloader",
					"ImageDownloader is not set, you may want to create one, or create a new LandscapeTexturer"
				)
			);
		}

		return false;
	}

	TArray<FString> DownloadedImages;
	FString ImageCRS;
	if (!ImageDownloader->DownloadImages(bIsUserInitiated, false, GlobalCoordinates, DownloadedImages, ImageCRS)) return false;

	TArray<ADecalActor*> NewDecals = UDecalCoordinates::CreateDecals(this->GetWorld(), DownloadedImages);
	DecalActors.Append(NewDecals);

#if WITH_EDITOR
	Concurrency::RunOnGameThreadAndWait([this, &NewDecals, SpawnedActorsPathOverride](){
		for (auto &DecalActor : NewDecals)
		{
			ULCBlueprintLibrary::SetFolderPath2(DecalActor, SpawnedActorsPathOverride, SpawnedActorsPath);
			DecalActor->GetDecal()->SortOrder = DecalsSortOrder;
		}
		return true;
	});
#endif

	if (bIsUserInitiated)
	{
		Concurrency::RunOnGameThreadAndWait([this, &NewDecals](){
			if (NewDecals.Num() > 0)
			{
				LCReporter::ShowInfo(
					FText::Format(
						LOCTEXT("LandscapeCreated2", "Successfully created {0} Decals"),
						FText::AsNumber(NewDecals.Num())
					),
					"SuppressSpawnedDecalsInfo",
					LOCTEXT("SpawnedDecalsTitle", "Spawned Decals")
				);
			}
			else
			{
				LCReporter::ShowError(
					LOCTEXT("LandscapeCreatedError", "There was an error while creating decals")
				);
			}
			return true;
		});
	}

	return NewDecals.Num() > 0;
}


#undef LOCTEXT_NAMESPACE
