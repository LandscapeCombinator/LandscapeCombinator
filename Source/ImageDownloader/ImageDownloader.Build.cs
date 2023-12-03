// Copyright 2023 LandscapeCombinator. All Rights Reserved.

using UnrealBuildTool;

public class ImageDownloader : ModuleRules
{
	public ImageDownloader(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_2;

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
				"PropertyEditor",
				"Landscape",

				// Other Dependencies
                "Coordinates",
                "GDALInterface",
                "FileDownloader",
                "ConcurrencyHelpers",
                "ConsoleHelpers",
				"LandscapeUtils"
            }
		);
	}
}
