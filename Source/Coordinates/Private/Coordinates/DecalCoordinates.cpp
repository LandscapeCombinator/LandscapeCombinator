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
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"

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
	
	OutCoordinates[0] = Left;
	OutCoordinates[1] = Right;
	OutCoordinates[2] = Bottom;
	OutCoordinates[3] = Top;

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
			LOCTEXT("UDecalCoordinates::PlaceDecal::M_GeoDecal", "Coordinates Internal Error: Could not find material M_GeoDecal.")
		);
		return;
	}

	FString MI_GeoDecal_Name;
	FString MI_GeoDecal_PackageName;

	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(FString("/Game/MI_GeoDecal_") + DecalActor->GetActorLabel(), "", MI_GeoDecal_PackageName, MI_GeoDecal_Name);

	UPackage* MI_GeoDecal_Package = CreatePackage(*MI_GeoDecal_PackageName);
	if (!MI_GeoDecal_Package)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("UDecalCoordinates::PlaceDecal::MI_GeoDecal_Package", "Coordinates Internal Error: Could not create MI_GeoDecal package.")
		);
		return;
	}

	UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
	if (!Factory)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("UDecalCoordinates::PlaceDecal::Factory", "Coordinates Internal Error: Could not create material instance factory.")
		);
		return;
	}
	UMaterialInstanceConstant* MI_GeoDecal =
		(UMaterialInstanceConstant*) Factory->FactoryCreateNew(
			UMaterialInstanceConstant::StaticClass(),
			MI_GeoDecal_Package,
			*MI_GeoDecal_Name,
			RF_Standalone | RF_Public | RF_Transactional,
			DecalActor,
			GWarn
		);
	if (!Factory)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("UDecalCoordinates::PlaceDecal::MI_GeoDecal", "Coordinates Internal Error: Could not create material instance MI_GeoDecal.")
		);
		return;
	}
	Factory->MarkAsGarbage();

	MI_GeoDecal->SetParentEditorOnly(M_GeoDecal);

	int Width, Height;
	TArray<FColor> Colors;

	if (!GDALInterface::ReadColorsFromFile(ReprojectedImage, Width, Height, Colors))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("UDecalCoordinates::PlaceDecal::ReadColorsFromFile", "Coordinates Internal Error: Could not read colors from file {0}."),
			FText::FromString(ReprojectedImage)
		));
		return;
	}
	
	FString TextureName;
	FString TexturePackageName;
	AssetToolsModule.Get().CreateUniqueAssetName(FString("/Game/T_GeoDecal_") + DecalActor->GetActorLabel(), "", TexturePackageName, TextureName);

	UPackage* TexturePackage = CreatePackage(*TexturePackageName);
	UTexture2D* Texture = FImageUtils::CreateTexture2D(
		Width, Height, Colors,
		TexturePackage, TextureName,
		RF_Standalone | RF_Public | RF_Transactional,
		FCreateTexture2DParameters()
	);
	FAssetRegistryModule::AssetCreated(Texture);
	TexturePackage->MarkPackageDirty();

	FMaterialParameterInfo TextureParam;
	TextureParam.Name = "Texture";
	TextureParam.Association = EMaterialParameterAssociation::GlobalParameter;
	TextureParam.Index = INDEX_NONE;
	MI_GeoDecal->SetTextureParameterValueEditorOnly(TextureParam, Texture);
	DecalActor->SetDecalMaterial(MI_GeoDecal);

	FAssetRegistryModule::AssetCreated(MI_GeoDecal);
	MI_GeoDecal_Package->MarkPackageDirty();
}

void UDecalCoordinates::CreateDecal(UWorld* World, FString Path)
{
	FVector4d UnusedCoordinates;
	CreateDecal(World, Path, UnusedCoordinates);
}

void UDecalCoordinates::CreateDecal(UWorld *World, FString Path, FVector4d &OutCoordinates)
{
	ADecalActor* DecalActor = World->SpawnActor<ADecalActor>();
	if (!DecalActor)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("UDecalCoordinates::CreateDecal", "Coordinates Internal Error: Could not spawn a Decal Actor.")
		);
		return;
	}
	FString BaseName = FPaths::GetBaseFilename(Path);
	DecalActor->SetActorLabel(FString("Decal_") + BaseName);

	UDecalCoordinates *DecalCoordinates = NewObject<UDecalCoordinates>(DecalActor->GetRootComponent());
	DecalCoordinates->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	DecalCoordinates->RegisterComponent();
	DecalActor->AddInstanceComponent(DecalCoordinates);

	DecalCoordinates->PathToGeoreferencedImage = Path;

	DecalCoordinates->PlaceDecal(OutCoordinates);
}

#undef LOCTEXT_NAMESPACE