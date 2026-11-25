// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

using UnrealBuildTool;

public class LandscapeUtils : ModuleRules
{
	public LandscapeUtils(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
#if UE_5_8_OR_LATER
		CppCompileWarningSettings.UnreachableCodeWarningLevel = WarningLevel.Error;
#endif
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		// Unreal Dependencies
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Landscape",
				"GeometryCore",
				"Coordinates",
				"LCCommon"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Foliage",
				"Slate",
				"GeometryFramework",
				"GeometryScriptingCore",

				// Landscape Combinator Dependencies
				"ConcurrencyHelpers",
				"GDALInterface"
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
