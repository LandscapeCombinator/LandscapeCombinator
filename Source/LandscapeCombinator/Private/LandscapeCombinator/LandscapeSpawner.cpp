// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeSpawner.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"
#include "LandscapeCombinator/LandscapeController.h"

#include "ImageDownloader/TilesCounter.h"
#include "Coordinates/DecalCoordinates.h"
#include "HeightmapModifier/HeightmapModifier.h"
#include "HeightmapModifier/BlendLandscape.h"
#include "LandscapeUtils/LandscapeUtils.h"
#include "Coordinates/LevelCoordinates.h"
#include "ImageDownloader/HMDebugFetcher.h"
#include "ImageDownloader/Transformers/HMMerge.h"
#include "LCCommon/LCSettings.h"
#include "LCCommon/LCBlueprintLibrary.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Async/Async.h"
#include "Components/DecalComponent.h"
#include "Internationalization/Regex.h"
#include "Kismet/GameplayStatics.h"
#include "LandscapeSubsystem.h"
#include "Misc/MessageDialog.h"
#include "Misc/EngineVersionComparison.h"
#include "HAL/ThreadManager.h"

#if WITH_EDITOR

#include "Editor.h"
#include "EditorModes.h"
#include "EditorModeManager.h"
#include "LandscapeEditorObject.h"
#include "LandscapeImportHelper.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Styling/AppStyle.h"

#endif

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

ALandscapeSpawner::ALandscapeSpawner() : AActor()
{
	PrimaryActorTick.bCanEverTick = false;

	HeightmapDownloader = CreateDefaultSubobject<UImageDownloader>(TEXT("HeightmapDownloader"));
	DecalDownloader = CreateDefaultSubobject<UImageDownloader>(TEXT("DecalDownloader"));
	PositionBasedGeneration = CreateDefaultSubobject<ULCPositionBasedGeneration >(TEXT("PositionBasedGeneration"));
}

TArray<UObject*> ALandscapeSpawner::GetGeneratedObjects() const
{
	TArray<UObject*> Result;

	for (auto &DecalActor : DecalActors)
	{
		ADecalActor *LoadedDecalActor = DecalActor.LoadSynchronous();
		if (LoadedDecalActor)
		{
			Result.Add(DecalActor.LoadSynchronous());
		}
		else
		{
			UE_LOG(LogLandscapeCombinator, Log, TEXT("Skipping invalid decal pointer in landscape spawner %s"), *GetActorNameOrLabel())
		}
	}

	for (auto &LandscapeStreamingProxy: SpawnedLandscapeStreamingProxies)
	{
		Result.Add(LandscapeStreamingProxy.LoadSynchronous());
	}
	Result.Add(SpawnedLandscape.LoadSynchronous());

	return Result;
}

bool GetPixels(FIntPoint& InsidePixels, TArray<FString> Files)
{
	if (Files.IsEmpty())
	{
		LCReporter::ShowError(
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

void ALandscapeSpawner::DeleteLandscape()
{
	Execute_Cleanup(this, false);
}

#if WITH_EDITOR

AActor *ALandscapeSpawner::Duplicate(FName FromName, FName ToName)
{
	if (ALandscapeSpawner *NewLandscapeSpawner =
		Cast<ALandscapeSpawner>(GEditor->GetEditorSubsystem<UEditorActorSubsystem>()->DuplicateActor(this)))
	{
		NewLandscapeSpawner->LandscapeTag = ULCBlueprintLibrary::ReplaceName(LandscapeTag, FromName, ToName);
		NewLandscapeSpawner->LandscapeLabel = ULCBlueprintLibrary::Replace(LandscapeLabel, FromName.ToString(), ToName.ToString());
		NewLandscapeSpawner->LandscapeToBlendWith.ActorTag =
			ULCBlueprintLibrary::ReplaceName(LandscapeToBlendWith.ActorTag, FromName, ToName);

		return NewLandscapeSpawner;
	}
	else
	{
		LCReporter::ShowError(LOCTEXT("ALandscapeSpawner::DuplicateActor", "Failed to duplicate actor."));
		return nullptr;
	}
}

bool ALandscapeSpawner::OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated)
{
	if (!Concurrency::RunOnGameThreadAndWait([this, bIsUserInitiated]() { return Execute_Cleanup(this, !bIsUserInitiated); })) return false;

	TObjectPtr<ALandscape> Landscape;
	TArray<ADecalActor*> Decals;
	return SpawnLandscape(SpawnedActorsPathOverride, bIsUserInitiated, Landscape, Decals);
}

bool ALandscapeSpawner::SpawnLandscape(FName SpawnedActorsPathOverride, bool bIsUserInitiated, TObjectPtr<ALandscape>& OutLandscape, TArray<ADecalActor*>& OutDecals)
{
	Modify();

#if UE_VERSION_OLDER_THAN(5,6,0)
	if (SpawnMethod == ESpawnMethod::ExtendExistingLandscape && SpawnMethod == ESpawnMethod::CreateFreshLandscapeIncrementally)
	{
		LCReporter::ShowError(
			LOCTEXT("ALandscapeSpawner::Unsupported",
				"Incremental landscape spawning and landscape extensions have only been implemented for UE 5.6 and up"
			)
		);
		
		return false;
	}
#endif

	if (!IsValid(HeightmapDownloader))
	{
		LCReporter::ShowError(
			LOCTEXT("ALandscapeSpawner::OnGenerate", "HeightmapDownloader is not set, you may want to create one, or create a new LandscapeSpawner")
		);

		return false;
	}

	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(this->GetWorld(), false);
	if (IsValid(GlobalCoordinates))
	{
		if (bIsUserInitiated && !Concurrency::RunOnGameThreadAndWait([&]() { return LCReporter::ShowMessage(
			LOCTEXT(
				"ALandscapeSpawner::OnGenerate::ExistingGlobal",
				"There already exists a LevelCoordinates actor. Continue?\n"
				"Press Yes if you want to spawn your landscape with respect to these level coordinates.\n"
				"Press No if you want to stop, then manually delete the existing LevelCoordinates, and then try again."
			),
			"SuppressExistingLevelCoordinates",
			LOCTEXT("ExistingGlobalTitle", "Already Existing Level Coordinates")
		); }))
		{
			return false;
		}
	}
	
	FVector2D *Altitudes = new FVector2D();
	FString *CRS = new FString();
	FVector4d *Coordinates= new FVector4d();

	ULandscapeSubsystem* LandscapeSubsystem = GetWorld()->GetSubsystem<ULandscapeSubsystem>();
	bool bIsGridBased = LandscapeSubsystem && LandscapeSubsystem->IsGridBased();

	if (SpawnMethod == ESpawnMethod::CreateFreshLandscape && !HeightmapDownloader->bMergeImages && !bIsGridBased)
	{
		LCReporter::ShowError(
			LOCTEXT("ALandscapeSpawner::OnGenerate::Merge",
				"You cannot create a landscape with multiple tiles in a basic level. You should either: "
				"1) Select CreateFreshLandscapeIncrementally option, "
				"2) Select the \"Merge Images\" option in the LandscapeSpawner, or "
				"3) Use a World Partition level (Open World)"
			)
		);
		
		return false;
	}

	if (HeightmapDownloader->bMergeImages && (SpawnMethod == ESpawnMethod::CreateFreshLandscapeIncrementally || SpawnMethod == ESpawnMethod::ExtendExistingLandscape))
	{
		if (!LCReporter::ShowMessage(
			LOCTEXT("ALandscapeSpawner::SpawnLandscape::Merge",
				"It's recommended to disable the \"Merge Images\" option in the LandscapeSpawner when "
				"using incremental or extension landscape spawning. Do you still want to continue with the \"Merge Images\" option enabled?"),
			"SuppressIncrementalMerge"
		))
		{
			return false;
		}
	}

	if ((ComponentsMethod == EComponentsMethod::Auto || ComponentsMethod == EComponentsMethod::AutoWithoutBorder) &&
		(SpawnMethod == ESpawnMethod::CreateFreshLandscapeIncrementally))
	{
		if (!LCReporter::ShowMessage(
			LOCTEXT("ALandscapeSpawner::SpawnLandscape::IncrementalAuto",
				"When creating a landscape incrementally, it's recommended to specify the number of components "
				"manually. Using 'Auto' or 'Auto Without Border' can make very small components and the generation will be very slow.\n"
				"Please choose a component count that matches one tile of your heightmap files.\n"
				"Ignore this and continue?"
			),
			"SuppressIncrementalAuto"
		))
		{
			return false;
		}
	}

	FString Name = GetWorld()->GetName() + "-" + LandscapeLabel;
	HMFetcher* Fetcher = HeightmapDownloader->CreateFetcher(
		bIsUserInitiated,
		Name,
		true,
		true,
		SpawnMethod == ESpawnMethod::CreateFreshLandscape, // convert to PNG only for non-extension
		SpawnMethod == ESpawnMethod::CreateFreshLandscapeIncrementally, // convert only first file to PNG for incremental spawning
		SpawnMethod == ESpawnMethod::CreateFreshLandscape, // add missing tiles only for non-incremental mode
		[Altitudes, Coordinates, CRS, this](HMFetcher *FetcherBeforePNG)
		{
			*CRS = FetcherBeforePNG->OutputCRS;
			TArray<FString> Files = FetcherBeforePNG->OutputFiles;
			if (SpawnMethod == ESpawnMethod::CreateFreshLandscapeIncrementally)
			{
				Files = { Files[0] };
			}
			return GDALInterface::GetMinMax(*Altitudes, Files) &&
				   GDALInterface::GetCoordinates(*Coordinates, Files);
		},
		GlobalCoordinates
	);

	if (!Fetcher)
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("NoFetcher", "There was an error while creating the fetcher for Landscape {0}."),
				FText::FromString(LandscapeLabel)
			)
		);
		
		return false;
	}

	if (!Fetcher->Fetch("", TArray<FString>()))
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("LandscapeCreated", "Could not create heightmaps files for Landscape {0}."),
				FText::FromString(LandscapeLabel)
			)
		);
		
		delete Fetcher;
		return false;
	}

	TArray<FString> Files = Fetcher->OutputFiles;
	FString FilesCRS = Fetcher->OutputCRS;

	delete Fetcher;

	int CmPerPixel = 0;

	if (!IsValid(GlobalCoordinates) && !ULCBlueprintLibrary::GetCmPerPixelForCRS(FilesCRS, CmPerPixel))
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("ErrorBound",
					"Please create a LevelCoordinates actor with CRS: '{0}', and set the scale that you wish here.\n"
					"Then, try again to spawn your landscape."
				),
				FText::FromString(FilesCRS)
			)
		);
		
		return false;
	}

	if (SpawnMethod == ESpawnMethod::CreateFreshLandscape || SpawnMethod == ESpawnMethod::CreateFreshLandscapeIncrementally)
	{
		if (!Concurrency::RunOnGameThreadAndWait([&]() {

			TArray<FString> Files2;
			if (SpawnMethod == ESpawnMethod::CreateFreshLandscapeIncrementally)
			{
				// move file to a subfolder to avoid `LandscapeUtils::SpawnLandscape` using the other files
				const FString BaseFolder = FPaths::GetPath(Files[0]);
				const FString SubFolder = BaseFolder / "Temp";
				IPlatformFile &PlatformFile = IPlatformFile::GetPlatformPhysical();
				if (!PlatformFile.CreateDirectory(*SubFolder))
				{
					LCReporter::ShowError(
						FText::Format(
							LOCTEXT("CreateTempSubdirectory", "Cannot create Temp subdirecotry in {0}"),
							FText::FromString(BaseFolder)
						)
					);
					return false;
				}

				// copy Files[0] to subfolder
				FString NewFile = SubFolder / "Temp." + FPaths::GetExtension(Files[0]);
				if (FPaths::FileExists(NewFile))
				{
					PlatformFile.DeleteFile(*NewFile);
				}
				if (!PlatformFile.CopyFile(*NewFile, *Files[0]))
				{
					LCReporter::ShowError(
						FText::Format(
							LOCTEXT("CopyFile", "Cannot copy {0} to {1}"),
							FText::FromString(Files[0]),
							FText::FromString(NewFile)
						)
					);
					return false;
				}
				Files2 = { NewFile };
			}
			else
			{
				Files2 = Files;
			}

			bool bSpawnLandscapeSuccess = LandscapeUtils::SpawnLandscape(
				SpawnMethod == ESpawnMethod::CreateFreshLandscapeIncrementally,
				Files2, LandscapeLabel, bCreateLandscapeStreamingProxies,
				ComponentsMethod == EComponentsMethod::Auto || ComponentsMethod == EComponentsMethod::AutoWithoutBorder,
				ComponentsMethod == EComponentsMethod::AutoWithoutBorder && SpawnMethod == ESpawnMethod::CreateFreshLandscape,
				QuadsPerSubsection, SectionsPerComponent, ComponentCount,
				OutLandscape, SpawnedLandscapeStreamingProxies
			);

			if (!bSpawnLandscapeSuccess || !IsValid(OutLandscape))
			{
				LCReporter::ShowError(
					FText::Format(
						LOCTEXT("LandscapeNotCreated", "Landscape {0} could not be created."),
						FText::FromString(LandscapeLabel)
					)
				);
				return false;
			}

			SpawnedLandscape = OutLandscape;
			SpawnedLandscape->Modify();
			if (!LandscapeLabel.IsEmpty())
				SpawnedLandscape->SetActorLabel(LandscapeLabel);
			ULCBlueprintLibrary::SetFolderPath2(SpawnedLandscape.Get(), SpawnedActorsPathOverride, SpawnedActorsPath);

			if (!LandscapeTag.IsNone()) SpawnedLandscape->Tags.Add(LandscapeTag);

			if (IsValid(LandscapeMaterial))
			{
				SpawnedLandscape->LandscapeMaterial = LandscapeMaterial;
				SpawnedLandscape->PostEditChange();
			}

			if (!IsValid(GlobalCoordinates))
			{
				ALevelCoordinates *LevelCoordinates = this->GetWorld()->SpawnActor<ALevelCoordinates>();
				TObjectPtr<UGlobalCoordinates> NewGlobalCoordinates = LevelCoordinates->GlobalCoordinates;

				NewGlobalCoordinates->CRS = FilesCRS;
				NewGlobalCoordinates->CmPerLongUnit = CmPerPixel;
				NewGlobalCoordinates->CmPerLatUnit = -CmPerPixel;

				double MinCoordWidth = (*Coordinates)[0];
				double MaxCoordWidth = (*Coordinates)[1];
				double MinCoordHeight = (*Coordinates)[2];
				double MaxCoordHeight = (*Coordinates)[3];
				NewGlobalCoordinates->WorldOriginLong = (MinCoordWidth + MaxCoordWidth) / 2;
				NewGlobalCoordinates->WorldOriginLat = (MinCoordHeight + MaxCoordHeight) / 2;
			}

			ULandscapeController *LandscapeController = NewObject<ULandscapeController>(SpawnedLandscape->GetRootComponent());
			LandscapeController->RegisterComponent();
			SpawnedLandscape->AddInstanceComponent(LandscapeController);

			UHeightmapModifier *HeightmapModifier = NewObject<UHeightmapModifier>(SpawnedLandscape->GetRootComponent());
			HeightmapModifier->RegisterComponent();
			SpawnedLandscape->AddInstanceComponent(HeightmapModifier);

			LandscapeController->Coordinates = *Coordinates;
			LandscapeController->CRS = *CRS;
			LandscapeController->Altitudes = *Altitudes;
			GetPixels(LandscapeController->InsidePixels, Files2);
			LandscapeController->ZScale = ZScale;

			LandscapeController->AdjustLandscape();
			return true;
		}))
		{
			return false;
		}
	}

	if (SpawnMethod == ESpawnMethod::CreateFreshLandscapeIncrementally)
	{
		if (!Concurrency::RunOnGameThreadAndWait([&]() {
			return LandscapeUtils::ExtendLandscape(OutLandscape, Files);
		}))
			return false;
	}
	else if (SpawnMethod == ESpawnMethod::ExtendExistingLandscape)
	{
		if (!Concurrency::RunOnGameThreadAndWait([&]() {
			return LandscapeUtils::ExtendLandscape(LandscapeToExtend, Files);
		}))
			return false;
	}

	Concurrency::RunOnGameThreadAndWait([&]() {
		UBlendLandscape *BlendLandscape = NewObject<UBlendLandscape>(SpawnedLandscape->GetRootComponent());
		BlendLandscape->RegisterComponent();
		SpawnedLandscape->AddInstanceComponent(BlendLandscape);

		if (bBlendLandscapeWithAnotherLandscape)
		{
			if (ALandscape *OtherLandscape = Cast<ALandscape>(LandscapeToBlendWith.GetActor(GetWorld())))
			{
				BlendLandscape->LandscapeToBlendWith = OtherLandscape;
				BlendLandscape->BlendWithLandscape(bIsUserInitiated);
			}
			else
			{
				FMessageDialog::Open(
					EAppMsgCategory::Error,
					EAppMsgType::Ok,
					FText::Format(
						LOCTEXT("BlendingError",
							"There was an error while getting the landscape to blend with,\n"
							"please double-check the settings."
						),
						FText::FromString(LandscapeLabel)
					)
				);
			}
		}

		UE_LOG(LogLandscapeCombinator, Log, TEXT("Created Landscape %s successfully."), *LandscapeLabel);
		
		return true;
	});

	switch (DecalCreation)
	{
		case EDecalCreation::None:
		{
			if (bIsUserInitiated)
			{
				LCReporter::ShowMessage(
					FText::Format(
						LOCTEXT("LandscapeCreated", "Landscape {0} was created successfully"),
						FText::FromString(LandscapeLabel)
					),
					"SuppressSpawnedLandscapeInfo",
					LOCTEXT("SpawnedLandscapeTitle", "Spawned Landscape"),
					false,
					FAppStyle::GetBrush("Icons.InfoWithColor.Large")
				);
			}
		
			return true;
		}

		case EDecalCreation::Mapbox:
		case EDecalCreation::MapTiler:
		{
			if (!IsValid(DecalDownloader))
			{
				FMessageDialog::Open(
					EAppMsgCategory::Error,
					EAppMsgType::Ok,
					FText::Format(
						LOCTEXT("NoDecalDownloader", "Internal Error: The Decal Downloader is not set. Please try again with a new Landscape Spawner."),
						FText::FromString(LandscapeLabel)
					)
				);

				return false;
			}

			if (!Concurrency::RunOnGameThreadAndWait([&]() {
				DecalDownloader->Modify();
				DecalDownloader->ParametersSelection = EParametersSelection::FromBoundingActor;
				DecalDownloader->ParametersBoundingActor = SpawnedLandscape.Get();

				if (DecalCreation == EDecalCreation::Mapbox)
				{
					DecalDownloader->ImageSourceKind = EImageSourceKind::Mapbox_Satellite;
					DecalDownloader->Mapbox_Token = Decals_Mapbox_Token;
					DecalDownloader->Mapbox_2x = Decals_Mapbox_2x;
					DecalDownloader->XYZ_Zoom = Decals_Mapbox_Zoom;
				}
				else if (DecalCreation == EDecalCreation::MapTiler)
				{
					DecalDownloader->ImageSourceKind = EImageSourceKind::MapTiler_Satellite;
					DecalDownloader->MapTiler_Token = Decals_MapTiler_Token;
					DecalDownloader->XYZ_Zoom = Decals_MapTiler_Zoom;
				}
				else
				{
					check(false);
					return false;
				}

				DecalDownloader->bMergeImages = bDecalMergeImages;
				return true;
			}))
			{
				return false;
			}


			TArray<FString> DownloadedImages;
			FString ImagesCRS;
			if (!DecalDownloader->DownloadImages(bIsUserInitiated, false, GlobalCoordinates, DownloadedImages, ImagesCRS)) return false;
			TArray<ADecalActor*> NewDecals = UDecalCoordinates::CreateDecals(this->GetWorld(), DownloadedImages);
			DecalActors.Append(NewDecals);

#if WITH_EDITOR
			Concurrency::RunOnGameThreadAndWait([this, &NewDecals, SpawnedActorsPathOverride]() {
				for (auto &DecalActor : NewDecals)
				{
					if (IsValid(DecalActor))
					{
						ULCBlueprintLibrary::SetFolderPath2(DecalActor, SpawnedActorsPathOverride, SpawnedActorsPath);
						DecalActor->GetDecal()->SortOrder = DecalsSortOrder;
					}
				}
				return true;
			});
#endif

			if (NewDecals.IsEmpty())
			{
				LCReporter::ShowError(
					FText::Format(
						LOCTEXT("LandscapeDecalsCreatedError", "There was an error while creating decals for Landscape {0}"),
						FText::FromString(LandscapeLabel)
					)
				);
				return false;
			}
			else if (bIsUserInitiated)
			{
				LCReporter::ShowInfo(
					FText::Format(
						LOCTEXT("LandscapeCreated2", "Landscape {0} was created successfully with {1} Decals"),
						FText::FromString(LandscapeLabel),
						FText::AsNumber(DecalActors.Num())
					),
					"SuppressSpawnedLandscapeInfo",
					LOCTEXT("SpawnedLandscapeTitle", "Spawned Landscape with Decals")
				);
			}
			return true;
		}

		default:
		{
			check(false)
			return false;
		}

	}

	return true;
}

#endif

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

#if WITH_EDITOR

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

#endif

bool ALandscapeSpawner::HasMapboxToken()
{
	return UImageDownloader::HasMapboxToken();
}

bool ALandscapeSpawner::HasMapTilerToken()
{
	return UImageDownloader::HasMapTilerToken();
}


#undef LOCTEXT_NAMESPACE
