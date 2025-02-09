// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeTexturer.h"
#include "LandscapeCombinator/LandscapeController.h"
#include "ImageDownloader/HMDebugFetcher.h"
#include "ImageDownloader/Transformers/HMCrop.h"
#include "LandscapeUtils/LandscapeUtils.h"
#include "Coordinates/DecalCoordinates.h"
#include "LCCommon/LCReporter.h"
#include "LCCommon/LCBlueprintLibrary.h"
#include "Components/DecalComponent.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

ALandscapeTexturer::ALandscapeTexturer()
{
	PrimaryActorTick.bCanEverTick = false;

	ImageDownloader = CreateDefaultSubobject<UImageDownloader>(TEXT("TextureDownloader"));
	ImageDownloader->bRemap = false;
}

void ALandscapeTexturer::CreateDecal(TObjectPtr<UGlobalCoordinates> GlobalCoordinates, FName SpawnedActorsPathOverride, TFunction<void(bool)> OnComplete)
{
	Execute_Cleanup(this, false);

	if (!IsValid(ImageDownloader))
	{
		ULCReporter::ShowError(
			LOCTEXT(
				"ALandscapeTexturer::CreateDecal::NoImageDownloader",
				"ImageDownloader is not set, you may want to create one, or create a new LandscapeTexturer"
			)
		);
	}

	ImageDownloader->DownloadImages(false, GlobalCoordinates, [this, SpawnedActorsPathOverride, OnComplete](TArray<FString> DownloadedImages, FString ImageCRS)
	{
		DecalActors = UDecalCoordinates::CreateDecals(this->GetWorld(), DownloadedImages);

#if WITH_EDITOR
		for (auto &DecalActor : DecalActors)
		{
			ULCBlueprintLibrary::SetFolderPath2(DecalActor, SpawnedActorsPathOverride, SpawnedActorsPath);
			DecalActor->GetDecal()->SortOrder = DecalsSortOrder;
		}
#endif

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

		if (OnComplete) OnComplete(DecalActors.Num() > 0);
	});
}


#undef LOCTEXT_NAMESPACE
