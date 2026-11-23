// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "Coordinates/DecalCoordinates.h"
#include "Coordinates/LevelCoordinates.h"
#include "Coordinates/LogCoordinates.h"
#include "GDALInterface/GDALInterface.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"

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
#include "Engine/Texture2D.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FCoordinatesModule"

UTexture2D* UDecalCoordinates::CreateDecalTexture(int Width, int Height, TArray<FColor>& Colors, UObject* Outer, FString TextureName, bool bSaveWithOuter)
{
	UTexture2D* Result = nullptr;
	TWeakObjectPtr<UObject> WeakOuter(Outer);
	Concurrency::RunOnGameThreadThrottledAndWait([&]() {
		if (!WeakOuter.IsValid()) return false;
#if WITH_EDITOR
		if (bSaveWithOuter)
		{
			Result = NewObject<UTexture2D>(WeakOuter.Get(), MakeUniqueObjectName(WeakOuter.Get(), UTexture2D::StaticClass(), FName(*TextureName)), RF_Transactional);
			Result->Source.Init(Width, Height, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Colors.GetData()));
			Result->SRGB = true;
			Result->MipGenSettings = TMGS_NoMipmaps;
			Result->NeverStream = true;
			Result->PostEditChange();
			return true;
		}
#endif
		Result = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8, MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), FName(*TextureName)));
		FTexture2DMipMap& Mip = Result->GetPlatformData()->Mips[0];
		void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Data, Colors.GetData(), (SIZE_T) Width * Height * sizeof(FColor));
		Mip.BulkData.Unlock();
		Result->UpdateResource();
		return true;
	});
	return Result;
}

UMaterialInstanceDynamic* UDecalCoordinates::SetDecalTexture(UDecalComponent* Decal, UMaterialInterface* Material, UTexture2D* DecalTexture, UObject* Outer)
{
	UMaterialInstanceDynamic* MID = nullptr;
	TWeakObjectPtr<UDecalComponent> WeakDecal(Decal);
	TWeakObjectPtr<UMaterialInterface> WeakMaterial(Material);
	TWeakObjectPtr<UTexture2D> WeakTexture(DecalTexture);
	TWeakObjectPtr<UObject> WeakOuter(Outer);
	Concurrency::RunOnGameThreadThrottledAndWait([&]() {
		if (!WeakDecal.IsValid() || !WeakMaterial.IsValid() || !WeakTexture.IsValid() || !WeakOuter.IsValid()) return false;
		MID = UMaterialInstanceDynamic::Create(WeakMaterial.Get(), WeakOuter.Get());
		MID->SetTextureParameterValue(FName("Texture"), WeakTexture.Get());
		WeakDecal->SetDecalMaterial(MID);
		return true;
	});
	return MID;
}

UMaterialInstanceDynamic* UDecalCoordinates::SetDecalTextureFromColors(
	UDecalComponent* Decal, UMaterialInterface* Material,
	int Width, int Height, TArray<FColor>& Colors, UObject* Outer, FString TextureName)
{
	UTexture2D* NewTexture = CreateDecalTexture(Width, Height, Colors, Outer, TextureName);
	if (!IsValid(NewTexture)) return nullptr;
	return SetDecalTexture(Decal, Material, NewTexture, Outer);
}

bool UDecalCoordinates::PlaceDecal(UMaterialInterface *Material)
{
	FVector4d Unused;
	return PlaceDecal(Material, Unused);
}

bool UDecalCoordinates::PlaceDecal(UMaterialInterface *Material, FVector4d &OutCoordinates)
{
	if (!IsValid(Material))
	{
		LCReporter::ShowError(LOCTEXT("UDecalCoordinates::PlaceDecal", "Invalid Material."));
		return false;
	}

	ADecalActor *DecalActor = Cast<ADecalActor>(GetOwner());
	if (!IsValid(DecalActor))
	{
		LCReporter::ShowError(
			LOCTEXT("UDecalCoordinates::PlaceDecal", "UDecalCoordinates must be placed on a Decal Actor.")
		);
		return false;
	}
	
	if (!FPaths::FileExists(PathToGeoreferencedImage))
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("UDecalCoordinates::PlaceDecal::NoFile", "File {0} does not exist."),
			FText::FromString(PathToGeoreferencedImage)
		));
		return false;
	}
	
	UWorld* World = GetWorld();

	UGlobalCoordinates* GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinatesFor(GetOwner(), true);
	if (!IsValid(GlobalCoordinates)) return false;

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

	const bool bSphere = GlobalCoordinates->IsSphere();
	FVector SphereCenter = FVector::ZeroVector;
	FQuat SphereRotation = FQuat::Identity;
	FVector SphereSize = FVector::ZeroVector;

	FVector2D TopLeft, BottomRight;
	if (bSphere)
	{
		UGlobalCoordinates::ComputeSphereDecalPlacement(GlobalCoordinates, FVector2D(West, South), FVector2D(East, North), SphereCenter, SphereRotation, SphereSize);
		const FVector TL = GlobalCoordinates->ToWorld(West, North);
		const FVector BR = GlobalCoordinates->ToWorld(East, South);
		TopLeft = FVector2D(TL.X, TL.Y);
		BottomRight = FVector2D(BR.X, BR.Y);
	}
	else
	{
		if (!GlobalCoordinates->GetUnrealCoordinatesFromCRS(West, North, GlobalCoordinates->CRS, TopLeft)) return false;
		if (!GlobalCoordinates->GetUnrealCoordinatesFromCRS(East, South, GlobalCoordinates->CRS, BottomRight)) return false;
	}

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
	
	Concurrency::RunOnGameThreadThrottledAndWait([this, Material, DecalActor, Bottom, Top, Right, Left, X, Y, Z, bSphere, SphereCenter, SphereRotation, SphereSize]()
	{
		if (!IsValid(DecalActor)) return false;

		if (bSphere)
		{
			DecalActor->GetDecal()->DecalSize = SphereSize;
			DecalActor->SetActorRotation(SphereRotation);
			DecalActor->SetActorLocation(SphereCenter);
		}
		else
		{
			DecalActor->GetDecal()->DecalSize = FVector(10000000, (Bottom - Top) / 2, (Right - Left) / 2);
			DecalActor->SetActorRotation(FRotator(-90, 0, 0));
			DecalActor->SetActorLocation(FVector(X, Y, Z));
		}
		
		return true;
	});

	Concurrency::RunOnGameThreadAndWait([&]()
	{
		Modify();
		Texture = nullptr;
		bTextureApplied = false;
		DecalMaterial = Material;
		bSphereDecal = bSphere;
		DecalBoundsMin = FVector2D(West, South);
		DecalBoundsMax = FVector2D(East, North);
		return true;
	});

	return ApplyDecalTexture();
}

FString UDecalCoordinates::ReprojectedImagePath() const
{
	return FPaths::GetPath(PathToGeoreferencedImage) / (FPaths::GetBaseFilename(PathToGeoreferencedImage) + "-reprojected.tif");
}

bool UDecalCoordinates::ApplyDecalTexture()
{
	ADecalActor* DecalActor = Cast<ADecalActor>(GetOwner());
	if (!IsValid(DecalActor) || !IsValid(DecalMaterial)) return false;

	if (!IsValid(Texture))
	{
		const FString ReprojectedImage = ReprojectedImagePath();
		int Width, Height;
		TArray<FColor> Colors;

		UE_LOG(LogCoordinates, Log, TEXT("Creating decal from file: %s"), *ReprojectedImage);

		if (!GDALInterface::ReadColorsFromFile(ReprojectedImage, Width, Height, Colors))
		{
			LCReporter::ShowError(FText::Format(
				LOCTEXT("UDecalCoordinates::PlaceDecal::ReadColorsFromFile", "Coordinates Internal Error: Could not read colors from file {0}."),
				FText::FromString(ReprojectedImage)
			));
			return false;
		}

		UTexture2D* NewTexture = CreateDecalTexture(Width, Height, Colors, this, FString("T_GeoDecal_") + DecalActor->GetActorNameOrLabel(), true);
		if (!IsValid(NewTexture)) return false;
		Concurrency::RunOnGameThreadAndWait([&]()
		{
			Modify();
			Texture = NewTexture;
			return true;
		});
	}

	UMaterialInstanceDynamic* MID = SetDecalTexture(DecalActor->GetDecal(), DecalMaterial, Texture, this);
	if (bSphereDecal && MID)
	{
		const UGlobalCoordinates* G = ALevelCoordinates::GetGlobalCoordinatesFor(GetOwner(), false);
		if (!G) UE_LOG(LogCoordinates, Warning, TEXT("ApplyDecalTexture: no LevelCoordinates found, the sphere mapping parameters were not set"));
		Concurrency::RunOnGameThreadAndWait([&]()
		{
			if (G) G->SetSphereMappingParameters(MID, DecalBoundsMin, DecalBoundsMax);
			return true;
		});
	}
	bTextureApplied = MID != nullptr;
	return bTextureApplied;
}

void UDecalCoordinates::RefreshSphereMapping(const UGlobalCoordinates* GlobalCoordinates)
{
	if (!bSphereDecal || !IsValid(GlobalCoordinates)) return;
	ADecalActor* DecalActor = Cast<ADecalActor>(GetOwner());
	if (!IsValid(DecalActor) || !IsValid(DecalActor->GetDecal())) return;
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(DecalActor->GetDecal()->GetDecalMaterial());
	if (IsValid(MID)) GlobalCoordinates->SetSphereMappingParameters(MID, DecalBoundsMin, DecalBoundsMax);
}

void UDecalCoordinates::OnRegister()
{
	Super::OnRegister();
	ADecalActor* DecalActor = Cast<ADecalActor>(GetOwner());
	if (!IsValid(DecalActor) || !IsValid(DecalMaterial) || !IsValid(Texture) || bTextureApplied) return;
	ApplyDecalTexture();
}

TArray<ADecalActor*> UDecalCoordinates::CreateDecals(UWorld *World, UMaterialInterface *Material, TArray<FString> Paths, ALevelCoordinates* Body)
{
	TArray<ADecalActor*> DecalActors;
	for (auto &Path : Paths)
	{
		FVector4d UnusedCoordinates;
		TObjectPtr<ADecalActor> DecalActor = UDecalCoordinates::CreateDecal(World, Material, Path, UnusedCoordinates, Body);
		if (IsValid(DecalActor))
		{
			DecalActors.Add(DecalActor);
		}
		else
		{
			LCReporter::ShowError(
				LOCTEXT("UDecalCoordinates::CreateDecals", "There was an error while creating a decal actor")
			);
			Concurrency::RunOnGameThread([DecalActors]{
				for (ADecalActor* Created : DecalActors) if (IsValid(Created)) Created->Destroy();
			});
			return {};
		}
	}

	return DecalActors;
}

ADecalActor* UDecalCoordinates::CreateDecal(UWorld* World, UMaterialInterface *Material, FString Path)
{
	FVector4d UnusedCoordinates;
	return CreateDecal(World, Material, Path, UnusedCoordinates);
}

ADecalActor* UDecalCoordinates::CreateDecal(UWorld *World, UMaterialInterface *Material, FString Path, FVector4d &OutCoordinates, ALevelCoordinates* Body)
{
	if (!IsValid(World)) return nullptr;

	ADecalActor* DecalActor = nullptr;
	UDecalCoordinates *DecalCoordinates = nullptr;

	Concurrency::RunOnGameThreadThrottledAndWait([&DecalActor, &DecalCoordinates, World, Path, Body]{
		DecalActor = World->SpawnActor<ADecalActor>();
		if (!IsValid(DecalActor)) return false;
		if (IsValid(Body)) ALevelCoordinates::BindBody(DecalActor, Body);
		FString BaseName = FPaths::GetBaseFilename(Path);

#if WITH_EDITOR
		DecalActor->SetActorLabel(FString("Decal_") + BaseName);
#endif

		DecalCoordinates = NewObject<UDecalCoordinates>(DecalActor->GetRootComponent());
		if (!IsValid(DecalCoordinates)) return false;
		DecalCoordinates->CreationMethod = EComponentCreationMethod::Instance;
		DecalCoordinates->RegisterComponent();
		DecalCoordinates->PathToGeoreferencedImage = Path;
		DecalActor->AddInstanceComponent(DecalCoordinates);
		return true;
	});

	if (!IsValid(DecalActor) || !IsValid(DecalCoordinates))
	{
		LCReporter::ShowError(
			LOCTEXT("UDecalCoordinates::CreateDecal", "Coordinates Internal Error: Could not spawn a Decal Actor.")
		);
		return nullptr;
	}

	if (DecalCoordinates->PlaceDecal(Material, OutCoordinates))
	{
		return DecalActor;
	}
	else
	{
		Concurrency::RunOnGameThread([DecalActor]{
			if (IsValid(DecalActor)) DecalActor->Destroy();
		});
		return nullptr;
	}
}

#undef LOCTEXT_NAMESPACE
