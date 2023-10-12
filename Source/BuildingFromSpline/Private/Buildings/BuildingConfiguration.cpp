// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "Buildings/BuildingConfiguration.h"

UBuildingConfiguration::UBuildingConfiguration()
{
	InteriorMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Project/M_White.M_White'")));
	ExteriorMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Project/MI_ExternalWalls.MI_ExternalWalls'")));
	FloorMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Project/MI_Floor.MI_Floor'")));
	CeilingMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Project/M_White.M_White'")));
	RoofMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Project/MI_Roof.MI_Roof'")));
}
