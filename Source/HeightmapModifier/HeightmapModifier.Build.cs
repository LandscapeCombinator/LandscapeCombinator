// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

using UnrealBuildTool;

public class HeightmapModifier : ModuleRules
{
	public HeightmapModifier(ReadOnlyTargetRules Target) : base(Target)
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
				// Unreal Dependencies
				"Projects",
				"InputCore",
				"CoreUObject",
				"Foliage",
				"Engine",
				"Landscape",
				"SlateCore",
				
				// Landscape Combinator Dependencies
				"GDALInterface",
				"LandscapeUtils",
				"ConsoleHelpers",
				"LCCommon",
				"ConcurrencyHelpers"
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
