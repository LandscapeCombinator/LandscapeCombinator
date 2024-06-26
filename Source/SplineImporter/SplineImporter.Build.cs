// Copyright 2023 LandscapeCombinator. All Rights Reserved.

using UnrealBuildTool;

public class SplineImporter : ModuleRules
{
	public SplineImporter(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				// Unreal Engine Dependencies
				"Core"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// Unreal Engine Dependencies
				"CoreUObject",
				"Engine",
				"PCG",
				"Foliage",
				"Landscape",
				"LandscapeEditor",
				"UnrealEd",
				"Slate",
				"EditorFramework",

				// Other Dependencies
				"LandscapeUtils",
				"Coordinates",
				"GDALInterface",
				"FileDownloader",
				"OSMUserData"
			}
		);
	}
}
