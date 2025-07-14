// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/WMSProvider.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"

#include "FileDownloader/Download.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Internationalization/Regex.h"
#include "Internationalization/TextLocalizationResource.h" 
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void FWMSProvider::SetFromURL(FString URL, TArray<FString> ExcludeCRS, TFunction<bool(FString)> LayerFilter, TFunction<void(bool)> OnComplete)
{
	CapabilitiesURL = URL;
	
	Names.Empty();
	Titles.Empty();
	Abstracts.Empty();
	CRSs.Empty();
	MinXs.Empty();
	MinYs.Empty();
	MaxXs.Empty();
	MaxYs.Empty();

	uint32 Hash = FTextLocalizationResource::HashString(URL);
	FString DownloadDir = Directories::DownloadDir();
	if (DownloadDir.IsEmpty())
	{
		if (OnComplete) OnComplete(false);
		return;
	}
	CapabilitiesFile = FPaths::Combine(DownloadDir, FString::Format(TEXT("capabilities_{0}.xsd"), { Hash }));

	Download::FromURL(URL, CapabilitiesFile, true,
		[this, ExcludeCRS, LayerFilter, OnComplete, URL](bool bSuccess)
		{
			if (bSuccess)
			{
				bool bLoaded = LoadFromFile(ExcludeCRS, LayerFilter);
				if (OnComplete) OnComplete(bLoaded);
				return;
			}
			else
			{
				LCReporter::ShowError(
					FText::Format(
						LOCTEXT("FWMSProvider::SetURL", "Error while downloading {0}."),
						FText::FromString(URL)
					)
				);
				if (OnComplete) OnComplete(false);
			}
		});
}


bool FWMSProvider::LoadFromFile(TArray<FString> ExcludeCRS, TFunction<bool(FString)> NameFilter)
{
	CapabilitiesContent = "";
	if (!FFileHelper::LoadFileToString(CapabilitiesContent, *CapabilitiesFile))
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("FWMSProvider::LoadFromFile", "Could not read file {0}."),
				FText::FromString(CapabilitiesFile)
			)
		);
		return false;
	}
	
	TArray<FString> Lines0, Lines;
	CapabilitiesContent = CapabilitiesContent.Replace(TEXT("\r\n"), TEXT("\n"));
	CapabilitiesContent.ParseIntoArray(Lines0, TEXT("\n"), true);
	for (auto& Line : Lines0)
	{
		bool bExclude = false;
		for (auto& CRS : ExcludeCRS)
		{
			if (Line.Contains(CRS))
			{
				bExclude = true;
				break;
			}
		}
		if (!bExclude)
		{
			Lines.Add(Line);
		}
	}

	CapabilitiesContent = FString::Join(Lines, TEXT(""));

	FRegexPattern LayerPattern(TEXT("<Layer([^>]*)>\\s*<Name>([^<]*)</Name>\\s*<Title>(.*?)</Title>"));
	FRegexMatcher LayerMatcher(LayerPattern, CapabilitiesContent);

	Names.Empty();
	Titles.Empty();
	Abstracts.Empty();
	CRSs.Empty();
	MinXs.Empty();
	MinYs.Empty();
	MaxXs.Empty();
	MaxYs.Empty();

	FString LastCRS = "";
	double LastMinX = 0;
	double LastMinY = 0;
	double LastMaxX = 0;
	double LastMaxY = 0;

	while (LayerMatcher.FindNext())
	{

		FString LayerParams = LayerMatcher.GetCaptureGroup(1);
		FString Name = LayerMatcher.GetCaptureGroup(2);
		FString Title = LayerMatcher.GetCaptureGroup(3);
		FString TitleRegex = Title.
			Replace(TEXT("("), TEXT("\\(")).
			Replace(TEXT(")"), TEXT("\\)")).
			Replace(TEXT("["), TEXT("\\[")).
			Replace(TEXT("]"), TEXT("\\]"));

		if (Names.Contains(Name)) continue;
		if (NameFilter && !NameFilter(Name)) continue;

		FRegexPattern AbstractPattern(*FString::Format(TEXT("{0}</Title>\\s*(<Abstract>.*?</Abstract>|<Abstract/>)"), { TitleRegex }));
		FRegexMatcher AbstractMatcher(AbstractPattern, CapabilitiesContent);
		FRegexPattern CRSPattern(*FString::Format(TEXT("{0}</Title>(.*?)BoundingBox .RS=\"([^\"]+)\"([^/]+)/"), { TitleRegex }));
		FRegexMatcher CRSMatcher(CRSPattern, CapabilitiesContent);

		bool bHasAbstract = AbstractMatcher.FindNext();
		FString Abstract = bHasAbstract ? AbstractMatcher.GetCaptureGroup(1) : Title;
		
		if (bHasAbstract)
		{
			if (Abstract == "<Abstract/>")
			{
				Abstract = Title;
			}
			else
			{
				bool bRemoved = Abstract.RemoveFromStart("<Abstract>") && Abstract.RemoveFromEnd("</Abstract>");
				if (!bRemoved)
				{
					UE_LOG(LogImageDownloader, Error, TEXT("Image Downloader: Internal error while parsing WMS capabilities"));
					continue;
				}
			}
		}

		UE_LOG(LogImageDownloader, Log, TEXT("Found layer %s"), *Name);
		UE_LOG(LogImageDownloader, Log, TEXT("Title: %s"), *Title);
		UE_LOG(LogImageDownloader, Log, TEXT("Abstract: %s"), *Abstract);
		
		if (CRSMatcher.FindNext() && !CRSMatcher.GetCaptureGroup(1).Contains("<Layer>"))
		{
			LastCRS = CRSMatcher.GetCaptureGroup(2);
			FString Bounds = CRSMatcher.GetCaptureGroup(3);
			
			FRegexPattern MinXPattern = FRegexPattern(FString("minx=\"([^\"]+)\""));
			FRegexMatcher MinXMatcher(MinXPattern, Bounds);
			FRegexPattern MinYPattern = FRegexPattern(FString("miny=\"([^\"]+)\""));
			FRegexMatcher MinYMatcher(MinYPattern, Bounds);
			FRegexPattern MaxXPattern = FRegexPattern(FString("maxx=\"([^\"]+)\""));
			FRegexMatcher MaxXMatcher(MaxXPattern, Bounds);
			FRegexPattern MaxYPattern = FRegexPattern(FString("maxy=\"([^\"]+)\""));
			FRegexMatcher MaxYMatcher(MaxYPattern, Bounds);

			if (MinXMatcher.FindNext() && MaxXMatcher.FindNext() && MinYMatcher.FindNext() && MaxYMatcher.FindNext())
			{
				LastMinX = FCString::Atod(*MinXMatcher.GetCaptureGroup(1));
				LastMaxX = FCString::Atod(*MaxXMatcher.GetCaptureGroup(1));
				LastMinY = FCString::Atod(*MinYMatcher.GetCaptureGroup(1));
				LastMaxY = FCString::Atod(*MaxYMatcher.GetCaptureGroup(1));
			}
			else
			{
				LastMinX = 0;
				LastMaxX = 0;
				LastMinY = 0;
				LastMaxY = 0;
			}
		}
	
		if (LastCRS.IsEmpty())
		{
			LastCRS = "EPSG:4326";
			LastMinX = 0;
			LastMaxX = 0;
			LastMinY = 0;
			LastMaxY = 0;
		}

		// For: https://elevation.nationalmap.gov/arcgis/services/3DEPElevation/ImageServer/WMSServer?request=GetCapabilities&service=WMS
		// The root layer (not queryable) has name 0 and can't be queried, but we still want
		// to set LastCRS and LastBounds from it for all the sublayers.
		// We usually don't want to add the root layer, so we stop (continue) now.
		// But this is commented for now as the NCOneMap WMS uses the root layer (not marked as queryable).
		// https://services.nconemap.gov/secure/services/Elevation/DEM03/ImageServer/WMSServer?request=GetCapabilities&service=WMS
		// if (Name == "0" && !LayerParams.Contains("queryable=\"1\"")) continue;
		
		// Initially, we ignored layers that explicitly contain queryable="0", but in NRW WMS server
		// there are layers with queryable="0" that should not be ignored. The check is commented out.
		// if (LayerParams.Contains("queryable=\"0\"")) continue;

		Names.Add(Name);
		Titles.Add(Title);
		Abstracts.Add(Abstract);
		CRSs.Add(LastCRS);
		MinXs.Add(LastMinX);
		MinYs.Add(LastMinY);
		MaxXs.Add(LastMaxX);
		MaxYs.Add(LastMaxY);
	}

	if (Titles.IsEmpty())
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("FWMSProvider::LoadFromFile::2", "Could not find any layer in file {0}."),
				FText::FromString(CapabilitiesFile)
			)
		);
		return false;
	}

	GetMapURL = FindGetMapURL();

	if (GetMapURL.IsEmpty())
	{
		LCReporter::ShowError(
			LOCTEXT("FWMSProvider::LoadFromFile::GetMapURL", "Could not find GetMapURL in XML capabilities.")
		);
		return false;
	}

	FindAndSetMaxSize();

	return true;
}

FString FWMSProvider::FindGetMapURL()
{
	FRegexPattern GetMapPattern(TEXT("<GetMap>.*?href=\"(http[^\"]+)\""));
	FRegexMatcher GetMapMatcher(GetMapPattern, CapabilitiesContent);

	if (GetMapMatcher.FindNext())
	{
		FString Result = GetMapMatcher.GetCaptureGroup(1);
		Result.RemoveFromEnd("amp;");

		if (!Result.EndsWith("?") && !Result.EndsWith("&"))
		{
			Result += "?";
		}
		return Result;
	}
	else
	{
		return "";
	}
}

void FWMSProvider::FindAndSetMaxSize()
{
	FRegexPattern MaxWidthPattern(TEXT("<MaxWidth>(.*)</MaxWidth>"));
	FRegexMatcher MaxWidthMatcher(MaxWidthPattern, CapabilitiesContent);
	if (MaxWidthMatcher.FindNext())
	{
		MaxWidth = FCString::Atoi(*MaxWidthMatcher.GetCaptureGroup(1));
	}
	else
	{
		UE_LOG(LogImageDownloader, Warning, TEXT("Could not find MaxWidth in the WMS capabilities."));
	}

	FRegexPattern MaxHeightPattern(TEXT("<MaxHeight>(.*)</MaxHeight>"));
	FRegexMatcher MaxHeightMatcher(MaxHeightPattern, CapabilitiesContent);
	if (MaxHeightMatcher.FindNext())
	{
		MaxHeight = FCString::Atoi(*MaxHeightMatcher.GetCaptureGroup(1));
	}
	else
	{
		UE_LOG(LogImageDownloader, Warning, TEXT("Could not find MaxHeight in the WMS capabilities."));
	}
}

bool FWMSProvider::CreateURL(
	int Width, int Height, FString Name, FString CRS, bool XIsLong,
	double MinAllowedLong, double MaxAllowedLong, double MinAllowedLat, double MaxAllowedLat,
	double MinLong, double MaxLong, double MinLat, double MaxLat,
	FString &URL, bool &bGeoTiff, FString &FileExt
)
{

#if 0 // skip bound checks for now, because we parse max longitude/latitude wrong for the NRW WMS server
	// if one bound is non zero, we check all the bounds
	if (MinAllowedLong != 0 && MaxAllowedLong != 0 && MinAllowedLat != 0 && MaxAllowedLat != 0)
	{
		if (MinLong < MinAllowedLong)
		{
			LCReporter::ShowError(FText::Format(
				LOCTEXT("GenericWMSMinLong", "MinLong ({0}) is smaller than MinAllowedLong ({1})"),
				FText::AsNumber(MinLong),
				FText::AsNumber(MinAllowedLong)
			));
			return false;
		}
		if (MaxLong > MaxAllowedLong)
		{
			LCReporter::ShowError(FText::Format(
				LOCTEXT("GenericWMSMaxLong", "MaxLong ({0}) is larger than MaxAllowedLong ({1})"),
				FText::AsNumber(MaxLong),
				FText::AsNumber(MaxAllowedLong)
			));
			return false;
		}
		if (MinLat < MinAllowedLat)
		{
			LCReporter::ShowError(FText::Format(
				LOCTEXT("GenericWMSMinLat", "MinLat ({0}) is smaller than MinAllowedLat ({1})"),
				FText::AsNumber(MinLat),
				FText::AsNumber(MinAllowedLat)
			));
			return false;
		}
		if (MaxLat > MaxAllowedLat)
		{
			LCReporter::ShowError(FText::Format(
				LOCTEXT("GenericWMSMaxLat", "MaxLat ({0}) is larger than MaxAllowedLat ({1})"),
				FText::AsNumber(MaxLat),
				FText::AsNumber(MaxAllowedLat)
			));
			return false;
		}
	}

	if (MinLong >= MaxLong)
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("MinMaxLong", "MinLong ({0}) is larger than MaxLong ({1})"),
			FText::AsNumber(MinLong),
			FText::AsNumber(MaxLong)
		));
		return false;
	}

	if (MinLong >= MaxLong)
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("MinMaxLat", "MinLat ({0}) is larger than MaxLat ({1})"),
			FText::AsNumber(MinLat),
			FText::AsNumber(MaxLat)
		));
		return false;
	}

	if (Width <= 0)
	{
		LCReporter::ShowError(
			LOCTEXT("GenericWMSInitErrorNegativeWidth", "The width is not positive.")
		);

		return false;
	}

	if (MaxWidth != 0 && Width > MaxWidth)
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("GenericWMSInitErrorLargeWidth", "The width is higher than {0}px, which is not supported by this WMS."),
			FText::AsNumber(MaxWidth, &FNumberFormattingOptions::DefaultNoGrouping())
		));

		return false;
	}

	if (Height <= 0)
	{
		LCReporter::ShowError(
			LOCTEXT("GenericWMSInitErrorNegativeHeight", "The height is not positive.")
		);

		return false;
	}

	if (MaxHeight != 0 && Height > MaxHeight)
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("GenericWMSInitErrorLargeHeight", "The height is higher than {0}px, which is not supported by this WMS."),
			FText::AsNumber(MaxHeight, &FNumberFormattingOptions::DefaultNoGrouping())
		));

		return false;
	}
#endif

	if (GetMapURL.IsEmpty())
	{
		LCReporter::ShowError(
			LOCTEXT("NoWMSProvider", "There is something wrong with the WMS provider (GetMap URL not found), please try to reload it.")
		);
		return false;
	}

	if (Name.IsEmpty())
	{
		LCReporter::ShowError(
			LOCTEXT("NoWMSName", "There is something wrong with the WMS layer name, please try to reload the WMS provider.")
		);
		return false;
	}

	if (CRS.IsEmpty())
	{
		LCReporter::ShowError(
			LOCTEXT("NoWMSCRS", "There is something wrong with the WMS CRS, please try to reload the WMS provider.")
		);
		return false;
	}

	int32 GeoTiffIndex = CapabilitiesContent.Find("image/geotiff");
	int32 TiffIndex = CapabilitiesContent.Find("image/tiff");
	bGeoTiff = GeoTiffIndex != INDEX_NONE;
	bool bTiff = TiffIndex != INDEX_NONE;


	FString ImageFormat;
	if (bGeoTiff) {
		ImageFormat = *CapabilitiesContent.Mid(GeoTiffIndex, 13);
	} else if (bTiff) {
		ImageFormat = *CapabilitiesContent.Mid(TiffIndex, 10);
	} else {
		ImageFormat = "image/png";
	}
	FileExt = bGeoTiff || bTiff ? "tif" : "png";

	if (XIsLong)
	{
		URL = FString::Format(
			TEXT("{0}LAYERS={1}&FORMAT={2}&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetMap&CRS={3}&STYLES=&BBOX={4},{5},{6},{7}&WIDTH={8}&HEIGHT={9}"),
			{
				GetMapURL, Name, ImageFormat, CRS,
				MinLong, MinLat, MaxLong, MaxLat,
				Width, Height
			}
		);
	}
	else
	{
		URL = FString::Format(
			TEXT("{0}LAYERS={1}&FORMAT={2}&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetMap&CRS={3}&STYLES=&BBOX={4},{5},{6},{7}&WIDTH={8}&HEIGHT={9}"),
			{
				GetMapURL, Name, ImageFormat, CRS,
				MinLat, MinLong, MaxLat, MaxLong,
				Width, Height
			}
		);
	}

	return true;
}


#undef LOCTEXT_NAMESPACE