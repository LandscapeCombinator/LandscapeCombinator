// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Elevation/HeightMapTable.h"

namespace GlobalSettings {
	HeightMapTable *HMTable;

	TSharedPtr<SEditableTextBox> WorldWidthBlock;
	TSharedPtr<SEditableTextBox> WorldHeightBlock;
	TSharedPtr<SEditableTextBox> ZScaleBlock;
	TSharedPtr<SEditableTextBox> WorldOriginXBlock;
	TSharedPtr<SEditableTextBox> WorldOriginYBlock;

	TSharedRef<SWidget> GlobalSettingsTable();

	struct WorldParametersV1 {
		int32 WorldWidthKm;
		int32 WorldHeightKm;
		double ZScale;
		double WorldOriginX;
		double WorldOriginY;

		friend FArchive& operator<<(FArchive& Ar, WorldParametersV1& Params) {
			Ar << Params.WorldWidthKm;
			Ar << Params.WorldHeightKm;
			Ar << Params.ZScale;
			Ar << Params.WorldOriginX;
			Ar << Params.WorldOriginY;
			return Ar;
		}
	};
	bool GetWorldParameters(WorldParametersV1& Params);

	FVector EPSG4326ToUnrealCoordinates(FVector2D Coordinates4326, double WorldWidthCm, double WorldHeightCm, double WorldOriginX, double WorldOriginY);
	FVector2D UnrealCoordinatesToEPSG326(FVector WorldCoordinates, double WorldWidthCm, double WorldHeightCm, double WorldOriginX, double WorldOriginY);
	double UnrealCoordinatesToEPSG326X(double X, double WorldWidthCm, double WorldHeightCm, double WorldOriginX, double WorldOriginY);
	double UnrealCoordinatesToEPSG326Y(double Y, double WorldWidthCm, double WorldHeightCm, double WorldOriginX, double WorldOriginY);
}