// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "GDALInterfaceModule.h"
#include "GDALInterface/LogGDALInterface.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FGDALInterfaceModule"

#pragma warning(disable: 4668)
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#pragma warning(default: 4668)

FString GetPluginBaseDir(FString PluginName)
{
    TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);

    if (Plugin.IsValid()) return Plugin->GetBaseDir();
	else return "";
}

void FGDALInterfaceModule::SetGDALPaths()
{
	FString PluginBaseDir = "";
	PluginBaseDir = GetPluginBaseDir("LandscapeCombinator");
	if (PluginBaseDir.IsEmpty()) PluginBaseDir = GetPluginBaseDir("ImageDownloader");
	if (PluginBaseDir.IsEmpty()) PluginBaseDir = GetPluginBaseDir("SplineImporter");
	if (PluginBaseDir.IsEmpty()) PluginBaseDir = GetPluginBaseDir("Coordinates");
	if (PluginBaseDir.IsEmpty()) PluginBaseDir = GetPluginBaseDir("HeightmapModifier");

	if (PluginBaseDir.IsEmpty())
	{
		UE_LOG(LogGDALInterface, Error, TEXT("Could not find plugin base directory."));
		return;
	}

	PluginBaseDir = FPaths::ConvertRelativePathToFull(PluginBaseDir);

	FString ShareFolder = "";

#if PLATFORM_WINDOWS
	ShareFolder = PluginBaseDir / "Source" / "ThirdParty" / "GDAL" / "Win64" / "share";
#elif PLATFORM_LINUX
	ShareFolder = PluginBaseDir / "Source" / "ThirdParty" / "GDAL" / "Linux" / "share";
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
