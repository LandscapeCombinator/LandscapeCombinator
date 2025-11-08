// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

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
		if (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion == 6)
		{
			PublicDefinitions.Add("UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_7=1"); // work-around for engine warnings in 5.6
		}

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				// Unreal Engine dependencies
				"Core"
			}
		);

		if (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion < 6)
		{
			PrivateDependencyModuleNames.Add("StructUtils");
		}

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// Unreal Engine dependencies
				"Projects",
				"InputCore",
				"GeometryCore",
				"GeometryFramework",
				"GeometryScriptingCore",
				"CoreUObject",
				"Engine",
				"Foliage",
				"Landscape",
				"PCG",
				"ApplicationCore",

				// Landscape Combinator dependencies
				"Coordinates",
				"ConsoleHelpers",
				"ConcurrencyHelpers",
				"GDALInterface",
				"LandscapeUtils",
				"HeightmapModifier",
				"ImageDownloader",
				"SplineImporter",
				"BuildingsFromSplines",
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
