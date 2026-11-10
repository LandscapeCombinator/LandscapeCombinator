// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IMAGEDOWNLOADER_API Directories {
public:
	static FString ImageDownloaderDir();
	static FString DownloadDir();
	static void CouldNotInitializeDirectory(FString Dir);
};
