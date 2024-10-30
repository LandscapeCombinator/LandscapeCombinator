// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMXYZ: public HMFetcher
{
public:
	HMXYZ(
		bool bSilentMode0,
		bool bAllowInvalidTiles0,
		FString Name0, FString Layer0, FString Format0, FString URL0,
		int Zoom0, int MinX0, int MaxX0, int MinY0, int MaxY0,
		bool bMaxY_IsNorth0, bool bGeoreferenceSlippyTiles0, bool bDecodeMapbox0,
		bool bUseTerrariumFormula0,
		FString CRS0)
	{
		bSilentMode = bSilentMode0;
		Name = Name0;
		Layer = Layer0;
		Format = Format0;
		URL = URL0;
		Zoom = Zoom0;
		MinX = MinX0;
		MaxX = MaxX0;
		MinY = MinY0;
		MaxY = MaxY0;
		bMaxY_IsNorth = bMaxY_IsNorth0;
		bGeoreferenceSlippyTiles = bGeoreferenceSlippyTiles0;
		CRS = CRS0;
		bDecodeMapbox = bDecodeMapbox0;
		bUseTerrariumFormula = bUseTerrariumFormula0;
		bAllowInvalidTiles = bAllowInvalidTiles0;
	};
	void Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

private:
	bool bSilentMode;
	bool bAllowInvalidTiles;
	FString Name;
	FString Layer;
	FString Format;
	FString URL;
	int Zoom;
	int MinX;
	int MinY;
	int MaxX;
	int MaxY;
	bool bMaxY_IsNorth;
	bool bGeoreferenceSlippyTiles;
	bool bDecodeMapbox;
	bool bUseTerrariumFormula;
	FString CRS;
};

#undef LOCTEXT_NAMESPACE