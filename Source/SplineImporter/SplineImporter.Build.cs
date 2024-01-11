// Copyright 2023 LandscapeCombinator. All Rights Reserved.

using UnrealBuildTool;

public class SplineImporter : ModuleRules
{
	public SplineImporter(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_2;

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
                "HTTP",
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
