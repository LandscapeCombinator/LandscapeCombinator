// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/WMSProvider.h"
#include "ImageDownloader/HMFetcher.h"
#include "ConsoleHelpers/ExternalTool.h"


#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "GenericPlatform/GenericPlatformMisc.h" 
#include "Delegates/Delegate.h"
#include "Landscape.h"

#include "ImageDownloader.generated.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

UENUM(BlueprintType)
enum class EImageSourceKind : uint8
{
	Select_A_Source,

	IGN_Heightmaps,
	IGN_Satellite,
	USGS_3DEPElevation,
	USGS_Topo,
	USGS_Imagery,
	SHOM,
	Australia_LiDAR5m UMETA(DisplayName = "Australia LiDAR 5m"),
	GenericWMS,
	
	MapTiler_Heightmaps UMETA(DisplayName = "MapTiler Heightmaps"),
	MapTiler_Satellite UMETA(DisplayName = "MapTiler Satellite"),
	Mapbox_Heightmaps UMETA(DisplayName = "Mapbox Heightmaps"),
	Mapbox_Satellite UMETA(DisplayName = "Mapbox Satellite"),
	GenericXYZ,

	Napoli,

	Viewfinder15,
	Viewfinder3,
	Viewfinder1,

	SwissALTI_3D,
	USGS_OneThird,
	Litto3D_Guadeloupe,

	LocalFile,
	LocalFolder,
	URL
};

UENUM(BlueprintType)
enum class EParametersSelection: uint8
{
	Manual,
	FromEPSG4326Box,
	FromEPSG4326Coordinates,
	FromBoundingActor,
};

UCLASS()
class IMAGEDOWNLOADER_API UImageDownloader : public UActorComponent
{
	GENERATED_BODY()

public:
	UImageDownloader();

	UPROPERTY()
	bool bSilentMode = false;

	/**********************
	 *  Heightmap Source  *
	 **********************/
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (DisplayPriority = "-20")
	)
	/* Please select the source from which to download the images. */
	EImageSourceKind ImageSourceKind;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "AllowsParametersSelection()",
			EditConditionHides, DisplayPriority = "-9"
		)
	)
	/* Whether to enter coordinates manually or using a bounding actor. */
	EParametersSelection ParametersSelection = EParametersSelection::Manual;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "AllowsParametersSelection() && ParametersSelection == EParametersSelection::FromBoundingActor",
			EditConditionHides, DisplayPriority = "-8"
		)
	)
	/* Select a Location Volume (or any other actor) that will be used to compute the WMS coordinates or XYZ tiles.
	   You can click the "Set Source Parameters From Actor" button to force refresh after you move the actor */
	AActor *ParametersBoundingActor = nullptr;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "AllowsParametersSelection() && ParametersSelection == EParametersSelection::FromEPSG4326Box",
			EditConditionHides, DisplayPriority = "-8"
		)
	)
	double MinLong = 0;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "AllowsParametersSelection() && ParametersSelection == EParametersSelection::FromEPSG4326Box",
			EditConditionHides, DisplayPriority = "-7"
		)
	)
	double MaxLong = 0;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "AllowsParametersSelection() && ParametersSelection == EParametersSelection::FromEPSG4326Box",
			EditConditionHides, DisplayPriority = "-6"
		)
	)
	double MinLat = 0;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "AllowsParametersSelection() && ParametersSelection == EParametersSelection::FromEPSG4326Box",
			EditConditionHides, DisplayPriority = "-5"
		)
	)
	double MaxLat = 0;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "AllowsParametersSelection() && ParametersSelection == EParametersSelection::FromEPSG4326Coordinates",
			EditConditionHides, DisplayPriority = "-8"
		)
	)
	double Longitude = 0;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "AllowsParametersSelection() && ParametersSelection == EParametersSelection::FromEPSG4326Coordinates",
			EditConditionHides, DisplayPriority = "-7"
		)
	)
	double Latitude = 0;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "AllowsParametersSelection() && ParametersSelection == EParametersSelection::FromEPSG4326Coordinates",
			EditConditionHides, DisplayPriority = "-6"
		)
	)
	// Approximate real-world size in meters
	double RealWorldWidth = 200;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "AllowsParametersSelection() && ParametersSelection == EParametersSelection::FromEPSG4326Coordinates",
			EditConditionHides, DisplayPriority = "-5"
		)
	)
	// Approximate real-world size in meters
	double RealWorldHeight = 200;

	/*******
	 * XYZ *
	 *******/
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition = "IsMapTiler() && !HasMapTilerToken()", EditConditionHides, DisplayPriority = "1", DisplayName = "MapTiler Token")
	)
	/* The MapTiler token can be set in the Editor Preferences, in Plugins -> Landscape Combinator. */
	FString MapTiler_Token;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition = "IsMapbox() && !HasMapboxToken()", EditConditionHides, DisplayPriority = "1")
	)
	/* The Mapbox token can be set in the Editor Preferences, in Plugins -> Landscape Combinator. */
	FString Mapbox_Token;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition = "IsMapbox()", EditConditionHides, DisplayPriority = "2")
	)
	/* Add @2x to the Mapbox query URL, tiles become 512x512 pixels instead of 256x256 pixels. This counts for more API requests than usual. */
	bool Mapbox_2x = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::GenericXYZ", EditConditionHides, DisplayPriority = "3")
	)
	FString XYZ_URL;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::GenericXYZ", EditConditionHides, DisplayPriority = "4")
	)
	/* Simple name for the downloaded files. */
	FString XYZ_Name;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::GenericXYZ", EditConditionHides, DisplayPriority = "5")
	)
	/* File format of the downloaded files (e.g. tif or png or xyz or xyz.gz). If the format contains a dot,
	   the tile will first be uncompressed using 7-Zip. */
	FString XYZ_Format;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition = "IsXYZ()", EditConditionHides, DisplayPriority = "7")
	)
	int XYZ_Zoom = 10;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition = "IsXYZ() && ParametersSelection == EParametersSelection::Manual", EditConditionHides, DisplayPriority = "10")
	)
	int XYZ_MinX;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition = "IsXYZ() && ParametersSelection == EParametersSelection::Manual", EditConditionHides, DisplayPriority = "11")
	)
	int XYZ_MaxX;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition = "IsXYZ() && ParametersSelection == EParametersSelection::Manual", EditConditionHides, DisplayPriority = "12")
	)
	int XYZ_MinY;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition = "IsXYZ() && ParametersSelection == EParametersSelection::Manual", EditConditionHides, DisplayPriority = "13")
	)
	int XYZ_MaxY;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::GenericXYZ", EditConditionHides, DisplayPriority = "20")
	)
	/* For XYZ or Slippy Tiles, MaxY is usually South, so you can leave this unchecked. */
	bool bMaxY_IsNorth = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::GenericXYZ", EditConditionHides, DisplayPriority = "21")
	)
	/* Add georeference to downloaded files using the Slippy Tile / XYZ convention. */
	bool bGeoreferenceSlippyTiles = true;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::GenericXYZ && !bGeoreferenceSlippyTiles", EditConditionHides, DisplayPriority = "22")
	)
	/* The coordinate system used by the downloaded files. */
	FString XYZ_CRS = "";

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition = "IsXYZ()", EditConditionHides, DisplayPriority = "23")
	)
	/* Ignore errors when the XYZ provider has missing tiles */
	bool bAllowInvalidTiles = true;




	/*****************
	 *  Generic WMS  *
	 *****************/
	
	UPROPERTY()
	FWMSProvider WMSProvider;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::GenericWMS", EditConditionHides, DisplayPriority = "-10")
	)
	FString CapabilitiesURL = "";

	UPROPERTY(
		EditAnywhere, Category = "Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::GenericWMS", EditConditionHides, DisplayPriority = "3")
	)
	bool WMS_X_IsLong = true;

	UPROPERTY(
		EditAnywhere, Category = "Source",
		meta = (
			EditCondition = "IsWMS() && HasMultipleLayers()",
			EditConditionHides, GetOptions=GetTitles, DisplayPriority = "4"
		)
	)
	FString WMS_Title;

	UFUNCTION()
	TArray<FString> GetTitles();
	
	UPROPERTY(
		VisibleAnywhere, Category = "Source",
		meta = (
			EditCondition = "IsWMS() && HasMultipleLayers()",
			EditConditionHides, DisplayPriority = "5"
		)
	)
	FString WMS_Name = "";

	UPROPERTY(
		VisibleAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "IsWMS() && HasMultipleLayers()",
			EditConditionHides, DisplayPriority = "6"
		)
	)
	FString WMS_Abstract;

	UPROPERTY(
		VisibleAnywhere, Category = "Source",
		meta = (
			EditCondition = "IsWMS()",
			EditConditionHides, DisplayPriority = "7"
		)
	)
	FString WMS_Help = "";

	UPROPERTY(
		VisibleAnywhere, Category = "Source",
		meta = (
			EditCondition = "IsWMS()",
			EditConditionHides, DisplayPriority = "8"
		)
	)
	FString WMS_CRS = "";

	UPROPERTY(
		VisibleAnywhere, Category = "Source",
		meta = (
			EditCondition = "IsWMS()",
			EditConditionHides, DisplayPriority = "9"
		)
	)
	FString WMS_SearchCRS = "";

	UPROPERTY(
		VisibleAnywhere, Category = "Source",
		meta = (
			EditCondition = "IsWMS() && ParametersSelection == EParametersSelection::Manual",
			EditConditionHides, DisplayPriority = "12"
		)
	)
	double WMS_MinAllowedLong;

	UPROPERTY(
		VisibleAnywhere, Category = "Source",
		meta = (
			EditCondition = "IsWMS() && ParametersSelection == EParametersSelection::Manual",
			EditConditionHides, DisplayPriority = "13"
		)
	)
	double WMS_MaxAllowedLong;

	UPROPERTY(
		VisibleAnywhere, Category = "Source",
		meta = (
			EditCondition = "IsWMS() && ParametersSelection == EParametersSelection::Manual",
			EditConditionHides, DisplayPriority = "14"
		)
	)
	double WMS_MinAllowedLat;

	UPROPERTY(
		VisibleAnywhere, Category = "Source",
		meta = (
			EditCondition = "IsWMS() && ParametersSelection == EParametersSelection::Manual",
			EditConditionHides, DisplayPriority = "15"
		)
	)
	double WMS_MaxAllowedLat;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "IsWMS() && ParametersSelection == EParametersSelection::Manual",
			EditConditionHides, DisplayPriority = "20"
		)
	)
	/* Enter the minimum longitude of the bounding box (left coordinate). Greater or equal than WMS_MinAllowedLong. */
	double WMS_MinLong;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "IsWMS() && ParametersSelection == EParametersSelection::Manual",
			EditConditionHides, DisplayPriority = "21"
		)
	)
	/* Enter the maximum longitude of the bounding box (right coordinate). Smaller or equal than WMS_MaxAllowedLong. */
	double WMS_MaxLong;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "IsWMS() && ParametersSelection == EParametersSelection::Manual",
			EditConditionHides, DisplayPriority = "22"
		)
	)
	/* Enter the minimum latitude of the bounding box (bottom coordinate). Greater or equal than WMS_MinAllowedLat. */
	double WMS_MinLat;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "IsWMS() && ParametersSelection == EParametersSelection::Manual",
			EditConditionHides, DisplayPriority = "23"
		)
	)
	/* Enter the maximum latitude of the bounding box (top coordinate). Smaller or equal than WMS_MaxAllowedLat. */
	double WMS_MaxLat;
	
	UPROPERTY(
		VisibleAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "IsWMS()",
			EditConditionHides, DisplayPriority = "30"
		)
	)
	/* Maximum allowed width from the WMS provider. */
	int WMS_MaxWidth = 0;

	UPROPERTY(
		VisibleAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "IsWMS()",
			EditConditionHides, DisplayPriority = "31"
		)
	)
	/* Maximum allowed width from the WMS provider. */
	int WMS_MaxHeight = 0;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "CanAutoSetSize()",
			EditConditionHides, DisplayPriority = "32"
		)
	)
	/* Set best size when downloading image from WMS */
	bool bAutoSetSize = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "IsWMS() && (!CanAutoSetSize() || !bAutoSetSize)",
			EditConditionHides, DisplayPriority = "33"
		)
	)
	/* Enter desired width for the downloaded heightmap from the WMS API. Smaller or equal than WMS_MaxWidth. */
	int WMS_Width = 1000;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "IsWMS() && (!CanAutoSetSize() || !bAutoSetSize)",
			EditConditionHides, DisplayPriority = "34"
		)
	)
	/* Enter desired height for the downloaded heightmap from the WMS API. Smaller or equal than WMS_MaxHeight. */
	int WMS_Height = 1000;


	/**********
	 * Napoli *
	 **********/

		UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "ImageSourceKind == EImageSourceKind::Napoli && ParametersSelection == EParametersSelection::Manual",
			EditConditionHides, DisplayPriority = "20"
		)
	)
	/* Enter the minimum longitude of the bounding box (left coordinate) using EPSG:32633. */
	double Napoli_MinLong;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "ImageSourceKind == EImageSourceKind::Napoli && ParametersSelection == EParametersSelection::Manual",
			EditConditionHides, DisplayPriority = "21"
		)
	)
	/* Enter the maximum longitude of the bounding box (right coordinate) using EPSG:32633. */
	double Napoli_MaxLong;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "ImageSourceKind == EImageSourceKind::Napoli && ParametersSelection == EParametersSelection::Manual",
			EditConditionHides, DisplayPriority = "22"
		)
	)
	/* Enter the minimum latitude of the bounding box (bottom coordinate) using EPSG:32633. */
	double Napoli_MinLat;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (
			EditCondition = "ImageSourceKind == EImageSourceKind::Napoli && ParametersSelection == EParametersSelection::Manual",
			EditConditionHides, DisplayPriority = "23"
		)
	)
	/* Enter the maximum latitude of the bounding box (top coordinate) using EPSG:32633. */
	double Napoli_MaxLat;


	/**************************
	 *  Viewfinder Panoramas  *
	 **************************/
		
	UPROPERTY(
		VisibleAnywhere, Category = "Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::Viewfinder15", EditConditionHides, DisplayPriority = "3")
	)
	FString Viewfinder15_Help = "Enter the comma-separated list of rectangles (e.g. 15-A, 15-B, 15-G, 15-H) from http://viewfinderpanoramas.org/Coverage%20map%20viewfinderpanoramas_org15.htm";
		
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition =
			"ImageSourceKind == EImageSourceKind::Viewfinder15 || ImageSourceKind == EImageSourceKind::Viewfinder3 || ImageSourceKind == EImageSourceKind::Viewfinder1",
			EditConditionHides, DisplayPriority = "4"
		)
	)
	FString Viewfinder_TilesString;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition =
			"ImageSourceKind == EImageSourceKind::Viewfinder3 || ImageSourceKind == EImageSourceKind::Viewfinder1",
			EditConditionHides, DisplayPriority = "5")
	)
	/* The tiles of Viewfinder Panoramas 1" and 3" contain subtiles, use this option if you want to filter them. */
	bool bFilterDegrees = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition =
			"bFilterDegrees && (ImageSourceKind == EImageSourceKind::Viewfinder3 || ImageSourceKind == EImageSourceKind::Viewfinder1)",
			EditConditionHides, DisplayPriority = "6")
	)
	int FilterMinLong = 0;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition =
			"bFilterDegrees && (ImageSourceKind == EImageSourceKind::Viewfinder3 || ImageSourceKind == EImageSourceKind::Viewfinder1)",
			EditConditionHides, DisplayPriority = "7")
	)
	int FilterMaxLong = 0;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition =
			"bFilterDegrees && (ImageSourceKind == EImageSourceKind::Viewfinder3 || ImageSourceKind == EImageSourceKind::Viewfinder1)",
			EditConditionHides, DisplayPriority = "8")
	)
	int FilterMinLat = 0;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition =
			"bFilterDegrees && (ImageSourceKind == EImageSourceKind::Viewfinder3 || ImageSourceKind == EImageSourceKind::Viewfinder1)",
			EditConditionHides, DisplayPriority = "9")
	)
	int FilterMaxLat = 0;
		
	UPROPERTY(
		VisibleAnywhere, Category = "Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::Viewfinder3", EditConditionHides, DisplayPriority = "3")
	)
	FString Viewfinder3_Help = "Enter the comma-separated list of rectangles (e.g. L31, L32) from http://viewfinderpanoramas.org/Coverage%20map%20viewfinderpanoramas_org3.htm";
		
	UPROPERTY(
		VisibleAnywhere, Category = "Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::Viewfinder1", EditConditionHides, DisplayPriority = "3")
	)
	FString Viewfinder1_Help = "Enter the comma-separated list of rectangles (e.g. L31, L32) from http://viewfinderpanoramas.org/Coverage%20map%20viewfinderpanoramas_org1.htm";


	/************
	 *  Litto3D *
	 ************/

	UPROPERTY(
		VisibleAnywhere, Category = "Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::Litto3D_Guadeloupe", EditConditionHides, DisplayPriority = "3")
	)
	FString Litto3D_Help =
		"Enter C:\\Path\\To\\Folder containing 7z files downloaded from https://diffusion.shom.fr/multiproduct/product/configure/id/108";

	UPROPERTY(
			EditAnywhere, BlueprintReadWrite, Category = "Source",
			meta = (EditCondition = "ImageSourceKind == EImageSourceKind::Litto3D_Guadeloupe", EditConditionHides, DisplayPriority = "4")
	)
	/* Enter C:\Path\To\Folder\ containing 7z files downloaded from https://diffusion.shom.fr/multiproduct/product/configure/id/108 */
	FString Litto3D_Folder;

	UPROPERTY(
			EditAnywhere, BlueprintReadWrite, Category = "Source",
			meta = (EditCondition = "ImageSourceKind == EImageSourceKind::Litto3D_Guadeloupe", EditConditionHides, DisplayPriority = "5")
	)
	/* Check this if you prefer to use the less precise 5m data instead of 1m data */
	bool bUse5mData = false;

	UPROPERTY(
			EditAnywhere, BlueprintReadWrite, Category = "Source",
			meta = (EditCondition = "ImageSourceKind == EImageSourceKind::Litto3D_Guadeloupe", EditConditionHides, DisplayPriority = "6")
	)
	/* Check this if the files have already been extracted once. Keep it checked if unsure. */
	bool bSkipExtraction = false;


	/*****************
	 *  SwissALTI3D  *
	 *****************/

	UPROPERTY(
		VisibleAnywhere, Category = "Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::SwissALTI_3D", EditConditionHides, DisplayPriority = "3")
	)
	FString SwissALTI3D_Help =
		"Download a CSV file from: https://www.swisstopo.admin.ch/fr/geodata/height/alti3d.html\n"
		"Then, enter the path on your computer to the CSV file below.";

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::SwissALTI_3D", EditConditionHides, DisplayPriority = "4")
	)
	/* Enter C:\Path\To\ListOfLinks.csv (see documentation for more details) */
	FString SwissALTI3D_ListOfLinks;

	
	/********************
	 *  USGS One Third  *
	 ********************/

	UPROPERTY(
		VisibleAnywhere, Category = "Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::USGS_OneThird", EditConditionHides, DisplayPriority = "3")
	)
	FString USGS_OneThird_Help =
		"Download a TXT file from https://apps.nationalmap.gov/downloader/ with\n"
		"Elevation Products (3DEP) > 1/3 arc second (DEM) > Current.\n"
		"Then, enter the path on your computer to the TXT file below.";

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::USGS_OneThird", EditConditionHides, DisplayPriority = "4")
	)
	/* Enter C:\Path\To\ListOfLinks.txt (see the help above, or the documentation for more details) */
	FString USGS_OneThird_ListOfLinks;


	/***************
	 * Local Files *
	 ***************/
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::LocalFile", EditConditionHides, DisplayPriority = "3")
	)
	/* Enter your C:\\Path\\To\\MyHeightmap.tif in GeoTIFF format */
	FString LocalFilePath;


	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::LocalFile || ImageSourceKind == EImageSourceKind::LocalFolder || ImageSourceKind == EImageSourceKind::URL", EditConditionHides, DisplayPriority = "3")
	)
	/* Enter the name of the CRS of your file(s) (e.g. EPSG:4326) */
	FString CRS = "EPSG:4326";



	/******************
	 *  Local Folder  *
	 ******************/

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::LocalFolder", EditConditionHides, DisplayPriority = "3")
	)
	/* Enter C:\Path\To\Folder\ containing heightmaps following the _x0_y0 convention */
	FString LocalFolderPath;

	

	/*******
	 * URL *
	 *******/	
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::URL", EditConditionHides, DisplayPriority = "3")
	)
	/* Enter URL to a heightmap in GeoTIFF format */
	FString URL;

	
	/***************
	 * Remap value *
	 ***************/

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Remap",
		meta = (DisplayPriority = "1")
	)
	/* Some sources use -99999 as no data. Check this option to remap -99999 to -10 (or any other mapping). */
	bool bRemap = true;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Remap",
		meta = (
			EditCondition = "bRemap",
			EditConditionHides, DisplayPriority = "2"
		)
	)
	float OriginalValue = -99999;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Remap",
		meta = (
			EditCondition = "bRemap",
			EditConditionHides, DisplayPriority = "3"
		)
	)
	float TransformedValue = -10;


	/*****************
	 * Preprocessing *
	 *****************/

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Preprocessing",
		meta = (DisplayPriority = "10")
	)
	/* Check this option if you want to run an external binary to prepare the heightmaps right after fetching them. */
	bool bPreprocess = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Preprocessing",
		meta = (EditCondition = "bPreprocess", EditConditionHides, DisplayPriority = "11")
	)
	TObjectPtr<UExternalTool> PreprocessingTool;


	/**************
	 * Resolution *
	 **************/

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Resolution",
		meta = (DisplayPriority = "20")
	)
	/* Check this if you wish to scale up or down the resolution of your heightmaps. */
	bool bScaleResolution = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Resolution",
		meta = (EditCondition = "bScaleResolution", EditConditionHides, DisplayPriority = "21")
	)
	/* When different than 100%, your heightmap resolution will be scaled up or down using GDAL. */
	int PrecisionPercent = 100;


	/**********************
	 * Adapt to Landscape *
	 **********************/

	UPROPERTY(
		EditAnywhere, Category = "AdaptToLandscape",
		meta = (DisplayPriority = "1")
	)
	/* Check this if you want to resize the image to match the TargetLandscape's resolution. */
	bool bAdaptResolution = false;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "AdaptToLandscape",
		meta = (EditCondition = "bAdaptResolution", EditConditionHides, DisplayPriority = "2")
	)
	TObjectPtr<ALandscape> TargetLandscape;


	/****************
	 * Crop to Area *
	 ****************/

	UPROPERTY(
		EditAnywhere, Category = "CropCoordinates",
		meta = (DisplayPriority = "1")
	)
	/* Check this if you want to crop the coordinates following the bounds of CroppingActor (usually a LocationVolume).
	   This is typically not needed for WMS sources where you choose the coordinates upfront. */
	bool bCropCoordinates = false;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "CropCoordinates",
		meta = (EditCondition = "bCropCoordinates", EditConditionHides, DisplayPriority = "2")
	)
	TObjectPtr<AActor> CroppingActor;
	

	/***********
	 * Actions *
	 ***********/
	
	void DownloadImages(TFunction<void(TArray<FString>)> OnComplete);
	void DownloadMergedImage(bool bEnsureOneBand, TFunction<void(FString, FString)> OnComplete);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ImageDownloader",
		meta = (DisplayPriority = "10")
	)
	void DownloadMergedImage() { DownloadMergedImage(false, nullptr); };
	
	/* This deletes all the images, included downloaded files. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ImageDownloader",
		meta = (DisplayPriority = "11")
	)
	void DeleteAllImages();

	/* This preserves downloaded files but deleted all transformed images. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ImageDownloader",
		meta = (DisplayPriority = "12")
	)
	void DeleteAllProcessedImages();

	/* Click this to force reloading the WMS Provider from the URL */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ImageDownloader",
		meta = (EditCondition = "IsWMS()", EditConditionHides, DisplayPriority = "3")
	)
	void ForceReloadWMS() { OnImageSourceChanged(nullptr); }

	/* Click this to set the WMS coordinates to the largest possible bounds */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ImageDownloader",
		meta = (EditCondition = "IsWMS()", EditConditionHides, DisplayPriority = "4")
	)
	void SetLargestPossibleCoordinates();

	/* Click this to set the WMS coordinates from a Location Volume or any other actor*/
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ImageDownloader",
		meta = (EditCondition = "IsWMS()", EditConditionHides, DisplayPriority = "5")
	)
	void SetSourceParameters();

	UFUNCTION(BlueprintCallable, Category = "ImageDownloader")
	bool SetSourceParametersBool(bool bDialog);

	UFUNCTION(BlueprintCallable, Category = "ImageDownloader")
	bool SetSourceParametersFromActor(bool bDialog);

	UFUNCTION(BlueprintCallable, Category = "ImageDownloader")
	bool SetSourceParametersFromEPSG4326Box(bool bDialog);

	UFUNCTION(BlueprintCallable, Category = "ImageDownloader")
	bool SetSourceParametersFromEPSG4326Coordinates(bool bDialog);
	
#if WITH_EDITOR
	void PostEditChangeProperty(struct FPropertyChangedEvent&);
#endif
	
	HMFetcher* CreateFetcher(FString Name, bool bForceMerge, bool bEnsureOneBand, bool bScaleAltitude, bool bConvertToPNG, TFunction<bool(HMFetcher*)> RunBeforePNG);

	UFUNCTION()
	static bool HasMapboxToken();

	UFUNCTION()
	static bool HasMapTilerToken();

	UFUNCTION()
	bool IsWMS();

	UFUNCTION()
	bool IsXYZ();

	UFUNCTION()
	bool AllowsParametersSelection();

	UFUNCTION()
	bool CanAutoSetSize();

	UFUNCTION()
	void AutoSetSize();

	UFUNCTION()
	bool IsMapbox();

	UFUNCTION()
	bool IsMapTiler();

private:

	void OnLayerChanged();
	void OnImageSourceChanged(TFunction<void(bool)> OnComplete);
	void ResetWMSProvider(TArray<FString> ExcludeCRS, TFunction<bool(FString)> LayerFilter, TFunction<void(bool)> OnComplete);
	
	HMFetcher* CreateInitialFetcher(FString Name);

	UFUNCTION()
	bool HasMultipleLayers();
	
	FString GetMapboxToken();
	FString GetMapTilerToken();
};

#undef LOCTEXT_NAMESPACE