// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "GDALInterfaceModule.h"
#include "GDALInterface/LogGDALInterface.h"

#include "Interfaces/IPluginManager.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterShape.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterActor.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionLevelHelper.h"
#include "WorldPartition/WorldPartitionActorLoaderInterface.h"
#include "WorldPartition/WorldPartitionEditorLoaderAdapter.h"

#define LOCTEXT_NAMESPACE "FGDALInterfaceModule"

#pragma warning(disable: 4668)
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#pragma warning(default: 4668)

void FGDALInterfaceModule::StartupModule()
{
	// GDAL
	GDALAllRegister();
	OGRRegisterAll();
	FString ShareFolder = FPaths:: ConvertRelativePathToFull(IPluginManager::Get().FindPlugin("Coordinates")->GetBaseDir()) / "Source" / "ThirdParty" / "GDAL" / "share";
	FString GDALData = (ShareFolder / "gdal").Replace(TEXT("/"), TEXT("\\"));
	FString PROJData = (ShareFolder / "proj").Replace(TEXT("/"), TEXT("\\"));
	FString OSMConf = (ShareFolder / "gdal" / "osmconf.ini").Replace(TEXT("/"), TEXT("\\"));
	UE_LOG(LogGDALInterface, Log, TEXT("GDAL Version: %s"), *FString(GDALVersionInfo("RELEASE_NAME")));
	UE_LOG(LogGDALInterface, Log, TEXT("Setting GDAL_DATA to: %s"), *GDALData);
	UE_LOG(LogGDALInterface, Log, TEXT("Setting PROJ_DATA to: %s"), *PROJData);
	CPLSetConfigOption("GDAL_DATA", TCHAR_TO_UTF8(*GDALData));
	CPLSetConfigOption("PROJ_DATA", TCHAR_TO_UTF8(*PROJData));
	CPLSetConfigOption("GDAL_PAM_ENABLED", "NO");
	CPLSetConfigOption("OSM_CONFIG_FILE", TCHAR_TO_UTF8(*OSMConf));
	const char* const ProjPaths[] = { TCHAR_TO_UTF8(*PROJData), nullptr };
	OSRSetPROJSearchPaths(ProjPaths);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGDALInterfaceModule, GDALInterface)
