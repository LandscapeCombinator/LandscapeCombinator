using UnrealBuildTool;
using System.IO;

public class StraightSkeleton : ModuleRules
{
	public StraightSkeleton(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		bUseRTTI = true;

		PublicDependencyModuleNames.Add("Core");
	}
}
