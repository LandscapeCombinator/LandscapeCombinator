// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LCIcons.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Interfaces/IPluginManager.h"

namespace
{
	TMap<FName, TSharedPtr<FSlateDynamicImageBrush>> Brushes;

	const FSlateBrush* Load(const FName Name)
	{
		TSharedPtr<FSlateDynamicImageBrush>& Brush = Brushes.FindOrAdd(Name);
		if (!Brush.IsValid())
		{
			TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("LandscapeCombinator"));
			if (!Plugin.IsValid()) return nullptr;

			const FString Path = Plugin->GetBaseDir() / TEXT("Source/ThirdParty/Lucide") / (Name.ToString() + TEXT(".png"));
			Brush = MakeShared<FSlateDynamicImageBrush>(FName(*Path), FVector2D(16.0, 16.0));
		}
		return Brush.Get();
	}
}

const FSlateBrush* LCIcons::Refresh() { return Load(TEXT("refresh-cw")); }
const FSlateBrush* LCIcons::Trash() { return Load(TEXT("trash")); }
const FSlateBrush* LCIcons::Cancel() { return Load(TEXT("x")); }
const FSlateBrush* LCIcons::Settings() { return Load(TEXT("settings")); }
const FSlateBrush* LCIcons::Unlink() { return Load(TEXT("unlink")); }
const FSlateBrush* LCIcons::Grip() { return Load(TEXT("grip-vertical")); }

void LCIcons::Shutdown() { Brushes.Empty(); }