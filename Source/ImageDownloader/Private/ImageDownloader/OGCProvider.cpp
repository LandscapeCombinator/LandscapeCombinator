// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/OGCProvider.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"

#include "FileDownloader/Download.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "XmlFile.h"
#include "XmlNode.h"
#include "Internationalization/Regex.h"
#include "Internationalization/TextLocalizationResource.h" 
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

FString FOGCProvider::ServiceName() const
{
	return Service == EOGCService::WCS ? TEXT("WCS") : TEXT("WMS");
}

void FOGCProvider::SetFromURL(EOGCService InService, FString URL, TArray<FString> ExcludeCRS, TFunction<bool(FString)> LayerFilter, TFunction<void(bool)> OnComplete)
{
	Service = InService;
	CapabilitiesURL = URL;
	MaxWidth = 0;
	MaxHeight = 0;
	
	Names.Empty();
	Titles.Empty();
	Abstracts.Empty();
	CRSs.Empty();
	MinXs.Empty();
	MinYs.Empty();
	MaxXs.Empty();
	MaxYs.Empty();
	XAxisLabels.Empty();
	YAxisLabels.Empty();
	ImageFormat.Empty();
	RequestURL.Empty();
	DescribeCoverageURL.Empty();

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
				if (Service == EOGCService::WCS)
				{
					// The capabilities only list the coverage identifiers: the CRS and bounds come from DescribeCoverage
					if (!LoadWCSCapabilities(LayerFilter))
					{
						if (OnComplete) OnComplete(false);
						return;
					}
					DescribeWCSCoverages(0, ExcludeCRS, OnComplete);
					return;
				}

				bool bLoaded = LoadWMSCapabilities(ExcludeCRS, LayerFilter);
				if (OnComplete) OnComplete(bLoaded);
				return;
			}
			else
			{
				LCReporter::ShowError(
					FText::Format(
						LOCTEXT("FOGCProvider::SetURL", "Error while downloading {0}."),
						FText::FromString(URL)
					)
				);
				if (OnComplete) OnComplete(false);
			}
		});
}


bool FOGCProvider::LoadWMSCapabilities(TArray<FString> ExcludeCRS, TFunction<bool(FString)> NameFilter)
{
	CapabilitiesContent = "";
	if (!FFileHelper::LoadFileToString(CapabilitiesContent, *CapabilitiesFile))
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("FOGCProvider::LoadFromFile", "Could not read file {0}."),
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

		int32 LayerBlockStart = LayerMatcher.GetMatchEnding();
		int32 NextLayerPos = CapabilitiesContent.Find(TEXT("<Layer"), ESearchCase::IgnoreCase, ESearchDir::FromStart, LayerBlockStart);
		int32 LayerBlockEnd = (NextLayerPos == INDEX_NONE) ? CapabilitiesContent.Len() : NextLayerPos;
		FString LayerBlock = CapabilitiesContent.Mid(LayerBlockStart, LayerBlockEnd - LayerBlockStart);

		FRegexPattern AbstractPattern(TEXT("^\\s*(<Abstract>.*?</Abstract>|<Abstract/>)"));
		FRegexMatcher AbstractMatcher(AbstractPattern, LayerBlock);
		FRegexPattern CRSPattern(TEXT("^(.*?)BoundingBox .RS=\"([^\"]+)\"([^/]+)/"));
		FRegexMatcher CRSMatcher(CRSPattern, LayerBlock);

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
		Titles.Add(FString::Printf(TEXT("%s (%s)"), *Title, *Name));
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
				LOCTEXT("FOGCProvider::LoadFromFile::2", "Could not find any layer in file {0}."),
				FText::FromString(CapabilitiesFile)
			)
		);
		return false;
	}

	RequestURL = FindWMSRequestURL();

	if (RequestURL.IsEmpty())
	{
		LCReporter::ShowError(
			LOCTEXT("FOGCProvider::LoadFromFile::RequestURL", "Could not find the GetMap URL in the XML capabilities.")
		);
		return false;
	}

	FindAndSetMaxSize();

	return true;
}

FString FOGCProvider::FindWMSRequestURL()
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

void FOGCProvider::FindAndSetMaxSize()
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

bool FOGCProvider::CreateURL(
	int Width, int Height, FString Name, FString CRS, bool XIsLong,
	double MinAllowedLong, double MaxAllowedLong, double MinAllowedLat, double MaxAllowedLat,
	double MinLong, double MaxLong, double MinLat, double MaxLat,
	FString &URL, bool &bGeoTiff, FString &FileExt
)
{

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

	if (MinLat >= MaxLat)
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
			LOCTEXT("GenericWMSInitErrorLargeWidth", "The width is higher than {0}px, which is not supported by this service."),
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
			LOCTEXT("GenericWMSInitErrorLargeHeight", "The height is higher than {0}px, which is not supported by this service."),
			FText::AsNumber(MaxHeight, &FNumberFormattingOptions::DefaultNoGrouping())
		));

		return false;
	}

	if (RequestURL.IsEmpty())
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("NoOGCProvider", "There is something wrong with the {0} provider (request URL not found), please try to reload it."),
			FText::FromString(ServiceName())
		));
		return false;
	}

	if (Name.IsEmpty())
	{
		LCReporter::ShowError(
			LOCTEXT("NoWMSName", "There is something wrong with the layer name, please try to reload the provider.")
		);
		return false;
	}

	if (CRS.IsEmpty())
	{
		LCReporter::ShowError(
			LOCTEXT("NoWMSCRS", "There is something wrong with the CRS, please try to reload the provider.")
		);
		return false;
	}

	if (Service == EOGCService::WCS)
	{
		return CreateWCSURL(Width, Height, Name, MinLong, MaxLong, MinLat, MaxLat, URL, bGeoTiff, FileExt);
	}

	int32 GeoTiffIndex = CapabilitiesContent.Find("image/geotiff");
	int32 TiffIndex = CapabilitiesContent.Find("image/tiff");
	bGeoTiff = GeoTiffIndex != INDEX_NONE;
	bool bTiff = TiffIndex != INDEX_NONE;


	FString WMSFormat;
	if (bGeoTiff) {
		WMSFormat = *CapabilitiesContent.Mid(GeoTiffIndex, 13);
	} else if (bTiff) {
		WMSFormat = *CapabilitiesContent.Mid(TiffIndex, 10);
	} else {
		WMSFormat = "image/png";
	}
	FileExt = bGeoTiff || bTiff ? "tif" : "png";

	// WMS 1.1.x uses long, lat axis order while 1.3.0 uses CRS axis order
	const FString WMSVersion = (XIsLong && CapabilitiesContent.Contains(TEXT("WMT_MS_Capabilities"))) ? TEXT("1.1.1") : TEXT("1.3.0");
	if (XIsLong)
	{
		URL = FString::Format(
			TEXT("{0}LAYERS={1}&FORMAT={2}&SERVICE=WMS&VERSION={10}&REQUEST=GetMap&CRS={3}&srs={3}&STYLES=&BBOX={4},{5},{6},{7}&WIDTH={8}&HEIGHT={9}"),
			{
				RequestURL, Name, WMSFormat, CRS,
				MinLong, MinLat, MaxLong, MaxLat,
				Width, Height, WMSVersion
			}
		);
	}
	else
	{
		URL = FString::Format(
			TEXT("{0}LAYERS={1}&FORMAT={2}&SERVICE=WMS&VERSION={10}&REQUEST=GetMap&CRS={3}&srs={3}&STYLES=&BBOX={4},{5},{6},{7}&WIDTH={8}&HEIGHT={9}"),
			{
				RequestURL, Name, WMSFormat, CRS,
				MinLat, MinLong, MaxLat, MaxLong,
				Width, Height, WMSVersion
			}
		);
	}

	return true;
}


// ---------------------------------------------------------------------------------------------------------------------
// WCS 2.0.1
// ---------------------------------------------------------------------------------------------------------------------

namespace
{
	// "wcs:CoverageId" -> "CoverageId": servers use different prefixes for the same namespaces
	FString LocalName(const FString& Tag)
	{
		int32 Index;
		return Tag.FindLastChar(TEXT(':'), Index) ? Tag.Mid(Index + 1) : Tag;
	}

	const FXmlNode* FindChild(const FXmlNode* Node, const TCHAR* Name)
	{
		if (!Node) return nullptr;
		for (const FXmlNode* Child : Node->GetChildrenNodes())
		{
			if (LocalName(Child->GetTag()) == Name) return Child;
		}
		return nullptr;
	}

	FString ChildText(const FXmlNode* Node, const TCHAR* Name)
	{
		const FXmlNode* Child = FindChild(Node, Name);
		return Child ? Child->GetContent().TrimStartAndEnd() : FString();
	}

	FString GetAttribute(const FXmlNode* Node, const TCHAR* Name)
	{
		if (!Node) return FString();
		for (const FXmlAttribute& Attribute : Node->GetAttributes())
		{
			if (LocalName(Attribute.GetTag()) == Name) return Attribute.GetValue();
		}
		return FString();
	}

	// "http://www.opengis.net/def/crs/EPSG/0/28992" or "urn:ogc:def:crs:EPSG::4326" -> "EPSG:28992"
	FString ToEPSG(const FString& SrsName)
	{
		int32 Index;
		if (SrsName.Contains(TEXT("/EPSG/")) && SrsName.FindLastChar(TEXT('/'), Index)) return TEXT("EPSG:") + SrsName.Mid(Index + 1);
		if (SrsName.Contains(TEXT("EPSG:")) && SrsName.FindLastChar(TEXT(':'), Index)) return TEXT("EPSG:") + SrsName.Mid(Index + 1);
		return SrsName;
	}

	bool IsYAxisLabel(const FString& Label)
	{
		const FString L = Label.ToLower();
		return L == TEXT("y") || L == TEXT("n") || L == TEXT("lat") || L == TEXT("latitude") || L == TEXT("north") || L == TEXT("northing");
	}

	bool ParseCorner(const FString& Text, double& OutFirst, double& OutSecond)
	{
		TArray<FString> Parts;
		Text.TrimStartAndEnd().ParseIntoArrayWS(Parts);
		if (Parts.Num() < 2) return false;
		OutFirst = FCString::Atod(*Parts[0]);
		OutSecond = FCString::Atod(*Parts[1]);
		return true;
	}

	// The URL of an operation (GetCoverage, DescribeCoverage...), made so that parameters can be appended
	FString FindOperationURL(const FXmlNode* Root, const TCHAR* OperationName)
	{
		const FXmlNode* Operations = FindChild(Root, TEXT("OperationsMetadata"));
		if (!Operations) return FString();

		for (const FXmlNode* Operation : Operations->GetChildrenNodes())
		{
			if (LocalName(Operation->GetTag()) != TEXT("Operation") || GetAttribute(Operation, TEXT("name")) != OperationName) continue;

			const FXmlNode* Get = FindChild(FindChild(FindChild(Operation, TEXT("DCP")), TEXT("HTTP")), TEXT("Get"));
			FString URL = GetAttribute(Get, TEXT("href")).Replace(TEXT("&amp;"), TEXT("&"));
			if (URL.IsEmpty()) return FString();
			if (!URL.EndsWith(TEXT("?")) && !URL.EndsWith(TEXT("&"))) URL += URL.Contains(TEXT("?")) ? TEXT("&") : TEXT("?");
			return URL;
		}
		return FString();
	}
}

bool FOGCProvider::LoadWCSCapabilities(TFunction<bool(FString)> NameFilter)
{
	CapabilitiesContent = "";
	if (!FFileHelper::LoadFileToString(CapabilitiesContent, *CapabilitiesFile))
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("FOGCProvider::LoadWCSCapabilities::Read", "Could not read file {0}."),
			FText::FromString(CapabilitiesFile)
		));
		return false;
	}

	FXmlFile Xml(CapabilitiesContent, EConstructMethod::ConstructFromBuffer);
	if (!Xml.IsValid())
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("FOGCProvider::LoadWCSCapabilities::Xml", "Could not parse the WCS capabilities in {0}: {1}"),
			FText::FromString(CapabilitiesFile), FText::FromString(Xml.GetLastError())
		));
		return false;
	}
	const FXmlNode* Root = Xml.GetRootNode();

	const FXmlNode* Contents = FindChild(Root, TEXT("Contents"));
	if (Contents)
	{
		for (const FXmlNode* Summary : Contents->GetChildrenNodes())
		{
			if (LocalName(Summary->GetTag()) != TEXT("CoverageSummary")) continue;

			const FString Id = ChildText(Summary, TEXT("CoverageId"));
			if (Id.IsEmpty() || Names.Contains(Id)) continue;
			if (NameFilter && !NameFilter(Id)) continue;

			// Many servers only give the identifier, which is then also the title
			FString Title = ChildText(Summary, TEXT("Title"));
			if (Title.IsEmpty() || Titles.Contains(Title)) Title = Id;
			FString Abstract = ChildText(Summary, TEXT("Abstract"));
			if (Abstract.IsEmpty()) Abstract = Title;

			Names.Add(Id);
			Titles.Add(Title);
			Abstracts.Add(Abstract);
			CRSs.Add("");
			MinXs.Add(0);
			MinYs.Add(0);
			MaxXs.Add(0);
			MaxYs.Add(0);
			XAxisLabels.Add("x");
			YAxisLabels.Add("y");
		}
	}

	if (Names.IsEmpty())
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("FOGCProvider::LoadWCSCapabilities::NoCoverage", "Could not find any coverage in file {0}."),
			FText::FromString(CapabilitiesFile)
		));
		return false;
	}

	// A GeoTIFF format keeps the georeferencing and the heights
	ImageFormat.Empty();
	if (const FXmlNode* ServiceMetadata = FindChild(Root, TEXT("ServiceMetadata")))
	{
		FString Fallback;
		for (const FXmlNode* Format : ServiceMetadata->GetChildrenNodes())
		{
			if (LocalName(Format->GetTag()) != TEXT("formatSupported")) continue;
			const FString Value = Format->GetContent().TrimStartAndEnd();
			const FString Lower = Value.ToLower();
			if (Lower == TEXT("image/tiff")) { ImageFormat = Value; break; }
			if (Fallback.IsEmpty() && Lower.Contains(TEXT("tiff"))) Fallback = Value;
		}
		if (ImageFormat.IsEmpty()) ImageFormat = Fallback;
	}
	if (ImageFormat.IsEmpty()) ImageFormat = "image/tiff";

	RequestURL = FindOperationURL(Root, TEXT("GetCoverage"));
	DescribeCoverageURL = FindOperationURL(Root, TEXT("DescribeCoverage"));
	if (DescribeCoverageURL.IsEmpty()) DescribeCoverageURL = RequestURL;

	if (RequestURL.IsEmpty())
	{
		LCReporter::ShowError(LOCTEXT("FOGCProvider::LoadWCSCapabilities::RequestURL", "Could not find the GetCoverage URL in the XML capabilities."));
		return false;
	}
	return true;
}

// Describes the coverages by groups, to keep the URLs short when a server has many coverages
void FOGCProvider::DescribeWCSCoverages(int32 StartIndex, TArray<FString> ExcludeCRS, TFunction<void(bool)> OnComplete)
{
	constexpr int32 GroupSize = 40;

	if (StartIndex >= Names.Num())
	{
		// Remove the coverages in an excluded CRS
		for (int32 Index = Names.Num() - 1; Index >= 0; Index--)
		{
			if (!ExcludeCRS.Contains(CRSs[Index])) continue;
			Names.RemoveAt(Index);
			Titles.RemoveAt(Index);
			Abstracts.RemoveAt(Index);
			CRSs.RemoveAt(Index);
			MinXs.RemoveAt(Index);
			MinYs.RemoveAt(Index);
			MaxXs.RemoveAt(Index);
			MaxYs.RemoveAt(Index);
			XAxisLabels.RemoveAt(Index);
			YAxisLabels.RemoveAt(Index);
		}
		if (OnComplete) OnComplete(!Names.IsEmpty());
		return;
	}

	const int32 EndIndex = FMath::Min(StartIndex + GroupSize, Names.Num());
	TArray<FString> Group;
	for (int32 Index = StartIndex; Index < EndIndex; Index++) Group.Add(Names[Index]);

	const FString URL = DescribeCoverageURL + TEXT("SERVICE=WCS&VERSION=2.0.1&REQUEST=DescribeCoverage&COVERAGEID=") + FString::Join(Group, TEXT(","));
	const FString DownloadDir = Directories::DownloadDir();
	if (DownloadDir.IsEmpty())
	{
		if (OnComplete) OnComplete(false);
		return;
	}
	const FString File = FPaths::Combine(DownloadDir, FString::Format(TEXT("describe_coverage_{0}.xsd"), { FTextLocalizationResource::HashString(URL) }));

	Download::FromURL(URL, File, true,
		[this, File, URL, EndIndex, ExcludeCRS, OnComplete](bool bSuccess)
		{
			if (!bSuccess)
			{
				LCReporter::ShowError(FText::Format(
					LOCTEXT("FOGCProvider::DescribeWCSCoverages::Download", "Error while downloading {0}."),
					FText::FromString(URL)
				));
				if (OnComplete) OnComplete(false);
				return;
			}
			if (!LoadWCSDescriptions(File))
			{
				if (OnComplete) OnComplete(false);
				return;
			}
			DescribeWCSCoverages(EndIndex, ExcludeCRS, OnComplete);
		});
}

bool FOGCProvider::LoadWCSDescriptions(const FString& File)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *File))
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("FOGCProvider::LoadWCSDescriptions::Read", "Could not read file {0}."),
			FText::FromString(File)
		));
		return false;
	}

	FXmlFile Xml(Content, EConstructMethod::ConstructFromBuffer);
	if (!Xml.IsValid())
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("FOGCProvider::LoadWCSDescriptions::Xml", "Could not parse the WCS coverage descriptions in {0}: {1}"),
			FText::FromString(File), FText::FromString(Xml.GetLastError())
		));
		return false;
	}

	for (const FXmlNode* Description : Xml.GetRootNode()->GetChildrenNodes())
	{
		if (LocalName(Description->GetTag()) != TEXT("CoverageDescription")) continue;

		FString Id = ChildText(Description, TEXT("CoverageId"));
		if (Id.IsEmpty()) Id = GetAttribute(Description, TEXT("id"));
		const int32 Index = Names.IndexOfByKey(Id);
		if (Index == INDEX_NONE) continue;

		const FXmlNode* Envelope = FindChild(FindChild(Description, TEXT("boundedBy")), TEXT("Envelope"));
		if (!Envelope) continue;

		double Lower0, Lower1, Upper0, Upper1;
		if (!ParseCorner(ChildText(Envelope, TEXT("lowerCorner")), Lower0, Lower1)) continue;
		if (!ParseCorner(ChildText(Envelope, TEXT("upperCorner")), Upper0, Upper1)) continue;

		// The coordinates follow the order of the axis labels. The labels are used in the requests, and "X" is the east-west axis.
		TArray<FString> Labels;
		GetAttribute(Envelope, TEXT("axisLabels")).ParseIntoArrayWS(Labels);
		if (Labels.Num() < 2)
		{
			Labels.Empty();
			Labels.Add("x");
			Labels.Add("y");
		}
		const bool bSwapped = IsYAxisLabel(Labels[0]) && !IsYAxisLabel(Labels[1]);

		CRSs[Index] = ToEPSG(GetAttribute(Envelope, TEXT("srsName")));
		MinXs[Index] = bSwapped ? Lower1 : Lower0;
		MaxXs[Index] = bSwapped ? Upper1 : Upper0;
		MinYs[Index] = bSwapped ? Lower0 : Lower1;
		MaxYs[Index] = bSwapped ? Upper0 : Upper1;
		XAxisLabels[Index] = bSwapped ? Labels[1] : Labels[0];
		YAxisLabels[Index] = bSwapped ? Labels[0] : Labels[1];
	}

	return true;
}

bool FOGCProvider::CreateWCSURL(
	int Width, int Height, FString Name,
	double MinLong, double MaxLong, double MinLat, double MaxLat,
	FString &URL, bool &bGeoTiff, FString &FileExt
)
{
	const int32 Index = Names.IndexOfByKey(Name);
	const FString XAxis = XAxisLabels.IsValidIndex(Index) ? XAxisLabels[Index] : FString("x");
	const FString YAxis = YAxisLabels.IsValidIndex(Index) ? YAxisLabels[Index] : FString("y");

	// The subset is in the CRS of the coverage, and the size of the image in pixels is given with scalesize
	URL = FString::Format(
		TEXT("{0}SERVICE=WCS&VERSION=2.0.1&REQUEST=GetCoverage&COVERAGEID={1}&FORMAT={2}&SUBSET={3}({4},{5})&SUBSET={6}({7},{8})&SCALESIZE={3}({9}),{6}({10})"),
		{ RequestURL, Name, ImageFormat, XAxis, MinLong, MaxLong, YAxis, MinLat, MaxLat, Width, Height }
	);

	bGeoTiff = true;
	FileExt = "tif";
	return true;
}

#undef LOCTEXT_NAMESPACE
