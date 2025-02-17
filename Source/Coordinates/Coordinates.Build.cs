// Copyright 2023 LandscapeCombinator. All Rights Reserved.

using UnrealBuildTool;

public class Coordinates : ModuleRules
{
	public Coordinates(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Landscape"
			}
		);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// Unreal Dependencies
				"Projects",
				"InputCore",
				"CoreUObject",
				"Engine",

				// Landscape Combinator Dependencies
				"GDALInterface",
				"LandscapeUtils",
				"FileDownloader",
				"LCCommon"
			}
		);
	}
}
