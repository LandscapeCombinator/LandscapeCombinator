// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "GDALInterfaceModule.h"
#include "GDALInterface/LogGDALInterface.h"

#include "WorldPartition/LoaderAdapter/LoaderAdapterShape.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterActor.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionLevelHelper.h"
#include "WorldPartition/WorldPartitionActorLoaderInterface.h"
#include "WorldPartition/WorldPartitionEditorLoaderAdapter.h"

#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FGDALInterfaceModule"

#pragma warning(disable: 4668)
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#pragma warning(default: 4668)

void FGDALInterfaceModule::SetGDALPaths()
{
	FString ProjectDir = FPaths:: ConvertRelativePathToFull(FPaths::ProjectDir());
	FString ShareFolder = "";

#if PLATFORM_WINDOWS
  ShareFolder = ProjectDir / "Binaries" / "Win64" / "Source" / "ThirdParty" / "GDAL" / "share";
#elif PLATFORM_LINUX
  ShareFolder = ProjectDir / "Binaries" / "Linux" / "Source" / "ThirdParty" / "GDAL" / "share";
#endif

	FString GDALData = (ShareFolder / "gdal");
	FString PROJData = (ShareFolder / "proj");
	FString OSMConf = (ShareFolder / "gdal" / "osmconf.ini");

#if PLATFORM_WINDOWS
	GDALData = GDALData.Replace(TEXT("/"), TEXT("\\"));
	PROJData = PROJData.Replace(TEXT("/"), TEXT("\\"));
	OSMConf = OSMConf.Replace(TEXT("/"), TEXT("\\"));
#endif
	
	if (!FPaths::FileExists(OSMConf))
	{
		UE_LOG(LogGDALInterface, Error, TEXT("Could not find OSM configuration file: %s"), *OSMConf);
		UE_LOG(LogGDALInterface, Error, TEXT("Failed to load GDALInterface module"));
		return;
	}

	CPLSetConfigOption("GDAL_DATA", TCHAR_TO_ANSI(*GDALData));
	CPLSetConfigOption("PROJ_DATA", TCHAR_TO_ANSI(*PROJData));
	CPLSetConfigOption("PROJ_LIB", TCHAR_TO_ANSI(*PROJData));
	CPLSetConfigOption("GDAL_PAM_ENABLED", "NO");
	CPLSetConfigOption("CPL_DEBUG", "YES");
	CPLSetConfigOption("OSM_CONFIG_FILE", TCHAR_TO_ANSI(*OSMConf));
	auto PROJDataConv = StringCast<ANSICHAR>(*PROJData);
	const char* PROJDataChar = PROJDataConv.Get();
	const char* const ProjPaths[] = { PROJDataChar, nullptr };
	UE_LOG(LogGDALInterface, Log, TEXT("GDAL Version: %s"), *FString(GDALVersionInfo("RELEASE_NAME")));
	UE_LOG(LogGDALInterface, Log, TEXT("Set GDAL_DATA to: %s"), *GDALData);
	UE_LOG(LogGDALInterface, Log, TEXT("Set PROJ_LIB to: %s"), *PROJData);
	UE_LOG(LogGDALInterface, Log, TEXT("Set PROJ_DATA to: %s"), *PROJData);
	UE_LOG(LogGDALInterface, Log, TEXT("Set OSRSetPROJSearchPaths to: %s"), *FString(PROJDataChar));
	UE_LOG(LogGDALInterface, Log, TEXT("Set OSM_CONFIG_FILE to: %s"), *OSMConf);
	OSRSetPROJSearchPaths(ProjPaths);
	GDALAllRegister();
	OGRRegisterAll();
}

void FGDALInterfaceModule::StartupModule()
{
	SetGDALPaths();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGDALInterfaceModule, GDALInterface)
