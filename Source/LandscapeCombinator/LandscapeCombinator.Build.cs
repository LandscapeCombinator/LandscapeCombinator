// Copyright 2023 LandscapeCombinator. All Rights Reserved.

// File originally written from the Unreal Engine template for plugins

using UnrealBuildTool;
using System.IO;

public class LandscapeCombinator : ModuleRules
{
	public LandscapeCombinator(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

        PublicDependencyModuleNames.AddRange(
			new string[]
			{
				// Unreal Engine dependencies
				"Core"
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// Unreal Engine dependencies
				"Projects",
				"InputCore",
				"EditorFramework",
				"UnrealEd",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"Foliage",
				"Landscape",
				"LandscapeEditor",
				"PropertyEditor",

				// Other dependencies
                "Coordinates",
				"ConsoleHelpers",
                "GDALInterface",
                "LandscapeUtils",
                "HeightmapModifier",
				"ImageDownloader",

				"SplineImporter", // artificial dependency
				"BuildingFromSpline" // artificial dependency
			}
		);
	}
}
