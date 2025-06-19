// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class BuildingsFromSplines : ModuleRules
{
	public BuildingsFromSplines(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"GeometryFramework",
			}
		);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// Unreal Engine Dependencies
				"CoreUObject",
				"Engine",
				"GeometryCore",
				"GeometryScriptingCore",
				"SlateCore",

				// Landscape Combinator Dependencies
				"OSMUserData",
				"LCCommon",
				"ConcurrencyHelpers"
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
