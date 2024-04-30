// Copyright 2023 LandscapeCombinator. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class BuildingFromSpline : ModuleRules
{
	public BuildingFromSpline(ReadOnlyTargetRules Target) : base(Target)
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
				// Unreal Engine Dependencies
				"CoreUObject",
				"Engine",
				"GeometryCore",
				"GeometryFramework",
				"GeometryScriptingCore",

				// Other Dependencies
				"OSMUserData"
			}
		);

		if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
            PrivateDependencyModuleNames.Add("PropertyEditor");
            PrivateDependencyModuleNames.Add("GeometryScriptingEditor");
		}


	}
}
