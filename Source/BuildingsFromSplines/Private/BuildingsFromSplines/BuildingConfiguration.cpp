// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "BuildingsFromSplines/BuildingConfiguration.h"
#include "BuildingsFromSplines/BuildingsFromSplines.h"
#include "ConcurrencyHelpers/LCReporter.h"
#include "LCCommon/Expression.h"

#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

#include "Materials/MaterialInterface.h"
#include "OSMUserData/OSMUserData.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#endif

#define LOCTEXT_NAMESPACE "FBuildingsFromSplinesModule"

UBuildingConfiguration::UBuildingConfiguration()
{
	// Materials.Add(Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Materials/MI_Flat_White.MI_Flat_White'"))));
	// Materials.Add(Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Materials/MI_Flat_Wall.MI_Flat_Wall'"))));
	// Materials.Add(Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Materials/MI_Flat_Floor.MI_Flat_Floor'"))));

	// FLevelDescription LevelDescription;
	// LevelDescription.LevelHeight = 300.0;
	// LevelDescription.FloorThickness = 20.0;
	// LevelDescription.FloorMaterialIndex = 0;
	// LevelDescription.UnderFloorMaterialIndex = 0;
	// LevelDescription.bResetWallSegmentsOnCorners = true;

	// FWallSegment WallSegment;
	// WallSegment.WallSegmentKind = EWallSegmentKind::Wall;
	// WallSegment.bAutoExpand = true;
	// WallSegment.SegmentLength = 500.0;

	// FWallSegment HoleSegment;
	// HoleSegment.WallSegmentKind = EWallSegmentKind::Hole;
	// FAttachment Attachment;

	// FWeightedObject Window;
	// Window.Object = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *FString("/Script/Engine.StaticMesh'/LandscapeCombinator/Meshes/SM_Window.SM_Window'")));
	// Attachment.MeshSelection.Add(Window);
	// Attachment.Offset = FVector(0.0, 0.0, 100.0);
	// Attachment.OverrideWidth = 100.0;
	// Attachment.OverrideHeight = 100.0;
	// HoleSegment.Attachments.Add(Attachment);

	// LevelDescription.WallSegments.Add(WallSegment);
	// LevelDescription.WallSegments.Add(HoleSegment);
	// LevelDescription.WallSegments.Add(WallSegment);

	// FLoop SegmentsLoop;
	// SegmentsLoop.StartIndex = 0;
	// SegmentsLoop.EndIndex = 1;
	// LevelDescription.WallSegmentLoops.Add(SegmentsLoop);
	// Levels.Add(LevelDescription);

	// FLoop LevelLoop;
	// LevelLoop.StartIndex = 0;
	// LevelLoop.EndIndex = 0;
	// LevelLoops.Add(LevelLoop);
}

int UBuildingConfiguration::ResolveMaterial(FString ExprStr)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("ResolveMaterial");

	FExpression *Expr = FExpression::Parse(ExprStr);
	if (!Expr) return 0;

	if (Expr->ExprType != EExprType::Concat) return 0;
	
	Expr->MakeChoices();
	if (Expr->Children.IsEmpty()) return 0;

	int Index = MaterialNamesArray.IndexOfByKey(Expr->Children[0]->Symbol);
	if (Index >= 0) return Index;
	else return 0;
}

bool UBuildingConfiguration::AutoComputeNumFloors(UOSMUserData *BuildingOSMUserData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("AutoComputeNumFloors");

	if (!bAutoComputeNumFloors) return false;
	if (!IsValid(BuildingOSMUserData)) return false;
	
	if (BuildingOSMUserData->Fields.Contains("building_levels"))
	{
		FString LevelsString = BuildingOSMUserData->Fields["building_levels"];
		int NumLevels = FCString::Atoi(*LevelsString);
		if (NumLevels > 0)
		{
			bUseRandomNumFloors = false;
			NumFloors = NumLevels;
			return true;
		}
		// we don't return false to give a chance to the 'height' field below
		else if (!LevelsString.IsEmpty())
		{
			UE_LOG(LogBuildingsFromSplines, Warning, TEXT("Ignoring levels field: '%s'"), *LevelsString);
		}
	}

	if (BuildingOSMUserData->Fields.Contains("height"))
	{
		FString HeightString = BuildingOSMUserData->Fields["height"];
		double Height = FCString::Atod(*HeightString);
		if (Height > 0)
		{
			bUseRandomNumFloors = false;
			NumFloors = FMath::Max(1, Height * 100 / 300);
			return true;
		}
		else if (!HeightString.IsEmpty())
		{
			UE_LOG(LogBuildingsFromSplines, Warning, TEXT("Ignoring height field: '%s'"), *HeightString);
		}
	}

	return false;
}

bool UBuildingConfiguration::CheckValidKey(FString LevelDescriptionKey) const
{
	bool bIsValidKey = LevelsMap.Contains(LevelDescriptionKey) && IsValid(LevelsMap[LevelDescriptionKey]);
	if (!bIsValidKey)
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("Invalid Key", "Unknown Level: '{0}'. Please adjust your expression."),
			FText::FromString(LevelDescriptionKey)
		));
	}
	return bIsValidKey;
}

#if WITH_EDITOR

bool UBuildingConfiguration::LoadFromClass(TSubclassOf<UBuildingConfiguration> BuildingConfigurationClass)
{
	if (!IsValid(BuildingConfigurationClass)) return false;

	UBuildingConfiguration* CDO = Cast<UBuildingConfiguration>(BuildingConfigurationClass->GetDefaultObject());
	UEngine::CopyPropertiesForUnrelatedObjects(CDO, this);
	return true;
}

TSubclassOf<UBuildingConfiguration> UBuildingConfiguration::CreateClass(const FString &AssetPath, const FString& AssetName)
{
	FString PackageName = AssetPath / AssetName;
	UPackage* Package = CreatePackage(*PackageName);
	if (!IsValid(Package))
	{
		LCReporter::ShowError(
			LOCTEXT("FailedCreatePackage", "Failed to create package for new Building Configuration.")
		);
		return nullptr;
	}
	
	UBlueprint* BuildingConfigurationClassBP = FindObject<UBlueprint>(Package, *AssetName);
	if (IsValid(BuildingConfigurationClassBP))
	{
		if (!LCReporter::ShowMessage(
			FText::Format(
				LOCTEXT("BlueprintExists", "Building Configuration blueprint {0} already exists, overwrite it?"),
				FText::FromString(PackageName)
			),
			"SuppressOverrideBuildingConfigurationBlueprint"
		))
		{
			return nullptr;
		}
	}
	else
	{
		BuildingConfigurationClassBP = FKismetEditorUtilities::CreateBlueprint(
			GetClass(),
			Package,
			FName(*AssetName),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			FName(AssetName)
		);
	}

	if (!IsValid(BuildingConfigurationClassBP))
	{
		LCReporter::ShowError(
			LOCTEXT("FailedCreateBlueprint", "Failed to create new Building Configuration blueprint.")
		);
		return nullptr;
	}

	UClass* GeneratedClass = BuildingConfigurationClassBP->GeneratedClass;
	if (!IsValid(GeneratedClass))
	{
		LCReporter::ShowError(
			LOCTEXT("FailedGetGeneratedClass", "Failed to get generated class for new Building Configuration blueprint.")
		);
		return nullptr;
	}

	UBuildingConfiguration* CDO = Cast<UBuildingConfiguration>(BuildingConfigurationClassBP->GeneratedClass->GetDefaultObject());
	UEngine::CopyPropertiesForUnrelatedObjects(this, CDO);

	FAssetRegistryModule::AssetCreated(BuildingConfigurationClassBP);
	Package->MarkPackageDirty();
	FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SavePackageArgs;
	SavePackageArgs.TopLevelFlags = EObjectFlags::RF_Public | EObjectFlags::RF_Standalone;
	if (!UPackage::SavePackage(Package, BuildingConfigurationClassBP, *PackageFileName, SavePackageArgs))
	{
		LCReporter::ShowError(
			LOCTEXT("FailedSavePackage", "Failed to save package to disk.")
		);
		return nullptr;
	}

	LCReporter::ShowInfo(
		FText::Format(
			LOCTEXT("ConversionSuccessful", "Creation of Building Configuration Blueprint Class {0} was successful."),
			FText::FromString(PackageName)
		),
		"SuppressBuildingConfigurationConversionSuccessful"
	);
	TSubclassOf<UBuildingConfiguration> Result = GeneratedClass;

	return Result;
}

#endif

#undef LOCTEXT_NAMESPACE
