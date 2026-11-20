// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FSlateBrush;

namespace LCIcons
{
	const FSlateBrush* Refresh();
	const FSlateBrush* Trash();
	const FSlateBrush* Cancel();
	const FSlateBrush* Settings();
	const FSlateBrush* Unlink();
	const FSlateBrush* Grip();

	void Shutdown();
}