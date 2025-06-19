// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

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
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		PublicDefinitions.Add("__STDC_VERSION__=0");

		// GDAL

		string Platform = Target.Platform == UnrealTargetPlatform.Win64 ? "Win64" : "Linux";
		string GDALDirectory = Path.Combine(PluginDirectory, "Source", "ThirdParty", "GDAL", Platform);
		PublicIncludePaths.Add(Path.Combine(GDALDirectory, "include"));

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(Path.Combine(GDALDirectory, "lib", "gdal.lib"));

			foreach (string DLLFile in Directory.GetFiles(Path.Combine(GDALDirectory, "bin")))
			{
				RuntimeDependencies.Add(Path.Combine("$(BinaryOutputDir)", Path.GetFileName(DLLFile)), DLLFile);
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			foreach (string SOFile in Directory.GetFiles(Path.Combine(GDALDirectory, "bin"), "*.so"))
			{
				if (SOFile.Contains("gdal") || SOFile.Contains("geos"))
				{
					PublicAdditionalLibraries.Add(SOFile);
				}
			}
			foreach (string SOFile in Directory.GetFiles(Path.Combine(GDALDirectory, "bin"), "*.so*"))
			{
				RuntimeDependencies.Add(Path.Combine("$(BinaryOutputDir)", Path.GetFileName(SOFile)), SOFile);
			}
		}

		// copy the share folders so that they can be used at runtime
		foreach (string File in Directory.GetFiles(Path.Combine(GDALDirectory, "share", "gdal")))
		{
			RuntimeDependencies.Add(Path.Combine("$(BinaryOutputDir)", "ThirdParty", "GDAL", "share", "gdal", Path.GetFileName(File)), File);
		}

		foreach (string File in Directory.GetFiles(Path.Combine(GDALDirectory, "share", "proj")))
		{
			RuntimeDependencies.Add(Path.Combine("$(BinaryOutputDir)", "ThirdParty", "GDAL", "share", "proj", Path.GetFileName(File)), File);
		}

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// Unreal Dependencies
				"InputCore",
				"CoreUObject",
				"Engine",
				"HTTP",
				"Projects",

				// Landscape Combinator Dependencies
				"FileDownloader",
				"ConcurrencyHelpers"
			}
		);
	}
}
