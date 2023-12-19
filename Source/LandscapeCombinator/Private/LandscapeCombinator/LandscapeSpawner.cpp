// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeSpawner.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"
#include "LandscapeCombinator/LandscapeController.h"

#include "ImageDownloader/TilesCounter.h"
#include "Coordinates/DecalCoordinates.h"
#include "HeightmapModifier/HeightmapModifier.h"
#include "HeightmapModifier/BlendLandscape.h"
#include "LandscapeUtils/LandscapeUtils.h"
#include "Coordinates/LevelCoordinates.h"
#include "Async/Async.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

ALandscapeSpawner::ALandscapeSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	HeightmapDownloader = CreateDefaultSubobject<UImageDownloader>(TEXT("HeightmapDownloader"));

	TextureDownloader = CreateDefaultSubobject<UImageDownloader>(TEXT("TextureDownloader"));

	MapboxSatelliteDownloader = CreateDefaultSubobject<UImageDownloader>(TEXT("MapboxSatelliteDownloader"));
}

bool GetPixels(FIntPoint& InsidePixels, TArray<FString> Files)
{
	if (Files.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("UImageDownloader::GetPixels", "Image Downloader Error: Empty list of files when trying to read the size")
		); 
		return false;
	}

	if (!GDALInterface::GetPixels(InsidePixels, Files[0])) return false;

	if (Files.Num() == 1) return true;

	TilesCounter TilesCounter(Files);
	TilesCounter.ComputeMinMaxTiles();
	InsidePixels[0] *= (TilesCounter.LastTileX + 1);
	InsidePixels[1] *= (TilesCounter.LastTileY + 1);
	return true;
}

bool GetCmPerPixelForCRS(FString CRS, int &CmPerPixel)
{
	if (CRS == "EPSG:4326" || CRS == "IGNF:WGS84G" || CRS == "EPSG:4269" || CRS == "EPSG:497" || CRS == "CRS:84")
	{
		CmPerPixel = 11111111;
		return true;
	}
	else if (CRS == "IGNF:LAMB93" || CRS == "EPSG:2154" || CRS == "EPSG:4559" || CRS == "EPSG:2056" || CRS == "EPSG:3857" || CRS == "EPSG:25832")
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
	if (!HeightmapDownloader)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("ALandscapeSpawner::SpawnLandscape", "HeightmapDownloader is not set, you may want to create one, or spawn a new LandscapeTexturer")
		);
		return;
	}

	if (ALevelCoordinates::GetGlobalCoordinates(this->GetWorld(), false))
	{
		EAppReturnType::Type UserResponse = FMessageDialog::Open(EAppMsgType::OkCancel,
			LOCTEXT(
				"ALandscapeSpawner::SpawnLandscape::ExistingGlobal",
				"There already exists a LevelCoordinates actor.\n"
				"Press OK if you want to spawn your landscape with respect to these level coordinates.\n"
				"Press Cancel if you want to delete the existing LevelCoordinates, and then press Spawn Landscape again."
			)
		);
		if (UserResponse == EAppReturnType::Cancel) 
		{
			UE_LOG(LogLandscapeCombinator, Log, TEXT("User cancelled spawning landscape."));
			return;
		}
	}
	
	FVector2D *Altitudes = new FVector2D();
	FString *CRS = new FString();
	FVector4d *Coordinates= new FVector4d();

	HMFetcher* Fetcher = HeightmapDownloader->CreateFetcher(
		LandscapeLabel,
		true,
		true,
		true,
		[Altitudes, Coordinates, CRS](HMFetcher *FetcherBeforePNG)
		{
			*CRS = FetcherBeforePNG->OutputCRS;
			return GDALInterface::GetMinMax(*Altitudes, FetcherBeforePNG->OutputFiles) &&
				   GDALInterface::GetCoordinates(*Coordinates, FetcherBeforePNG->OutputFiles);
		}
	);

	if (!Fetcher)
	{			
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("NoFetcher", "There was an error while creating the fetcher for Landscape {0}."),
				FText::FromString(LandscapeLabel)
			)
		); 
		return;
	}

	Fetcher->Fetch("", TArray<FString>(), [Fetcher, Altitudes, CRS, Coordinates, this](bool bSuccess)
	{
		if (bSuccess)
		{
			// GameThread to spawn a landscape
			AsyncTask(ENamedThreads::GameThread, [bSuccess, Fetcher, Altitudes, CRS, Coordinates, this]()
			{
				int CmPerPixel = 0;
				TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(this->GetWorld(), false);

				if (!GlobalCoordinates && !GetCmPerPixelForCRS(Fetcher->OutputCRS, CmPerPixel))
				{
					FMessageDialog::Open(EAppMsgType::Ok,
						FText::Format(
							LOCTEXT("ErrorBound",
								"Please create a LevelCoordinates actor with CRS: '{0}', and set the scale that you wish here.\n"
								"Then, try again to spawn your landscape."
							),
							FText::FromString(Fetcher->OutputCRS)
						)
					);
					
					delete Fetcher;
					return;
				}

				ALandscape *CreatedLandscape = LandscapeUtils::SpawnLandscape(
					Fetcher->OutputFiles, LandscapeLabel, bCreateLandscapeStreamingProxies,
					ComponentsMethod == EComponentsMethod::Auto || ComponentsMethod == EComponentsMethod::AutoWithoutBorder,
					ComponentsMethod == EComponentsMethod::AutoWithoutBorder,
					QuadsPerSubsection, SectionsPerComponent, ComponentCount
				);

				if (CreatedLandscape)	
				{
					CreatedLandscape->SetActorLabel(LandscapeLabel);

					if (!GlobalCoordinates)
					{
						ALevelCoordinates *LevelCoordinates = this->GetWorld()->SpawnActor<ALevelCoordinates>();
						TObjectPtr<UGlobalCoordinates> NewGlobalCoordinates = LevelCoordinates->GlobalCoordinates;

						NewGlobalCoordinates->CRS = Fetcher->OutputCRS;
						NewGlobalCoordinates->CmPerLongUnit = CmPerPixel;
						NewGlobalCoordinates->CmPerLatUnit = -CmPerPixel;

						double MinCoordWidth = (*Coordinates)[0];
						double MaxCoordWidth = (*Coordinates)[1];
						double MinCoordHeight = (*Coordinates)[2];
						double MaxCoordHeight = (*Coordinates)[3];
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

					LandscapeController->Coordinates = *Coordinates;
					LandscapeController->CRS = *CRS;
					LandscapeController->Altitudes = *Altitudes;
					GetPixels(LandscapeController->InsidePixels, Fetcher->OutputFiles);
					LandscapeController->ZScale = ZScale;

					LandscapeController->AdjustLandscape();

					delete Fetcher;
					UE_LOG(LogLandscapeCombinator, Log, TEXT("Created Landscape %s successfully."), *LandscapeLabel);

					if (!bCreateMapboxDecals && !bCreateCustomDecals)
					{
						FMessageDialog::Open(EAppMsgType::Ok,
							FText::Format(
								LOCTEXT("LandscapeCreated", "Landscape {0} was created successfully"),
								FText::FromString(LandscapeLabel)
							)
						);
					}

					if (bCreateMapboxDecals)
					{
						if (!MapboxSatelliteDownloader)
						{
							FMessageDialog::Open(EAppMsgType::Ok,
								FText::Format(
									LOCTEXT("NoMapboxSatelliteDownloader", "Internal Error: The Mapbox Satellite Downloader is not set. Please try again with a new Landscape Spawner."),
									FText::FromString(LandscapeLabel)
								)
							);
							return;
						}

						MapboxSatelliteDownloader->ImageSourceKind = EImageSourceKind::Mapbox_Satellite;
						if (HeightmapDownloader && HeightmapDownloader->ImageSourceKind == EImageSourceKind::Mapbox_Heightmaps)
						{
							MapboxSatelliteDownloader->Mapbox_Token = HeightmapDownloader->Mapbox_Token;
						}
						else
						{
							MapboxSatelliteDownloader->Mapbox_Token = Decals_Mapbox_Token;
						}

						if (HeightmapDownloader && HeightmapDownloader->ImageSourceKind == EImageSourceKind::Mapbox_Heightmaps && HeightmapDownloader->XYZ_Zoom == Decals_Mapbox_Zoom)
						{
							MapboxSatelliteDownloader->ParametersSelection = EParametersSelection::Manual;
							MapboxSatelliteDownloader->XYZ_MinX = HeightmapDownloader->XYZ_MinX;
							MapboxSatelliteDownloader->XYZ_MaxX = HeightmapDownloader->XYZ_MaxX;
							MapboxSatelliteDownloader->XYZ_MinY = HeightmapDownloader->XYZ_MinY;
							MapboxSatelliteDownloader->XYZ_MaxY = HeightmapDownloader->XYZ_MaxY;
						}
						else
						{
							MapboxSatelliteDownloader->ParametersSelection = EParametersSelection::FromBoundingActor;
							MapboxSatelliteDownloader->ParametersBoundingActor = CreatedLandscape;
						}
						
						MapboxSatelliteDownloader->Mapbox_2x = Decals_Mapbox_2x;
						MapboxSatelliteDownloader->XYZ_Zoom = Decals_Mapbox_Zoom;

						//MapboxSatelliteDownloader->DownloadImages([this](TArray<FString> DownloadedImages)
						//{
						//	for (auto &DownloadedImage : DownloadedImages)
						//	{
						//		UDecalCoordinates::CreateDecal(this->GetWorld(), DownloadedImage);
						//	}

						MapboxSatelliteDownloader->DownloadMergedImage([this](FString DownloadedImage)
						{
							UDecalCoordinates::CreateDecal(this->GetWorld(), DownloadedImage);
							FMessageDialog::Open(EAppMsgType::Ok,
								FText::Format(
									LOCTEXT("LandscapeCreated2", "Landscape {0} was created successfully with Mapbox Decals"),
									FText::FromString(LandscapeLabel)
								)
							);
						});
					}

					if (bCreateCustomDecals)
					{
						if (!TextureDownloader)
						{
							FMessageDialog::Open(EAppMsgType::Ok,
								FText::Format(
									LOCTEXT("NoMapboxSatelliteDownloader", "Internal Error: The Texture Downloader is not set. Please try again with a new Landscape Spawner."),
									FText::FromString(LandscapeLabel)
								)
							);
							delete Fetcher;
							return;
						}

						//TextureDownloader->DownloadImages([this](TArray<FString> DownloadedImages)
						//{
						//	for (auto &DownloadedImage : DownloadedImages)
						//	{
						//		UDecalCoordinates::CreateDecal(this->GetWorld(), DownloadedImage);
						//	}

						TextureDownloader->DownloadMergedImage([this](FString DownloadedImage)
						{
							UDecalCoordinates::CreateDecal(this->GetWorld(), DownloadedImage);
							FMessageDialog::Open(EAppMsgType::Ok,
								FText::Format(
									LOCTEXT("LandscapeCreated3", "Landscape {0} was created successfully with TextureDownloader Decals"),
									FText::FromString(LandscapeLabel)
								)
							);
						});
					}
				}
				else
				{
					delete Fetcher;
					UE_LOG(LogLandscapeCombinator, Error, TEXT("Could not create Landscape %s."), *LandscapeLabel);
					FMessageDialog::Open(EAppMsgType::Ok,
						FText::Format(
							LOCTEXT("LandscapeNotCreated", "Landscape {0} could not be created."),
							FText::FromString(LandscapeLabel)
						)
					);
				}
			});
			return;
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

			delete Fetcher;
			return;
		}
	});

	return;
}


void ALandscapeSpawner::SetComponentCountFromMethod()
{
	switch (ComponentsMethod)
	{
	case EComponentsMethod::Manual:
		break;
	case EComponentsMethod::AutoWithoutBorder:
		break;
	case EComponentsMethod::Auto:
		break;

	case EComponentsMethod::Preset_8129_8129:
		QuadsPerSubsection = 127;
		SectionsPerComponent = 2;
		ComponentCount = FIntPoint(32, 32);
		break;

	case EComponentsMethod::Preset_4033_4033:
		QuadsPerSubsection = 63;
		SectionsPerComponent = 2;
		ComponentCount = FIntPoint(32, 32);
		break;

	case EComponentsMethod::Preset_2017_2017:
		QuadsPerSubsection = 63;
		SectionsPerComponent = 2;
		ComponentCount = FIntPoint(16, 16);
		break;

	case EComponentsMethod::Preset_1009_1009:
		QuadsPerSubsection = 63;
		SectionsPerComponent = 2;
		ComponentCount = FIntPoint(8, 8);
		break;

	case EComponentsMethod::Preset_1009_1009_2:
		QuadsPerSubsection = 63;
		SectionsPerComponent = 1;
		ComponentCount = FIntPoint(16, 16);
		break;

	case EComponentsMethod::Preset_505_505:
		QuadsPerSubsection = 63;
		SectionsPerComponent = 2;
		ComponentCount = FIntPoint(4, 4);
		break;

	case EComponentsMethod::Preset_505_505_2:
		QuadsPerSubsection = 63;
		SectionsPerComponent = 1;
		ComponentCount = FIntPoint(8, 8);
		break;

	case EComponentsMethod::Preset_253_253:
		QuadsPerSubsection = 63;
		SectionsPerComponent = 2;
		ComponentCount = FIntPoint(2, 2);
		break;

	case EComponentsMethod::Preset_253_253_2:
		QuadsPerSubsection = 63;
		SectionsPerComponent = 1;
		ComponentCount = FIntPoint(4, 4);
		break;

	case EComponentsMethod::Preset_127_127:
		QuadsPerSubsection = 63;
		SectionsPerComponent = 2;
		ComponentCount = FIntPoint(1, 1);
		break;

	case EComponentsMethod::Preset_127_127_2:
		QuadsPerSubsection = 63;
		SectionsPerComponent = 1;
		ComponentCount = FIntPoint(2, 2);
		break;

	default:
		break;
	}
}


void ALandscapeSpawner::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	if (!Event.Property)
	{
		Super::PostEditChangeProperty(Event);
		return;
	}

	FName PropertyName = Event.Property->GetFName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(ALandscapeSpawner, ComponentsMethod))
	{
		SetComponentCountFromMethod();
	}

	Super::PostEditChangeProperty(Event);
}


#undef LOCTEXT_NAMESPACE
