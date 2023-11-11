// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class Directories {
public:
	static FString ImageDownloaderDir();
	static FString DownloadDir();
	static void CouldNotInitializeDirectory(FString Dir);
};
