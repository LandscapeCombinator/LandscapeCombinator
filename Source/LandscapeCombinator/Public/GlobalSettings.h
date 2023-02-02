#pragma once

namespace GlobalSettings {
	TSharedPtr<SVerticalBox> HeightMaps;
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

}