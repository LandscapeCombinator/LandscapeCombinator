// Copyright 2023 LandscapeCombinator. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class GDALInterface : ModuleRules
{
	public GDALInterface(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_2;

		// GDAL
		PublicIncludePaths.Add(Path.Combine(PluginDirectory, "Source", "ThirdParty", "GDAL", "include"));
		PublicAdditionalLibraries.Add(Path.Combine(PluginDirectory, "Source", "ThirdParty", "GDAL", "lib", "gdal.lib"));

		foreach (string DLLFile in Directory.GetFiles(Path.Combine(PluginDirectory, "Source", "ThirdParty", "GDAL", "bin")))
		{
			RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Binaries", "Win64", Path.GetFileName(DLLFile)), DLLFile);
			RuntimeDependencies.Add(Path.Combine("$(BinaryOutputDir)", Path.GetFileName(DLLFile)), DLLFile);
		}

		foreach (string File in Directory.GetFiles(Path.Combine(PluginDirectory, "Source", "ThirdParty", "GDAL", "share", "gdal")))
		{
			RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Binaries", "Win64", Path.GetFileName(File)), File);
			RuntimeDependencies.Add(Path.Combine("$(BinaryOutputDir)", "Source", "ThirdParty", "GDAL", "share", "gdal", Path.GetFileName(File)), File);
		}

		foreach (string File in Directory.GetFiles(Path.Combine(PluginDirectory, "Source", "ThirdParty", "GDAL", "share", "proj")))
		{
			RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Binaries", "Win64", Path.GetFileName(File)), File);
			RuntimeDependencies.Add(Path.Combine("$(BinaryOutputDir)", "Source", "ThirdParty", "GDAL", "share", "proj", Path.GetFileName(File)), File);
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
			}
		);
	}
}
