// Copyright 2023 LandscapeCombinator. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class BuildingFromSpline : ModuleRules
{
	public BuildingFromSpline(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_2;

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
				"Projects",
				"InputCore",
				"EditorFramework",
				"UnrealEd",
				"ToolMenus",
				"CoreUObject",
				"Engine",
				"GeometryCore",
				"GeometryFramework",
				"GeometryScriptingCore",
				"GeometryScriptingEditor",

				// Other Dependencies
				"OSMUserData"
			}
		);
	}
}
