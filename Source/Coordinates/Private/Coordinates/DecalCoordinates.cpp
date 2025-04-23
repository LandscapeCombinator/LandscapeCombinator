// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "Coordinates/DecalCoordinates.h"
#include "Coordinates/LevelCoordinates.h"
#include "Coordinates/LogCoordinates.h"
#include "GDALInterface/GDALInterface.h"
#include "LCReporter/LCReporter.h"
#include "ConcurrencyHelpers//Concurrency.h"

#include "Components/DecalComponent.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "TextureResource.h"

#define LOCTEXT_NAMESPACE "FCoordinatesModule"

bool UDecalCoordinates::PlaceDecal()
{
	FVector4d Unused;
	return PlaceDecal(Unused);
}

bool UDecalCoordinates::PlaceDecal(FVector4d &OutCoordinates)
{
	ADecalActor *DecalActor = Cast<ADecalActor>(GetOwner());
	if (!DecalActor)
	{
		ULCReporter::ShowError(
			LOCTEXT("UDecalCoordinates::PlaceDecal", "UDecalCoordinates must be placed on a Decal Actor.")
		);
		return false;
	}
	
	if (!FPaths::FileExists(PathToGeoreferencedImage))
	{
		ULCReporter::ShowError(FText::Format(
			LOCTEXT("UDecalCoordinates::PlaceDecal::NoFile", "File {0} does not exist."),
			FText::FromString(PathToGeoreferencedImage)
		));
		return false;
	}
	
	UWorld* World = this->GetWorld();

	UGlobalCoordinates* GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World, true);
	if (!GlobalCoordinates) return false;

	FString ReprojectedImage = FPaths::GetPath(PathToGeoreferencedImage) / (FPaths::GetBaseFilename(PathToGeoreferencedImage) + "-reprojected.tif");
	
	if (!GDALInterface::Warp(PathToGeoreferencedImage, ReprojectedImage, "", GlobalCoordinates->CRS, true, 0)) 
		return false;

	FVector4d Coordinates;
	if (!GDALInterface::GetCoordinates(Coordinates, { ReprojectedImage }))
		return false;

	// In the image coordinates
	double West = Coordinates[0];
	double East = Coordinates[1];
	double South = Coordinates[2];
	double North = Coordinates[3];

	FVector2D TopLeft, BottomRight;
	if (!GlobalCoordinates->GetUnrealCoordinatesFromCRS(West, North, GlobalCoordinates->CRS, TopLeft)) return false;
	if (!GlobalCoordinates->GetUnrealCoordinatesFromCRS(East, South, GlobalCoordinates->CRS, BottomRight)) return false;

	// Unreal Coordinates
	double Left = TopLeft[0];
	double Top = TopLeft[1];
	double Right = BottomRight[0];
	double Bottom = BottomRight[1];
	
	OutCoordinates[0] = Left;
	OutCoordinates[1] = Right;
	OutCoordinates[2] = Bottom;
	OutCoordinates[3] = Top;

	double X = (Left + Right) / 2;
	double Y = (Top + Bottom) / 2;
	double Z = DecalActor->GetActorLocation().Z;

	UMaterial *M_GeoDecal = nullptr;
	UMaterialInstanceDynamic *MI_GeoDecal = nullptr;
	
	Concurrency::RunOnGameThreadAndWait([this, &M_GeoDecal, &MI_GeoDecal, DecalActor, Bottom, Top, Right, Left, X, Y, Z]()
	{
		DecalActor->GetDecal()->DecalSize = FVector(10000000, (Bottom - Top) / 2, (Right - Left) / 2);
		DecalActor->SetActorRotation(FRotator(-90, 0, 0));
		DecalActor->SetActorLocation(FVector(X, Y, Z));
		M_GeoDecal = 
			Cast<UMaterial>(
				StaticLoadObject(
					UMaterial::StaticClass(),
					nullptr,
					*FString("/Script/Engine.Material'/LandscapeCombinator/Materials/M_GeoDecal.M_GeoDecal'")
				)
			);

		if (!IsValid(M_GeoDecal)) return;
		
		MI_GeoDecal = UMaterialInstanceDynamic::Create(M_GeoDecal, this);
	});

	if (!IsValid(M_GeoDecal) || !IsValid(MI_GeoDecal))
	{
		ULCReporter::ShowError(
			LOCTEXT("UDecalCoordinates::PlaceDecal::M_GeoDecal", "Coordinates Internal Error: Could not find material M_GeoDecal.")
		);
		return false;
	}

	int Width, Height;
	TArray<FColor> Colors;

	UE_LOG(LogCoordinates, Log, TEXT("Creating decal from file: %s"), *ReprojectedImage);

	if (!GDALInterface::ReadColorsFromFile(ReprojectedImage, Width, Height, Colors))
	{
		ULCReporter::ShowError(FText::Format(
			LOCTEXT("UDecalCoordinates::PlaceDecal::ReadColorsFromFile", "Coordinates Internal Error: Could not read colors from file {0}."),
			FText::FromString(ReprojectedImage)
		));
		return false;
	}

	Concurrency::RunOnGameThreadAndWait([&Width, &Height, &Colors, this, MI_GeoDecal, DecalActor]() {
		Texture = FImageUtils::CreateTexture2D(
			Width, Height, Colors,
			this, FString("T_GeoDecal_") + DecalActor->GetActorNameOrLabel(),
			RF_Public | RF_Transactional,
			FCreateTexture2DParameters()
		);

		MI_GeoDecal->SetTextureParameterValue(FName("Texture"), Texture);
		DecalActor->SetDecalMaterial(MI_GeoDecal);
	});

	return true;
}

TArray<ADecalActor*> UDecalCoordinates::CreateDecals(UWorld *World, TArray<FString> Paths)
{
	TArray<ADecalActor*> DecalActors;
	bool bFailed = false;
	for (auto &Path : Paths)
	{
		TObjectPtr<ADecalActor> DecalActor = UDecalCoordinates::CreateDecal(World, Path);
		if (IsValid(DecalActor))
		{
			DecalActors.Add(DecalActor);
		}
		else
		{
			ULCReporter::ShowError(
				LOCTEXT("UDecalCoordinates::CreateDecals", "There was an error while creating a decal actor")
			);
			return {};
		}
	}

	return DecalActors;
}

ADecalActor* UDecalCoordinates::CreateDecal(UWorld* World, FString Path)
{
	FVector4d UnusedCoordinates;
	return CreateDecal(World, Path, UnusedCoordinates);
}

ADecalActor* UDecalCoordinates::CreateDecal(UWorld *World, FString Path, FVector4d &OutCoordinates)
{
	if (!IsValid(World))
	{
		ULCReporter::ShowError(
			LOCTEXT("UDecalCoordinates::CreateDecal::InvalidWorld", "Invalid World.")
		);
		return nullptr;
	}

	ADecalActor* DecalActor = nullptr;
	UDecalCoordinates *DecalCoordinates = nullptr;
	
	Concurrency::RunOnGameThreadAndWait([&DecalActor, &DecalCoordinates, World, Path]{
		DecalActor = World->SpawnActor<ADecalActor>();
		if (!IsValid(DecalActor)) return;
		FString BaseName = FPaths::GetBaseFilename(Path);

#if WITH_EDITOR
		DecalActor->SetActorLabel(FString("Decal_") + BaseName);
#endif

		DecalCoordinates = NewObject<UDecalCoordinates>(DecalActor->GetRootComponent());
		if (!IsValid(DecalCoordinates)) return;
		DecalCoordinates->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		DecalCoordinates->RegisterComponent();
		DecalCoordinates->PathToGeoreferencedImage = Path;
		DecalActor->AddInstanceComponent(DecalCoordinates);
	});

	if (!IsValid(DecalActor) || !IsValid(DecalCoordinates))
	{
		ULCReporter::ShowError(
			LOCTEXT("UDecalCoordinates::CreateDecal", "Coordinates Internal Error: Could not spawn a Decal Actor.")
		);
		return nullptr;
	}

	if (DecalCoordinates->PlaceDecal(OutCoordinates))
	{
		return DecalActor;
	}
	else
	{
		Concurrency::RunOnGameThreadAndWait([&DecalActor]{
			DecalActor->Destroy();
		});
		return nullptr;
	}
}

#undef LOCTEXT_NAMESPACE