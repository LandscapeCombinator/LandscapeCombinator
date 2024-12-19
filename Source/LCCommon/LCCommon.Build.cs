// Copyright 2023 LandscapeCombinator. All Rights Reserved.

using UnrealBuildTool;

public class LCCommon : ModuleRules
{
	public LCCommon(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		// Unreal Dependencies
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"DeveloperSettings"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"SlateCore"
			}
		);

		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.Add("UnrealEd");
		}

	}
}
