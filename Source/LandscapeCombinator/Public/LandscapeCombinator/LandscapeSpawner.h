// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Coordinates/GlobalCoordinates.h"
#include "LandscapeCombinator/HMFetcher.h"

#include "ConsoleHelpers/ExternalTool.h"

#include "CoreMinimal.h"
#include "Landscape.h"

#include "LandscapeSpawner.generated.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

UENUM(BlueprintType)
enum class EHeightMapSourceKind : uint8
{
	Viewfinder15,
	Viewfinder3,
	Viewfinder1,
	SwissALTI_3D,
	USGS_OneThird,
	RGE_ALTI,
	LocalFile,
	LocalFolder,
	Litto3D_Guadeloupe,
	URL
};

UCLASS(BlueprintType)
class LANDSCAPECOMBINATOR_API ALandscapeSpawner : public AActor
{
	GENERATED_BODY()

public:
	ALandscapeSpawner();
	~ALandscapeSpawner();

	/**********************
	 *  Heightmap Source  *
	 **********************/
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Source",
		meta = (DisplayPriority = "1")
	)
	/* Please select the source from which we download the heightmaps. */
	EHeightMapSourceKind HeightMapSourceKind;


	/**************************
	 *  Viewfinder Panoramas  *
	 **************************/
		
	UPROPERTY(
		VisibleAnywhere, Category = "LandscapeSpawner|Source",
		meta = (EditCondition = "HeightMapSourceKind == EHeightMapSourceKind::Viewfinder15", EditConditionHides, DisplayPriority = "3")
	)
	FString Viewfinder15_Help = "Enter the comma-separated list of rectangles (e.g. 15-A, 15-B, 15-G, 15-H) from http://viewfinderpanoramas.org/Coverage%20map%20viewfinderpanoramas_org15.htm";
		
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Source",
		meta = (EditCondition =
			"HeightMapSourceKind == EHeightMapSourceKind::Viewfinder15 || HeightMapSourceKind == EHeightMapSourceKind::Viewfinder3 || HeightMapSourceKind == EHeightMapSourceKind::Viewfinder1",
			EditConditionHides, DisplayPriority = "4"
		)
	)
	FString Viewfinder_TilesString;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Source",
		meta = (EditCondition =
			"HeightMapSourceKind == EHeightMapSourceKind::Viewfinder3 || HeightMapSourceKind == EHeightMapSourceKind::Viewfinder1",
			EditConditionHides, DisplayPriority = "5")
	)
	/* The tiles of Viewfinder Panoramas 1" and 3" contain subtiles, use this option if you want to filter them. */
	bool bFilterDegrees = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Source",
		meta = (EditCondition =
			"bFilterDegrees && (HeightMapSourceKind == EHeightMapSourceKind::Viewfinder3 || HeightMapSourceKind == EHeightMapSourceKind::Viewfinder1)",
			EditConditionHides, DisplayPriority = "6")
	)
	int FilterMinLong = 0;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Source",
		meta = (EditCondition =
			"bFilterDegrees && (HeightMapSourceKind == EHeightMapSourceKind::Viewfinder3 || HeightMapSourceKind == EHeightMapSourceKind::Viewfinder1)",
			EditConditionHides, DisplayPriority = "7")
	)
	int FilterMaxLong = 0;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Source",
		meta = (EditCondition =
			"bFilterDegrees && (HeightMapSourceKind == EHeightMapSourceKind::Viewfinder3 || HeightMapSourceKind == EHeightMapSourceKind::Viewfinder1)",
			EditConditionHides, DisplayPriority = "8")
	)
	int FilterMinLat = 0;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Source",
		meta = (EditCondition =
			"bFilterDegrees && (HeightMapSourceKind == EHeightMapSourceKind::Viewfinder3 || HeightMapSourceKind == EHeightMapSourceKind::Viewfinder1)",
			EditConditionHides, DisplayPriority = "9")
	)
	int FilterMaxLat = 0;
		
	UPROPERTY(
		VisibleAnywhere, Category = "LandscapeSpawner|Source",
		meta = (EditCondition = "HeightMapSourceKind == EHeightMapSourceKind::Viewfinder3", EditConditionHides, DisplayPriority = "3")
	)
	FString Viewfinder3_Help = "Enter the comma-separated list of rectangles (e.g. L31, L32) from http://viewfinderpanoramas.org/Coverage%20map%20viewfinderpanoramas_org3.htm";
		
	UPROPERTY(
		VisibleAnywhere, Category = "LandscapeSpawner|Source",
		meta = (EditCondition = "HeightMapSourceKind == EHeightMapSourceKind::Viewfinder1", EditConditionHides, DisplayPriority = "3")
	)
	FString Viewfinder1_Help = "Enter the comma-separated list of rectangles (e.g. L31, L32) from http://viewfinderpanoramas.org/Coverage%20map%20viewfinderpanoramas_org1.htm";

	
	/*****************
	 *  SwissALTI3D  *
	 *****************/

	UPROPERTY(
		VisibleAnywhere, Category = "LandscapeSpawner|Source",
		meta = (EditCondition = "HeightMapSourceKind == EHeightMapSourceKind::SwissALTI_3D", EditConditionHides, DisplayPriority = "3")
	)
	FString SwissALTI3D_Help =
		"Download a CSV file from: https://www.swisstopo.admin.ch/fr/geodata/height/alti3d.html"
		"Then, enter the path on your computer to the CSV file below.";

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Source",
		meta = (EditCondition = "HeightMapSourceKind == EHeightMapSourceKind::SwissALTI_3D", EditConditionHides, DisplayPriority = "4")
	)
	/* Enter C:\Path\To\ListOfLinks.csv (see documentation for more details) */
	FString SwissALTI3D_ListOfLinks;

	
	/********************
	 *  USGS One Third  *
	 ********************/

	UPROPERTY(
		VisibleAnywhere, Category = "LandscapeSpawner|Source",
		meta = (EditCondition = "HeightMapSourceKind == EHeightMapSourceKind::USGS_OneThird", EditConditionHides, DisplayPriority = "3")
	)
	FString USGS_OneThird_Help =
		"Download a TXT file from https://apps.nationalmap.gov/downloader/ with\n"
		"Elevation Products (3DEP) > 1/3 arc second (DEM) > Current.\n"
		"Then, enter the path on your computer to the TXT file below.";

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Source",
		meta = (EditCondition = "HeightMapSourceKind == EHeightMapSourceKind::USGS_OneThird", EditConditionHides, DisplayPriority = "4")
	)
	/* Enter C:\Path\To\ListOfLinks.txt (see the help above, or the documentation for more details) */
	FString USGS_OneThird_ListOfLinks;
	

	
	/******************
	 *  Local Folder  *
	 ******************/

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Source",
		meta = (EditCondition = "HeightMapSourceKind == EHeightMapSourceKind::Folder", EditConditionHides, DisplayPriority = "3")
	)
	/* Enter C:\Path\To\Folder\ containing heightmaps following the _x0_y0 convention */
	FString Folder;

	

	/************
	 *  Litto3D *
	 ************/

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Source",
		meta = (EditCondition = "HeightMapSourceKind == EHeightMapSourceKind::Litto3D_Guadeloupe", EditConditionHides, DisplayPriority = "3")
	)
	/* Enter C:\Path\To\Folder\ containing 7z files downloaded from https://diffusion.shom.fr/multiproduct/product/configure/id/108 */
	FString Litto3D_Folder;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Source",
		meta = (EditCondition = "HeightMapSourceKind == EHeightMapSourceKind::Litto3D_Guadeloupe", EditConditionHides, DisplayPriority = "4")
	)
	/* Check this if you prefer to use the less precise 5m data instead of 1m data */
	bool bUse5mData = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Source",
		meta = (EditCondition = "HeightMapSourceKind == EHeightMapSourceKind::Litto3D_Guadeloupe", EditConditionHides, DisplayPriority = "4")
	)
	/* Check this if the files have already been extracted once. Keep it checked if unsure. */
	bool bSkipExtraction = false;



	/**************
	 *  RGE ALTI  *
	 **************/
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Source",
		meta = (EditCondition = "HeightMapSourceKind == EHeightMapSourceKind::RGE_ALTI", EditConditionHides, DisplayPriority = "3")
	)
	/* Enter the minimum longitude of the bounding box in EPSG 2154 coordinates (left coordinate) */
	int RGEALTI_MinLong;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Source",
		meta = (EditCondition = "HeightMapSourceKind == EHeightMapSourceKind::RGE_ALTI", EditConditionHides, DisplayPriority = "4")
	)
	/* Enter the maximum longitude of the bounding box in EPSG 2154 coordinates (right coordinate) */
	int RGEALTI_MaxLong;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Source",
		meta = (EditCondition = "HeightMapSourceKind == EHeightMapSourceKind::RGE_ALTI", EditConditionHides, DisplayPriority = "5")
	)
	/* Enter the minimum latitude of the bounding box in EPSG 2154 coordinates (bottom coordinate) */
	int RGEALTI_MinLat;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Source",
		meta = (EditCondition = "HeightMapSourceKind == EHeightMapSourceKind::RGE_ALTI", EditConditionHides, DisplayPriority = "6")
	)
	/* Enter the maximum latitude of the bounding box in EPSG 2154 coordinates (top coordinate) */
	int RGEALTI_MaxLat;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Source",
		meta = (EditCondition = "HeightMapSourceKind == EHeightMapSourceKind::RGE_ALTI", EditConditionHides, DisplayPriority = "7")
	)
	/* When set to true, you can specify the width and height of the desired heightmap,
	 * so that the RGE ALTI web API resizes your image before download.
	 * Maximum size for this API is 10 000 pixels. */
	bool bResizeRGEAltiUsingWebAPI;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Source",
		meta = (EditCondition = "HeightMapSourceKind == EHeightMapSourceKind::RGE_ALTI && bResizeRGEALTIUsingWebAPI", EditConditionHides, DisplayPriority = "8")
	)
	/* Enter desired width for the downloaded heightmap from RGE ALTI web API */
	int RGEALTIWidth;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Source",
		meta = (EditCondition = "HeightMapSourceKind == EHeightMapSourceKind::RGE_ALTI && bResizeRGEALTIUsingWebAPI", EditConditionHides, DisplayPriority = "9")
	)
	/* Enter desired height for the downloaded heightmap from RGE ALTI web API */
	int RGEALTIHeight;

	

	/***************
	 * Local Files *
	 ***************/	
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Source",
		meta = (EditCondition = "HeightMapSourceKind == EHeightMapSourceKind::LocalFile", EditConditionHides, DisplayPriority = "3")
	)
	/* Enter your C:\\Path\\To\\MyHeightmap.tif in GeoTIFF format */
	FString LocalFilePath;

	

	/*******
	 * URL *
	 *******/	
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Source",
		meta = (EditCondition = "HeightMapSourceKind == EHeightMapSourceKind::LocalFile", EditConditionHides, DisplayPriority = "3")
	)
	/* Enter URL to a heightmap in GeoTIFF format */
	FString URL;


	/********************
	 * General Settings *
	 ********************/	

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|General",
		meta = (DisplayPriority = "0")
	)
	/* Label of the landscape to create. */
	FString LandscapeLabel;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|General",
		meta = (DisplayPriority = "1")
	)
	/* The scale in the Z-axis of your heightmap, ZScale = 1 corresponds to real-world size. */
	double ZScale = 1;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|General",
		meta = (DisplayPriority = "2")
	)
	/* When the landscape components do not exactly match with the total size of the heightmaps,
	 * Unreal Engine adds some padding at the border. Check this option if you are willing to
	 * drop some data at the border in order to make your heightmaps exactly match the landscape
	 * components. */
	bool bDropData = true;



	/*****************
	 * Preprocessing *
	 *****************/

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Preprocessing",
		meta = (DisplayPriority = "10")
	)
	/* Check this option if you want to run an external binary to prepare the heightmaps right after fetching them. */
	bool bPreprocess = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Preprocessing",
		meta = (EditCondition = "bPreprocess", EditConditionHides, DisplayPriority = "11")
	)
	TObjectPtr<UExternalTool> PreprocessingTool;


	/**************
	 * Resolution *
	 **************/

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Resolution",
		meta = (DisplayPriority = "20")
	)
	/* Check this if you wish to scale up or down the resolution of your heightmaps. */
	bool bChangeResolution = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|Resolution",
		meta = (EditCondition = "bChangeResolution", EditConditionHides, DisplayPriority = "21")
	)
	/* When different than 100%, your heightmap resolution will be scaled up or down using GDAL. */
	int PrecisionPercent = 100;


	
	/***********
	 * Actions *
	 ***********/
	
	/* Spawn the Landscape. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (DisplayPriority = "10")
	)
	void SpawnLandscape();
	
	/* This deletes all the heightmaps, included downloaded files. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (DisplayPriority = "11")
	)
	void DeleteAllHeightmaps();

	/* This preserves downloaded files. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (DisplayPriority = "12")
	)
	void DeleteAllProcessedHeightmaps();

	

private:
	HMFetcher* CreateInitialFetcher();
	HMFetcher* CreateFetcher(HMFetcher *InitialFetcher);

	// After fetching the heightmaps, this holds the min max altitudes as computed by GDAL right before converting to 16-bit PNG
	FVector2D Altitudes;
};

#undef LOCTEXT_NAMESPACE