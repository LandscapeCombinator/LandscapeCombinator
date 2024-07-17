// Copyright 2023 LandscapeCombinator. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

using Microsoft.Extensions.Logging;
using UnrealBuildTool.Rules;

public class GDALInterface : ModuleRules
{

	public GDALInterface(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		// GDAL

		string Platform = Target.Platform == UnrealTargetPlatform.Win64 ? "Win64" : "Linux";
		PublicIncludePaths.Add(Path.Combine(PluginDirectory, "Source", "ThirdParty", "GDAL", Platform, "include"));


		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(Path.Combine(PluginDirectory, "Source", "ThirdParty", "GDAL", "Win64", "lib", "gdal.lib"));

			foreach (string DLLFile in Directory.GetFiles(Path.Combine(PluginDirectory, "Source", "ThirdParty", "GDAL", "Win64", "bin")))
			{
				RuntimeDependencies.Add(Path.Combine("$(BinaryOutputDir)", Path.GetFileName(DLLFile)), DLLFile);
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			foreach (string SOFile in Directory.GetFiles(Path.Combine(PluginDirectory, "Source", "ThirdParty", "GDAL", "Linux", "bin"), "*.so"))
			{
				PublicAdditionalLibraries.Add(SOFile);
			}
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

				// Other Dependencies
				"FileDownloader",
			}
		);
	}
}
