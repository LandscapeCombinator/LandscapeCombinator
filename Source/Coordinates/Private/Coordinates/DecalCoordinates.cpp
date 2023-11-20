// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "Coordinates/DecalCoordinates.h"
#include "Coordinates/LevelCoordinates.h"
#include "Coordinates/LogCoordinates.h"
#include "GDALInterface/GDALInterface.h"

#include "Engine/DecalActor.h"
#include "Components/DecalComponent.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Materials/MaterialInstanceConstant.h"
#include "ImageUtils.h"

#define LOCTEXT_NAMESPACE "FCoordinatesModule"

void UDecalCoordinates::PlaceDecal()
{
	ADecalActor *DecalActor = Cast<ADecalActor>(GetOwner());
	if (!DecalActor)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("UDecalCoordinates::PlaceDecal", "UDecalCoordinates must be placed on a Decal Actor.")
		);
		return;
	}
	UWorld* World = this->GetWorld();

	UGlobalCoordinates* GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World, true);

	FString ReprojectedImage = FPaths::Combine(FPaths::ProjectSavedDir(), "temp.tif");
	
	if (!GDALInterface::Warp(PathToGeoreferencedImage, ReprojectedImage, "", GlobalCoordinates->CRS, 0))
	{
		return;
	}

	FVector4d Coordinates;
	if (!GDALInterface::GetCoordinates(Coordinates, { ReprojectedImage }))
	{
		return;
	}

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

	double X = (Left + Right) / 2;
	double Y = (Top + Bottom) / 2;
	double Z = DecalActor->GetActorLocation().Z;

	DecalActor->GetDecal()->DecalSize = FVector(100000, (Bottom - Top) / 2, (Right - Left) / 2);
	DecalActor->SetActorRotation(FRotator(-90, 0, 0));
	DecalActor->SetActorLocation(FVector(X, Y, Z));


	UMaterial *M_GeoDecal = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), nullptr, *FString("/Script/Engine.Material'/LandscapeCombinator/Project/M_GeoDecal.M_GeoDecal'")));
	if (!M_GeoDecal)
	{
		UE_LOG(LogCoordinates, Error, TEXT("Could not find M_GeoDecal"));
		return;
	}

	UMaterialInstanceDynamic* MDynGeoDecal = UMaterialInstanceDynamic::Create(M_GeoDecal, DecalActor);
	if (!MDynGeoDecal)
	{
		UE_LOG(LogCoordinates, Error, TEXT("Could not create dynamic material instance"));
		return;
	}

	int Width, Height;
	TArray<FColor> Colors;

	if (!GDALInterface::ReadColorsFromFile(ReprojectedImage, Width, Height, Colors))
	{
		UE_LOG(LogCoordinates, Error, TEXT("Could not read colors from file %s"), *ReprojectedImage);
		return;
	}

	FCreateTexture2DParameters Parameters;
	UTexture2D* Texture = FImageUtils::CreateTexture2D(Width, Height, Colors, DecalActor, DecalActor->GetActorLabel() + "Texture", EObjectFlags::RF_Dynamic, Parameters);

	MDynGeoDecal->SetTextureParameterValue("Texture", Texture);

	DecalActor->SetDecalMaterial(MDynGeoDecal);

}

#undef LOCTEXT_NAMESPACE