// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeSpawner.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"
#include "LandscapeCombinator/LandscapeController.h"
#include "LandscapeCombinator/Directories.h"

#include "LandscapeCombinator/HMLocalFile.h"
#include "LandscapeCombinator/HMLocalFolder.h"
#include "LandscapeCombinator/HMURL.h"

#include "LandscapeCombinator/HMRGEALTI.h"
#include "LandscapeCombinator/HMSwissALTI3DRenamer.h"
#include "LandscapeCombinator/HMLitto3DGuadeloupe.h"
#include "LandscapeCombinator/HMLitto3DGuadeloupeRenamer.h"
#include "LandscapeCombinator/HMDegreeRenamer.h"
#include "LandscapeCombinator/HMViewfinder15Downloader.h"
#include "LandscapeCombinator/HMViewfinder15Renamer.h"
#include "LandscapeCombinator/HMViewfinderDownloader.h"
#include "LandscapeCombinator/HMDegreeFilter.h"

#include "LandscapeCombinator/HMDebugFetcher.h"
#include "LandscapeCombinator/HMPreprocess.h"
#include "LandscapeCombinator/HMResolution.h"
#include "LandscapeCombinator/HMReproject.h"
#include "LandscapeCombinator/HMToPNG.h"
#include "LandscapeCombinator/HMSetEPSG.h"
#include "LandscapeCombinator/HMConvert.h"
#include "LandscapeCombinator/HMAddMissingTiles.h"
#include "LandscapeCombinator/HMListDownloader.h"
#include "LandscapeCombinator/HMFunction.h"

#include "HeightmapModifier/HeightmapModifier.h"
#include "HeightmapModifier/BlendLandscape.h"
#include "LandscapeUtils/LandscapeUtils.h"
#include "Coordinates/LevelCoordinates.h"
#include "GDALInterface/GDALInterface.h"
#include "HAL/FileManagerGeneric.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

ALandscapeSpawner::ALandscapeSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	PreprocessingTool = CreateDefaultSubobject<UExternalTool>(TEXT("Preprocessing Tool"));
}

ALandscapeSpawner::~ALandscapeSpawner()
{
	MarkAsGarbage();
}

HMFetcher* ALandscapeSpawner::CreateInitialFetcher()
{
	switch (HeightMapSourceKind)
	{
		case EHeightMapSourceKind::LocalFile:
		{
			return new HMDebugFetcher("LocalFile", new HMLocalFile(LocalFilePath, EPSG), true);
		}

		case EHeightMapSourceKind::LocalFolder:
		{
			return new HMDebugFetcher("LocalFolder", new HMLocalFolder(Folder, EPSG), true);
		}

		case EHeightMapSourceKind::URL:
		{
			return new HMDebugFetcher("URL", new HMURL(URL, LandscapeLabel + ".tif", EPSG), true);
		}

		case EHeightMapSourceKind::Litto3D_Guadeloupe:
		{
			HMFetcher *Fetcher1 = new HMDebugFetcher("Litto3DGuadeloupe", new HMLitto3DGuadeloupe(Litto3D_Folder, bUse5mData, bSkipExtraction), true);
			HMFetcher *Fetcher2 = new HMDebugFetcher("Litto3DGuadeloupeRenamer", new HMLitto3DGuadeloupeRenamer(LandscapeLabel));
			return Fetcher1->AndThen(Fetcher2);
		}

		case EHeightMapSourceKind::RGE_ALTI:
		{
			HMFetcher* Fetcher1 = new HMDebugFetcher("RGE_ALTI",
				HMRGEALTI::RGEALTI(
					LandscapeLabel,
					"https://wxs.ign.fr/altimetrie/geoportail/r/wms?LAYERS=RGEALTI-MNT_PYR-ZIP_FXX_LAMB93_WMS&FORMAT=image/geotiff&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetMap&CRS=EPSG:2154&BBOX=",
					2154,
					RGEALTI_MinLong, RGEALTI_MaxLong, RGEALTI_MinLat, RGEALTI_MaxLat,
					bResizeRGEAltiUsingWebAPI, RGEALTI_Width, RGEALTI_Height
				),
				true
			);
			HMFetcher* Fetcher2 = new HMDebugFetcher("RGE_ALTI_FixNoData", new HMFunction(LandscapeLabel, [](float x) { return x == -99999 ? 0 : x; }));
			return Fetcher1->AndThen(Fetcher2);
		}

		case EHeightMapSourceKind::RGE_ALTI_REUNION:
		{
			HMFetcher* Fetcher1 = new HMDebugFetcher("RGE_ALTI_REUNION",
				HMRGEALTI::RGEALTI(
					LandscapeLabel,
					"https://wxs.ign.fr/altimetrie/geoportail/r/wms?LAYERS=RGEALTI-MNT_PYR-ZIP_REU_RGR92UTM40S_WMS&FORMAT=image/geotiff&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetMap&CRS=IGNF:RGR92GEO&BBOX=",
					4971,
					RGEALTI_REU_MinLong, RGEALTI_REU_MaxLong, RGEALTI_REU_MinLat, RGEALTI_REU_MaxLat,
					bResizeRGEAltiUsingWebAPI, RGEALTI_Width, RGEALTI_Height
				),
				true
			);
			HMFetcher* Fetcher2 = new HMDebugFetcher("RGE_ALTI_FixNoData", new HMFunction(LandscapeLabel, [](float x) { return x == -99999 ? 0 : x; }));
			return Fetcher1->AndThen(Fetcher2);
		}

		case EHeightMapSourceKind::Viewfinder15:
		{
			HMFetcher *Fetcher1 = new HMDebugFetcher("ViewfinderDownloader", new HMViewfinderDownloader(Viewfinder_TilesString, "http://www.viewfinderpanoramas.org/DEM/TIF15/", true), true);
			HMFetcher *Fetcher2 = new HMDebugFetcher("Viewfinder15Renamer", new HMViewfinder15Renamer(LandscapeLabel));
			return Fetcher1->AndThen(Fetcher2);
		}

		case EHeightMapSourceKind::Viewfinder3:
		{
			HMFetcher *Result = new HMDebugFetcher("ViewfinderDownloader", new HMViewfinderDownloader(Viewfinder_TilesString, "http://viewfinderpanoramas.org/dem3/", false), true);
			if (bFilterDegrees)
			{
				Result = Result->AndThen(new HMDebugFetcher("DegreeFilter", new HMDegreeFilter(LandscapeLabel, FilterMinLong, FilterMaxLong, FilterMinLat, FilterMaxLat)));
			}
			Result = Result->AndThen(new HMDebugFetcher("Convert", new HMConvert(LandscapeLabel, "tif")));
			Result = Result->AndThen(new HMDebugFetcher("DegreeRenamer", new HMDegreeRenamer(LandscapeLabel)));
			return Result;
		}

		case EHeightMapSourceKind::Viewfinder1:
		{
			HMFetcher *Result = new HMDebugFetcher("ViewfinderDownloader", new HMViewfinderDownloader(Viewfinder_TilesString, "http://viewfinderpanoramas.org/dem1/", false), true);
			if (bFilterDegrees)
			{
				Result = Result->AndThen(new HMDebugFetcher("DegreeFilter", new HMDegreeFilter(LandscapeLabel, FilterMinLong, FilterMaxLong, FilterMinLat, FilterMaxLat)));
			}
			Result = Result->AndThen(new HMDebugFetcher("Convert", new HMConvert(LandscapeLabel, "tif")));
			Result = Result->AndThen(new HMDebugFetcher("DegreeRenamer", new HMDegreeRenamer(LandscapeLabel)));
			return Result;
		}

		case EHeightMapSourceKind::SwissALTI_3D:
		{
			HMFetcher *Fetcher1 = new HMDebugFetcher("ListDownloader", new HMListDownloader(SwissALTI3D_ListOfLinks), true);
			HMFetcher *Fetcher2 = new HMDebugFetcher("SetEPSG", new HMSetEPSG());
			HMFetcher *Fetcher3 = new HMDebugFetcher("SwissALTI3DRenamer", new HMSwissALTI3DRenamer(LandscapeLabel));
			return Fetcher1->AndThen(Fetcher2)->AndThen(Fetcher3);
		}

		case EHeightMapSourceKind::USGS_OneThird:
		{
			HMFetcher *Fetcher1 = new HMDebugFetcher("ListDownloader", new HMListDownloader(USGS_OneThird_ListOfLinks), true);
			HMFetcher *Fetcher2 = new HMDebugFetcher("SetEPSG", new HMSetEPSG());
			HMFetcher *Fetcher3 = new HMDebugFetcher("DegreeRenamer", new HMDegreeRenamer(LandscapeLabel));
			return Fetcher1->AndThen(Fetcher2)->AndThen(Fetcher3);
		}

		default:
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("InterfaceFromKindError", "Internal error: heightmap kind '{0}' is not supprted."),
					FText::AsNumber((int) HeightMapSourceKind)
				)
			); 
			return nullptr;
	}
}

HMFetcher* ALandscapeSpawner::CreateFetcher(HMFetcher *InitialFetcher)
{
	if (!InitialFetcher) return nullptr;

	HMFetcher *Result = InitialFetcher;
	
	if (bPreprocess)
	{
		Result = Result->AndThen(new HMDebugFetcher("Preprocess", new HMPreprocess(LandscapeLabel, PreprocessingTool)));
	}

	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(this->GetWorld(), false);

	if (GlobalCoordinates)
	{
		Result = Result->AndThen(new HMDebugFetcher("Reproject", new HMReproject(LandscapeLabel, GlobalCoordinates->EPSG)));
	}

	Result = Result->AndRun([this](HMFetcher *FetcherBeforePNG)
		{
			GDALInterface::GetMinMax(this->Altitudes, FetcherBeforePNG->OutputFiles);
		}
	);


	Result = Result->AndThen(new HMDebugFetcher("ToPNG", new HMToPNG(LandscapeLabel)));
	Result = Result->AndThen(new HMDebugFetcher("AddMissingTiles", new HMAddMissingTiles()));

	if (bChangeResolution)
	{
		Result = Result->AndThen(new HMDebugFetcher("Resolution", new HMResolution(LandscapeLabel, PrecisionPercent)));
	}

	return Result;
}

bool GetPixels(FIntPoint& InsidePixels, TArray<FString> Files)
{
	if (Files.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("ALandscapeSpawner::GetPixels", "Landscape Combinator Error: Empty list of files when trying to read the size")
		); 
		return false;
	}

	if (!GDALInterface::GetPixels(InsidePixels, Files[0])) return false;

	if (Files.Num() == 1) return true;

	HMTilesCounter TilesCounter(Files);
	TilesCounter.ComputeMinMaxTiles();
	InsidePixels[0] *= (TilesCounter.LastTileX + 1);
	InsidePixels[1] *= (TilesCounter.LastTileY + 1);
	return true;
}

bool GetCmPerPixelForEPSG(int EPSG, int &CmPerPixel)
{
	if (EPSG == 4326 || EPSG == 4269 || EPSG == 4971)
	{
		CmPerPixel = 11111111;
		return true;
	}
	else if (EPSG == 2154 || EPSG == 4559 || EPSG == 2056)
	{
		CmPerPixel = 100;
		return true;
	}
	else
	{
		return false;
	}
}

void ALandscapeSpawner::SpawnLandscape()
{
	HMFetcher *InitialFetcher = CreateInitialFetcher();
	HMFetcher* Fetcher = CreateFetcher(InitialFetcher);

	if (!Fetcher)
	{			
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("ErrorBound", "There was an error while creating heightmap files for Landscape {0}."),
				FText::FromString(LandscapeLabel)
			)
		); 
		return;
	}

	Fetcher->Fetch(0, TArray<FString>(), [InitialFetcher, Fetcher, this](bool bSuccess)
	{
		// GameThread to spawn a landscape
		AsyncTask(ENamedThreads::GameThread, [bSuccess, InitialFetcher, Fetcher, this]()
		{	
			if (bSuccess)
			{
				ALandscape *CreatedLandscape = LandscapeUtils::SpawnLandscape(Fetcher->OutputFiles, LandscapeLabel, bDropData);

				if (CreatedLandscape)	
				{
					CreatedLandscape->SetActorLabel(LandscapeLabel);

					FVector4d OriginalCoordinates;
					if (!GDALInterface::GetCoordinates(OriginalCoordinates, InitialFetcher->OutputFiles))
					{
						FMessageDialog::Open(EAppMsgType::Ok,
							FText::Format(
								LOCTEXT("ErrorBound", "There was an internal error while getting coordinates for Landscape {0}."),
								FText::FromString(LandscapeLabel)
							)
						);
						// delete this; // FIXME: destroy Fetcher
						return;
					}
					
					TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(this->GetWorld(), false);

					if (!GlobalCoordinates)
					{
						int CmPerPixel;
						if (!GetCmPerPixelForEPSG(Fetcher->OutputEPSG, CmPerPixel))
						{
							FMessageDialog::Open(EAppMsgType::Ok,
								FText::Format(
									LOCTEXT("ErrorBound",
										"Please create a LevelCoordinates actor with EPSG {0}, and set the scale that you wish here."
										"Then, in the LandscapeController component of your landscape, click on the Adjust Landscape button."
									),
									FText::AsNumber(Fetcher->OutputEPSG)
								)
							);
							return;
						}

						ALevelCoordinates *LevelCoordinates = this->GetWorld()->SpawnActor<ALevelCoordinates>();
						TObjectPtr<UGlobalCoordinates> NewGlobalCoordinates = LevelCoordinates->GlobalCoordinates;

						NewGlobalCoordinates->EPSG = Fetcher->OutputEPSG;
						NewGlobalCoordinates->CmPerLongUnit = CmPerPixel;
						NewGlobalCoordinates->CmPerLatUnit = -CmPerPixel;

						FVector4d Coordinates;
						if (!GDALInterface::ConvertCoordinates(OriginalCoordinates, Coordinates, InitialFetcher->OutputEPSG, NewGlobalCoordinates->EPSG))
						{
							FMessageDialog::Open(EAppMsgType::Ok,
								FText::Format(
									LOCTEXT("ErrorBound", "There was an internal error while setting Global Coordinates world origin from Landscape {0}."),
									FText::FromString(LandscapeLabel)
								)
							);
							return;
						}

						double MinCoordWidth = Coordinates[0];
						double MaxCoordWidth = Coordinates[1];
						double MinCoordHeight = Coordinates[2];
						double MaxCoordHeight = Coordinates[3];
						NewGlobalCoordinates->WorldOriginLong = (MinCoordWidth + MaxCoordWidth) / 2;
						NewGlobalCoordinates->WorldOriginLat = (MinCoordHeight + MaxCoordHeight) / 2;
					}

					ULandscapeController *LandscapeController = NewObject<ULandscapeController>(CreatedLandscape->GetRootComponent());
					LandscapeController->RegisterComponent();
					CreatedLandscape->AddInstanceComponent(LandscapeController);

					UHeightmapModifier *HeightmapModifier = NewObject<UHeightmapModifier>(CreatedLandscape->GetRootComponent());
					HeightmapModifier->RegisterComponent();
					CreatedLandscape->AddInstanceComponent(HeightmapModifier);

					UBlendLandscape *BlendLandscape = NewObject<UBlendLandscape>(CreatedLandscape->GetRootComponent());
					BlendLandscape->RegisterComponent();
					CreatedLandscape->AddInstanceComponent(BlendLandscape);

					LandscapeController->OriginalCoordinates = OriginalCoordinates;
					LandscapeController->OriginalEPSG = InitialFetcher->OutputEPSG;
					LandscapeController->Altitudes = Altitudes;
					GetPixels(LandscapeController->InsidePixels, Fetcher->OutputFiles);
					LandscapeController->ZScale = ZScale;

					LandscapeController->AdjustLandscape();

					UE_LOG(LogLandscapeCombinator, Log, TEXT("Created Landscape %s successfully."), *LandscapeLabel);
					FMessageDialog::Open(EAppMsgType::Ok,
						FText::Format(
							LOCTEXT("LandscapeCreated", "Landscape {0} was created successfully"),
							FText::FromString(LandscapeLabel)
						)
					);
				}
				else
				{
					UE_LOG(LogLandscapeCombinator, Error, TEXT("Could not create Landscape %s."), *LandscapeLabel);
					FMessageDialog::Open(EAppMsgType::Ok,
						FText::Format(
							LOCTEXT("LandscapeNotCreated", "Landscape {0} could not be created."),
							FText::FromString(LandscapeLabel)
						)
					);
				}
            }
			else
			{
				UE_LOG(LogLandscapeCombinator, Error, TEXT("Could not create heightmaps files for Landscape %s."), *LandscapeLabel);
				FMessageDialog::Open(EAppMsgType::Ok,
					FText::Format(
						LOCTEXT("LandscapeCreated", "Could not create heightmaps files for Landscape {0}."),
						FText::FromString(LandscapeLabel)
					)
				);
			}
			
			//delete this; // FIXME: destroy Fetcher
		});
	});

	return;
}

void ALandscapeSpawner::DeleteAllHeightmaps()
{
	FString LandscapeCombinatorDir = Directories::LandscapeCombinatorDir();
	if (!LandscapeCombinatorDir.IsEmpty())
	{
		IPlatformFile::GetPlatformPhysical().DeleteDirectoryRecursively(*LandscapeCombinatorDir);
	}

	FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Deleting", "Finished Deleting files."));
}

void ALandscapeSpawner::DeleteAllProcessedHeightmaps()
{
	TArray<FString> FilesAndFolders;

	FString LandscapeCombinatorDir = Directories::LandscapeCombinatorDir();

	UE_LOG(LogLandscapeCombinator, Log, TEXT("DeleteAllProcessedHeightmaps"));

	if (!LandscapeCombinatorDir.IsEmpty())
	{
		FFileManagerGeneric::Get().FindFiles(FilesAndFolders, *FPaths::Combine(LandscapeCombinatorDir, FString("*")), true, true);
		UE_LOG(LogLandscapeCombinator, Log, TEXT("Deleting %d files and folders in %s"), FilesAndFolders.Num(), *LandscapeCombinatorDir);
		for (auto& File0 : FilesAndFolders)
		{
			if (!File0.Equals("Download"))
			{
				FString File = FPaths::Combine(LandscapeCombinatorDir, File0);
				UE_LOG(LogLandscapeCombinator, Log, TEXT("Deleting %s."), *File);
				IPlatformFile::GetPlatformPhysical().DeleteFile(*File);
				IPlatformFile::GetPlatformPhysical().DeleteDirectoryRecursively(*File);
			}
		}
	}

	FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Deleting", "Finished Deleting files."));
}

#undef LOCTEXT_NAMESPACE