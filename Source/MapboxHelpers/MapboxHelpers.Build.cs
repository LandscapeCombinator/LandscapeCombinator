// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

using UnrealBuildTool;

public class MapboxHelpers : ModuleRules
{
	public MapboxHelpers(ReadOnlyTargetRules Target) : base(Target)
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
				"GDALInterface",
				"ConcurrencyHelpers"
			}
		);
	}
}
