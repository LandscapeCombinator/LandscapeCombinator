// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

using UnrealBuildTool;

public class ConcurrencyHelpers : ModuleRules
{
	public ConcurrencyHelpers(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		// Unreal Dependencies
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"DeveloperSettings"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
				"Slate",
				"SlateCore",
				"HTTP",
			}
		);
		
		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.Add("UnrealEd");
		}
	}
}
