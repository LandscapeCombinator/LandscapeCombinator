// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "BuildingFromSpline.h"

#define LOCTEXT_NAMESPACE "FBuildingFromSplineModule"

const FName FBuildingFromSplineModule::BuildingFromSplineTabName("BuildingFromSpline");

void FBuildingFromSplineModule::StartupModule()
{
}

void FBuildingFromSplineModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBuildingFromSplineModule, BuildingFromSpline)
