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
			RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Binaries", "Win64", Path.GetFileName(DLLFile)), DLLFile);

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
				"Projects",
				"InputCore",
				"EditorFramework",
				"UnrealEd",
				"ToolMenus",
				"CoreUObject",
				"Engine",
			}
		);
	}
}
