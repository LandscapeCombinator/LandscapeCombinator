#pragma once

#include "CoreMinimal.h"
#include "ImageDownloader/HMFetcher.h"

/** Creates a sloping seabed from valid elevations at or below zero metres. */
class HMCreateSeabed : public HMFetcher
{
public:
	HMCreateSeabed(FString InName, double InMaxDepthMeters, int32 InShoreTransitionPixels)
		: Name(MoveTemp(InName)), MaxDepthMeters(InMaxDepthMeters), ShoreTransitionPixels(InShoreTransitionPixels) {}

protected:
	virtual FString GetOutputDir() override;
	virtual bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;

private:
	FString Name;
	double MaxDepthMeters;
	int32 ShoreTransitionPixels;
};
