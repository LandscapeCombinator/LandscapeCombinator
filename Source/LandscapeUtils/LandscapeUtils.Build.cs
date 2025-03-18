// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

using UnrealBuildTool;

public class LandscapeUtils : ModuleRules
{
	public LandscapeUtils(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		// Unreal Dependencies
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Foliage",
				"Landscape",
				"Slate",
				"GeometryCore",
				"GeometryFramework",
				"GeometryScriptingCore",

				// Landscape Combinator Dependencies
				"LCReporter"
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
			PrivateDependencyModuleNames.Add("EditorFramework");
			PrivateDependencyModuleNames.Add("LandscapeEditor");
		}
	}
}
