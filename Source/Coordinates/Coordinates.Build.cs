// Copyright 2023 LandscapeCombinator. All Rights Reserved.

using UnrealBuildTool;

public class Coordinates : ModuleRules
{
	public Coordinates(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_2;

        PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
		);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
            {
				// Unreal Dependencies
				"Projects",
				"InputCore",
				"EditorFramework",
				"UnrealEd",
				"ToolMenus",
				"CoreUObject",
				"Engine",
				"AssetTools",
                "AssetRegistry",

				// Other Dependencies
				"GDALInterface",
				"LandscapeUtils"
			}
		);
	}
}
