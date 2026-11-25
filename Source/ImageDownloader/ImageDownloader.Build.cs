// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

using UnrealBuildTool;

public class ImageDownloader : ModuleRules
{
	public ImageDownloader(ReadOnlyTargetRules Target) : base(Target)
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

		// Unreal Dependencies
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Landscape",
				"ConsoleHelpers",
				"Coordinates"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// Unreal Engine Dependencies
				"Slate",
				"SlateCore",
				"HTTP",
				"XmlParser",
				"Projects",

				// Landscape Combinator Dependencies
				"GDALInterface",
				"FileDownloader",
				"ConcurrencyHelpers",
				"LandscapeUtils",
				"MapboxHelpers",
				"LCCommon"
			}
		);
	}
}
