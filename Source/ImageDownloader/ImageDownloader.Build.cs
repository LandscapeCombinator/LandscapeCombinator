// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

using UnrealBuildTool;

public class ImageDownloader : ModuleRules
{
	public ImageDownloader(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		// Unreal Dependencies
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// Unreal Engine Dependencies
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"HTTP",
				"Projects",
				"Landscape",

				// Landscape Combinator Dependencies
				"Coordinates",
				"GDALInterface",
				"FileDownloader",
				"ConcurrencyHelpers",
				"ConsoleHelpers",
				"LandscapeUtils",
				"MapboxHelpers",
				"LCCommon",
				"LCReporter"
			}
		);
	}
}
