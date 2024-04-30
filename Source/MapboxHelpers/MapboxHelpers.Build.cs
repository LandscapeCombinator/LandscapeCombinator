// Copyright 2023 LandscapeCombinator. All Rights Reserved.

using UnrealBuildTool;

public class MapboxHelpers : ModuleRules
{
	public MapboxHelpers(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

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
				"CoreUObject",
				"Engine",
				
				// Landscape Combinator Dependencies 
				"GDALInterface"
			}
		);
	}
}
