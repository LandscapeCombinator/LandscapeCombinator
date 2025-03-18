// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeTexturer.h"
#include "LandscapeCombinator/LandscapeController.h"
#include "ImageDownloader/HMDebugFetcher.h"
#include "ImageDownloader/Transformers/HMCrop.h"
#include "LandscapeUtils/LandscapeUtils.h"
#include "Coordinates/DecalCoordinates.h"
#include "LCReporter/LCReporter.h"
#include "LCCommon/LCBlueprintLibrary.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "Components/DecalComponent.h"


#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

ALandscapeTexturer::ALandscapeTexturer()
{
	PrimaryActorTick.bCanEverTick = false;

	ImageDownloader = CreateDefaultSubobject<UImageDownloader>(TEXT("TextureDownloader"));
	ImageDownloader->bRemap = false;
	PositionBasedGeneration = CreateDefaultSubobject<ULCPositionBasedGeneration >(TEXT("PositionBasedGeneration"));
}

void ALandscapeTexturer::CreateDecals(TObjectPtr<UGlobalCoordinates> GlobalCoordinates, FName SpawnedActorsPathOverride, bool bIsUserInitiated, TFunction<void(bool)> OnComplete)
{
	if (bDeleteOldDecalsWhenCreatingDecals)
	{
		if (!Execute_Cleanup(this, false))
		{
			if (OnComplete) OnComplete(false);
			return;
		}
	}

	if (!IsValid(ImageDownloader))
	{
		if (bIsUserInitiated)
		{
			Concurrency::RunOnGameThreadAndWait([](){
				ULCReporter::ShowError(
					LOCTEXT(
						"ALandscapeTexturer::CreateDecal::NoImageDownloader",
						"ImageDownloader is not set, you may want to create one, or create a new LandscapeTexturer"
					)
				);
			});
		}

		if (OnComplete) OnComplete(false);
		return;
	}
	ImageDownloader->DownloadImages(false, GlobalCoordinates, [this, bIsUserInitiated, SpawnedActorsPathOverride, OnComplete](TArray<FString> DownloadedImages, FString ImageCRS)
	{
		TArray<ADecalActor*> NewDecals = UDecalCoordinates::CreateDecals(this->GetWorld(), DownloadedImages);
		DecalActors.Append(NewDecals);

#if WITH_EDITOR
		Concurrency::RunOnGameThreadAndWait([this, &NewDecals, SpawnedActorsPathOverride](){
			for (auto &DecalActor : NewDecals)
			{
				ULCBlueprintLibrary::SetFolderPath2(DecalActor, SpawnedActorsPathOverride, SpawnedActorsPath);
				DecalActor->GetDecal()->SortOrder = DecalsSortOrder;
			}
		});
#endif

		if (bIsUserInitiated)
		{
			Concurrency::RunOnGameThreadAndWait([this, &NewDecals](){
				if (DecalActors.Num() > 0)
				{
					ULCReporter::ShowInfo(
						FText::Format(
							LOCTEXT("LandscapeCreated2", "Successfully created {0} Decals"),
							FText::AsNumber(DecalActors.Num())
						),
						"SuppressSpawnedDecalsInfo",
						LOCTEXT("SpawnedDecalsTitle", "Spawned Decals")
					);
				}
				else
				{
					ULCReporter::ShowError(
						LOCTEXT("LandscapeCreatedError", "There was an error while creating decals")
					);
				}
			});
		}

		if (OnComplete) OnComplete(NewDecals.Num() > 0);
	});
}


#undef LOCTEXT_NAMESPACE
