// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImageDownloader/HMFetcher.h"

class HMXYZ: public HMFetcher
{
public:
	HMXYZ(
		bool bAllowInvalidTiles0, bool bEnableParallelDownload0,
		FString Name0, FString Layer0, FString Format0, FString URL0,
		int Zoom0, int MinX0, int MaxX0, int MinY0, int MaxY0,
		bool bMaxY_IsNorth0, bool bGeoreferenceSlippyTiles0, bool bDecodeMapbox0,
		bool bUseTerrariumFormula0,
		FString CRS0)
	{
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
		bEnableParallelDownload = bEnableParallelDownload0;
	};

	FString GetOutputDir() override
	{
		return FPaths::Combine(ImageDownloaderDir, Name + "-XYZ");
	}
	bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;

protected:
	bool bAllowInvalidTiles;
	bool bEnableParallelDownload;
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
