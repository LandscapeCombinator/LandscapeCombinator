// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/ImageDownloader.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/HMDebugFetcher.h"

#include "ImageDownloader/Downloaders/HMLocalFile.h"
#include "ImageDownloader/Downloaders/HMLocalFolder.h"
#include "ImageDownloader/Downloaders/HMURL.h"

#include "ImageDownloader/Downloaders/HMLitto3DGuadeloupe.h"
#include "ImageDownloader/Downloaders/HMViewfinder15Downloader.h"
#include "ImageDownloader/Downloaders/HMViewfinderDownloader.h"
#include "ImageDownloader/Downloaders/HMListDownloader.h"

#include "ImageDownloader/Transformers/HMSwissALTI3DRenamer.h"
#include "ImageDownloader/Transformers/HMLitto3DGuadeloupeRenamer.h"
#include "ImageDownloader/Transformers/HMViewfinder15Renamer.h"

#include "ImageDownloader/Transformers/HMDegreeFilter.h"
#include "ImageDownloader/Transformers/HMDegreeRenamer.h"
#include "ImageDownloader/Transformers/HMPreprocess.h"
#include "ImageDownloader/Transformers/HMResolution.h"
#include "ImageDownloader/Transformers/HMReproject.h"
#include "ImageDownloader/Transformers/HMEnsureOneBand.h"
#include "ImageDownloader/Transformers/HMCrop.h"
#include "ImageDownloader/Transformers/HMToPNG.h"
#include "ImageDownloader/Transformers/HMReadCRS.h"
#include "ImageDownloader/Transformers/HMWriteCRS.h"
#include "ImageDownloader/Transformers/HMConvert.h"
#include "ImageDownloader/Transformers/HMAddMissingTiles.h"
#include "ImageDownloader/Transformers/HMFunction.h"

#include "Coordinates/LevelCoordinates.h"
#include "HAL/FileManagerGeneric.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"



UImageDownloader::UImageDownloader()
{
	PreprocessingTool = CreateDefaultSubobject<UExternalTool>(TEXT("Preprocessing Tool"));
}

TArray<FString> UImageDownloader::GetTitles()
{
	return WMSProvider.Titles;
}

FString RenameCRS(FString CRS)
{
	if (CRS == "IGNF:UTM20W84GUAD")
	{
		return "EPSG:4559";
	}
	else
	{
		return CRS;
	}
}


HMFetcher* UImageDownloader::CreateInitialFetcher(FString Name)
{
	switch (ImageSourceKind)
	{
		case EImageSourceKind::LocalFile:
		{
			return new HMDebugFetcher("LocalFile", new HMLocalFile(LocalFilePath, CRS), true);
		}

		case EImageSourceKind::LocalFolder:
		{
			return new HMDebugFetcher("LocalFolder", new HMLocalFolder(LocalFolderPath, CRS), true);
		}

		case EImageSourceKind::URL:
		{
			return new HMDebugFetcher("URL", new HMURL(URL, Name + ".tif", CRS), true);
		}

		case EImageSourceKind::Litto3D_Guadeloupe:
		{
			HMFetcher *Fetcher1 = new HMDebugFetcher("Litto3DGuadeloupe", new HMLitto3DGuadeloupe(Litto3D_Folder, bUse5mData, bSkipExtraction), true);
			HMFetcher *Fetcher2 = new HMDebugFetcher("Litto3DGuadeloupeRenamer", new HMLitto3DGuadeloupeRenamer(Name));
			return Fetcher1->AndThen(Fetcher2);
		}

		case EImageSourceKind::Viewfinder15:
		{
			HMFetcher *Fetcher1 = new HMDebugFetcher("ViewfinderDownloader", new HMViewfinderDownloader(Viewfinder_TilesString, "http://www.viewfinderpanoramas.org/DEM/TIF15/", true), true);
			HMFetcher *Fetcher2 = new HMDebugFetcher("Viewfinder15Renamer", new HMViewfinder15Renamer(Name));
			return Fetcher1->AndThen(Fetcher2);
		}

		case EImageSourceKind::Viewfinder3:
		{
			HMFetcher *Result = new HMDebugFetcher("ViewfinderDownloader", new HMViewfinderDownloader(Viewfinder_TilesString, "http://viewfinderpanoramas.org/dem3/", false), true);
			if (bFilterDegrees)
			{
				Result = Result->AndThen(new HMDebugFetcher("DegreeFilter", new HMDegreeFilter(Name, FilterMinLong, FilterMaxLong, FilterMinLat, FilterMaxLat)));
			}
			Result = Result->AndThen(new HMDebugFetcher("Convert", new HMConvert(Name, "tif")));
			Result = Result->AndThen(new HMDebugFetcher("DegreeRenamer", new HMDegreeRenamer(Name)));
			return Result;
		}

		case EImageSourceKind::Viewfinder1:
		{
			HMFetcher *Result = new HMDebugFetcher("ViewfinderDownloader", new HMViewfinderDownloader(Viewfinder_TilesString, "http://viewfinderpanoramas.org/dem1/", false), true);
			if (bFilterDegrees)
			{
				Result = Result->AndThen(new HMDebugFetcher("DegreeFilter", new HMDegreeFilter(Name, FilterMinLong, FilterMaxLong, FilterMinLat, FilterMaxLat)));
			}
			Result = Result->AndThen(new HMDebugFetcher("Convert", new HMConvert(Name, "tif")));
			Result = Result->AndThen(new HMDebugFetcher("DegreeRenamer", new HMDegreeRenamer(Name)));
			return Result;
		}

		case EImageSourceKind::SwissALTI_3D:
		{
			HMFetcher *Fetcher1 = new HMDebugFetcher("ListDownloader", new HMListDownloader(SwissALTI3D_ListOfLinks), true);
			HMFetcher *Fetcher2 = new HMDebugFetcher("ReadCRS", new HMReadCRS());
			HMFetcher *Fetcher3 = new HMDebugFetcher("SwissALTI3DRenamer", new HMSwissALTI3DRenamer(Name));
			return Fetcher1->AndThen(Fetcher2)->AndThen(Fetcher3);
		}

		case EImageSourceKind::USGS_OneThird:
		{
			HMFetcher *Fetcher1 = new HMDebugFetcher("ListDownloader", new HMListDownloader(USGS_OneThird_ListOfLinks), true);
			HMFetcher *Fetcher2 = new HMDebugFetcher("ReadCRS", new HMReadCRS());
			HMFetcher *Fetcher3 = new HMDebugFetcher("DegreeRenamer", new HMDegreeRenamer(Name));
			return Fetcher1->AndThen(Fetcher2)->AndThen(Fetcher3);
		}

		default:
		{
			if (IsWMS())
			{
				bool bGeoTiff;
				FString QueryURL, FileExt;
				bool bSuccess = WMSProvider.CreateURL(
					WMS_Width, WMS_Height,
					WMS_Name, WMS_CRS,
					WMS_MinAllowedLong, WMS_MaxAllowedLong, WMS_MinAllowedLat, WMS_MaxAllowedLat,
					WMS_MinLong, WMS_MaxLong, WMS_MinLat, WMS_MaxLat,
					QueryURL, bGeoTiff, FileExt
				);

				if (!bSuccess) return nullptr;

				uint32 Hash = FTextLocalizationResource::HashString(QueryURL);
				FString FileName = FString::Format(TEXT("WMS_{0}.{1}"), { Hash, FileExt });
				
				HMFetcher *WMSFetcher= new HMURL(QueryURL, FileName, RenameCRS(WMS_CRS));

				if (!WMSFetcher) return nullptr;

				HMFetcher* Result = new HMDebugFetcher("WMS_Download", WMSFetcher, true);

				if (!bGeoTiff)
				{
					Result = Result->AndThen(new HMDebugFetcher("WMS_WriteCRS", new HMWriteCRS(Name, WMS_Width, WMS_Height, WMS_MinLong, WMS_MaxLong, WMS_MinLat, WMS_MaxLat)));
				}

				return Result;
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok,
					FText::Format(
						LOCTEXT("InterfaceFromKindError", "Internal error: heightmap kind '{0}' is not supprted."),
						FText::AsNumber((int) ImageSourceKind)
					)
				); 
				return nullptr;
			}
		}
	}
}

HMFetcher* UImageDownloader::CreateFetcher(FString Name, bool bEnsureOneBand, bool bScaleAltitude, bool bConvertToPNG, FVector4d CropCoordinates, FIntPoint CropPixels, TFunction<bool(HMFetcher*)> RunBeforePNG)
{
	HMFetcher *Result = CreateInitialFetcher(Name);
	if (!Result) return nullptr;
	
	if (bRemap)
	{
		Result = Result->AndThen(new HMDebugFetcher("Convert", new HMConvert(Name, "tif")));
		Result = Result->AndThen(new HMDebugFetcher("FixNoData", new HMFunction([this](float x) { return x == OriginalValue ? TransformedValue : x; })));
	}
	
	if (bPreprocess)
	{
		Result = Result->AndThen(new HMDebugFetcher("Preprocess", new HMPreprocess(Name, PreprocessingTool)));
	}

	if (bEnsureOneBand && IsWMS())
	{
		Result = Result->AndThen(new HMDebugFetcher("EnsureOneBand", new HMEnsureOneBand()));
	}

	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(this->GetWorld(), false);

	if (GlobalCoordinates)
	{
		Result = Result->AndThen(new HMDebugFetcher("Reproject", new HMReproject(Name, GlobalCoordinates->CRS)));
	}

	if (RunBeforePNG)
	{
		Result = Result->AndRun(RunBeforePNG);
	}

	if (CropCoordinates != FVector4d::Zero())
	{
		Result = Result->AndThen(new HMDebugFetcher("FitToLandscape", new HMCrop(Name, CropCoordinates, CropPixels)));
	}
	
	if (bConvertToPNG)
	{
		Result = Result->AndThen(new HMDebugFetcher("ToPNG", new HMToPNG(Name, bScaleAltitude)));
		Result = Result->AndThen(new HMDebugFetcher("AddMissingTiles", new HMAddMissingTiles()));
	}

	if (bChangeResolution)
	{
		Result = Result->AndThen(new HMDebugFetcher("Resolution", new HMResolution(Name, PrecisionPercent)));
	}

	return Result;
}

void UImageDownloader::DeleteAllImages()
{
	FString ImageDownloaderDir = Directories::ImageDownloaderDir();
	if (!ImageDownloaderDir.IsEmpty())
	{
		if (!IPlatformFile::GetPlatformPhysical().DeleteDirectoryRecursively(*ImageDownloaderDir))
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("UImageDownloader::DeleteAllImages", "Could not delete all the files in {0}"),
					FText::FromString(ImageDownloaderDir)
				)
			);
			return;
		}
	}

	FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Deleting", "Finished Deleting files."));
}

void UImageDownloader::DeleteAllProcessedImages()
{
	TArray<FString> Files;
	TArray<FString> Folders;
	IPlatformFile &PlatformFile = IPlatformFile::GetPlatformPhysical();

	FString ImageDownloaderDir = Directories::ImageDownloaderDir();

	UE_LOG(LogImageDownloader, Log, TEXT("DeleteAllProcessedImages"));

	if (!ImageDownloaderDir.IsEmpty())
	{
		FFileManagerGeneric::Get().FindFiles(Files, *FPaths::Combine(ImageDownloaderDir, FString("*")), true, false);
		UE_LOG(LogImageDownloader, Log, TEXT("Deleting %d files"), Files.Num());
		for (auto& File0 : Files)
		{
			FString File = FPaths::Combine(ImageDownloaderDir, File0);
			if (!IPlatformFile::GetPlatformPhysical().DeleteFile(*File))
			{
				FMessageDialog::Open(EAppMsgType::Ok,
					FText::Format(
						LOCTEXT("UImageDownloader::DeleteAllProcessedImages::Files", "Could not delete file {0}"),
						FText::FromString(File)
					)
				);
				return;
			}
		}
		
		FFileManagerGeneric::Get().FindFiles(Folders, *FPaths::Combine(ImageDownloaderDir, FString("*")), false, true);
		UE_LOG(LogImageDownloader, Log, TEXT("Deleting %d folders"), Folders.Num());
		for (auto& Folder0 : Folders)
		{
			if (!Folder0.Equals("Download"))
			{
				FString Folder = FPaths::Combine(ImageDownloaderDir, Folder0);
				UE_LOG(LogImageDownloader, Log, TEXT("Deleting folder %s."), *Folder);
				if (!IPlatformFile::GetPlatformPhysical().DeleteDirectoryRecursively(*Folder))
				{
					FMessageDialog::Open(EAppMsgType::Ok,
						FText::Format(
							LOCTEXT("UImageDownloader::DeleteAllProcessedImages::Folders", "Could not delete folder {0}"),
							FText::FromString(Folder)
						)
					);
					return;
				}
			}
		}
	}

	FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("UImageDownloader::DeleteAllProcessedImages::OK", "Finished Deleting files."));
}

bool UImageDownloader::IsWMS()
{
	return
		ImageSourceKind == EImageSourceKind::OpenStreetMap_FR ||
		ImageSourceKind == EImageSourceKind::Terrestris_OSM ||
		ImageSourceKind == EImageSourceKind::GenericWMS ||
		ImageSourceKind == EImageSourceKind::IGN ||
		ImageSourceKind == EImageSourceKind::SHOM ||
		ImageSourceKind == EImageSourceKind::USGS_Imagery ||
		ImageSourceKind == EImageSourceKind::USGS_Topo ||
		ImageSourceKind == EImageSourceKind::USGS_3DEPElevation;
}

bool UImageDownloader::HasMultipleLayers()
{
	return WMSProvider.Titles.Num() >= 2;
}

void UImageDownloader::SetLargestPossibleCoordinates()
{
	WMS_MinLong = WMS_MinAllowedLong;
	WMS_MinLat = WMS_MinAllowedLat;
	WMS_MaxLong = WMS_MaxAllowedLong;
	WMS_MaxLat = WMS_MaxAllowedLat;
}

void UImageDownloader::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	ON_SCOPE_EXIT {
		Super::PostEditChangeProperty(Event);
	};

	if (!Event.Property)
	{
		return;
	}

	FName PropertyName = Event.Property->GetFName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UImageDownloader, ImageSourceKind))
	{
		OnImageSourceChanged(nullptr);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UImageDownloader, CapabilitiesURL))
	{
		ResetWMSProvider(TArray<FString>(), nullptr, nullptr);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UImageDownloader, WMS_Title))
	{
		OnLayerChanged();
	}

	Super::PostEditChangeProperty(Event);
}


void UImageDownloader::OnLayerChanged()
{
	int LayerIndex = WMSProvider.Titles.Find(WMS_Title);
	if (LayerIndex == INDEX_NONE)
	{
		WMS_Help = FString::Format(TEXT("Could not find Layer {0}"), { WMS_Title });
		return;
	}
	WMS_Name = WMSProvider.Names[LayerIndex];
	WMS_Help = "Please enter the coordinates using the following CRS:";
	WMS_CRS = WMSProvider.CRSs[LayerIndex];
	WMS_MinAllowedLong = WMSProvider.MinXs[LayerIndex];
	WMS_MaxAllowedLong = WMSProvider.MaxXs[LayerIndex];
	WMS_MinAllowedLat = WMSProvider.MinYs[LayerIndex];
	WMS_MaxAllowedLat = WMSProvider.MaxYs[LayerIndex];
	WMS_SearchCRS = FString::Format(TEXT("https://duckduckgo.com/?q={0}+site%3Aepsg.io"), { WMS_CRS });
	WMS_Abstract = WMSProvider.Abstracts[LayerIndex];
}

void UImageDownloader::OnImageSourceChanged(TFunction<void(bool)> OnComplete)
{
	if (ImageSourceKind == EImageSourceKind::IGN)
	{
		CapabilitiesURL = "https://wxs.ign.fr/altimetrie/geoportail/r/wms?SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";
		ResetWMSProvider(TArray<FString>(), nullptr, OnComplete);
	}
	else if (ImageSourceKind == EImageSourceKind::SHOM)
	{
		CapabilitiesURL = "https://services.data.shom.fr/INSPIRE/wms/r?service=WMS&version=1.3.0&request=GetCapabilities";
		ResetWMSProvider(TArray<FString>(), nullptr, OnComplete);
	}
	else if (ImageSourceKind == EImageSourceKind::USGS_3DEPElevation)
	{
		CapabilitiesURL = "https://elevation.nationalmap.gov/arcgis/services/3DEPElevation/ImageServer/WMSServer?request=GetCapabilities&service=WMS";
		ResetWMSProvider(TArray<FString>(), nullptr, OnComplete);
	}
	else if (ImageSourceKind == EImageSourceKind::USGS_Topo)
	{
		CapabilitiesURL = "https://basemap.nationalmap.gov/arcgis/services/USGSTopo/MapServer/WMSServer?request=GetCapabilities&service=WMS";
		ResetWMSProvider(TArray<FString>(), nullptr, OnComplete);
	}
	else if (ImageSourceKind == EImageSourceKind::USGS_Imagery)
	{
		CapabilitiesURL = "https://basemap.nationalmap.gov/arcgis/services/USGSImageryOnly/MapServer/WMSServer?request=GetCapabilities&service=WMS";
		ResetWMSProvider(TArray<FString>(), nullptr, OnComplete);
	}
	else if (ImageSourceKind == EImageSourceKind::OpenStreetMap_FR)
	{
		CapabilitiesURL = "https://wms.openstreetmap.fr/wms?request=GetCapabilities&service=WMS";
		ResetWMSProvider(TArray<FString>(), nullptr, OnComplete);
	}
	else if (ImageSourceKind == EImageSourceKind::Terrestris_OSM)
	{
		CapabilitiesURL = "https://ows.terrestris.de/osm/service?SERVICE=WMS&VERSION=1.1.1&REQUEST=GetCapabilities";
		ResetWMSProvider(TArray<FString>(), nullptr, OnComplete);
	}
}

void UImageDownloader::ResetWMSProvider(TArray<FString> ExcludeCRS, TFunction<bool(FString)> LayerFilter, TFunction<void(bool)> OnComplete)
{
	if (!CapabilitiesURL.IsEmpty())
	{
		WMSProvider.SetFromURL(
			CapabilitiesURL,
			ExcludeCRS,
			LayerFilter,
			[this, OnComplete](bool bSuccess)
			{
				WMS_Abstract = "";
				WMS_Title = "";
				WMS_Name = "";
				WMS_Help = "";
				WMS_CRS = "";
				WMS_SearchCRS = "";
				WMS_MinAllowedLong = 0;
				WMS_MinAllowedLat = 0;
				WMS_MaxAllowedLat = 0;
				WMS_MaxAllowedLong = 0;
				WMS_MaxWidth = 0;
				WMS_MaxHeight = 0;

				if (!bSuccess)
				{
					WMS_Help = FString::Format(TEXT("There was an error while loading WMS Provider from URL: {0}"), { CapabilitiesURL });
				}
				
				WMS_MaxWidth = WMSProvider.MaxWidth;
				WMS_MaxHeight = WMSProvider.MaxHeight;

				if (!WMSProvider.Titles.Contains(WMS_Title) && !WMSProvider.Titles.IsEmpty())
				{
					WMS_Title = WMSProvider.Titles[0];
					OnLayerChanged();
				}

				if (OnComplete) OnComplete(bSuccess);
			}
		);
	}
}


#undef LOCTEXT_NAMESPACE