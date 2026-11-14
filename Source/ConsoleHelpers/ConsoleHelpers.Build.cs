// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

using UnrealBuildTool;

public class ConsoleHelpers : ModuleRules
{
	public ConsoleHelpers(ReadOnlyTargetRules Target) : base(Target)
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
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"HTTP",

				"ConcurrencyHelpers"
			}
		);
	}
}
