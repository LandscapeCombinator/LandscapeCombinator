// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using System;

using Microsoft.Extensions.Logging;
using UnrealBuildTool.Rules;

public class GDALInterface : ModuleRules
{

	public GDALInterface(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
#if UE_5_8_OR_LATER
		CppCompileWarningSettings.UnreachableCodeWarningLevel = WarningLevel.Error;
#endif
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		if (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion == 6)
		{
			PublicDefinitions.Add("UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_7=1"); // work-around for engine warnings in 5.6
		}

		// GDAL

		string Platform = Target.Platform == UnrealTargetPlatform.Win64 ? "Win64" : Target.Platform == UnrealTargetPlatform.Mac ? "Mac" : "Linux";
		string GDALDirectory = Path.Combine(PluginDirectory, "Source", "ThirdParty", "GDAL", Platform);
		PublicIncludePaths.Add(Path.Combine(GDALDirectory, "include"));

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(Path.Combine(GDALDirectory, "lib", "gdal.lib"));
			PublicDelayLoadDLLs.Add("gdal.dll");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(Path.Combine(GDALDirectory, "bin", "libgdal.dylib"));
		}
		else
		{
			string SOFile = Path.Combine(GDALDirectory, "bin", "libgdal.so");
			PublicAdditionalLibraries.Add(SOFile);
			PublicRuntimeLibraryPaths.Add(Path.Combine(GDALDirectory, "bin"));
		}

		RuntimeDependencies.Add("$(PluginDir)/Source/ThirdParty/GDAL/" + Platform + "/bin/*");
		// copy the share folders so that they can be used at runtime
		RuntimeDependencies.Add("$(PluginDir)/Source/ThirdParty/GDAL/" + Platform + "/share/...");

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GeometryCore"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// Unreal Dependencies
				"InputCore",
				"HTTP",
				"Projects",

				// Landscape Combinator Dependencies
				"FileDownloader",
				"ConcurrencyHelpers"
			}
		);
	}
}
