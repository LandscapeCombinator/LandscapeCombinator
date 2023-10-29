// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/HMRGEAlti.h"

#include "Internationalization/TextLocalizationResource.h" 

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

HMURL* HMRGEALTI::RGEALTI(
	FString LandscapeLabel, FString URL,
	int EPSG,
	double MinLong, double MaxLong,
	double MinLat, double MaxLat,
	bool bOverrideWidthAndHeight, int Width0, int Height0
)
{
	int Width = Width0;
	int Height = Height0;

	if (!bOverrideWidthAndHeight)
	{
		if (EPSG == 2154)
		{
			Width = MaxLong - MinLong;
			Height = MaxLat - MinLat;
		}
		else
		{
			Width = (MaxLong - MinLong) * 111111.11;
			Height = (MaxLat - MinLat) * 111111.11;
		}
	}

	if (Width <= 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("RGEALTIInitErrorNegativeWidth", "The width of {0} is not positive."),
			FText::FromString(LandscapeLabel)
		));

		return nullptr;
	}

	if (Width > 10000)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("RGEALTIInitErrorLargeWidth", "The width of {0} is higher than 10000px, which is not supported by RGE ALTI 1m WMS."),
			FText::FromString(LandscapeLabel)
		));

		return nullptr;
	}

	if (Height <= 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("RGEALTIInitErrorNegativeHeight", "The height of {0} is not positive."),
			FText::FromString(LandscapeLabel)
		));

		return nullptr;
	}

	if (Height > 10000)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("RGEALTIInitErrorLargeHeight", "The height of {0} is higher than 10000px, which is not supported by RGE ALTI 1m WMS."),
			FText::FromString(LandscapeLabel)
		));

		return nullptr;
	}

	FString CoordinatesString = FString::Format(
		TEXT("{0}_{1}_{2}_{3}_{4}_{5}"),
		{ MinLong, MaxLong, MinLat, MaxLat, Width, Height }
	);
	int Hash = FTextLocalizationResource::HashString(CoordinatesString);

	HMURL* Result = new HMURL(
		FString::Format(
			TEXT("{0}{1},{2},{3},{4}&WIDTH={5}&HEIGHT={6}&STYLES="),
			{ URL, MinLong, MinLat, MaxLong, MaxLat, Width, Height }
		),
		FString::Format(TEXT("RGE_ALTI_{0}.tif"), { Hash }),
		EPSG
	);
	return Result;
}

#undef LOCTEXT_NAMESPACE