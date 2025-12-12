// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "SplineImporter/SplineImporter.h"
#include "SplineImporter/LogSplineImporter.h"
#include "SplineImporter/Overpass.h"
#include "LandscapeUtils/LandscapeUtils.h"
#include "GDALInterface/GDALInterface.h"
#include "OSMUserData/OSMUserData.h"
#include "LCCommon/LCSettings.h"
#include "LCCommon/LCBlueprintLibrary.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "LandscapeSplineActor.h"
#include "ILandscapeSplineInterface.h"
#include "EditorSupportDelegates.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeSplineControlPoint.h" 
#include "LandscapeSplinesComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/SWindow.h"
#include "Misc/CString.h"
#include "Logging/StructuredLog.h"
#include "Async/Async.h"
#include "Stats/Stats.h"
#include "Stats/StatsMisc.h"
#include "ObjectEditorUtils.h"
#include "Engine/World.h"
#include "Misc/MessageDialog.h"
#include "Polygon2.h"
#include "Algo/Reverse.h"
#include "Styling/AppStyle.h"
#include "Subsystems/PCGSubsystem.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

using namespace UE::Geometry;

void ASplineImporter::SetOverpassShortQuery()
{
	if (Source == EVectorSource::OSM_Roads)
	{
		OverpassShortQuery = "way[\"highway\"][\"highway\"!~\"path\"][\"highway\"!~\"track\"];";
	}
	else if (Source == EVectorSource::OSM_Buildings)
	{
		OverpassShortQuery = "way[\"building\"];";
	}
	else if (Source == EVectorSource::OSM_Rivers)
	{
		OverpassShortQuery = "way[\"waterway\"=\"river\"];";
	}
	else if (Source == EVectorSource::OSM_Forests)
	{
		OverpassShortQuery = "way[\"landuse\"=\"forest\"];way[\"natural\"=\"wood\"];";
	}
	else if (Source == EVectorSource::OSM_Beaches)
	{
		OverpassShortQuery = "way[\"natural\"=\"beach\"];";
	}
	else if (Source == EVectorSource::OSM_Parks)
	{
		OverpassShortQuery = "way[\"leisure\"=\"park\"];";
	}
	else if (Source == EVectorSource::OSM_SkiSlopes)
	{
		OverpassShortQuery = "way[\"piste:type\"=\"downhill\"][!\"area\"];";
	}
	else if (Source == EVectorSource::OSM_Grass)
	{
		OverpassShortQuery = "way[\"landuse\"=\"grass\"];way[\"natural\"=\"grassland\"];";
	}

	Super::SetOverpassShortQuery();
}

bool ASplineImporter::OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated)
{
	Modify();

	TArray<AActor*> ActorsOrLandscapesToPlaceSplines;
	
	Concurrency::RunOnGameThreadAndWait([&ActorsOrLandscapesToPlaceSplines, this]() {
		ActorsOrLandscapesToPlaceSplines = ActorsOrLandscapesToPlaceSplinesSelection.GetAllActors(this->GetWorld());
		return true;
	});

	if (ActorsOrLandscapesToPlaceSplines.IsEmpty())
	{
		LCReporter::ShowError(
			LOCTEXT("ASplineImporter::GenerateSplines::1", "Please select an actor on which to place your splines.")
		);

		return false;
	}

	for (auto &ActorOrLandscapeToPlaceSplines: ActorsOrLandscapesToPlaceSplines)
	{
		if (
			!ActorOrLandscapeToPlaceSplines->GetRootComponent() ||
			ActorOrLandscapeToPlaceSplines->GetRootComponent()->Mobility != EComponentMobility::Static
		)
		{
			LCReporter::ShowError(FText::Format(
				LOCTEXT("ASplineImporter::GenerateSplines::NoStatic", "Please make sure that {0}'s mobility is set to static."),
				FText::FromString(ActorOrLandscapeToPlaceSplines->GetActorNameOrLabel())
			));
			
			return false;
		}
	}

	FText IntroMessage;
	ALandscape *Landscape = nullptr;
	FString ActorOrLandscapeToPlaceSplinesLabel = ActorsOrLandscapesToPlaceSplines[0]->GetActorNameOrLabel();

	if (bUseLandscapeSplines)
	{
		if (!ActorsOrLandscapesToPlaceSplines[0]->IsA<ALandscape>())
		{
			FMessageDialog::Open(
				EAppMsgCategory::Error,
				EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("ASplineImporter::GenerateSplines::2", "Actor {0} is not a landscape"),
					FText::FromString(ActorOrLandscapeToPlaceSplinesLabel)
				)
			);
			return false;
		}
		Landscape = Cast<ALandscape>(ActorsOrLandscapesToPlaceSplines[0]);

		IntroMessage = FText::Format(
			LOCTEXT("ASplineImporter::GenerateSplines::Intro1",
				"Landscape splines will now be added to Landscape {0}. You can monitor the progress in the Output Log.\n"
				"After the splines are added, you must go to\n"
				"Landscape Mode > Manage > Splines\n"
				"to manage the splines as usual. By selecting all control points or all segments, and then going to their Details panel, you can choose "
				"to paint a landscape layer for the roads, or you can add spline meshes to form your roads.\nContinue?"	
			),
			FText::FromString(ActorOrLandscapeToPlaceSplinesLabel)
		);
	}
	else
	{
		IntroMessage = FText::Format(
			LOCTEXT("ASplineImporter::GenerateSplines::Intro2",
				"Splines will now be added to {0}. You can monitor the progress in the Output Log. "
				"For splines that represent buildings perimeters, you can use the plugin's class "
				"BuildingsFromSplines to create buildings automatically.\nContinue?"
			),
			FText::FromString(ActorOrLandscapeToPlaceSplinesLabel)
		);
	}
	
	if (bIsUserInitiated && !LCReporter::ShowMessage(
		IntroMessage,
		"SuppressSplineIntroduction",
		LOCTEXT("SplineIntroductionTitle", "Information on Splines"),
		true,
		FAppStyle::GetBrush("Icons.InfoWithColor.Large")
	))
	{
		UE_LOG(LogSplineImporter, Log, TEXT("User cancelled adding splines."));
		return false;
	}

	if (bDeleteOldSplinesWhenCreatingSplines)
	{
		if (!Concurrency::RunOnGameThreadAndWait([this, bIsUserInitiated]() { return Execute_Cleanup(this, !bIsUserInitiated); }))
		{
			return false;
		}
	}

	GDALDataset* Dataset = LoadGDALDataset(bIsUserInitiated);
	if (!Dataset) return false;

	TArray<FPointList> PointLists = GDALInterface::GetPointLists(Dataset, AlreadyHandledFeatures);
	GDALClose(Dataset);

	if (bIsUserInitiated && PointLists.IsEmpty())
	{
		LCReporter::ShowError(
			LOCTEXT("No splines", "Warning: The dataset did not contain any spline, please double check your query.")
		);
		return false;
	}

	FCollisionQueryParams CollisionQueryParams;
	
	if (!Concurrency::RunOnGameThreadAndWait([&]() {
		return LandscapeUtils::CustomCollisionQueryParams(ActorsOrLandscapesToPlaceSplines, CollisionQueryParams);
	}))
	{
		return false;
	}

	UGlobalCoordinates *GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(this->GetWorld(), true);
	if (!GlobalCoordinates) return false;

	OGRCoordinateTransformation *OGRTransform = GlobalCoordinates->GetCRSTransformer("EPSG:4326");

	if (!OGRTransform)
	{
		LCReporter::ShowError(
			LOCTEXT("LandscapeNotFound", "Landscape Combinator Error: Could not create OGR Coordinate Transformation.")
		);
		return false;
	}

#if WITH_EDITOR
	if (bUseLandscapeSplines)
	{
		return Concurrency::RunOnGameThreadAndWait([&]() {
			if (GenerateLandscapeSplines(bIsUserInitiated, Landscape, CollisionQueryParams, OGRTransform, GlobalCoordinates, PointLists))
			{
				if (bFlushPCGCacheAfterImport)
					if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetSubsystemForCurrentWorld())
						PCGSubsystem->FlushCache();
				return true;
			}
			else
			{
				return false;
			}
		});
	}
	else
	{
		return Concurrency::RunOnGameThreadAndWait([&]() {
			if (GenerateRegularSplines(bIsUserInitiated, SpawnedActorsPathOverride, CollisionQueryParams, OGRTransform, GlobalCoordinates, PointLists))
			{
				if (bFlushPCGCacheAfterImport)
					if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetSubsystemForCurrentWorld())
						PCGSubsystem->FlushCache();
				return true;
			}
			else
			{
				return false;
			}
		});
	}
#else

	if (bUseLandscapeSplines)
	{
		UE_LOG(LogSplineImporter, Error, TEXT("Cannot create landscape splines at runtime"));
		return false;
	}
	return Concurrency::RunOnGameThreadAndWait([&]() {
		if (GenerateRegularSplines(bIsUserInitiated, SpawnedActorsPathOverride, CollisionQueryParams, OGRTransform, GlobalCoordinates, PointLists))
		{
			if (bFlushPCGCacheAfterImport)
				if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetSubsystemForCurrentWorld())
					PCGSubsystem->FlushCache();
			return true;
		}
		else
		{
			return false;
		}
	});


#endif
}

bool ASplineImporter::GenerateRegularSplines(
	bool bIsUserInitiated,
	FName SpawnedActorsPathOverride,
	FCollisionQueryParams CollisionQueryParams,
	OGRCoordinateTransformation *OGRTransform,
	UGlobalCoordinates *GlobalCoordinates,
	TArray<FPointList> &PointLists
)
{
	UWorld *World = GetWorld();

	if (!IsValid(World))
	{
		LCReporter::ShowError(
			LOCTEXT("NoWorld", "Internal error while creating splines: NULL World pointer.")
		);
		return false;
	}

	AActor *SplineOwner = nullptr;
	if (SplineOwnerKind == ESplineOwnerKind::SingleSplineCollection)
	{
		Concurrency::RunOnGameThreadAndWait([&SplineOwner, &SpawnedActorsPathOverride, World, this]()
		{
			SplineOwner = World->SpawnActor<ASplineCollection>();
			if (!SplineOwner) return false;

	#if WITH_EDITOR
			SplineOwner->SetActorLabel("SC_" + this->GetActorNameOrLabel());
			ULCBlueprintLibrary::SetFolderPath2(SplineOwner, SpawnedActorsPathOverride, SpawnedActorsPath);
	#endif
			
			SplineOwner->Tags.Add(SplinesTag);
			return true;
		});

		if (!SplineOwner)
		{
			LCReporter::ShowError(
				LOCTEXT("SpawnActorFailed", "Internal error while creating splines. Could not spawn a SplineCollection.")
			);
			return false;
		}
		SplineOwners.Add(SplineOwner);
	}

	const int NumLists = PointLists.Num();

	int i = 0;
	bool bAtLeastOneSuccess = false;
	for (auto &PointList : PointLists)
	{
		i++;
		if (SplineOwnerKind == ESplineOwnerKind::ManySplineCollections)
		{
			Concurrency::RunOnGameThreadAndWait([&SplineOwner, &SpawnedActorsPathOverride, World, i, this]()
			{
				SplineOwner = World->SpawnActor<ASplineCollection>();
				if (!IsValid(SplineOwner)) return false;

#if WITH_EDITOR
				SplineOwner->SetActorLabel("SC_" + this->GetActorNameOrLabel() + FString::FromInt(i));
				ULCBlueprintLibrary::SetFolderPath2(SplineOwner, SpawnedActorsPathOverride, SpawnedActorsPath);
#endif

				SplineOwner->Tags.Add(SplinesTag);
				return true;
			});
			
			if (!SplineOwner)
			{
				LCReporter::ShowError(
					LOCTEXT("SpawnActorFailed", "Internal error while creating splines. Could not spawn a SplineCollection.")
				);
				return false;
			}
			SplineOwners.Add(SplineOwner);
		}
		else if (SplineOwnerKind == ESplineOwnerKind::CustomActor)
		{
			if (!Concurrency::RunOnGameThreadAndWait([&SplineOwner, &SpawnedActorsPathOverride, World, i, this]()
			{
				if (!IsValid(ActorToSpawn))
				{
					LCReporter::ShowError(LOCTEXT("SpawnActorFailed1", "Please specify a valid actor to spawn."));
					return false;
				}

				SplineOwner = World->SpawnActor(ActorToSpawn);
				if (!IsValid(SplineOwner))
				{
					LCReporter::ShowError(FText::Format(
						LOCTEXT("SpawnActorFailed2", "Internal error while creating splines. Could not spawn {0}."),
						FText::FromString(ActorToSpawn->GetName())
					));
					return false;
				}

#if WITH_EDITOR
				SplineOwner->SetActorLabel(SplineOwner->GetActorNameOrLabel() + "_" + FString::FromInt(i));
				ULCBlueprintLibrary::SetFolderPath2(SplineOwner, SpawnedActorsPathOverride, SpawnedActorsPath);
#endif
				SplineOwner->Tags.Add(SplinesTag);
				return true;
			}))
			{
				return false;
			}

			SplineOwners.Add(SplineOwner);
		}

		if (AddRegularSpline(SplineOwner, CollisionQueryParams, OGRTransform, GlobalCoordinates, PointList))
			bAtLeastOneSuccess = true;
	}

	if (bIsUserInitiated && !bAtLeastOneSuccess)
	{
		LCReporter::ShowError(
			LOCTEXT("NoSplinePoints", "No Splines were generated, make sure to save your map and check collision settings on the target actor")
		);
		return false;
	}
	else
	{
		return true;
	}
}

bool ASplineImporter::AddRegularSpline(
	AActor* SplineOwner,
	FCollisionQueryParams CollisionQueryParams,
	OGRCoordinateTransformation *OGRTransform,
	UGlobalCoordinates *GlobalCoordinates,
	FPointList &PointList
)
{
	UWorld *World = SplineOwner->GetWorld();
	
	int NumPoints = PointList.Points.Num();
	if (NumPoints == 0) return false;

	OGRPoint First = PointList.Points[0];
	OGRPoint Last = PointList.Points.Last();
	const bool bIsLoop = (First == Last);
			
	USplineComponent *SplineComponent = nullptr;
	if (ASplineCollection* SplineCollection = Cast<ASplineCollection>(SplineOwner))
	{
		SplineComponent = NewObject<USplineComponent>(SplineOwner);
		if (!IsValid(SplineComponent))
		{
			LCReporter::ShowError(
				LOCTEXT("SplineComponentFailed", "Could not create SplineComponent.")
			);
			return false;
		}
		SplineCollection->SplineComponents.Add(SplineComponent);
		SplineComponent->RegisterComponent();
		SplineComponent->ClearSplinePoints();
		SplineComponent->SetMobility(EComponentMobility::Static);
		SplineOwner->AddInstanceComponent(SplineComponent);
		SplineComponent->AttachToComponent(SplineOwner->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	}
	else
	{
		SplineComponent = SplineOwner->GetComponentByClass<USplineComponent>();
		if (!IsValid(SplineComponent))
		{
			LCReporter::ShowError(FText::Format(
				LOCTEXT("SplineComponentFailed", "Could not get valid SplineComponent in {0}."),
				FText::FromString(ActorToSpawn->GetName())
			));
			return false;
		}
	}

	SplineComponent->ComponentTags.Add(SplineComponentsTag);

	UOSMUserData *OSMUserData = NewObject<UOSMUserData>(SplineComponent);
	OSMUserData->Fields = PointList.Fields;
	SplineComponent->AddAssetUserData(OSMUserData);

	SplineComponent->ClearSplinePoints();

	int ExpectedNumPoints = NumPoints;
	if (bIsLoop) ExpectedNumPoints--;  // don't add last point in case the spline is a closed loop

	TArray<FVector2D> UE2DPoints;
	for (int i = 0; i < ExpectedNumPoints; i++)
	{
		OGRPoint Point = PointList.Points[i];
		double Longitude = Point.getX();
		double Latitude = Point.getY();

		double x = 0;
		double y = 0;

		if (!GetUECoordinates(Longitude, Latitude, OGRTransform, GlobalCoordinates, x, y)) return false;
		
		UE2DPoints.Add({ x, y });
	}

	if (bResamplePointsAtDistance)
	{
		if (ResampleDistance <= 0)
		{
			LCReporter::ShowError(LOCTEXT("ResampleDistance", "Resample Distance must be positive."));
			return false;
		}

		TObjectPtr<USplineComponent> TemporarySpline = NewObject<USplineComponent>();
		TemporarySpline->ClearSplinePoints();
		for (auto &UE2DPoint: UE2DPoints)
		{
			TemporarySpline->AddSplinePoint( { UE2DPoint.X, UE2DPoint.Y, 0 }, ESplineCoordinateSpace::World, false);
		}
		TemporarySpline->UpdateSpline();

		float SplineLength = TemporarySpline->GetSplineLength();
		UE2DPoints.Empty(SplineLength / ResampleDistance + 1);

		float CurrentDistance = 0;

		if (bExactDistance)
		{
			while (CurrentDistance <= SplineLength)
			{
				FVector V = TemporarySpline->GetLocationAtDistanceAlongSpline(CurrentDistance, ESplineCoordinateSpace::World);
				UE2DPoints.Add({ V.X, V.Y });
				CurrentDistance += ResampleDistance;
			}

			// if the last sampled pointed is not at the end of the spline, add one point
			if (CurrentDistance - ResampleDistance < SplineLength)
			{
				FVector V = TemporarySpline->GetLocationAtDistanceAlongSpline(SplineLength, ESplineCoordinateSpace::World);
				UE2DPoints.Add({ V.X, V.Y });
			}
		}
		else
		{
			int n = FMath::FloorToInt(SplineLength / ResampleDistance) + 2;
			float Interval = SplineLength / (n - 1);
			for (int i = 0; i < n; i++)
			{
				FVector V = TemporarySpline->GetLocationAtDistanceAlongSpline(i * Interval, ESplineCoordinateSpace::World);
				UE2DPoints.Add({ V.X, V.Y });
			}
		}

		TemporarySpline->MarkAsGarbage();
	}



	TArray<FVector> SplinePoints;
	TArray<FVector2D> SplinePoints2D;

	for (auto &UE2DPoint: UE2DPoints)
	{
		double z = 0;
		if (LandscapeUtils::GetZ(World, CollisionQueryParams, UE2DPoint.X, UE2DPoint.Y, z, bDebugLineTraces))
		{
			FVector Location = FVector(UE2DPoint.X, UE2DPoint.Y, z) + SplinePointsOffset;
			SplinePoints.Add(Location);
			SplinePoints2D.Add( { Location.X, Location.Y });
		}
		else
		{
			UE_LOG(LogSplineImporter, Warning, TEXT("No collision for point %f, %f"), UE2DPoint.X, UE2DPoint.Y);
		}
	}

	int NumSplinePoints = SplinePoints.Num();
	if (NumSplinePoints < ExpectedNumPoints)
	{
		UE_LOG(LogSplineImporter, Warning, TEXT("Got only %d/%d Spline Points"), SplinePoints.Num(), ExpectedNumPoints);
	}
	if (SplinePoints.IsEmpty()) return false;

	if (bSkip2DColinearVertices && SplinePoints2D.Num() >= 3)
	{
		int i = bIsLoop ? 0 : 1; // skip first point if not loop

		// skip last point if not loop; the array size can change during iteration, so we don't precompute the max bound
		while (i <= (bIsLoop ?  SplinePoints2D.Num() - 1 :  SplinePoints2D.Num() - 2))  
		{
			const FVector2D& A = SplinePoints2D[(i - 1 + SplinePoints2D.Num()) % SplinePoints2D.Num()];
			const FVector2D& B = SplinePoints2D[i];
			const FVector2D& C = SplinePoints2D[(i + 1) % SplinePoints2D.Num()];

			FVector2D AB = B - A;
			FVector2D BC = C - B;

			if (AB.Length() >= UE_DOUBLE_SMALL_NUMBER && BC.Length() >= UE_DOUBLE_SMALL_NUMBER)
			{
				const double Angle = FMath::Abs(FMath::RadiansToDegrees(FMath::Acos(FVector2D::DotProduct(AB, BC) / (AB.Length() * BC.Length()))));
				if (Angle <= ColinearityAngleThreshold)
				{
					SplinePoints.RemoveAt(i);
					SplinePoints2D.RemoveAt(i);
					continue; // don't increment i in that case, as arrays have shifted
				}
			}

			i++;
		}
	}
	
	TPolygon2<double> SplinePolygon(SplinePoints2D);

	// clockwise for TPolygon2 is counter-clockwise in game when see from top (inverted Y axis)
	// so we swap the array when the current (inverted) orientation matches
	// the spline direction given by the user
	if (bIsLoop)
	{
		if (SplinePolygon.IsClockwise())
		{
			if (SplineDirection == ESplineDirection::Clockwise) Algo::Reverse(SplinePoints);
		}
		else
		{
			if (SplineDirection == ESplineDirection::CounterClockwise) Algo::Reverse(SplinePoints);
		}
	}

	for (auto &SplinePoint : SplinePoints) SplineComponent->AddSplinePoint(SplinePoint, ESplineCoordinateSpace::World, false);

	SplineComponent->SetClosedLoop(bIsLoop);

	if (Source == EVectorSource::OSM_Buildings)
	{
		auto &Points = SplineComponent->SplineCurves.Position.Points;
		for (FInterpCurvePoint<FVector> &Point : SplineComponent->SplineCurves.Position.Points)
		{
			Point.InterpMode = CIM_Linear;
		}
	}

	SplineComponent->UpdateSpline();
	SplineComponent->bSplineHasBeenEdited = true;

	return true;
}

bool ASplineImporter::GetUECoordinates(
	double Longitude, double Latitude,
	OGRCoordinateTransformation *OGRTransform, UGlobalCoordinates *GlobalCoordinates,
	double &OutX, double &OutY
)
{
	if (bSkipCoordinateConversion)
	{
		OutX = Longitude * ScaleX;
		OutY = Latitude * ScaleY;
		return true;
	}
	else
	{
		double ConvertedLongitude = Longitude;
		double ConvertedLatitude = Latitude;

		if (!GDALInterface::Transform(OGRTransform, &ConvertedLongitude, &ConvertedLatitude)) return false;
		
		FVector2D XY;
		GlobalCoordinates->GetUnrealCoordinatesFromCRS(ConvertedLongitude, ConvertedLatitude, XY);

		OutX = XY[0];
		OutY = XY[1];
		return true;
	}
}

bool ASplineImporter::Cleanup_Implementation(bool bSkipPrompt) 
{
	Modify();

	if (DeleteGeneratedObjects(bSkipPrompt))
	{
		SplineOwners.Empty();
		AlreadyHandledFeatures.Empty();
		return true;
	}
	else
	{
		return false;
	}
}

#if WITH_EDITOR

bool ASplineImporter::GenerateLandscapeSplines(
	bool bIsUserInitiated,
	ALandscape* Landscape,
	FCollisionQueryParams CollisionQueryParams,
	OGRCoordinateTransformation* OGRTransform,
	UGlobalCoordinates* GlobalCoordinates,
	TArray<FPointList>& PointLists
)
{
	FString LandscapeLabel = Landscape->GetActorNameOrLabel();

	// SplineOwner should be a Landscape Spline Actor when there are Landscape Streaming Proxies
	// This avoids a Map Check error: "Meshes in LEVEL out of date compared to landscape spline in LEVEL. Rebuild landscape splines"
	// TODO: create one spline actor per landscape streaming proxy?
	TArray<ALandscapeStreamingProxy*> LandscapeStreamingProxies = LandscapeUtils::GetLandscapeStreamingProxies(Landscape);
	ILandscapeSplineInterface* SplineOwner = nullptr;
	if (LandscapeStreamingProxies.IsEmpty())
	{
		SplineOwner = Landscape;
	}
	else if (IsValid(Landscape->GetLandscapeInfo()))
	{
		SplineOwner = Landscape->GetLandscapeInfo()->CreateSplineActor(Landscape->GetActorLocation());
	}
	else
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("NoLandscapeSplineActor", "Could not create a landscape spline actor for Landscape {0}."),
				FText::FromString(LandscapeLabel)
			)
		);
		return false;
	}


	ULandscapeSplinesComponent* LandscapeSplinesComponent = SplineOwner->GetSplinesComponent();
	if (!LandscapeSplinesComponent)
	{
		UE_LOG(LogSplineImporter, Log, TEXT("Did not find a landscape splines component. Creating one."));
		SplineOwner->CreateSplineComponent();
		LandscapeSplinesComponent = SplineOwner->GetSplinesComponent();
	}
	else
	{
		UE_LOG(LogSplineImporter, Log, TEXT("Found a landscape splines component"));
	}

	if (!LandscapeSplinesComponent)
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("NoLandscapeSplinesComponent", "Could not create a landscape splines component for Landscape {0}."),
				FText::FromString(LandscapeLabel)
			)
		);
		return false;
	}

	LandscapeSplinesComponent->Modify();
	LandscapeSplinesComponent->ShowSplineEditorMesh(true);

	TMap<FVector2D, ULandscapeSplineControlPoint*> Points;

	const int NumLists = PointLists.Num();

	for (auto& PointList : PointLists)
	{
		AddLandscapeSplinesPoints(CollisionQueryParams, OGRTransform, GlobalCoordinates, LandscapeSplinesComponent, PointList, Points);
	}

	UE_LOG(LogSplineImporter, Log, TEXT("Found %d control points"), Points.Num());
	if (bIsUserInitiated && Points.Num() == 0)
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT(
					"NoLandscapeSplinesPoints",
					"Could not generate spline points on landscape, please make sure your level is saved and check your landscape collision settings. "
					"You may also investigate warnings/errors in the Output Log, especially about missed collisions."
				),
				FText::FromString(LandscapeLabel)
			)
		);
		return false;
	}

	for (auto& PointList : PointLists)
	{
		AddLandscapeSplines(LandscapeSplinesComponent, PointList, Points);
	}

	UE_LOG(LogSplineImporter, Log, TEXT("Added %d segments"), LandscapeSplinesComponent->GetSegments().Num());
	if (bIsUserInitiated && LandscapeSplinesComponent->GetSegments().Num() == 0)
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT(
					"NoLandscapeSplinesSegments",
					"Something went wrong when generating spline segments on landscape."
				),
				FText::FromString(LandscapeLabel)
			)
		);
		return false;
	}

	LandscapeSplinesComponent->RebuildAllSplines();
	LandscapeSplinesComponent->MarkRenderStateDirty();
	Landscape->RequestSplineLayerUpdate();

	LandscapeSplinesComponent->PostEditChange();
	return true;
}

void ASplineImporter::AddLandscapeSplinesPoints(
	FCollisionQueryParams CollisionQueryParams,
	OGRCoordinateTransformation* OGRTransform,
	UGlobalCoordinates* GlobalCoordinates,
	ULandscapeSplinesComponent* LandscapeSplinesComponent,
	FPointList& PointList,
	TMap<FVector2D, ULandscapeSplineControlPoint*>& ControlPoints
)
{
	if (!IsValid(LandscapeSplinesComponent)) return;
	UWorld* World = LandscapeSplinesComponent->GetWorld();
	if (!IsValid(World)) return;
	FTransform WorldToComponent = LandscapeSplinesComponent->GetComponentToWorld().Inverse();

	for (OGRPoint& Point : PointList.Points)
	{
		double Longitude = Point.getX();
		double Latitude = Point.getY();

		double x = 0;
		double y = 0;
		if (!GetUECoordinates(Longitude, Latitude, OGRTransform, GlobalCoordinates, x, y)) return;

		double z = 0;
		if (LandscapeUtils::GetZ(World, CollisionQueryParams, x, y, z, bDebugLineTraces))
		{
			FVector Location = FVector(x, y, z) + SplinePointsOffset;
			FVector LocalLocation = LandscapeSplinesComponent->GetComponentToWorld().InverseTransformPosition(Location);
			ULandscapeSplineControlPoint* ControlPoint = NewObject<ULandscapeSplineControlPoint>(LandscapeSplinesComponent, NAME_None, RF_Transactional);
			ControlPoint->Location = LocalLocation;
			LandscapeSplinesComponent->GetControlPoints().Add(ControlPoint);
			ControlPoint->LayerName = "Road";
			ControlPoint->Width = 300; // half-width in cm
			ControlPoint->SideFalloff = 200;
			ControlPoints.Add({ Longitude, Latitude }, ControlPoint);
		}
		else
		{
			UE_LOG(LogSplineImporter, Warning, TEXT("No collision for point %f, %f"), x, y);
		}
	}
}

void ASplineImporter::AddLandscapeSplines(
	ULandscapeSplinesComponent* LandscapeSplinesComponent,
	FPointList& PointList,
	TMap<FVector2D, ULandscapeSplineControlPoint*>& Points
)
{
	int NumPoints = PointList.Points.Num();
	for (int i = 0; i < NumPoints - 1; i++)
	{
		OGRPoint Point1 = PointList.Points[i];
		OGRPoint Point2 = PointList.Points[i + 1];

		FVector2D OGRLocation1 = { Point1.getX(), Point1.getY() };
		FVector2D OGRLocation2 = { Point2.getX(), Point2.getY() };

		// this may happen when GetZ returned false in `AddPoints`
		if (!Points.Contains(OGRLocation1) || !Points.Contains(OGRLocation2))
		{
			continue;
		}

		ULandscapeSplineControlPoint* ControlPoint1 = Points[OGRLocation1];
		ULandscapeSplineControlPoint* ControlPoint2 = Points[OGRLocation2];

		ControlPoint1->Modify();
		ControlPoint2->Modify();

		ULandscapeSplineSegment* NewSegment;

		NewSegment = NewObject<ULandscapeSplineSegment>(LandscapeSplinesComponent, NAME_None, RF_Transactional);
		LandscapeSplinesComponent->GetSegments().Add(NewSegment);

		NewSegment->LayerName = "Road";
		NewSegment->Connections[0].ControlPoint = ControlPoint1;
		NewSegment->Connections[1].ControlPoint = ControlPoint2;

		float TangentLen = (ControlPoint2->Location - ControlPoint1->Location).Size() / LandscapeSplinesStraightness;

		NewSegment->Connections[0].TangentLen = TangentLen;
		NewSegment->Connections[1].TangentLen = TangentLen;
		NewSegment->AutoFlipTangents();
		ControlPoint1->ConnectedSegments.Add(FLandscapeSplineConnection(NewSegment, 0));
		ControlPoint2->ConnectedSegments.Add(FLandscapeSplineConnection(NewSegment, 1));

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4)
		ControlPoint1->AutoCalcRotation(true);
		ControlPoint2->AutoCalcRotation(true);
#else
		ControlPoint1->AutoCalcRotation();
		ControlPoint2->AutoCalcRotation();
#endif

		// FIXME: Update is Slow
		//ControlPoint1->UpdateSplinePoints();
		//ControlPoint2->UpdateSplinePoints();
	}
}

AActor *ASplineImporter::Duplicate(FName FromName, FName ToName)
{
	if (ASplineImporter *NewSplineImporter =
		Cast<ASplineImporter>(GEditor->GetEditorSubsystem<UEditorActorSubsystem>()->DuplicateActor(this)))
	{
		NewSplineImporter->ActorsOrLandscapesToPlaceSplinesSelection.ActorTag =
			ULCBlueprintLibrary::ReplaceName(
				ActorsOrLandscapesToPlaceSplinesSelection.ActorTag,
				FromName,
				ToName
			);
		NewSplineImporter->BoundingActorSelection.ActorTag =
			ULCBlueprintLibrary::ReplaceName(
				BoundingActorSelection.ActorTag,
				FromName,
				ToName
			);
		NewSplineImporter->SplinesTag = ULCBlueprintLibrary::ReplaceName(SplinesTag, FromName, ToName);
		NewSplineImporter->SplineComponentsTag = ULCBlueprintLibrary::ReplaceName(SplineComponentsTag, FromName, ToName);
		return NewSplineImporter;
	}
	else
	{
		LCReporter::ShowError(LOCTEXT("ASplineImporter::DuplicateActor", "Failed to duplicate actor."));
		return nullptr;
	}
}

#endif

#undef LOCTEXT_NAMESPACE