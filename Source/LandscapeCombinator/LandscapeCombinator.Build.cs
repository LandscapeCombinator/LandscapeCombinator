// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class LandscapeCombinator : ModuleRules
{
	public LandscapeCombinator(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;


		// GDAL
		PublicIncludePaths.Add(Path.Combine(PluginDirectory, "ThirdParty", "GDAL", "include"));
		PublicAdditionalLibraries.Add(Path.Combine(PluginDirectory, "ThirdParty", "GDAL", "lib", "gdal.lib"));
		foreach (string DLLFile in Directory.GetFiles(Path.Combine(PluginDirectory, "ThirdParty", "GDAL", "bin")))
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
				"Foliage"
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
