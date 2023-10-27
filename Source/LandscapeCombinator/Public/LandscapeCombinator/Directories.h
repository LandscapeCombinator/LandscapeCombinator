// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class Directories {
public:
	static FString LandscapeCombinatorDir();
	static FString DownloadDir();
	static void CouldNotInitializeDirectory(FString Dir);
};
