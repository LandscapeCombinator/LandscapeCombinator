// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

// File originally written from the Unreal Engine template for plugins

using UnrealBuildTool;
using System.IO;

public class LandscapeCombinator : ModuleRules
{
	public LandscapeCombinator(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
#if UE_5_8_OR_LATER
		CppCompileWarningSettings.UnreachableCodeWarningLevel = WarningLevel.Error;
#endif
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		if (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion == 6)
		{
			PublicDefinitions.Add("UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_7=1"); // work-around for engine warnings in 5.6
		}

		RuntimeDependencies.Add("$(PluginDir)/Source/ThirdParty/Lucide/*.png");

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				// Unreal Engine dependencies (included by the public headers)
				"Core",
				"CoreUObject",
				"Engine",
				"Landscape",
				"PCG",
				"Slate",
				"SlateCore",
				"GeometryCore",
				"GeometryFramework",

				// Landscape Combinator dependencies (included by the public headers)
				"BuildingsFromSplines",
				"ConcurrencyHelpers",
				"ConsoleHelpers",
				"Coordinates",
				"HeightmapModifier",
				"ImageDownloader",
				"LCCommon",
				"LandscapeUtils",
				"SplineImporter"
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
				"GeometryAlgorithms",
				"GeometryScriptingCore",
				"Foliage",
				"ApplicationCore",

				// Landscape Combinator dependencies
				"GDALInterface",
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
					"LevelEditor",
					"ToolMenus"
				}
			);
		}
	}
}
