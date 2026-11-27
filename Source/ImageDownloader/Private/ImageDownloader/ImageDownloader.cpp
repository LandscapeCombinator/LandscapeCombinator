// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

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
#include "ImageDownloader/Downloaders/HMOGC.h"

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
#include "ImageDownloader/Transformers/HMResolution.h"
#include "ImageDownloader/Transformers/HMPercentResolution.h"
#include "ImageDownloader/Transformers/HMToPNG.h"
#include "ImageDownloader/Transformers/HMMerge.h"
#include "ImageDownloader/Transformers/HMReadCRS.h"
#include "ImageDownloader/Transformers/HMConvert.h"
#include "ImageDownloader/Transformers/HMAddMissingTiles.h"
#include "ImageDownloader/Transformers/HMFunction.h"

#include "LCCommon/LCSettings.h"
#include "Coordinates/LevelCoordinates.h"
#include "LandscapeUtils/LandscapeUtils.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Engine/World.h"
#include "HAL/FileManagerGeneric.h"
#include "Misc/MessageDialog.h"
#include "Logging/StructuredLog.h"
#include "Internationalization/TextLocalizationResource.h" 

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

static void AllowedBoundsInLayerCRS(
	const FOGCProvider& Provider, const FString& LayerName, const FString& LayerCRS,
	double& MinLong, double& MaxLong, double& MinLat, double& MaxLat
)
{
	if (MinLong == 0 || MaxLong == 0 || MinLat == 0 || MaxLat == 0) return;
	const int32 LayerIndex = Provider.Names.Find(LayerName);
	if (!Provider.CRSs.IsValidIndex(LayerIndex)) return;
	const FString& BoundsCRS = Provider.CRSs[LayerIndex];
	if (BoundsCRS.IsEmpty() || BoundsCRS == LayerCRS) return;

	FVector4d Bounds(MinLong, MaxLong, MinLat, MaxLat);
	FVector4d Converted;
	if (GDALInterface::ConvertCoordinates(Bounds, /*bCrop=*/false, Converted, BoundsCRS, LayerCRS))
	{
		MinLong = FMath::Min(Converted[0], Converted[1]);
		MaxLong = FMath::Max(Converted[0], Converted[1]);
		MinLat = FMath::Min(Converted[2], Converted[3]);
		MaxLat = FMath::Max(Converted[2], Converted[3]);
	}
	else
	{
		MinLong = MaxLong = MinLat = MaxLat = 0;
	}
}


UImageDownloader::UImageDownloader()
{
	PreprocessingTool = CreateDefaultSubobject<UExternalTool>(TEXT("Preprocessing Tool"));
}

bool UImageDownloader::ConfigureForTiles(int Zoom, int MinX, int MaxX, int MinY, int MaxY, int PaddingPx, bool bGeodetic)
{
	bTileOutsideCoverage = false;
	bCropToGeodeticTile = false;
	if (IsXYZ())
	{
		if (bGeodetic)
		{
			if (!bGeoreferenceSlippyTiles)
			{
				LCReporter::ShowError(NSLOCTEXT("FImageDownloaderModule", "ConfigureForTilesGeodeticXYZNotGeoreferenced", "The geodetic tile grid of Position Based Generation requires XYZ sources with Georeference Slippy Tiles enabled."));
				return false;
			}
			const double TileDegrees = 360.0 / (1 << Zoom);
			const double MaxMercatorLat = 85.0511;
			ParametersSelection.MinLong = -180.0 + MinX * TileDegrees + 1e-9;
			ParametersSelection.MaxLong = -180.0 + (MaxX + 1) * TileDegrees - 1e-9;
			ParametersSelection.MinLat = FMath::Max(90.0 - (MaxY + 1) * TileDegrees, -MaxMercatorLat) + 1e-9;
			ParametersSelection.MaxLat = FMath::Min(90.0 - MinY * TileDegrees, MaxMercatorLat);
			bTileOutsideCoverage = ParametersSelection.MinLat >= ParametersSelection.MaxLat;
			bCropToGeodeticTile = true;
			XYZ_Zoom = Zoom;
			ParametersSelection.ParametersSelectionMethod = EParametersSelectionMethod::FromEPSG4326Box;
			return true;
		}
		XYZ_MinX = MinX;
		XYZ_MaxX = MaxX;
		XYZ_MinY = MinY;
		XYZ_MaxY = MaxY;
		XYZ_Zoom = Zoom;
		ParametersSelection.ParametersSelectionMethod = EParametersSelectionMethod::Manual;
		return true;
	}
	else if (IsOGC())
	{
		const FString GridCRS = bGeodetic ? TEXT("EPSG:4326") : TEXT("EPSG:3857");
		FVector4d GridCoordinates;
		if (bGeodetic)
		{
			const double TileDegrees = 360.0 / (1 << Zoom);
			GridCoordinates = FVector4d(-180.0 + MinX * TileDegrees, -180.0 + (MaxX + 1) * TileDegrees, 90.0 - (MaxY + 1) * TileDegrees, 90.0 - MinY * TileDegrees);
		}
		else
		{
			double MinLong3857, MaxLat3857, MaxLong3857, MinLat3857;
			GDALInterface::XYZTileToEPSG3857(MinX, MinY, Zoom, MinLong3857, MaxLat3857);
			GDALInterface::XYZTileToEPSG3857(MaxX + 1, MaxY + 1, Zoom, MaxLong3857, MinLat3857);
			GridCoordinates = FVector4d(MinLong3857, MaxLong3857, MinLat3857, MaxLat3857);
		}
		FVector4d CoordinatesOGC;
		const double PaddingUnits = OGC_ResolutionPixelsPerUnit > 0 ? PaddingPx / OGC_ResolutionPixelsPerUnit : 0;

		if (!GDALInterface::ConvertCoordinates(GridCoordinates, /*bCrop=*/false, CoordinatesOGC, GridCRS, OGC_CRS))
		{
			LCReporter::ShowError(
				FText::Format(
					NSLOCTEXT("FImageDownloaderModule", "ConfigureForTilesWMSConvertFailed",
						"Failed to convert tile coordinates from the tile grid CRS to the CRS of the layer ('{0}'). "
						"Please check that the layer's CRS is valid."
					),
					FText::FromString(OGC_CRS)
				)
			);
			return false;
		}

		// The 3857 -> layer CRS conversion can return the antimeridian with the wrong sign (+180 for the west edge, -180 for the east edge)
		if (CoordinatesOGC[0] > CoordinatesOGC[1] && CoordinatesOGC[0] >= 179.999) CoordinatesOGC[0] -= 360;
		if (CoordinatesOGC[0] >= CoordinatesOGC[1] && CoordinatesOGC[1] <= -179.999) CoordinatesOGC[1] += 360;

		OGC_MinLong = CoordinatesOGC[0] - PaddingUnits;
		OGC_MaxLong = CoordinatesOGC[1] + PaddingUnits;
		OGC_MinLat  = CoordinatesOGC[2] - PaddingUnits;
		OGC_MaxLat  = CoordinatesOGC[3] + PaddingUnits;

		double AllowedMinLong = OGC_MinAllowedLong, AllowedMaxLong = OGC_MaxAllowedLong, AllowedMinLat = OGC_MinAllowedLat, AllowedMaxLat = OGC_MaxAllowedLat;
		AllowedBoundsInLayerCRS(OGC_Provider, OGC_Name, OGC_CRS, AllowedMinLong, AllowedMaxLong, AllowedMinLat, AllowedMaxLat);
		if (AllowedMinLong != 0 && AllowedMaxLong != 0 && AllowedMinLat != 0 && AllowedMaxLat != 0)
		{
			OGC_MinLong = FMath::Max(OGC_MinLong, AllowedMinLong);
			OGC_MaxLong = FMath::Min(OGC_MaxLong, AllowedMaxLong);
			OGC_MinLat  = FMath::Max(OGC_MinLat, AllowedMinLat);
			OGC_MaxLat  = FMath::Min(OGC_MaxLat, AllowedMaxLat);
		}

		bTileOutsideCoverage = OGC_MinLong >= OGC_MaxLong || OGC_MinLat >= OGC_MaxLat;

		ParametersSelection.ParametersSelectionMethod = EParametersSelectionMethod::Manual;
		return true;
	}
	else
	{
		FString SourceName = StaticEnum<EImageSourceKind>()->GetDisplayNameTextByValue((int64)ImageSourceKind).ToString();

		LCReporter::ShowError(
			FText::Format(
				NSLOCTEXT("FImageDownloaderModule", "ConfigureForTilesNotSupported",
					"Position Based Generation requires an XYZ, WMS or WCS based image source, but the current source is '{0}'.\n"
					"Either disable Position Based Generation, or change the Image Source Kind."
				),
				FText::FromString(SourceName)
			)
		);

		return false;
	}
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
	return OGC_Provider.Titles;
}

HMFetcher* UImageDownloader::CreateInitialFetcher(bool bIsUserInitiated, FString Name)
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
			if (IsOGC())
			{
				if (!SetSourceParametersBool(true))
				{
					return nullptr;
				}

				if (bAutoSetResolution) AutoSetResolution();

				double AllowedMinLong = OGC_MinAllowedLong, AllowedMaxLong = OGC_MaxAllowedLong, AllowedMinLat = OGC_MinAllowedLat, AllowedMaxLat = OGC_MaxAllowedLat;
				AllowedBoundsInLayerCRS(OGC_Provider, OGC_Name, OGC_CRS, AllowedMinLong, AllowedMaxLong, AllowedMinLat, AllowedMaxLat);

				OGC_Provider.bTransparent = bOGCTransparent;
				HMFetcher *OGCFetcher = new HMOGC(
					OGC_Provider, bEnableOGCParallelDownload,
					OGC_MaxTileWidth, OGC_MaxTileHeight,
					OGC_Name, OGC_CRS, OGC_X_IsLong,
					AllowedMinLong, AllowedMaxLong, AllowedMinLat, AllowedMaxLat,
					OGC_MinLong, OGC_MaxLong, OGC_MinLat, OGC_MaxLat,
					bOGCSingleTile, OGC_ResolutionPixelsPerUnit,
					bOGCSingleTile && bOGC_MaxResolution && (!CanAutoSetResolution() || !bAutoSetResolution)
				);

				if (!OGCFetcher) return nullptr;

				return new HMDebugFetcher("OGC_Download", OGCFetcher, true);
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
					if (bIsUserInitiated && !ShowMapboxFreeTierWarning()) return nullptr;

					FString MapboxToken2 = GetMapboxToken();
					if (MapboxToken2.IsEmpty())
					{
						LCReporter::ShowError(
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
					if (bIsUserInitiated && !ShowMapTilerFreeTierWarning()) return nullptr;

					FString MapTilerToken2 = GetMapTilerToken();
					if (MapTilerToken2.IsEmpty())
					{
						LCReporter::ShowError(
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
				else if (IsAWSTerrainTiles())
				{
					Layer = "AWSTerrainTiles";
					Format = "png";
					URL2 = "https://s3.amazonaws.com/elevation-tiles-prod/terrarium/{z}/{x}/{y}.png";

					bGeoreferenceSlippyTiles2 = true;
					bMaxY_IsNorth2 = false;
				}
				else if (IsNextZen())
				{
					FString NextZenToken2 = GetNextZenToken();
					if (NextZenToken2.IsEmpty())
					{
						LCReporter::ShowError(
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
						bAllowInvalidTiles, bEnableXYZParallelDownload,
						Name, Layer, Format, URL2, XYZ_Zoom, XYZ_MinX, XYZ_MaxX, XYZ_MinY, XYZ_MaxY,
						bMaxY_IsNorth2, bGeoreferenceSlippyTiles2,
							ImageSourceKind == EImageSourceKind::MapTiler_Heightmaps ||
							ImageSourceKind == EImageSourceKind::Mapbox_Heightmaps ||
							ImageSourceKind == EImageSourceKind::NextZen_Heightmaps ||
							ImageSourceKind == EImageSourceKind::AWS_TerrainTiles,
							ImageSourceKind == EImageSourceKind::NextZen_Heightmaps ||
							ImageSourceKind == EImageSourceKind::AWS_TerrainTiles,
						XYZ_CRS
					),
					true
				);

				return Result;
			}
			else
			{
				LCReporter::ShowError(
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
	bool bIsUserInitiated, FString Name, bool bEnsureOneBand, bool bScaleAltitude,
	bool bConvertToPNG, bool bConvertFirstOnly, bool bAddMissingTiles,
	TFunction<bool(HMFetcher*)> RunBeforePNG, TObjectPtr<UGlobalCoordinates> GlobalCoordinates
)
{
	HMFetcher *Result = CreateInitialFetcher(bIsUserInitiated, Name);
	if (!Result) return nullptr;
	Result->SetIsUserInitiated(bIsUserInitiated);
	if (bRemap)
	{
		Result = Result->AndThen(new HMDebugFetcher("Convert", new HMConvert(Name, "tif")));
		Result = Result->AndThen(new HMDebugFetcher("FixNoData", new HMFunction([this](float x) { return x == OriginalValue ? TransformedValue : x; })));
	}
	
	if (bPreprocess)
	{
		Result = Result->AndThen(new HMDebugFetcher("Preprocess", new HMPreprocess(Name, PreprocessingTool)));
	}

	if (bEnsureOneBand && IsOGC())
	{
		Result = Result->AndThen(new HMDebugFetcher("EnsureOneBand", new HMEnsureOneBand()));
	}

	if (GlobalCoordinates)
	{
		Result = Result->AndThen(new HMDebugFetcher("Reproject", new HMReproject(Name, GlobalCoordinates->CRS, bEnsureOneBand)));
	}

	if (bMergeImages)
	{
		Result = Result->AndThen(new HMDebugFetcher("Merge", new HMMerge(Name)));
	}

#if WITH_EDITOR
	if (bAdaptResolution)
	{
		FIntPoint ImageSize(0, 0);
		if (!IsValid(TargetLandscape))
		{
			LCReporter::ShowError(
				LOCTEXT("UImageDownloader::CreateFetcher::NoLandscape", "Please select a target landscape if you want to resize the image.")
			);
			return nullptr;
		}

		ImageSize.X = TargetLandscape->ComputeComponentCounts().X * TargetLandscape->ComponentSizeQuads + 1;
		ImageSize.Y = TargetLandscape->ComputeComponentCounts().Y * TargetLandscape->ComponentSizeQuads + 1;

		Result = Result->AndThen(new HMDebugFetcher("AdaptImage", new HMResolution(Name, ImageSize)));
	}
#else
	if (bAdaptResolution)
	{
		UE_LOG(LogImageDownloader, Error, TEXT("Adapt Resolution is only supported in editor mode"));
		return nullptr;
	}

#endif

	if (bCropCoordinates || bCropToGeodeticTile)
	{
		if (!IsValid(GlobalCoordinates))
		{
			if ((AllowsParametersSelection() && bCropFollowingParametersSelection && ParametersSelection.ParametersSelectionMethod == EParametersSelectionMethod::FromBoundingActor) ||
				!AllowsParametersSelection() || !(bCropFollowingParametersSelection || bCropToGeodeticTile))
			{
				LCReporter::ShowError(
					LOCTEXT("UImageDownloader::CreateFetcher::CropActor", "Bounding actor cannot be used to crop coordinates if there is no Level Coordinates.")
				);
				return nullptr;
			}
		}

		Result = Result->AndThen(new HMDebugFetcher("Crop", new HMCrop(Name, AllowsParametersSelection() && (bCropFollowingParametersSelection || bCropToGeodeticTile), ParametersSelection, CroppingActor, UGlobalCoordinates::GetSphereRadiusCm(GlobalCoordinates))));
	}

	if (RunBeforePNG)
	{
		Result = Result->AndRun(RunBeforePNG);
	}
	
	if (bConvertToPNG)
	{
		Result = Result->AndThen(new HMDebugFetcher("ToPNG", new HMToPNG(Name, bScaleAltitude, bHideNoDataFlag, bConvertFirstOnly)));
	}

	if (bAddMissingTiles)
	{
		Result = Result->AndThen(new HMDebugFetcher("AddMissingTiles", new HMAddMissingTiles()));
	}

	if (bScaleResolution)
	{
		Result = Result->AndThen(new HMDebugFetcher("PercentResolution", new HMPercentResolution(Name, PrecisionPercent, ScaleResamplingMethod)));
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
		return ProjectSettingsMapTilerToken.TrimStartAndEnd();
	}
	else
	{
		return MapTiler_Token.TrimStartAndEnd();
	}
}

FString UImageDownloader::GetMapboxToken()
{
	FString ProjectSettingsMapboxToken = GetDefault<ULCSettings>()->Mapbox_Token;
	if (!ProjectSettingsMapboxToken.IsEmpty())
	{
		return ProjectSettingsMapboxToken.TrimStartAndEnd();
	}
	else
	{
		return Mapbox_Token.TrimStartAndEnd();
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

bool UImageDownloader::IsGenericOGC()
{
	return ImageSourceKind == EImageSourceKind::GenericWMS || ImageSourceKind == EImageSourceKind::GenericWCS;
}

EOGCService UImageDownloader::GetOGCService()
{
	return
		ImageSourceKind == EImageSourceKind::GenericWCS ||
		ImageSourceKind == EImageSourceKind::Netherlands_AHN4 ||
		ImageSourceKind == EImageSourceKind::Spain_MDT5
			? EOGCService::WCS : EOGCService::WMS;
}

bool UImageDownloader::IsOGC()
{
	return
		ImageSourceKind == EImageSourceKind::GenericWMS ||
		ImageSourceKind == EImageSourceKind::GenericWCS ||
		ImageSourceKind == EImageSourceKind::Netherlands_AHN4 ||
		ImageSourceKind == EImageSourceKind::Spain_MDT5 ||
		ImageSourceKind == EImageSourceKind::Mars_Elevation ||
		ImageSourceKind == EImageSourceKind::Mars_Imagery ||
		ImageSourceKind == EImageSourceKind::Mercury_Elevation ||
		ImageSourceKind == EImageSourceKind::Mercury_Imagery ||
		ImageSourceKind == EImageSourceKind::Venus_Imagery ||
		ImageSourceKind == EImageSourceKind::Io_Imagery ||
		ImageSourceKind == EImageSourceKind::Europa_Imagery ||
		ImageSourceKind == EImageSourceKind::Ganymede_Imagery ||
		ImageSourceKind == EImageSourceKind::Callisto_Imagery ||
		ImageSourceKind == EImageSourceKind::Titan_Imagery ||
		ImageSourceKind == EImageSourceKind::Enceladus_Imagery ||
		ImageSourceKind == EImageSourceKind::Pluto_Imagery ||
		ImageSourceKind == EImageSourceKind::Charon_Imagery ||
		ImageSourceKind == EImageSourceKind::Ceres_Imagery ||
		ImageSourceKind == EImageSourceKind::Vesta_Imagery ||
		ImageSourceKind == EImageSourceKind::Triton_Imagery ||
		ImageSourceKind == EImageSourceKind::Moon_Elevation ||
		ImageSourceKind == EImageSourceKind::Moon_Imagery ||
		ImageSourceKind == EImageSourceKind::Australia_LiDAR5m || 
		ImageSourceKind == EImageSourceKind::IGN ||
		ImageSourceKind == EImageSourceKind::SHOM ||
		ImageSourceKind == EImageSourceKind::EOX_Satellite ||
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

bool UImageDownloader::IsAWSTerrainTiles()
{
	return ImageSourceKind == EImageSourceKind::AWS_TerrainTiles;
}

bool UImageDownloader::IsXYZ()
{
	return IsMapbox() || IsMapTiler() || IsNextZen() || IsAWSTerrainTiles() || ImageSourceKind == EImageSourceKind::GenericXYZ;
}

bool UImageDownloader::HasMultipleLayers()
{
	return OGC_Provider.Titles.Num() >= 2;
}

void UImageDownloader::SetLargestPossibleCoordinates()
{
	OGC_MinLong = OGC_MinAllowedLong;
	OGC_MinLat = OGC_MinAllowedLat;
	OGC_MaxLong = OGC_MaxAllowedLong;
	OGC_MaxLat = OGC_MaxAllowedLat;
}

void UImageDownloader::SetSourceParameters()
{
	SetSourceParametersBool(true);
}

bool UImageDownloader::AllowsParametersSelection()
{
	return IsOGC() || (IsXYZ() && bGeoreferenceSlippyTiles) || ImageSourceKind == EImageSourceKind::Napoli;
}

bool UImageDownloader::CanAutoSetResolution()
{
	return ImageSourceKind == EImageSourceKind::IGN;
}

void UImageDownloader::AutoSetResolution()
{
	if (ImageSourceKind == EImageSourceKind::IGN)
	{
		OGC_ResolutionPixelsPerUnit = 1;
		OGC_MinLat = FMath::RoundToZero(OGC_MinLat);
		OGC_MaxLat = FMath::RoundToZero(OGC_MaxLat);
		OGC_MinLong = FMath::RoundToZero(OGC_MinLong);
		OGC_MaxLong = FMath::RoundToZero(OGC_MaxLong);
	}
}

bool UImageDownloader::SetSourceParametersBool(bool bDialog)
{
	if (ParametersSelection.ParametersSelectionMethod == EParametersSelectionMethod::Manual) return true;

	if (!AllowsParametersSelection())
	{
		if (bDialog)
		{
			LCReporter::ShowError(
				LOCTEXT("UImageDownloader::SetSourceParameters::0", "Custom Parameters Selection is supported only for WMS, WCS, XYZ and Napoli.")
			);
		}
		return false;
	}

	if (ParametersSelection.ParametersSelectionMethod == EParametersSelectionMethod::FromBoundingActor)
	{
		return SetSourceParametersFromActor(bDialog);
	}
	else if (ParametersSelection.ParametersSelectionMethod == EParametersSelectionMethod::FromEPSG4326Box)
	{
		return SetSourceParametersFromEPSG4326Box(bDialog);
	}
	else if (ParametersSelection.ParametersSelectionMethod == EParametersSelectionMethod::FromEPSG4326Coordinates)
	{
		return SetSourceParametersFromEPSG4326Coordinates(bDialog);
	}
	else
	{
		LCReporter::ShowError(
			LOCTEXT("UImageDownloader::SetSourceParameters::1", "Internal Error: Unsupported parameter selection method.")
		);
		return false;
	}
}

bool UImageDownloader::SetSourceParametersFromEPSG4326Coordinates(bool bDialog)
{
	FVector4d Coordinates = UGlobalCoordinates::GetCoordinatesFromSize(ParametersSelection.Longitude, ParametersSelection.Latitude, ParametersSelection.RealWorldWidth, ParametersSelection.RealWorldHeight, UGlobalCoordinates::GetSphereRadiusCm(GetTypedOuter<AActor>() ? ALevelCoordinates::GetGlobalCoordinatesFor(GetTypedOuter<AActor>(), false) : ALevelCoordinates::GetGlobalCoordinates(GetWorld(), false)));
	ParametersSelection.MinLong = Coordinates[0];
	ParametersSelection.MaxLong = Coordinates[1];
	ParametersSelection.MinLat = Coordinates[2];
	ParametersSelection.MaxLat = Coordinates[3];
	return SetSourceParametersFromEPSG4326Box(bDialog);
}

bool UImageDownloader::SetSourceParametersFromEPSG4326Box(bool bDialog)
{
	if (ImageSourceKind == EImageSourceKind::Napoli)
	{
		FVector4d InCoordinates, OutCoordinates;
		InCoordinates[0] = ParametersSelection.MinLong;
		InCoordinates[1] = ParametersSelection.MaxLong;
		InCoordinates[2] = ParametersSelection.MinLat;
		InCoordinates[3] = ParametersSelection.MaxLat;

		if (!GDALInterface::ConvertCoordinates(InCoordinates, OutCoordinates, "EPSG:4326", "EPSG:32633")) return false;

		Napoli_MinLong = OutCoordinates[0];
		Napoli_MaxLong = OutCoordinates[1];
		Napoli_MinLat = OutCoordinates[2];
		Napoli_MaxLat = OutCoordinates[3];

		return true;
	}
	else if (IsOGC())
	{
		FVector4d InCoordinates, OutCoordinates;
		InCoordinates[0] = ParametersSelection.MinLong;
		InCoordinates[1] = ParametersSelection.MaxLong;
		InCoordinates[2] = ParametersSelection.MinLat;
		InCoordinates[3] = ParametersSelection.MaxLat;

		if (!GDALInterface::ConvertCoordinates(InCoordinates, OutCoordinates, "EPSG:4326", OGC_CRS)) return false;

		OGC_MinLong = OutCoordinates[0];
		OGC_MaxLong = OutCoordinates[1];
		OGC_MinLat = OutCoordinates[2];
		OGC_MaxLat = OutCoordinates[3];

		return true;
	}
	else if (IsXYZ() && bGeoreferenceSlippyTiles)
	{
		double MinLatRad = FMath::DegreesToRadians(ParametersSelection.MinLat);
		double MaxLatRad = FMath::DegreesToRadians(ParametersSelection.MaxLat);
		double n = 1 << XYZ_Zoom;
		XYZ_MinX = (ParametersSelection.MinLong + 180) / 360 * n;
		XYZ_MaxX = (ParametersSelection.MaxLong + 180) / 360 * n;
		
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

	if (!IsValid(ParametersSelection.ParametersBoundingActor))
	{
		if (bDialog)
		{
			LCReporter::ShowError(LOCTEXT("UImageDownloader::SetSourceParameters", "Please select a bounding actor."));
		}
		return false;
	}

	UE_LOG(LogImageDownloader, Log, TEXT("Set Source Parameters From Actor %s"), *ParametersSelection.ParametersBoundingActor->GetActorNameOrLabel());

	FString SourceCRS = "";
	
	if (ImageSourceKind == EImageSourceKind::Napoli)
	{
		SourceCRS = "EPSG:32633";
	}
	else if (IsOGC())
	{
		SourceCRS = OGC_CRS;
	}
	else if (IsXYZ() && bGeoreferenceSlippyTiles)
	{
		SourceCRS = "EPSG:3857";
	}

	if (SourceCRS.IsEmpty())
	{
		if (bDialog)
		{
			LCReporter::ShowError(FText::Format(
				LOCTEXT("UImageDownloader::SetSourceParameters::1", "Please make sure that the CRS is not empty."),
				FText::FromString(ParametersSelection.ParametersBoundingActor->GetActorNameOrLabel())
			));
		}
		return false;
	}

	if (!LandscapeUtils::GetActorCRSBounds(ParametersSelection.ParametersBoundingActor, SourceCRS, Coordinates))
	{
		if (bDialog)
		{
			LCReporter::ShowError(FText::Format(
				LOCTEXT("UImageDownloader::SetSourceParameters::2", "Could not read coordinates from Actor {0}."),
				FText::FromString(ParametersSelection.ParametersBoundingActor->GetActorNameOrLabel())
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
	if (IsOGC())
	{
		OGC_MinLong = ActorMinLong;
		OGC_MaxLong = ActorMaxLong;
		OGC_MinLat = ActorMinLat;
		OGC_MaxLat = ActorMaxLat;

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
		ResetOGCProvider(TArray<FString>(), nullptr, nullptr);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UImageDownloader, OGC_Title))
	{
		OnLayerChanged();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UImageDownloader, ParametersSelection))
	{
		SetSourceParametersBool(false);
	}
	else if (
		PropertyName == GET_MEMBER_NAME_CHECKED(UImageDownloader, ParametersSelection.ParametersBoundingActor) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UImageDownloader, ParametersSelection.MinLong) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UImageDownloader, ParametersSelection.MaxLong) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UImageDownloader, ParametersSelection.MinLat) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UImageDownloader, ParametersSelection.MaxLat)
	)
	{
		SetSourceParametersBool(true);
	}

	Super::PostEditChangeProperty(Event);
}

#endif


void UImageDownloader::OnLayerChanged()
{
	int LayerIndex = OGC_Provider.Titles.Find(OGC_Title);
	if (LayerIndex == INDEX_NONE)
	{
		OGC_Help = FString::Format(TEXT("Could not find Layer {0}"), { OGC_Title });
		return;
	}
	OGC_Name = OGC_Provider.Names[LayerIndex];
	OGC_Help = "Please enter the coordinates using the following CRS:";
	OGC_CRS = OGC_Provider.CRSs[LayerIndex];
	// The WCS provider already orders the bounds as (long, lat)
	if (OGC_X_IsLong || OGC_Provider.Service == EOGCService::WCS)
	{
		OGC_MinAllowedLong = OGC_Provider.MinXs[LayerIndex];
		OGC_MaxAllowedLong = OGC_Provider.MaxXs[LayerIndex];
		OGC_MinAllowedLat = OGC_Provider.MinYs[LayerIndex];
		OGC_MaxAllowedLat = OGC_Provider.MaxYs[LayerIndex];
	}
	else
	{
		OGC_MinAllowedLong = OGC_Provider.MinYs[LayerIndex];
		OGC_MaxAllowedLong = OGC_Provider.MaxYs[LayerIndex];
		OGC_MinAllowedLat = OGC_Provider.MinXs[LayerIndex];
		OGC_MaxAllowedLat = OGC_Provider.MaxXs[LayerIndex];
	}
	if (CanAutoSetResolution())
	{
		// IGN works with whole units (see AutoSetResolution), so we shrink the allowed extent to whole units
		OGC_MinAllowedLong = FMath::CeilToDouble(OGC_MinAllowedLong);
		OGC_MaxAllowedLong = FMath::FloorToDouble(OGC_MaxAllowedLong);
		OGC_MinAllowedLat = FMath::CeilToDouble(OGC_MinAllowedLat);
		OGC_MaxAllowedLat = FMath::FloorToDouble(OGC_MaxAllowedLat);
	}
	OGC_SearchCRS = FString::Format(TEXT("https://duckduckgo.com/?q={0}+site%3Aepsg.io"), { OGC_CRS });
	OGC_Abstract = OGC_Provider.Abstracts[LayerIndex];
}

void UImageDownloader::OnImageSourceChanged(TFunction<void(bool)> OnComplete)
{
	if (ImageSourceKind == EImageSourceKind::IGN)
	{
		CapabilitiesURL = "https://data.geopf.fr/wms-r?SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = true;
	}
	else if (ImageSourceKind == EImageSourceKind::Mars_Elevation)
	{
		CapabilitiesURL = "https://planetarymaps.usgs.gov/cgi-bin/mapserv?map=/maps/mars/mars_simp_cyl_elev.map&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = false;
	}
	else if (ImageSourceKind == EImageSourceKind::Mars_Imagery)
	{
		CapabilitiesURL = "https://planetarymaps.usgs.gov/cgi-bin/mapserv?map=/maps/mars/mars_simp_cyl.map&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = false;
	}
	else if (ImageSourceKind == EImageSourceKind::Mercury_Elevation)
	{
		CapabilitiesURL = "https://planetarymaps.usgs.gov/cgi-bin/mapserv?map=/maps/mercury/mercury_simp_cyl_elev.map&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = false;
	}
	else if (ImageSourceKind == EImageSourceKind::Mercury_Imagery)
	{
		CapabilitiesURL = "https://planetarymaps.usgs.gov/cgi-bin/mapserv?map=/maps/mercury/mercury_simp_cyl.map&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = false;
	}
	else if (ImageSourceKind == EImageSourceKind::Venus_Imagery)
	{
		CapabilitiesURL = "https://planetarymaps.usgs.gov/cgi-bin/mapserv?map=/maps/venus/venus_simp_cyl.map&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = false;
	}
	else if (ImageSourceKind == EImageSourceKind::Io_Imagery)
	{
		CapabilitiesURL = "https://planetarymaps.usgs.gov/cgi-bin/mapserv?map=/maps/jupiter/io_simp_cyl.map&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = false;
	}
	else if (ImageSourceKind == EImageSourceKind::Europa_Imagery)
	{
		CapabilitiesURL = "https://planetarymaps.usgs.gov/cgi-bin/mapserv?map=/maps/jupiter/europa_simp_cyl.map&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = false;
	}
	else if (ImageSourceKind == EImageSourceKind::Ganymede_Imagery)
	{
		CapabilitiesURL = "https://planetarymaps.usgs.gov/cgi-bin/mapserv?map=/maps/jupiter/ganymede_simp_cyl.map&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = false;
	}
	else if (ImageSourceKind == EImageSourceKind::Callisto_Imagery)
	{
		CapabilitiesURL = "https://planetarymaps.usgs.gov/cgi-bin/mapserv?map=/maps/jupiter/callisto_simp_cyl.map&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = false;
	}
	else if (ImageSourceKind == EImageSourceKind::Titan_Imagery)
	{
		CapabilitiesURL = "https://planetarymaps.usgs.gov/cgi-bin/mapserv?map=/maps/saturn/titan_simp_cyl.map&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = false;
	}
	else if (ImageSourceKind == EImageSourceKind::Enceladus_Imagery)
	{
		CapabilitiesURL = "https://planetarymaps.usgs.gov/cgi-bin/mapserv?map=/maps/saturn/enceladus_simp_cyl.map&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = false;
	}
	else if (ImageSourceKind == EImageSourceKind::Pluto_Imagery)
	{
		CapabilitiesURL = "https://planetarymaps.usgs.gov/cgi-bin/mapserv?map=/maps/pluto/pluto_simp_cyl.map&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = false;
	}
	else if (ImageSourceKind == EImageSourceKind::Charon_Imagery)
	{
		CapabilitiesURL = "https://planetarymaps.usgs.gov/cgi-bin/mapserv?map=/maps/pluto/charon_simp_cyl.map&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = false;
	}
	else if (ImageSourceKind == EImageSourceKind::Ceres_Imagery)
	{
		CapabilitiesURL = "https://planetarymaps.usgs.gov/cgi-bin/mapserv?map=/maps/asteroid_belt/ceres_simp_cyl.map&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = false;
	}
	else if (ImageSourceKind == EImageSourceKind::Vesta_Imagery)
	{
		CapabilitiesURL = "https://planetarymaps.usgs.gov/cgi-bin/mapserv?map=/maps/asteroid_belt/vesta_simp_cyl.map&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = false;
	}
	else if (ImageSourceKind == EImageSourceKind::Triton_Imagery)
	{
		CapabilitiesURL = "https://planetarymaps.usgs.gov/cgi-bin/mapserv?map=/maps/neptune/triton_simp_cyl.map&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = false;
	}
	else if (ImageSourceKind == EImageSourceKind::Moon_Elevation)
	{
		CapabilitiesURL = "https://planetarymaps.usgs.gov/cgi-bin/mapserv?map=/maps/earth/moon_simp_cyl_elev.map&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = false;
	}
	else if (ImageSourceKind == EImageSourceKind::Moon_Imagery)
	{
		CapabilitiesURL = "https://planetarymaps.usgs.gov/cgi-bin/mapserv?map=/maps/earth/moon_simp_cyl.map&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = false;
	}
	else if (ImageSourceKind == EImageSourceKind::Australia_LiDAR5m)
	{
		CapabilitiesURL = "http://gaservices.ga.gov.au/site_9/services/DEM_LiDAR_5m/MapServer/WMSServer?request=GetCapabilities&service=WMS";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = true;
	}
	else if (ImageSourceKind == EImageSourceKind::SHOM)
	{
		CapabilitiesURL = "https://services.data.shom.fr/INSPIRE/wms/r?service=WMS&version=1.3.0&request=GetCapabilities";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = true;
	}
	else if (ImageSourceKind == EImageSourceKind::USGS_3DEPElevation)
	{
		CapabilitiesURL = "https://elevation.nationalmap.gov/arcgis/services/3DEPElevation/ImageServer/WMSServer?request=GetCapabilities&service=WMS";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = true;
	}
	else if (ImageSourceKind == EImageSourceKind::USGS_Topo)
	{
		CapabilitiesURL = "https://basemap.nationalmap.gov/arcgis/services/USGSTopo/MapServer/WMSServer?request=GetCapabilities&service=WMS";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = true;
	}
	else if (ImageSourceKind == EImageSourceKind::EOX_Satellite)
	{
		CapabilitiesURL = "https://tiles.maps.eox.at/wms?service=WMS&version=1.1.1&request=GetCapabilities";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = true;
	}
	else if (ImageSourceKind == EImageSourceKind::USGS_Imagery)
	{
		CapabilitiesURL = "https://basemap.nationalmap.gov/arcgis/services/USGSImageryOnly/MapServer/WMSServer?request=GetCapabilities&service=WMS";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = true;
	}
	else if (ImageSourceKind == EImageSourceKind::Netherlands_AHN4)
	{
		CapabilitiesURL = "https://service.pdok.nl/rws/ahn/wcs/v1_0?SERVICE=WCS&request=GetCapabilities&version=2.0.1";
		PreferredLayerTitle = "dtm_05m";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = true;
	}
	else if (ImageSourceKind == EImageSourceKind::Spain_MDT5)
	{
		CapabilitiesURL = "https://servicios.idee.es/wcs-inspire/mdt?version=2.0.1&request=GetCapabilities&service=WCS";
		PreferredLayerTitle = "Elevacion25830_5";
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
		OGC_X_IsLong = true;
	}
	else if (IsXYZ())
	{
		if (IsMapbox())
		{
			if (!ShowMapboxFreeTierWarning())
			{
				if (OnComplete) OnComplete(false);
				return;
			}
		}
		else if (IsMapTiler())
		{
			if (!ShowMapTilerFreeTierWarning())
			{
				if (OnComplete) OnComplete(false);
				return;
			}
		}
		CapabilitiesURL.Empty();
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
	}
	else
	{
		ResetOGCProvider(TArray<FString>(), nullptr, OnComplete);
	}
}

void UImageDownloader::ResetOGCProvider(TArray<FString> ExcludeCRS, TFunction<bool(FString)> LayerFilter, TFunction<void(bool)> OnComplete)
{
	if (CapabilitiesURL.IsEmpty())
	{
		OGC_Abstract = "";
		OGC_Title = "";
		OGC_Name = "";
		OGC_Help = "";
		OGC_CRS = "";
		OGC_SearchCRS = "";
		OGC_MinAllowedLong = 0;
		OGC_MinAllowedLat = 0;
		OGC_MaxAllowedLat = 0;
		OGC_MaxAllowedLong = 0;
		OGC_MaxWidth = 0;
		OGC_MaxHeight = 0;
	}
	else
	{
		OGC_Provider.SetFromURL(
			GetOGCService(),
			CapabilitiesURL,
			ExcludeCRS,
			LayerFilter,
			[this, OnComplete](bool bSuccess)
			{
				OGC_Abstract = "";
				OGC_Title = "";
				OGC_Name = "";
				OGC_Help = "";
				OGC_CRS = "";
				OGC_SearchCRS = "";
				OGC_MinAllowedLong = 0;
				OGC_MinAllowedLat = 0;
				OGC_MaxAllowedLat = 0;
				OGC_MaxAllowedLong = 0;
				OGC_MaxWidth = 0;
				OGC_MaxHeight = 0;

				if (!bSuccess)
				{
					OGC_Help = FString::Format(TEXT("There was an error while loading the provider from URL: {0}"), { CapabilitiesURL });
				}
				
				OGC_MaxWidth = OGC_Provider.MaxWidth;
				OGC_MaxHeight = OGC_Provider.MaxHeight;

				if (!OGC_Provider.Titles.Contains(OGC_Title) && !OGC_Provider.Titles.IsEmpty())
				{
					// Presets can ask for a particular layer or coverage
					OGC_Title = OGC_Provider.Titles.Contains(PreferredLayerTitle) ? PreferredLayerTitle : OGC_Provider.Titles[0];
					OnLayerChanged();
				}
				PreferredLayerTitle = "";

				if (OnComplete) OnComplete(bSuccess);
			}
		);
	}
}

bool UImageDownloader::DownloadImages(bool bIsUserInitiated, bool bEnsureOneBand, TObjectPtr<UGlobalCoordinates> GlobalCoordinates, TArray<FString> &OutDownloadedImages, FString &OutImagesCRS)
{
	AActor *Owner = GetOwner();
	if (!IsValid(Owner))
	{
		LCReporter::ShowError(
			LOCTEXT("UImageDownloader::DownloadImages::NoOwner", "Internal Error: Could not find UImageDownloader Owner.")
		);
		return false;
	}
	
	FString Name = Owner->GetWorld()->GetName() + "-" + Owner->GetActorNameOrLabel();
	HMFetcher *Fetcher = CreateFetcher(bIsUserInitiated, Name, bEnsureOneBand, false, false, false, false, nullptr, GlobalCoordinates);

	if (!Fetcher)
	{
		LCReporter::ShowError(
			LOCTEXT("UImageDownloader::DownloadImages::NoFetcher", "Could not make image fetcher.")
		);
		return false;
	}

	if (!Fetcher->Fetch("", {}))
	{
		delete Fetcher;
		LCReporter::ShowError(
			LOCTEXT("UImageDownloader::DownloadImages::Failure", "There was an error while downloading or preparing the files.")
		);
		return false;
	}

	else
	{
		OutDownloadedImages = Fetcher->OutputFiles;
		OutImagesCRS = Fetcher->OutputCRS;
		delete Fetcher;
		return true;
	}
}

bool UImageDownloader::ShowMapTilerFreeTierWarning()
{
	return LCReporter::ShowMessage(
		LOCTEXT(
			"UImageDownloader::OnImageSourceChanged::MapTilerWarning",
			"Please check your MapTiler account to make sure you remain within the free tier.\nRequests can be expensive once you go beyond the free tier.\nContinue?"
		),
		"SuppressFreeTierWarning",
		LOCTEXT("MapTilerWarningTitle", "MapTiler Free Tier Warning")
	);
}

bool UImageDownloader::ShowMapboxFreeTierWarning()
{
	return LCReporter::ShowMessage(
		LOCTEXT(
			"UImageDownloader::OnImageSourceChanged::MapboxWarning",
			"Please check your Mapbox account to make sure you remain within the free tier.\nRequests can be expensive once you go beyond the free tier.\n"
			"The 2x option counts for more API requests than usual.\nContinue?"
		),
		"SuppressFreeTierWarning",
		LOCTEXT("MapboxWarningTitle", "Mapbox Free Tier Warning")
	);
}

#undef LOCTEXT_NAMESPACE
