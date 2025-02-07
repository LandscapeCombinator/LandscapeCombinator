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
#include "ImageDownloader/Downloaders/HMXYZ.h"
#include "ImageDownloader/Downloaders/HMNapoli.h"
#include "ImageDownloader/Downloaders/HMWMS.h"

#include "ImageDownloader/Transformers/HMSwissALTI3DRenamer.h"
#include "ImageDownloader/Transformers/HMLitto3DGuadeloupeRenamer.h"
#include "ImageDownloader/Transformers/HMViewfinder15Renamer.h"

#include "ImageDownloader/Transformers/HMDecodeMapbox.h"
#include "ImageDownloader/Transformers/HMDegreeFilter.h"
#include "ImageDownloader/Transformers/HMDegreeRenamer.h"
#include "ImageDownloader/Transformers/HMPreprocess.h"
#include "ImageDownloader/Transformers/HMResolution.h"
#include "ImageDownloader/Transformers/HMReproject.h"
#include "ImageDownloader/Transformers/HMEnsureOneBand.h"
#include "ImageDownloader/Transformers/HMCrop.h"
#include "ImageDownloader/Transformers/HMToPNG.h"
#include "ImageDownloader/Transformers/HMMerge.h"
#include "ImageDownloader/Transformers/HMReadCRS.h"
#include "ImageDownloader/Transformers/HMConvert.h"
#include "ImageDownloader/Transformers/HMAddMissingTiles.h"
#include "ImageDownloader/Transformers/HMFunction.h"

#include "LCCommon/LCReporter.h"
#include "LCCommon/LCSettings.h"
#include "Coordinates/LevelCoordinates.h"
#include "LandscapeUtils/LandscapeUtils.h"

#include "HAL/FileManagerGeneric.h"
#include "Misc/MessageDialog.h"
#include "Logging/StructuredLog.h"
#include "Internationalization/TextLocalizationResource.h" 

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"


UImageDownloader::UImageDownloader()
{
	PreprocessingTool = CreateDefaultSubobject<UExternalTool>(TEXT("Preprocessing Tool"));
}

FString RenameCRS(FString CRS)
{
	if (CRS == "IGNF:UTM20W84GUAD")
	{
		return "EPSG:4559";
	}
	else if (CRS == "IGNF:RGR92UTM40S")
	{
		return "EPSG:2975";
	}
	else
	{
		return CRS;
	}
}

TArray<FString> UImageDownloader::GetTitles()
{
	return WMS_Provider.Titles;
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

		case EImageSourceKind::Napoli:
		{
			if (!SetSourceParametersBool(true))
			{
				return nullptr;
			}

			return new HMDebugFetcher("Napoli", new HMNapoli(Napoli_MinLong, Napoli_MaxLong, Napoli_MinLat, Napoli_MaxLat), true);
		}

		default:
		{
			if (IsWMS())
			{
				if (!SetSourceParametersBool(true))
				{
					return nullptr;
				}

				AutoSetResolution();

				HMFetcher *WMSFetcher = new HMWMS(
					WMS_Provider,
					WMS_MaxTileWidth, WMS_MaxTileHeight,
					WMS_Name, WMS_CRS, WMS_X_IsLong,
					WMS_MinAllowedLong, WMS_MaxAllowedLong, WMS_MinAllowedLat, WMS_MaxAllowedLat,
					WMS_MinLong, WMS_MaxLong, WMS_MinLat, WMS_MaxLat,
					bWMSSingleTile, WMS_ResolutionPixelsPerUnit
				);

				if (!WMSFetcher) return nullptr;

				return new HMDebugFetcher("WMS_Download", WMSFetcher, true);
			}
			else if (IsXYZ())
			{

				if (!SetSourceParametersBool(true))
				{
					return nullptr;
				}
				FString Layer, Format;
				FString URL2;
				bool bGeoreferenceSlippyTiles2;
				bool bMaxY_IsNorth2;


				if (IsMapbox())
				{
					if (!ULCReporter::ShowMapboxFreeTierWarning()) return nullptr;

					FString MapboxToken2 = GetMapboxToken();
					if (MapboxToken2.IsEmpty())
					{
						ULCReporter::ShowError(
							LOCTEXT("MapboxTokenMissing", "Please add a Mapbox Token (can be obtained from a free Mapbox account) in your Editor Preferences or in the Details Panel.")
						); 
						return nullptr;
					}

					if (ImageSourceKind == EImageSourceKind::Mapbox_Heightmaps)
					{
						Layer = "MapboxTerrainDEMV1";
						Format = "png";
						if (Mapbox_2x)
						{
							URL2 = FString("https://api.mapbox.com/v4/mapbox.mapbox-terrain-dem-v1/{z}/{x}/{y}@2x.pngraw?access_token=") + MapboxToken2;
						}
						else
						{
							URL2 = FString("https://api.mapbox.com/v4/mapbox.mapbox-terrain-dem-v1/{z}/{x}/{y}.pngraw?access_token=") + MapboxToken2;
						}

						bGeoreferenceSlippyTiles2 = true;
						bMaxY_IsNorth2 = false;
					}
					else if (ImageSourceKind == EImageSourceKind::Mapbox_Satellite)
					{
						Layer = "MapboxSatellite";
						Format = "jpg";
						if (Mapbox_2x)
						{
							URL2 = FString("https://api.mapbox.com/v4/mapbox.satellite/{z}/{x}/{y}@2x.jpg90?access_token=") + MapboxToken2;
						}
						else
						{
							URL2 = FString("https://api.mapbox.com/v4/mapbox.satellite/{z}/{x}/{y}.jpg90?access_token=") + MapboxToken2;
						}

						bGeoreferenceSlippyTiles2 = true;
						bMaxY_IsNorth2 = false;
					}
					else
					{
						return nullptr;
					}
				}
				else if (IsMapTiler())
				{
					if (!ULCReporter::ShowMapTilerFreeTierWarning()) return nullptr;

					FString MapTilerToken2 = GetMapTilerToken();
					if (MapTilerToken2.IsEmpty())
					{
						ULCReporter::ShowError(
							LOCTEXT("MapTilerTokenMissing", "Please add a MapTiler Token (can be obtained from a free MapTiler account) in your Editor Preferences or in the Details Panel.")
						); 
						return nullptr;
					}

					if (ImageSourceKind == EImageSourceKind::MapTiler_Heightmaps)
					{
						Layer = "MapTilerTerrainRGB";
						Format = "webp";
						URL2 = FString("https://api.maptiler.com/tiles/terrain-rgb-v2/{z}/{x}/{y}.webp?key=") + MapTilerToken2;

						bGeoreferenceSlippyTiles2 = true;
						bMaxY_IsNorth2 = false;
					}
					else if (ImageSourceKind == EImageSourceKind::MapTiler_Satellite)
					{
						Layer = "MapTilerSatellite";
						Format = "jpg";
						URL2 = FString("https://api.maptiler.com/tiles/satellite-v2/{z}/{x}/{y}.jpg?key=") + MapTilerToken2;

						bGeoreferenceSlippyTiles2 = true;
						bMaxY_IsNorth2 = false;
					}
					else
					{
						return nullptr;
					}
				}
				else if (IsNextZen())
				{
					FString NextZenToken2 = GetNextZenToken();
					if (NextZenToken2.IsEmpty())
					{
						ULCReporter::ShowError(
							LOCTEXT("NextZenTokenMissing", "Please add a NextZen Token (can be obtained from a free NextZen account) in your Editor Preferences or in the Details Panel.")
						); 
						return nullptr;
					}

					if (ImageSourceKind == EImageSourceKind::NextZen_Heightmaps)
					{
						Layer = "NextZenTerrainRGB";
						Format = "png";
						URL2 = FString("https://tile.nextzen.org/tilezen/terrain/v1/256/terrarium/{z}/{x}/{y}.png?api_key=") + NextZenToken2;

						bGeoreferenceSlippyTiles2 = true;
						bMaxY_IsNorth2 = false;
					}
					else
					{
						return nullptr;
					}
				}
				else
				{
					Layer = XYZ_Name;
					Format = XYZ_Format;
					URL2 = XYZ_URL;
					bGeoreferenceSlippyTiles2 = bGeoreferenceSlippyTiles;
					bMaxY_IsNorth2 = bMaxY_IsNorth;
				}
				
				HMFetcher *Result = new HMDebugFetcher(
					"XYZ_Download",
					new HMXYZ(
						bAllowInvalidTiles,
						Name, Layer, Format, URL2, XYZ_Zoom, XYZ_MinX, XYZ_MaxX, XYZ_MinY, XYZ_MaxY,
						bMaxY_IsNorth2, bGeoreferenceSlippyTiles2,
							ImageSourceKind == EImageSourceKind::MapTiler_Heightmaps ||
							ImageSourceKind == EImageSourceKind::Mapbox_Heightmaps ||
							ImageSourceKind == EImageSourceKind::NextZen_Heightmaps,
							ImageSourceKind == EImageSourceKind::NextZen_Heightmaps,
						XYZ_CRS
					),
					true
				);

				return Result;
			}
			else
			{
				ULCReporter::ShowError(
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

HMFetcher* UImageDownloader::CreateFetcher(
	FString Name, bool bEnsureOneBand, bool bScaleAltitude, bool bConvertToPNG,
	TFunction<bool(HMFetcher*)> RunBeforePNG, TObjectPtr<UGlobalCoordinates> GlobalCoordinates
)
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

	if (GlobalCoordinates)
	{
		Result = Result->AndThen(new HMDebugFetcher("Reproject", new HMReproject(Name, GlobalCoordinates->CRS)));
	}

	if (bAdaptResolution || bCropCoordinates)
	{
		FIntPoint ImageSize(0, 0);

#if WITH_EDITOR
		if (bAdaptResolution)
		{
			if (!TargetLandscape)
			{
				ULCReporter::ShowError(
					LOCTEXT("UImageDownloader::CreateFetcher::NoLandscape", "Please select a target landscape if you want to resize the image.")
				);
				return nullptr;
			}

			ImageSize.X = TargetLandscape->ComputeComponentCounts().X * TargetLandscape->ComponentSizeQuads + 1;
			ImageSize.Y = TargetLandscape->ComputeComponentCounts().Y * TargetLandscape->ComponentSizeQuads + 1;
		}
#else
		if (bAdaptResolution)
		{
			UE_LOG(LogImageDownloader, Error, TEXT("Adapt Resolution is only supported in editor mode"));
			return nullptr;
		}

#endif

		FVector4d Coordinates(0, 0, 0, 0);
		if (bCropCoordinates)
		{
			if (!IsValid(CroppingActor))
			{
				ULCReporter::ShowError(
					LOCTEXT("UImageDownloader::CreateFetcher::NoActor", "Please select a Cropping Actor if you want to crop the output image.")
				);
				return nullptr;
			}

			if (!ALevelCoordinates::GetActorCRSBounds(CroppingActor, Coordinates))
			{
				ULCReporter::ShowError(FText::Format(
					LOCTEXT("UImageDownloader::CreateFetcher::NoCoordinates", "Could not compute bounding coordinates of Actor {0}"),
					FText::FromString(CroppingActor->GetActorNameOrLabel())
				));
				return nullptr;
			}
		}

		Result = Result->AndThen(new HMDebugFetcher("AdaptImage", new HMCrop(Name, Coordinates, ImageSize)));
	}

	if (bMergeImages)
	{
		Result = Result->AndThen(new HMDebugFetcher("Merge", new HMMerge(Name)));
	}

	if (RunBeforePNG)
	{
		Result = Result->AndRun(RunBeforePNG);
	}
	
	if (bConvertToPNG)
	{
		Result = Result->AndThen(new HMDebugFetcher("ToPNG", new HMToPNG(Name, bScaleAltitude)));
		Result = Result->AndThen(new HMDebugFetcher("AddMissingTiles", new HMAddMissingTiles()));
	}

	if (bScaleResolution)
	{
		Result = Result->AndThen(new HMDebugFetcher("Resolution", new HMResolution(Name, PrecisionPercent)));
	}

	return Result;
}

bool UImageDownloader::HasMapTilerToken()
{
	return !GetDefault<ULCSettings>()->MapTiler_Token.IsEmpty();
}

bool UImageDownloader::HasMapboxToken()
{
	return !GetDefault<ULCSettings>()->Mapbox_Token.IsEmpty();
}

bool UImageDownloader::HasNextZenToken()
{
	return !GetDefault<ULCSettings>()->NextZen_Token.IsEmpty();
}

FString UImageDownloader::GetMapTilerToken()
{
	FString ProjectSettingsMapTilerToken = GetDefault<ULCSettings>()->MapTiler_Token;
	if (!ProjectSettingsMapTilerToken.IsEmpty())
	{
		return ProjectSettingsMapTilerToken;
	}
	else
	{
		return MapTiler_Token;
	}
}

FString UImageDownloader::GetMapboxToken()
{
	FString ProjectSettingsMapboxToken = GetDefault<ULCSettings>()->Mapbox_Token;
	if (!ProjectSettingsMapboxToken.IsEmpty())
	{
		return ProjectSettingsMapboxToken;
	}
	else
	{
		return Mapbox_Token;
	}
}

FString UImageDownloader::GetNextZenToken()
{
	FString ProjectSettingsNextZenToken = GetDefault<ULCSettings>()->NextZen_Token;
	if (!ProjectSettingsNextZenToken.IsEmpty())
	{
		return ProjectSettingsNextZenToken;
	}
	else
	{
		return NextZen_Token;
	}
}

void UImageDownloader::DeleteAllImages()
{
	FString ImageDownloaderDir = Directories::ImageDownloaderDir();
	if (!ImageDownloaderDir.IsEmpty())
	{
		if (!IPlatformFile::GetPlatformPhysical().DeleteDirectoryRecursively(*ImageDownloaderDir))
		{
			ULCReporter::ShowError(
				FText::Format(
					LOCTEXT("UImageDownloader::DeleteAllImages", "Could not delete all the files in {0}"),
					FText::FromString(ImageDownloaderDir)
				)
			);
			return;
		}
	}

	ULCReporter::ShowInfo(
		LOCTEXT("FinishedDeletingFiles", "Finished Deleting files."),
		"SuppressFinishedDeletingFiles"
	);
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
				ULCReporter::ShowError(
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
					ULCReporter::ShowError(
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

	ULCReporter::ShowInfo(
		LOCTEXT("UImageDownloader::DeleteAllProcessedImages::OK", "Finished Deleting files."),
		"SuppressFinishedDeletingFiles"
	);
}

bool UImageDownloader::IsWMS()
{
	return
		ImageSourceKind == EImageSourceKind::GenericWMS ||
		ImageSourceKind == EImageSourceKind::Australia_LiDAR5m || 
		ImageSourceKind == EImageSourceKind::IGN_Heightmaps ||
		ImageSourceKind == EImageSourceKind::IGN_Satellite ||
		ImageSourceKind == EImageSourceKind::SHOM ||
		ImageSourceKind == EImageSourceKind::USGS_Imagery ||
		ImageSourceKind == EImageSourceKind::USGS_Topo ||
		ImageSourceKind == EImageSourceKind::USGS_3DEPElevation;
}

bool UImageDownloader::IsMapbox()
{
	return
		ImageSourceKind == EImageSourceKind::Mapbox_Heightmaps ||
		ImageSourceKind == EImageSourceKind::Mapbox_Satellite;

}

bool UImageDownloader::IsMapTiler()
{
	return
		ImageSourceKind == EImageSourceKind::MapTiler_Heightmaps ||
		ImageSourceKind == EImageSourceKind::MapTiler_Satellite;

}

bool UImageDownloader::IsNextZen()
{
	return ImageSourceKind == EImageSourceKind::NextZen_Heightmaps;
}

bool UImageDownloader::IsXYZ()
{
	return IsMapbox() || IsMapTiler() || IsNextZen() || ImageSourceKind == EImageSourceKind::GenericXYZ;
}

bool UImageDownloader::HasMultipleLayers()
{
	return WMS_Provider.Titles.Num() >= 2;
}

void UImageDownloader::SetLargestPossibleCoordinates()
{
	WMS_MinLong = WMS_MinAllowedLong;
	WMS_MinLat = WMS_MinAllowedLat;
	WMS_MaxLong = WMS_MaxAllowedLong;
	WMS_MaxLat = WMS_MaxAllowedLat;
}

void UImageDownloader::SetSourceParameters()
{
	SetSourceParametersBool(true);
}

bool UImageDownloader::AllowsParametersSelection()
{
	return IsWMS() || (IsXYZ() && bGeoreferenceSlippyTiles) || ImageSourceKind == EImageSourceKind::Napoli;
}

bool UImageDownloader::CanAutoSetResolution()
{
	return ImageSourceKind == EImageSourceKind::IGN_Heightmaps;
}

void UImageDownloader::AutoSetResolution()
{
	if (ImageSourceKind == EImageSourceKind::IGN_Heightmaps)
	{
		WMS_ResolutionPixelsPerUnit = 1;
		// WMS_MinLat = FMath::RoundToZero(WMS_MinLat);
		// WMS_MaxLat = FMath::RoundToZero(WMS_MaxLat);
		// WMS_MinLong = FMath::RoundToZero(WMS_MinLong);
		// WMS_MaxLong = FMath::RoundToZero(WMS_MaxLong);
		
		// WMS_Width = WMS_MaxLong - WMS_MinLong;
		// WMS_Height = WMS_MaxLat - WMS_MinLat;
	}
}

bool UImageDownloader::SetSourceParametersBool(bool bDialog)
{
	if (ParametersSelection == EParametersSelection::Manual) return true;

	if (!AllowsParametersSelection())
	{
		if (bDialog)
		{
			ULCReporter::ShowError(
				LOCTEXT("UImageDownloader::SetSourceParameters::0", "Custom Parameters Selection is supported only for WMS, XYZ and Napoli.")
			);
		}
		return false;
	}

	if (ParametersSelection == EParametersSelection::FromBoundingActor)
	{
		return SetSourceParametersFromActor(bDialog);
	}
	else if (ParametersSelection == EParametersSelection::FromEPSG4326Box)
	{
		return SetSourceParametersFromEPSG4326Box(bDialog);
	}
	else if (ParametersSelection == EParametersSelection::FromEPSG4326Coordinates)
	{
		return SetSourceParametersFromEPSG4326Coordinates(bDialog);
	}
	else
	{
		ULCReporter::ShowError(
			LOCTEXT("UImageDownloader::SetSourceParameters::1", "Internal Error: Unsupported parameter selection method.")
		);
		return false;
	}
}

bool UImageDownloader::SetSourceParametersFromEPSG4326Coordinates(bool bDialog)
{
	double LongDiff = RealWorldWidth / 40000000 * 360 / 2;
	double LatDiff  = RealWorldHeight / 40000000 * 360 / 2;
	MinLong = Longitude - LongDiff;
	MaxLong = Longitude + LongDiff;
	MinLat = Latitude - LatDiff;
	MaxLat = Latitude + LatDiff;
	return SetSourceParametersFromEPSG4326Box(bDialog);
}

bool UImageDownloader::SetSourceParametersFromEPSG4326Box(bool bDialog)
{
	if (ImageSourceKind == EImageSourceKind::Napoli)
	{
		FVector4d InCoordinates, OutCoordinates;
		InCoordinates[0] = MinLong;
		InCoordinates[1] = MaxLong;
		InCoordinates[2] = MinLat;
		InCoordinates[3] = MaxLat;

		if (!GDALInterface::ConvertCoordinates(InCoordinates, OutCoordinates, "EPSG:4326", "EPSG:32633")) return false;

		Napoli_MinLong = OutCoordinates[0];
		Napoli_MaxLong = OutCoordinates[1];
		Napoli_MinLat = OutCoordinates[2];
		Napoli_MaxLat = OutCoordinates[3];

		return true;
	}
	else if (IsWMS())
	{
		FVector4d InCoordinates, OutCoordinates;
		InCoordinates[0] = MinLong;
		InCoordinates[1] = MaxLong;
		InCoordinates[2] = MinLat;
		InCoordinates[3] = MaxLat;

		if (!GDALInterface::ConvertCoordinates(InCoordinates, OutCoordinates, "EPSG:4326", WMS_CRS)) return false;

		WMS_MinLong = OutCoordinates[0];
		WMS_MaxLong = OutCoordinates[1];
		WMS_MinLat = OutCoordinates[2];
		WMS_MaxLat = OutCoordinates[3];

		return true;
	}
	else if (IsXYZ() && bGeoreferenceSlippyTiles)
	{
		double MinLatRad = FMath::DegreesToRadians(MinLat);
		double MaxLatRad = FMath::DegreesToRadians(MaxLat);
		double n = 1 << XYZ_Zoom;
		XYZ_MinX = (MinLong + 180) / 360 * n;
		XYZ_MaxX = (MaxLong + 180) / 360 * n;
		
		XYZ_MinY = (1.0 - asinh(FMath::Tan(MaxLatRad)) / UE_PI) / 2.0 * n;
		XYZ_MaxY = (1.0 - asinh(FMath::Tan(MinLatRad)) / UE_PI) / 2.0 * n;

		return true;
	}
	else
	{
		return false;
	}
}


bool UImageDownloader::SetSourceParametersFromActor(bool bDialog)
{
	FVector4d Coordinates;

	if (!ParametersBoundingActor)
	{
		if (bDialog)
		{
			ULCReporter::ShowError(LOCTEXT("UImageDownloader::SetSourceParameters", "Please select a bounding actor."));
		}
		return false;
	}

	UE_LOG(LogImageDownloader, Log, TEXT("Set Source Parameters From Actor %s"), *ParametersBoundingActor->GetActorNameOrLabel());

	FString SourceCRS = "";
	
	if (ImageSourceKind == EImageSourceKind::Napoli)
	{
		SourceCRS = "EPSG:32633";
	}
	else if (IsWMS())
	{
		SourceCRS = WMS_CRS;
	}
	else if (IsXYZ() && bGeoreferenceSlippyTiles)
	{
		SourceCRS = "EPSG:3857";
	}

	if (SourceCRS.IsEmpty())
	{
		if (bDialog)
		{
			ULCReporter::ShowError(FText::Format(
				LOCTEXT("UImageDownloader::SetSourceParameters::1", "Please make sure that the CRS is not empty."),
				FText::FromString(ParametersBoundingActor->GetActorNameOrLabel())
			));
		}
		return false;
	}

	if (!ALevelCoordinates::GetActorCRSBounds(ParametersBoundingActor, SourceCRS, Coordinates))
	{
		if (bDialog)
		{
			ULCReporter::ShowError(FText::Format(
				LOCTEXT("UImageDownloader::SetSourceParameters::2", "Could not read coordinates from Actor {0}."),
				FText::FromString(ParametersBoundingActor->GetActorNameOrLabel())
			));
		}
		return false;
	}
	
	double ActorMinLong = Coordinates[0];
	double ActorMaxLong = Coordinates[1];
	double ActorMinLat = Coordinates[2];
	double ActorMaxLat = Coordinates[3];
	
	if (ImageSourceKind == EImageSourceKind::Napoli)
	{
		Napoli_MinLong = ActorMinLong;
		Napoli_MaxLong = ActorMaxLong;
		Napoli_MinLat = ActorMinLat;
		Napoli_MaxLat = ActorMaxLat;

		return true;
	}
	if (IsWMS())
	{
		WMS_MinLong = ActorMinLong;
		WMS_MaxLong = ActorMaxLong;
		WMS_MinLat = ActorMinLat;
		WMS_MaxLat = ActorMaxLat;

		return true;
	}
	else if (IsXYZ() && bGeoreferenceSlippyTiles)
	{
		GDALInterface::EPSG3857ToXYZTile(ActorMinLong, ActorMaxLat, XYZ_Zoom, XYZ_MinX, XYZ_MinY);
		GDALInterface::EPSG3857ToXYZTile(ActorMaxLong, ActorMinLat, XYZ_Zoom, XYZ_MaxX, XYZ_MaxY);

		return true;
	}
	else
	{
		return false;
	}
}

#if WITH_EDITOR

void UImageDownloader::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	if (!Event.Property)
	{
		Super::PostEditChangeProperty(Event);
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
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UImageDownloader, ParametersSelection))
	{
		SetSourceParametersBool(false);
	}
	else if (
		PropertyName == GET_MEMBER_NAME_CHECKED(UImageDownloader, ParametersBoundingActor) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UImageDownloader, MinLong) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UImageDownloader, MaxLong) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UImageDownloader, MinLat) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UImageDownloader, MaxLat)
	)
	{
		SetSourceParametersBool(true);
	}

	Super::PostEditChangeProperty(Event);
}

#endif


void UImageDownloader::OnLayerChanged()
{
	int LayerIndex = WMS_Provider.Titles.Find(WMS_Title);
	if (LayerIndex == INDEX_NONE)
	{
		WMS_Help = FString::Format(TEXT("Could not find Layer {0}"), { WMS_Title });
		return;
	}
	WMS_Name = WMS_Provider.Names[LayerIndex];
	WMS_Help = "Please enter the coordinates using the following CRS:";
	WMS_CRS = WMS_Provider.CRSs[LayerIndex];
	if (WMS_X_IsLong)
	{
		WMS_MinAllowedLong = WMS_Provider.MinXs[LayerIndex];
		WMS_MaxAllowedLong = WMS_Provider.MaxXs[LayerIndex];
		WMS_MinAllowedLat = WMS_Provider.MinYs[LayerIndex];
		WMS_MaxAllowedLat = WMS_Provider.MaxYs[LayerIndex];
	}
	else
	{
		WMS_MinAllowedLong = WMS_Provider.MinYs[LayerIndex];
		WMS_MaxAllowedLong = WMS_Provider.MaxYs[LayerIndex];
		WMS_MinAllowedLat = WMS_Provider.MinXs[LayerIndex];
		WMS_MaxAllowedLat = WMS_Provider.MaxXs[LayerIndex];
	}
	WMS_SearchCRS = FString::Format(TEXT("https://duckduckgo.com/?q={0}+site%3Aepsg.io"), { WMS_CRS });
	WMS_Abstract = WMS_Provider.Abstracts[LayerIndex];
}

void UImageDownloader::OnImageSourceChanged(TFunction<void(bool)> OnComplete)
{
	if (ImageSourceKind == EImageSourceKind::IGN_Heightmaps)
	{
		CapabilitiesURL = "https://data.geopf.fr/wms-r?SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";
		ResetWMSProvider(TArray<FString>(), nullptr, OnComplete);
		WMS_X_IsLong = true;
	}
	else if (ImageSourceKind == EImageSourceKind::IGN_Satellite)
	{
		CapabilitiesURL = "https://data.geopf.fr/annexes/ressources/wms-r/satellite.xml";
		ResetWMSProvider(TArray<FString>(), nullptr, OnComplete);
		WMS_X_IsLong = true;
	}
	else if (ImageSourceKind == EImageSourceKind::Australia_LiDAR5m)
	{
		CapabilitiesURL = "http://gaservices.ga.gov.au/site_9/services/DEM_LiDAR_5m/MapServer/WMSServer?request=GetCapabilities&service=WMS";
		ResetWMSProvider(TArray<FString>(), nullptr, OnComplete);
		WMS_X_IsLong = true;
	}
	else if (ImageSourceKind == EImageSourceKind::SHOM)
	{
		CapabilitiesURL = "https://services.data.shom.fr/INSPIRE/wms/r?service=WMS&version=1.3.0&request=GetCapabilities";
		ResetWMSProvider(TArray<FString>(), nullptr, OnComplete);
		WMS_X_IsLong = true;
	}
	else if (ImageSourceKind == EImageSourceKind::USGS_3DEPElevation)
	{
		CapabilitiesURL = "https://elevation.nationalmap.gov/arcgis/services/3DEPElevation/ImageServer/WMSServer?request=GetCapabilities&service=WMS";
		ResetWMSProvider(TArray<FString>(), nullptr, OnComplete);
		WMS_X_IsLong = true;
	}
	else if (ImageSourceKind == EImageSourceKind::USGS_Topo)
	{
		CapabilitiesURL = "https://basemap.nationalmap.gov/arcgis/services/USGSTopo/MapServer/WMSServer?request=GetCapabilities&service=WMS";
		ResetWMSProvider(TArray<FString>(), nullptr, OnComplete);
		WMS_X_IsLong = true;
	}
	else if (ImageSourceKind == EImageSourceKind::USGS_Imagery)
	{
		CapabilitiesURL = "https://basemap.nationalmap.gov/arcgis/services/USGSImageryOnly/MapServer/WMSServer?request=GetCapabilities&service=WMS";
		ResetWMSProvider(TArray<FString>(), nullptr, OnComplete);
		WMS_X_IsLong = true;
	}
	else if (IsMapbox())
	{
		if (!ULCReporter::ShowMapboxFreeTierWarning())
		{
			if (OnComplete) OnComplete(false);
			return;
		}
		ResetWMSProvider(TArray<FString>(), nullptr, OnComplete);
	}
	else if (IsMapTiler())
	{
		if (!ULCReporter::ShowMapTilerFreeTierWarning())
		{
			if (OnComplete) OnComplete(false);
			return;
		}
		ResetWMSProvider(TArray<FString>(), nullptr, OnComplete);
	}
	else
	{
		ResetWMSProvider(TArray<FString>(), nullptr, OnComplete);
	}
}

void UImageDownloader::ResetWMSProvider(TArray<FString> ExcludeCRS, TFunction<bool(FString)> LayerFilter, TFunction<void(bool)> OnComplete)
{
	if (CapabilitiesURL.IsEmpty())
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
	}
	else
	{
		WMS_Provider.SetFromURL(
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
				
				WMS_MaxWidth = WMS_Provider.MaxWidth;
				WMS_MaxHeight = WMS_Provider.MaxHeight;

				if (!WMS_Provider.Titles.Contains(WMS_Title) && !WMS_Provider.Titles.IsEmpty())
				{
					WMS_Title = WMS_Provider.Titles[0];
					OnLayerChanged();
				}

				if (OnComplete) OnComplete(bSuccess);
			}
		);
	}
}

void UImageDownloader::DownloadImages(bool bEnsureOneBand, TObjectPtr<UGlobalCoordinates> GlobalCoordinates, TFunction<void(TArray<FString>, FString)> OnComplete)
{
	AActor *Owner = GetOwner();
	if (!IsValid(Owner))
	{
		ULCReporter::ShowError(
			LOCTEXT("UImageDownloader::DownloadImages::NoOwner", "Internal Error: Could not find UImageDownloader Owner.")
		);
		if (OnComplete) OnComplete(TArray<FString>(), "");
		return;
	}
	
	HMFetcher *Fetcher = CreateFetcher(Owner->GetActorNameOrLabel(), bEnsureOneBand, false, false, nullptr, GlobalCoordinates);

	if (!Fetcher)
	{
		ULCReporter::ShowError(
			LOCTEXT("UImageDownloader::DownloadImages::NoFetcher", "Could not make image fetcher.")
		);
		if (OnComplete) OnComplete(TArray<FString>(), "");
		return;
	}

	Fetcher->Fetch("", {}, [this, Fetcher, OnComplete](bool bSuccess)
	{
		if (bSuccess)
		{
			if (OnComplete)
			{
				OnComplete(Fetcher->OutputFiles, Fetcher->OutputCRS);
			}
			delete Fetcher;
			return;
		}
		else
		{
			delete Fetcher;
			ULCReporter::ShowError(
				LOCTEXT("UImageDownloader::DownloadImages::Failure", "There was an error while downloading or preparing the files.")
			);
			if (OnComplete) OnComplete(TArray<FString>(), "");
			return;
		}
	});
}

#undef LOCTEXT_NAMESPACE
