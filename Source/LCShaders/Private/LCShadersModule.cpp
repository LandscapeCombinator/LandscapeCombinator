// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "LCShadersModule.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

void FLCShadersModule::StartupModule()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("LandscapeCombinator"));
	if (!Plugin.IsValid()) return;

	const FString ShaderDirectory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
	if (!FPaths::DirectoryExists(ShaderDirectory))
	{
		UE_LOG(LogTemp, Error, TEXT("LCShaders: shader directory %s does not exist"), *ShaderDirectory);
		return;
	}

	AddShaderSourceDirectoryMapping(TEXT("/Plugin/LandscapeCombinator"), ShaderDirectory);
}

IMPLEMENT_MODULE(FLCShadersModule, LCShaders)