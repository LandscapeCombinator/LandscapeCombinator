// Copyright 2023 LandscapeCombinator. All Rights Reserved.

// File originally written from the Unreal Engine template for plugins

using UnrealBuildTool;
using System.IO;

public class LandscapeCombinator : ModuleRules
{
	public LandscapeCombinator(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// GDAL
		PublicIncludePaths.Add(Path.Combine(PluginDirectory, "Source", "ThirdParty", "GDAL", "include"));
		PublicAdditionalLibraries.Add(Path.Combine(PluginDirectory, "Source", "ThirdParty", "GDAL", "lib", "gdal.lib"));
		foreach (string DLLFile in Directory.GetFiles(Path.Combine(PluginDirectory, "Source", "ThirdParty", "GDAL", "bin")))
			RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Binaries", "Win64", Path.GetFileName(DLLFile)), DLLFile);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"EditorFramework",
				"UnrealEd",
				"ToolMenus",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"Landscape",
				"LandscapeEditor",
				"HTTP",
				"XmlParser",
				"PropertyEditor",
				"Foliage",
				"FoliageEdit",
				"PCG",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
