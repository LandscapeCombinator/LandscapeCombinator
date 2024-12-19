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
				"GeometryFramework",
				"CoreUObject",
				"Engine",
				"Foliage",
				"Landscape",
				"PCG",
				"ApplicationCore",
				"StructUtils",

				// Landscape Combinator dependencies
				"Coordinates",
				"ConsoleHelpers",
				"ConcurrencyHelpers",
				"GDALInterface",
				"LandscapeUtils",
				"HeightmapModifier",
				"ImageDownloader",
				"SplineImporter",
				"BuildingFromSpline",
				"LCCommon"
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"LandscapeEditor",
					"EditorFramework",
					"PropertyEditor",
					"UnrealEd",
					"UMGEditor",
					"ToolMenus",
					"Blutility",
					"Slate",
					"SlateCore"
				}
			);
		}
	}
}
