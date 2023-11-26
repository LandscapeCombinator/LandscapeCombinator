// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "Coordinates/DecalCoordinates.h"
#include "Coordinates/LevelCoordinates.h"
#include "Coordinates/LogCoordinates.h"
#include "GDALInterface/GDALInterface.h"


#include "Engine/DecalActor.h"
#include "Components/DecalComponent.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"

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
	
	if (!GDALInterface::Warp(PathToGeoreferencedImage, ReprojectedImage, "", GlobalCoordinates->CRS, 0)) return;

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

	double X = (Left + Right) / 2;
	double Y = (Top + Bottom) / 2;
	double Z = DecalActor->GetActorLocation().Z;

	DecalActor->GetDecal()->DecalSize = FVector(100000, (Bottom - Top) / 2, (Right - Left) / 2);
	DecalActor->SetActorRotation(FRotator(-90, 0, 0));
	DecalActor->SetActorLocation(FVector(X, Y, Z));


	UMaterial *M_GeoDecal = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), nullptr, *FString("/Script/Engine.Material'/LandscapeCombinator/Project/M_GeoDecal.M_GeoDecal'")));
	if (!M_GeoDecal)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("UDecalCoordinates::PlaceDecal::M_GeoDecal", "Could not find material M_GeoDecal.")
		);
		return;
	}

	FString MI_GeoDecal_Name = FString("MI_GeoDecal_") + DecalActor->GetActorLabel();
	UPackage* Package = CreatePackage(*(FString("/Game/") + MI_GeoDecal_Name));
	UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
	UMaterialInstanceConstant* MI_GeoDecal =
		(UMaterialInstanceConstant*) Factory->FactoryCreateNew(
			UMaterialInstanceConstant::StaticClass(),
			Package,
			*MI_GeoDecal_Name,
			RF_Standalone | RF_Public | RF_Transactional,
			DecalActor,
			GWarn
		);
	Factory->MarkAsGarbage();

	if (!MI_GeoDecal)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("UDecalCoordinates::PlaceDecal::MI_GeoDecal", "Could not create material instance for M_GeoDecal.")
		);
		return;
	}

	MI_GeoDecal->SetParentEditorOnly(M_GeoDecal);

	int Width, Height;
	TArray<FColor> Colors;

	if (!GDALInterface::ReadColorsFromFile(ReprojectedImage, Width, Height, Colors))
	{
		UE_LOG(LogCoordinates, Error, TEXT("Could not read colors from file %s"), *ReprojectedImage);
		return;
	}

	FCreateTexture2DParameters Parameters;
	UTexture2D* Texture = FImageUtils::CreateTexture2D(
		Width, Height, Colors,
		MI_GeoDecal, FString("T_") + DecalActor->GetActorLabel(),
		RF_Standalone | RF_Public | RF_Transactional,
		Parameters
	);


	FMaterialParameterInfo TextureParam;
	TextureParam.Name = "Texture";
	TextureParam.Association = EMaterialParameterAssociation::GlobalParameter;
	TextureParam.Index = INDEX_NONE;
	MI_GeoDecal->SetTextureParameterValueEditorOnly(TextureParam, Texture);
	DecalActor->SetDecalMaterial(MI_GeoDecal);

	FAssetRegistryModule::AssetCreated(MI_GeoDecal);
	Package->MarkPackageDirty();
}

#undef LOCTEXT_NAMESPACE