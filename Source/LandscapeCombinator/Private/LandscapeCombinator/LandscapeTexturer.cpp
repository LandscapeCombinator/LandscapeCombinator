// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeTexturer.h"
#include "Coordinates/LCMaterialBinding.h"
#include "LandscapeCombinator/LandscapeController.h"
#include "ImageDownloader/HMDebugFetcher.h"
#include "ImageDownloader/Transformers/HMCrop.h"
#include "LandscapeUtils/LandscapeUtils.h"
#include "Coordinates/DecalCoordinates.h"
#include "LCCommon/LCBlueprintLibrary.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"
#include "GDALInterface/GDALInterface.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"


#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

ALandscapeTexturer::ALandscapeTexturer()
{
	PrimaryActorTick.bCanEverTick = false;

	ImageDownloader = CreateDefaultSubobject<UImageDownloader>(TEXT("TextureDownloader"));
	ImageDownloader->bRemap = false;
	PositionBasedGeneration = CreateDefaultSubobject<ULCPositionBasedGeneration >(TEXT("PositionBasedGeneration"));
}

void ALandscapeTexturer::RemoveMaterialBindings()
{
	Concurrency::RunOnGameThreadAndWait([this]()
	{
		const FString KeyPrefix = GetActorInstanceGuid().ToString() + TEXT("_");
		for (AActor* Target : MaterialTargetSelection.GetAllActors(GetWorld(), false))
		{
			ULCMaterialBinding::RemoveSourcesWithPrefix(Target, KeyPrefix);
		}
		return true;
	});
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

	if (bApplyToBodySphere && UGlobalCoordinates::IsSphere(GlobalCoordinates))
	{
		if (!SatelliteLayerName.IsNone() && bIsUserInitiated)
		{
			Concurrency::RunOnGameThreadAndWait([]() {
				LCReporter::ShowError(LOCTEXT(
					"ALandscapeTexturer::CreateDecals::SphereLayerIgnored",
					"Satellite Layer Name is ignored when Apply To Body Sphere is enabled: the images are applied to the body sphere only."
				));
				return true;
			});
		}
		return ApplyToBodySphere(GlobalCoordinates, DownloadedImages, bIsUserInitiated);
	}

	ALevelCoordinates* Body = nullptr;
	Concurrency::RunOnGameThreadAndWait([this, &Body]() { Body = ALevelCoordinates::GetBoundBody(this); return true; });
	TArray<ADecalActor*> NewDecals = UDecalCoordinates::CreateDecals(GetWorld(), DecalMaterial, DownloadedImages, Body);
	DecalActors.Append(NewDecals);

	Concurrency::RunOnGameThreadAndWait([this, &NewDecals, SpawnedActorsPathOverride](){
		for (auto &DecalActor : NewDecals)
		{
			ALevelCoordinates::InheritBody(DecalActor, this);
#if WITH_EDITOR
			ULCBlueprintLibrary::SetFolderPath2(DecalActor, SpawnedActorsPathOverride, SpawnedActorsPath);
#endif
			DecalActor->GetDecal()->SetSortOrder(DecalsSortOrder);
		}
		return true;
	});

	if (!SatelliteLayerName.IsNone())
	{
		TArray<AActor*> Targets;
		Concurrency::RunOnGameThreadAndWait([this, &Targets, bIsUserInitiated](){
			Targets = MaterialTargetSelection.GetAllActors(GetWorld(), bIsUserInitiated);
			return true;
		});

		const FString KeyBase = GetActorInstanceGuid().ToString();

		for (AActor* Target : Targets) ULCMaterialBinding::RemoveSourcesWithPrefix(Target, KeyBase + TEXT("_"));

		for (int32 i = 0; i < NewDecals.Num(); i++)
		{
			ADecalActor* DecalActor = NewDecals[i];
			if (!IsValid(DecalActor)) continue;
			UDecalCoordinates* DecalCoordinates = DecalActor->FindComponentByClass<UDecalCoordinates>();
			if (!IsValid(DecalCoordinates) || !IsValid(DecalCoordinates->Texture)) continue;

			const FName LayerName = i == 0 ? SatelliteLayerName : FName(*FString::Printf(TEXT("%s_%d"), *SatelliteLayerName.ToString(), i));
			const bool bSphere = DecalCoordinates->bSphereDecal;
			const FVector2D BoundsMin = bSphere ? DecalCoordinates->DecalBoundsMin : DecalCoordinates->DecalWorldBoundsMin;
			const FVector2D BoundsMax = bSphere ? DecalCoordinates->DecalBoundsMax : DecalCoordinates->DecalWorldBoundsMax;

			for (AActor* Target : Targets)
			{
				ULCMaterialBinding::BindTexture(Target, FString::Printf(TEXT("%s_%d"), *KeyBase, i), LayerName, DecalCoordinates->Texture, BoundsMin, BoundsMax);
			}
		}
	}

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

bool ALandscapeTexturer::ApplyToBodySphere(TObjectPtr<UGlobalCoordinates> GlobalCoordinates, const TArray<FString>& DownloadedImages, bool bIsUserInitiated)
{
	if (DownloadedImages.Num() != 1)
	{
		if (bIsUserInitiated)
		{
			Concurrency::RunOnGameThreadAndWait([&]() {
				LCReporter::ShowError(FText::Format(
					LOCTEXT("ALandscapeTexturer::ApplyToBodySphere::NotOneImage", "Apply To Body Sphere needs exactly one downloaded image, got {0}. Use a single WMS tile."),
					FText::AsNumber(DownloadedImages.Num())
				));
				return true;
			});
		}
		return false;
	}

	const FString& Image = DownloadedImages[0];
	const FString Reprojected = FPaths::GetPath(Image) / (FPaths::GetBaseFilename(Image) + "-sphere.tif");
	if (!GDALInterface::Warp(Image, Reprojected, "", GlobalCoordinates->CRS, true, 0)) return false;

	FVector4d Coordinates;
	if (!GDALInterface::GetCoordinates(Coordinates, Reprojected)) return false;

	int Width, Height;
	TArray<FColor> Colors;
	if (!GDALInterface::ReadColorsFromFile(Reprojected, Width, Height, Colors))
	{
		if (bIsUserInitiated)
		{
			Concurrency::RunOnGameThreadAndWait([&]() {
				LCReporter::ShowError(FText::Format(
					LOCTEXT("ALandscapeTexturer::ApplyToBodySphere::ReadColors", "Could not read colors from file {0}."),
					FText::FromString(Reprojected)
				));
				return true;
			});
		}
		return false;
	}

	return Concurrency::RunOnGameThreadAndWait([&]() {
		static const FName SphereTag("LCBodySphere");
		ALevelCoordinates* Body = ALevelCoordinates::GetBoundBody(this);

		AStaticMeshActor* Sphere = nullptr;
		for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
		{
			if (It->ActorHasTag(SphereTag) && (!Body || ALevelCoordinates::GetBoundBody(*It) == Body))
			{
				Sphere = *It;
				break;
			}
		}

		if (!IsValid(Sphere) || !IsValid(BodySphereMaterial))
		{
			if (bIsUserInitiated)
			{
				LCReporter::ShowError(LOCTEXT("ALandscapeTexturer::ApplyToBodySphere::NoSphere", "Could not find the body sphere of this texturer's body. Click Create World Map on its Level Coordinates first."));
			}
			return false;
		}

		UStaticMeshComponent* Component = Sphere->GetStaticMeshComponent();
		UTexture2D* Texture = UDecalCoordinates::CreateDecalTexture(Width, Height, Colors, Sphere, FString("T_BodySphere_") + GetActorNameOrLabel(), true);
		if (!IsValid(Texture)) return false;

		Sphere->Modify();
		Component->Modify();
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BodySphereMaterial, Component);
		MID->SetTextureParameterValue(FName("Texture"), Texture);
		GlobalCoordinates->SetSphereMappingParameters(MID, FVector2D(Coordinates[0], Coordinates[2]), FVector2D(Coordinates[1], Coordinates[3]));
		Component->SetMaterial(0, MID);

		if (bIsUserInitiated)
		{
			LCReporter::ShowInfo(
				FText::Format(
					LOCTEXT("ALandscapeTexturer::ApplyToBodySphere::Done", "Applied a {0}x{1} texture to {2}"),
					FText::AsNumber(Width), FText::AsNumber(Height), FText::FromString(Sphere->GetActorNameOrLabel())
				),
				"SuppressSpawnedDecalsInfo",
				LOCTEXT("SpawnedDecalsTitle", "Spawned Decals")
			);
		}
		return true;
	});
}


#undef LOCTEXT_NAMESPACE
