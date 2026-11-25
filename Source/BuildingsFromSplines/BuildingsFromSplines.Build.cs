// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class BuildingsFromSplines : ModuleRules
{
	public BuildingsFromSplines(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
#if UE_5_8_OR_LATER
		CppCompileWarningSettings.UnreachableCodeWarningLevel = WarningLevel.Error;
#endif
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GeometryCore",
				"GeometryFramework",
				"ConcurrencyHelpers",
				"Coordinates",
				"LCCommon"
			}
		);

		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// Unreal Engine Dependencies
				"GeometryScriptingCore",
				"SlateCore",

				// Landscape Combinator Dependencies
				"OSMUserData",
				"StraightSkeletonWrapper",
				"LandscapeUtils"
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("InputCore");
			PrivateDependencyModuleNames.Add("UnrealEd");
			PrivateDependencyModuleNames.Add("PropertyEditor");
			PrivateDependencyModuleNames.Add("Slate");
			PrivateDependencyModuleNames.Add("GeometryScriptingEditor");
			PrivateDependencyModuleNames.Add("ModelingComponentsEditorOnly");
		}

	}
}
