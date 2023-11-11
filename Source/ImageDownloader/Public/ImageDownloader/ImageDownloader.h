// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/WMSProvider.h"
#include "ImageDownloader/HMFetcher.h"
#include "ConsoleHelpers/ExternalTool.h"

#include "CoreMinimal.h"

#include "ImageDownloader.generated.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

UENUM(BlueprintType)
enum class EImageSourceKind : uint8
{
	IGN,
	USGS_3DEPElevation,
	USGS_Topo,
	USGS_Imagery,
	SHOM,
	OpenStreetMap_FR,
	Terrestris_OSM,
	GenericWMS,

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


UCLASS()
class IMAGEDOWNLOADER_API UImageDownloader : public UActorComponent
{
	GENERATED_BODY()

public:
	UImageDownloader();

	/**********************
	 *  Heightmap Source  *
	 **********************/
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (DisplayPriority = "1")
	)
	/* Please select the source from which to download the images. */
	EImageSourceKind ImageSourceKind;

	
	/*****************
	 *  Generic WMS  *
	 *****************/
	
	UPROPERTY()
	FWMSProvider WMSProvider;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::GenericWMS", EditConditionHides, DisplayPriority = "2")
	)
	FString CapabilitiesURL = "";

	UPROPERTY(
		EditAnywhere, Category = "ImageDownloader|Source",
		meta = (
			EditCondition = "IsWMS() && HasMultipleLayers()",
			EditConditionHides, GetOptions=GetTitles, DisplayPriority = "4"
		)
	)
	FString WMS_Title;

	UFUNCTION()
	TArray<FString> GetTitles();
	
	UPROPERTY(
		VisibleAnywhere, Category = "ImageDownloader|Source",
		meta = (
			EditCondition = "IsWMS() && HasMultipleLayers()",
			EditConditionHides, DisplayPriority = "5"
		)
	)
	FString WMS_Name = "";

	UPROPERTY(
		VisibleAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (
			EditCondition = "IsWMS() && HasMultipleLayers()",
			EditConditionHides, DisplayPriority = "6"
		)
	)
	FString WMS_Abstract;

	UPROPERTY(
		VisibleAnywhere, Category = "ImageDownloader|Source",
		meta = (
			EditCondition = "IsWMS()",
			EditConditionHides, DisplayPriority = "7"
		)
	)
	FString WMS_Help = "";

	UPROPERTY(
		VisibleAnywhere, Category = "ImageDownloader|Source",
		meta = (
			EditCondition = "IsWMS()",
			EditConditionHides, DisplayPriority = "8"
		)
	)
	FString WMS_CRS = "";

	UPROPERTY(
		VisibleAnywhere, Category = "ImageDownloader|Source",
		meta = (
			EditCondition = "IsWMS()",
			EditConditionHides, DisplayPriority = "9"
		)
	)
	FString WMS_Link = "";

	UPROPERTY(
		VisibleAnywhere, Category = "ImageDownloader|Source",
		meta = (
			EditCondition = "IsWMS()",
			EditConditionHides, DisplayPriority = "10"
		)
	)
	double WMS_MinAllowedLong;

	UPROPERTY(
		VisibleAnywhere, Category = "ImageDownloader|Source",
		meta = (
			EditCondition = "IsWMS()",
			EditConditionHides, DisplayPriority = "11"
		)
	)
	double WMS_MaxAllowedLong;

	UPROPERTY(
		VisibleAnywhere, Category = "ImageDownloader|Source",
		meta = (
			EditCondition = "IsWMS()",
			EditConditionHides, DisplayPriority = "12"
		)
	)
	double WMS_MinAllowedLat;

	UPROPERTY(
		VisibleAnywhere, Category = "ImageDownloader|Source",
		meta = (
			EditCondition = "IsWMS()",
			EditConditionHides, DisplayPriority = "13"
		)
	)
	double WMS_MaxAllowedLat;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (
			EditCondition = "IsWMS()",
			EditConditionHides, DisplayPriority = "20"
		)
	)
	/* Enter the minimum longitude of the bounding box (left coordinate). Greater or equal than WMS_MinAllowedLong. */
	double WMS_MinLong;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (
			EditCondition = "IsWMS()",
			EditConditionHides, DisplayPriority = "21"
		)
	)
	/* Enter the maximum longitude of the bounding box (right coordinate). Smaller or equal than WMS_MaxAllowedLong. */
	double WMS_MaxLong;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (
			EditCondition = "IsWMS()",
			EditConditionHides, DisplayPriority = "22"
		)
	)
	/* Enter the minimum latitude of the bounding box (bottom coordinate). Greater or equal than WMS_MinAllowedLat. */
	double WMS_MinLat;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (
			EditCondition = "IsWMS()",
			EditConditionHides, DisplayPriority = "23"
		)
	)
	/* Enter the maximum latitude of the bounding box (top coordinate). Smaller or equal than WMS_MaxAllowedLat. */
	double WMS_MaxLat;
	
	UPROPERTY(
		VisibleAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (
			EditCondition = "IsWMS()",
			EditConditionHides, DisplayPriority = "24"
		)
	)
	/* Maximum allowed width from the WMS provider. */
	int WMS_MaxWidth = 0;

	UPROPERTY(
		VisibleAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (
			EditCondition = "IsWMS()",
			EditConditionHides, DisplayPriority = "25"
		)
	)
	/* Maximum allowed width from the WMS provider. */
	int WMS_MaxHeight = 0;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (
			EditCondition = "IsWMS()",
			EditConditionHides, DisplayPriority = "26"
		)
	)
	/* Enter desired width for the downloaded heightmap from the WMS API. Smaller or equal than WMS_MaxWidth. */
	int WMS_Width = 1000;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (
			EditCondition = "IsWMS()",
			EditConditionHides, DisplayPriority = "27"
		)
	)
	/* Enter desired height for the downloaded heightmap from the WMS API. Smaller or equal than WMS_MaxHeight. */
	int WMS_Height = 1000;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (
			EditCondition = "IsWMS()",
			EditConditionHides, DisplayPriority = "30"
		)
	)
	/* Some services use -99999 as no data. Check this option to remap -99999 to -100 (or any other mapping). */
	bool bWMS_Remap = true;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (
			EditCondition = "IsWMS() && bWMS_Remap",
			EditConditionHides, DisplayPriority = "31"
		)
	)
	float WMS_OriginalValue = -99999;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (
			EditCondition = "IsWMS() && bWMS_Remap",
			EditConditionHides, DisplayPriority = "32"
		)
	)
	float WMS_TransformedValue = -100;


	/**************************
	 *  Viewfinder Panoramas  *
	 **************************/
		
	UPROPERTY(
		VisibleAnywhere, Category = "ImageDownloader|Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::Viewfinder15", EditConditionHides, DisplayPriority = "3")
	)
	FString Viewfinder15_Help = "Enter the comma-separated list of rectangles (e.g. 15-A, 15-B, 15-G, 15-H) from http://viewfinderpanoramas.org/Coverage%20map%20viewfinderpanoramas_org15.htm";
		
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (EditCondition =
			"ImageSourceKind == EImageSourceKind::Viewfinder15 || ImageSourceKind == EImageSourceKind::Viewfinder3 || ImageSourceKind == EImageSourceKind::Viewfinder1",
			EditConditionHides, DisplayPriority = "4"
		)
	)
	FString Viewfinder_TilesString;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (EditCondition =
			"ImageSourceKind == EImageSourceKind::Viewfinder3 || ImageSourceKind == EImageSourceKind::Viewfinder1",
			EditConditionHides, DisplayPriority = "5")
	)
	/* The tiles of Viewfinder Panoramas 1" and 3" contain subtiles, use this option if you want to filter them. */
	bool bFilterDegrees = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (EditCondition =
			"bFilterDegrees && (ImageSourceKind == EImageSourceKind::Viewfinder3 || ImageSourceKind == EImageSourceKind::Viewfinder1)",
			EditConditionHides, DisplayPriority = "6")
	)
	int FilterMinLong = 0;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (EditCondition =
			"bFilterDegrees && (ImageSourceKind == EImageSourceKind::Viewfinder3 || ImageSourceKind == EImageSourceKind::Viewfinder1)",
			EditConditionHides, DisplayPriority = "7")
	)
	int FilterMaxLong = 0;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (EditCondition =
			"bFilterDegrees && (ImageSourceKind == EImageSourceKind::Viewfinder3 || ImageSourceKind == EImageSourceKind::Viewfinder1)",
			EditConditionHides, DisplayPriority = "8")
	)
	int FilterMinLat = 0;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (EditCondition =
			"bFilterDegrees && (ImageSourceKind == EImageSourceKind::Viewfinder3 || ImageSourceKind == EImageSourceKind::Viewfinder1)",
			EditConditionHides, DisplayPriority = "9")
	)
	int FilterMaxLat = 0;
		
	UPROPERTY(
		VisibleAnywhere, Category = "ImageDownloader|Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::Viewfinder3", EditConditionHides, DisplayPriority = "3")
	)
	FString Viewfinder3_Help = "Enter the comma-separated list of rectangles (e.g. L31, L32) from http://viewfinderpanoramas.org/Coverage%20map%20viewfinderpanoramas_org3.htm";
		
	UPROPERTY(
		VisibleAnywhere, Category = "ImageDownloader|Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::Viewfinder1", EditConditionHides, DisplayPriority = "3")
	)
	FString Viewfinder1_Help = "Enter the comma-separated list of rectangles (e.g. L31, L32) from http://viewfinderpanoramas.org/Coverage%20map%20viewfinderpanoramas_org1.htm";


	/************
	 *  Litto3D *
	 ************/

	UPROPERTY(
		VisibleAnywhere, Category = "ImageDownloader|Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::Litto3D_Guadeloupe", EditConditionHides, DisplayPriority = "3")
	)
	FString Litto3D_Help =
		"Enter C:\\Path\\To\\Folder containing 7z files downloaded from https://diffusion.shom.fr/multiproduct/product/configure/id/108";

	UPROPERTY(
	        EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
	        meta = (EditCondition = "ImageSourceKind == EImageSourceKind::Litto3D_Guadeloupe", EditConditionHides, DisplayPriority = "4")
	)
	/* Enter C:\Path\To\Folder\ containing 7z files downloaded from https://diffusion.shom.fr/multiproduct/product/configure/id/108 */
	FString Litto3D_Folder;

	UPROPERTY(
	        EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
	        meta = (EditCondition = "ImageSourceKind == EImageSourceKind::Litto3D_Guadeloupe", EditConditionHides, DisplayPriority = "5")
	)
	/* Check this if you prefer to use the less precise 5m data instead of 1m data */
	bool bUse5mData = false;

	UPROPERTY(
	        EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
	        meta = (EditCondition = "ImageSourceKind == EImageSourceKind::Litto3D_Guadeloupe", EditConditionHides, DisplayPriority = "6")
	)
	/* Check this if the files have already been extracted once. Keep it checked if unsure. */
	bool bSkipExtraction = false;


	/*****************
	 *  SwissALTI3D  *
	 *****************/

	UPROPERTY(
		VisibleAnywhere, Category = "ImageDownloader|Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::SwissALTI_3D", EditConditionHides, DisplayPriority = "3")
	)
	FString SwissALTI3D_Help =
		"Download a CSV file from: https://www.swisstopo.admin.ch/fr/geodata/height/alti3d.html\n"
		"Then, enter the path on your computer to the CSV file below.";

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::SwissALTI_3D", EditConditionHides, DisplayPriority = "4")
	)
	/* Enter C:\Path\To\ListOfLinks.csv (see documentation for more details) */
	FString SwissALTI3D_ListOfLinks;

	
	/********************
	 *  USGS One Third  *
	 ********************/

	UPROPERTY(
		VisibleAnywhere, Category = "ImageDownloader|Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::USGS_OneThird", EditConditionHides, DisplayPriority = "3")
	)
	FString USGS_OneThird_Help =
		"Download a TXT file from https://apps.nationalmap.gov/downloader/ with\n"
		"Elevation Products (3DEP) > 1/3 arc second (DEM) > Current.\n"
		"Then, enter the path on your computer to the TXT file below.";

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::USGS_OneThird", EditConditionHides, DisplayPriority = "4")
	)
	/* Enter C:\Path\To\ListOfLinks.txt (see the help above, or the documentation for more details) */
	FString USGS_OneThird_ListOfLinks;


	/***************
	 * Local Files *
	 ***************/
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::LocalFile", EditConditionHides, DisplayPriority = "3")
	)
	/* Enter your C:\\Path\\To\\MyHeightmap.tif in GeoTIFF format */
	FString LocalFilePath;


	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::LocalFile || ImageSourceKind == EImageSourceKind::LocalFolder || ImageSourceKind == EImageSourceKind::URL", EditConditionHides, DisplayPriority = "3")
	)
	/* Enter the name of the CRS of your file(s) (e.g. EPSG:4326) */
	FString CRS = "EPSG:4326";



	/******************
	 *  Local Folder  *
	 ******************/

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::LocalFolder", EditConditionHides, DisplayPriority = "3")
	)
	/* Enter C:\Path\To\Folder\ containing heightmaps following the _x0_y0 convention */
	FString LocalFolderPath;

	

	/*******
	 * URL *
	 *******/	
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Source",
		meta = (EditCondition = "ImageSourceKind == EImageSourceKind::URL", EditConditionHides, DisplayPriority = "3")
	)
	/* Enter URL to a heightmap in GeoTIFF format */
	FString URL;


	/*****************
	 * Preprocessing *
	 *****************/

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Preprocessing",
		meta = (DisplayPriority = "10")
	)
	/* Check this option if you want to run an external binary to prepare the heightmaps right after fetching them. */
	bool bPreprocess = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Preprocessing",
		meta = (EditCondition = "bPreprocess", EditConditionHides, DisplayPriority = "11")
	)
	TObjectPtr<UExternalTool> PreprocessingTool;


	/**************
	 * Resolution *
	 **************/

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Resolution",
		meta = (DisplayPriority = "20")
	)
	/* Check this if you wish to scale up or down the resolution of your heightmaps. */
	bool bChangeResolution = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ImageDownloader|Resolution",
		meta = (EditCondition = "bChangeResolution", EditConditionHides, DisplayPriority = "21")
	)
	/* When different than 100%, your heightmap resolution will be scaled up or down using GDAL. */
	int PrecisionPercent = 100;


	
	/***********
	 * Actions *
	 ***********/
	
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

	/* Click this to set the min and max coordinates to the largest possible bounds */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ImageDownloader",
		meta = (EditCondition = "IsWMS()", EditConditionHides, DisplayPriority = "4")
	)
	void SetLargestPossibleCoordinates();
	
	void PostEditChangeProperty(struct FPropertyChangedEvent&);
	
	HMFetcher* CreateFetcher(FString Name, bool bEnsureOneBand, bool bScaleAltitude, FVector4d CropCoordinates, FIntPoint CropPixels, TFunction<bool(HMFetcher*)> RunBeforePNG);

	UFUNCTION()
	bool IsWMS();

private:

	void OnLayerChanged();
	void OnImageSourceChanged(TFunction<void(bool)> OnComplete);
	void ResetWMSProvider(TArray<FString> ExcludeCRS, TFunction<bool(FString)> LayerFilter, TFunction<void(bool)> OnComplete);
	
	HMFetcher* CreateInitialFetcher(FString Name);

	UFUNCTION()
	bool HasMultipleLayers();

};

#undef LOCTEXT_NAMESPACE