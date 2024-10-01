// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "Coordinates/DecalCoordinates.h"
#include "Coordinates/LevelCoordinates.h"
#include "Coordinates/LogCoordinates.h"
#include "GDALInterface/GDALInterface.h"


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

void UDecalCoordinates::PlaceDecal()
{
	FVector4d Unused;
	PlaceDecal(Unused);
}

void UDecalCoordinates::PlaceDecal(FVector4d &OutCoordinates)
{
	ADecalActor *DecalActor = Cast<ADecalActor>(GetOwner());
	if (!DecalActor)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("UDecalCoordinates::PlaceDecal", "UDecalCoordinates must be placed on a Decal Actor.")
		);
		return;
	}
	
	if (!FPaths::FileExists(PathToGeoreferencedImage))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("UDecalCoordinates::PlaceDecal::NoFile", "File {0} does not exist."),
			FText::FromString(PathToGeoreferencedImage)
		));
		return;
	}
	
	UWorld* World = this->GetWorld();

	UGlobalCoordinates* GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World, true);
	if (!GlobalCoordinates) return;

	FString ReprojectedImage = FPaths::Combine(FPaths::ProjectSavedDir(), "temp.tif");
	
	if (!GDALInterface::Warp(PathToGeoreferencedImage, ReprojectedImage, "", GlobalCoordinates->CRS, true, 0)) return;

	FVector4d Coordinates;
	if (!GDALInterface::GetCoordinates(Coordinates, { ReprojectedImage })) return;

	// In the image coordinates
	double West = Coordinates[0];
	double East = Coordinates[1];
	double South = Coordinates[2];
	double North = Coordinates[3];

	FVector2D TopLeft, BottomRight;
	if (!GlobalCoordinates->GetUnrealCoordinatesFromCRS(West, North, GlobalCoordinates->CRS, TopLeft)) return;
	if (!GlobalCoordinates->GetUnrealCoordinatesFromCRS(East, South, GlobalCoordinates->CRS, BottomRight)) return;

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

	DecalActor->GetDecal()->DecalSize = FVector(10000000, (Bottom - Top) / 2, (Right - Left) / 2);
	DecalActor->SetActorRotation(FRotator(-90, 0, 0));
	DecalActor->SetActorLocation(FVector(X, Y, Z));


	UMaterial *M_GeoDecal = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), nullptr, *FString("/Script/Engine.Material'/LandscapeCombinator/Materials/M_GeoDecal.M_GeoDecal'")));
	if (!M_GeoDecal)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("UDecalCoordinates::PlaceDecal::M_GeoDecal", "Coordinates Internal Error: Could not find material M_GeoDecal.")
		);
		return;
	}

	UMaterialInstanceDynamic *MI_GeoDecal = UMaterialInstanceDynamic::Create(M_GeoDecal, this);

	int Width, Height;
	TArray<FColor> Colors;

	UE_LOG(LogCoordinates, Log, TEXT("Creating decal from file: %s"), *ReprojectedImage);

	if (!GDALInterface::ReadColorsFromFile(ReprojectedImage, Width, Height, Colors))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("UDecalCoordinates::PlaceDecal::ReadColorsFromFile", "Coordinates Internal Error: Could not read colors from file {0}."),
			FText::FromString(ReprojectedImage)
		));
		return;
	}

	Texture = FImageUtils::CreateTexture2D(
		Width, Height, Colors,
		this, FString("T_GeoDecal_") + DecalActor->GetActorLabel(),
		RF_Public | RF_Transactional,
		FCreateTexture2DParameters()
	);

	MI_GeoDecal->SetTextureParameterValue(FName("Texture"), Texture);
	DecalActor->SetDecalMaterial(MI_GeoDecal);
}

ADecalActor* UDecalCoordinates::CreateDecal(UWorld* World, FString Path)
{
	FVector4d UnusedCoordinates;
	return CreateDecal(World, Path, UnusedCoordinates);
}

ADecalActor* UDecalCoordinates::CreateDecal(UWorld *World, FString Path, FVector4d &OutCoordinates)
{
	ADecalActor* DecalActor = World->SpawnActor<ADecalActor>();
	if (!DecalActor)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("UDecalCoordinates::CreateDecal", "Coordinates Internal Error: Could not spawn a Decal Actor.")
		);
		return nullptr;
	}
	FString BaseName = FPaths::GetBaseFilename(Path);

#if WITH_EDITOR
	DecalActor->SetActorLabel(FString("Decal_") + BaseName);
#endif

	UDecalCoordinates *DecalCoordinates = NewObject<UDecalCoordinates>(DecalActor->GetRootComponent());
	DecalCoordinates->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	DecalCoordinates->RegisterComponent();
	DecalActor->AddInstanceComponent(DecalCoordinates);

	DecalCoordinates->PathToGeoreferencedImage = Path;

	DecalCoordinates->PlaceDecal(OutCoordinates);

	return DecalActor;
}

#undef LOCTEXT_NAMESPACE