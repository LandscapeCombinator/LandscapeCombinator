// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "SplineImporter/SplineImporter.h"
#include "SplineImporter/LogSplineImporter.h"
#include "SplineImporter/Overpass.h"
#include "FileDownloader/Download.h"
#include "LandscapeUtils/LandscapeUtils.h"
#include "GDALInterface/GDALInterface.h"
#include "OSMUserData/OSMUserData.h"
#include "LCCommon/LCReporter.h"
#include "LCCommon/LCSettings.h"
#include "LCCommon/LCBlueprintLibrary.h"

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
#include "Misc/ScopedSlowTask.h"
#include "Stats/Stats.h"
#include "Stats/StatsMisc.h"
#include "ObjectEditorUtils.h"
#include "Engine/World.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

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

	Super::SetOverpassShortQuery();
}

void ASplineImporter::OnGenerate(FName SpawnedActorsPathOverride, TFunction<void(bool)> OnComplete)
{
	AActor *ActorOrLandscapeToPlaceSplines = ActorOrLandscapeToPlaceSplinesSelection.GetActor(this->GetWorld());

	if (!IsValid(ActorOrLandscapeToPlaceSplines))
	{
		ULCReporter::ShowError(
			LOCTEXT("ASplineImporter::GenerateSplines::1", "Please select an actor on which to place your splines.")
		);

		if (OnComplete) OnComplete(false);
		return;
	}
	
	if (
		!ActorOrLandscapeToPlaceSplines->GetRootComponent() ||
		ActorOrLandscapeToPlaceSplines->GetRootComponent()->Mobility != EComponentMobility::Static
	)
	{
		ULCReporter::ShowError(FText::Format(
			LOCTEXT("ASplineImporter::GenerateSplines::NoStatic", "Please make sure that {0}'s mobility is set to static."),
			FText::FromString(ActorOrLandscapeToPlaceSplines->GetActorNameOrLabel())
		));
		
		if (OnComplete) OnComplete(false);
		return;
	}

	FText IntroMessage;
	ALandscape *Landscape = nullptr;
	FString ActorOrLandscapeToPlaceSplinesLabel = ActorOrLandscapeToPlaceSplines->GetActorNameOrLabel();

	if (bUseLandscapeSplines)
	{
		if (!ActorOrLandscapeToPlaceSplines->IsA<ALandscape>())
		{
			FMessageDialog::Open(
				EAppMsgCategory::Error,
				EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("ASplineImporter::GenerateSplines::2", "Actor {0} is not a landscape"),
					FText::FromString(ActorOrLandscapeToPlaceSplinesLabel)
				)
			);
			return;
		}
		Landscape = Cast<ALandscape>(ActorOrLandscapeToPlaceSplines);

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
	
	if (!ULCReporter::ShowMessage(
		IntroMessage,
		"SuppressSplineIntroduction",
		LOCTEXT("SplineIntroductionTitle", "Information on Splines"),
		true,
		FAppStyle::GetBrush("Icons.InfoWithColor.Large")
	))
	{
		UE_LOG(LogSplineImporter, Log, TEXT("User cancelled adding splines."));

		if (OnComplete) OnComplete(false);
		return;
	}

	// Delete existing spline collections before generating new ones
	Execute_Cleanup(this, false);

	LoadGDALDataset([this, OnComplete, Landscape, ActorOrLandscapeToPlaceSplines, SpawnedActorsPathOverride](GDALDataset* Dataset) {
		if (Dataset)
		{
			TArray<FPointList> PointLists = GDALInterface::GetPointLists(Dataset);
			GDALClose(Dataset);

			if (PointLists.IsEmpty())
			{
				ULCReporter::ShowError(
					LOCTEXT("No splines", "Warning: The dataset did not contain any spline, please double check your query.")
				);

				if (OnComplete) OnComplete(false);
				return;
			}

			FCollisionQueryParams CollisionQueryParams = LandscapeUtils::CustomCollisionQueryParams(ActorOrLandscapeToPlaceSplines);
			UGlobalCoordinates *GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(this->GetWorld(), true);

			if (!GlobalCoordinates)
			{
				if (OnComplete) OnComplete(false);
				return;
			}

			OGRCoordinateTransformation *OGRTransform = GlobalCoordinates->GetCRSTransformer("EPSG:4326");

			if (!OGRTransform)
			{
				ULCReporter::ShowError(
					LOCTEXT("LandscapeNotFound", "Landscape Combinator Error: Could not create OGR Coordinate Transformation.")
				);
				if (OnComplete) OnComplete(false);
				return;
			}

#if WITH_EDITOR
			if (bUseLandscapeSplines)
			{
				GenerateLandscapeSplines(Landscape, CollisionQueryParams, OGRTransform, GlobalCoordinates, PointLists);
			}
			else
			{
				GenerateRegularSplines(SpawnedActorsPathOverride, CollisionQueryParams, OGRTransform, GlobalCoordinates, PointLists);
			}
#else

			if (bUseLandscapeSplines)
			{
				UE_LOG(LogSplineImporter, Error, TEXT("Cannot create landscape splines at runtime"));
				if (OnComplete) OnComplete(false);
				return;
			}
			GenerateRegularSplines(SpawnedActorsPathOverride, CollisionQueryParams, OGRTransform, GlobalCoordinates, PointLists);

#endif

			if (OnComplete) OnComplete(true);
		}
		else
		{
			if (OnComplete) OnComplete(false);
		}
	});
}

void ASplineImporter::GenerateRegularSplines(
	FName SpawnedActorsPathOverride,
	FCollisionQueryParams CollisionQueryParams,
	OGRCoordinateTransformation *OGRTransform,
	UGlobalCoordinates *GlobalCoordinates,
	TArray<FPointList> &PointLists
)
{
	UWorld *World = GetWorld();

	if (!World)
	{
		ULCReporter::ShowError(
			LOCTEXT("NoWorld", "Internal error while creating splines: NULL World pointer.")
		);
		return;
	}

	AActor *SplineOwner = nullptr;
	if (SplineOwnerKind == ESplineOwnerKind::SingleSplineCollection)
	{
		SplineOwner = World->SpawnActor<ASplineCollection>();

#if WITH_EDITOR
		SplineOwner->SetActorLabel("SC_" + this->GetActorNameOrLabel());
		ULCBlueprintLibrary::SetFolderPath2(SplineOwner, SpawnedActorsPathOverride, SpawnedActorsPath);
#endif

		if (!SplineOwner)
		{
			ULCReporter::ShowError(
				LOCTEXT("NoWorld", "Internal error while creating splines. Could not spawn a SplineCollection.")
			);
			return;
		}
		
		SplineOwner->Tags.Add(SplinesTag);
		SplineOwners.Add(SplineOwner);
	}

	const int NumLists = PointLists.Num();
	FScopedSlowTask SplinesTask = FScopedSlowTask(NumLists,
		FText::Format(
			LOCTEXT("PointsTask", "Adding splines from {0} lines"),
			FText::AsNumber(NumLists)
		)
	);
	SplinesTask.MakeDialog();

	int i = 0;
	for (auto &PointList : PointLists)
	{
		i++;
		if (SplineOwnerKind == ESplineOwnerKind::ManySplineCollections)
		{
			SplineOwner = World->SpawnActor<ASplineCollection>();

			if (!SplineOwner)
			{
				ULCReporter::ShowError(
					LOCTEXT("SpawnActorFailed", "Internal error while creating splines. Could not spawn a SplineCollection.")
				);
				return;
			}

#if WITH_EDITOR
			SplineOwner->SetActorLabel("SC_" + this->GetActorNameOrLabel() + FString::FromInt(i));
			ULCBlueprintLibrary::SetFolderPath2(SplineOwner, SpawnedActorsPathOverride, SpawnedActorsPath);
#endif

			SplineOwner->Tags.Add(SplinesTag);
			SplineOwners.Add(SplineOwner);
		}
		else if (SplineOwnerKind == ESplineOwnerKind::CustomActor)
		{
			SplineOwner = World->SpawnActor(ActorToSpawn);
			
			if (!SplineOwner)
			{
				ULCReporter::ShowError(FText::Format(
					LOCTEXT("SpawnActorFailed2", "Internal error while creating splines. Could not spawn {0}."),
					FText::FromString(ActorToSpawn->GetName())
				));
				return;
			}

#if WITH_EDITOR
			SplineOwner->SetActorLabel(SplineOwner->GetActorNameOrLabel() + "_" + FString::FromInt(i));
			ULCBlueprintLibrary::SetFolderPath2(SplineOwner, SpawnedActorsPathOverride, SpawnedActorsPath);
#endif

			SplineOwner->Tags.Add(SplinesTag);
			SplineOwners.Add(SplineOwner);
		}
		SplinesTask.EnterProgressFrame();
		AddRegularSpline(SplineOwner, CollisionQueryParams, OGRTransform, GlobalCoordinates, PointList);
	}
}

void ASplineImporter::AddRegularSpline(
	AActor* SplineOwner,
	FCollisionQueryParams CollisionQueryParams,
	OGRCoordinateTransformation *OGRTransform,
	UGlobalCoordinates *GlobalCoordinates,
	FPointList &PointList
)
{
	UWorld *World = SplineOwner->GetWorld();
	
	int NumPoints = PointList.Points.Num();
	if (NumPoints == 0) return;

	OGRPoint First = PointList.Points[0];
	OGRPoint Last = PointList.Points.Last();
			
	USplineComponent *SplineComponent = nullptr;
	if (ASplineCollection* SplineCollection = Cast<ASplineCollection>(SplineOwner))
	{
		SplineComponent = NewObject<USplineComponent>(SplineOwner);
		if (!IsValid(SplineComponent))
		{
			ULCReporter::ShowError(
				LOCTEXT("SplineComponentFailed", "Could not create SplineComponent.")
			);
			return;
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
			ULCReporter::ShowError(FText::Format(
				LOCTEXT("SplineComponentFailed", "Could not get valid SplineComponent in {0}."),
				FText::FromString(ActorToSpawn->GetName())
			));
			return;
		}
	}

	SplineComponent->ComponentTags.Add(SplineComponentsTag);

	UOSMUserData *OSMUserData = NewObject<UOSMUserData>(SplineComponent);
	OSMUserData->Fields = PointList.Fields;
	SplineComponent->AddAssetUserData(OSMUserData);

	SplineComponent->ClearSplinePoints();

	for (int i = 0; i < NumPoints; i++)
	{
		if (Last != First || i < NumPoints - 1) // don't add last point in case the spline is a closed loop
		{
			OGRPoint Point = PointList.Points[i];
			double Longitude = Point.getX();
			double Latitude = Point.getY();
		
			// copy before converting
			double ConvertedLongitude = Longitude;
			double ConvertedLatitude = Latitude;

			if (!GDALInterface::Transform(OGRTransform, &ConvertedLongitude, &ConvertedLatitude)) return;
			
			FVector2D XY;
			GlobalCoordinates->GetUnrealCoordinatesFromCRS(ConvertedLongitude, ConvertedLatitude, XY);

			double x = XY[0];
			double y = XY[1];
			double z;
			if (LandscapeUtils::GetZ(World, CollisionQueryParams, x, y, z))
			{
				FVector Location = FVector(x, y, z) + SplinePointsOffset;
				SplineComponent->AddSplinePoint(Location, ESplineCoordinateSpace::World, false);
			}
			else
			{
				UE_LOG(LogSplineImporter, Warning, TEXT("No collision for point %f, %f"), x, y);
			}
		}
	}

	if (First == Last) SplineComponent->SetClosedLoop(true);
		

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
}

#if WITH_EDITOR

void ASplineImporter::GenerateLandscapeSplines(
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
		ULCReporter::ShowError(
			FText::Format(
				LOCTEXT("NoLandscapeSplineActor", "Could not create a landscape spline actor for Landscape {0}."),
				FText::FromString(LandscapeLabel)
			)
		);
		return;
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
		ULCReporter::ShowError(
			FText::Format(
				LOCTEXT("NoLandscapeSplinesComponent", "Could not create a landscape splines component for Landscape {0}."),
				FText::FromString(LandscapeLabel)
			)
		);
		return;
	}

	LandscapeSplinesComponent->Modify();
	LandscapeSplinesComponent->ShowSplineEditorMesh(true);

	TMap<FVector2D, ULandscapeSplineControlPoint*> Points;

	const int NumLists = PointLists.Num();
	FScopedSlowTask PointsTask = FScopedSlowTask(NumLists,
		FText::Format(
			LOCTEXT("PointsTask", "Adding points from {0} lines"),
			FText::AsNumber(NumLists)
		)
	);
	PointsTask.MakeDialog();

	for (auto& PointList : PointLists)
	{
		PointsTask.EnterProgressFrame();
		AddLandscapeSplinesPoints(Landscape, CollisionQueryParams, OGRTransform, GlobalCoordinates, LandscapeSplinesComponent, PointList, Points);
	}

	UE_LOG(LogSplineImporter, Log, TEXT("Found %d control points"), Points.Num());

	FScopedSlowTask LandscapeSplinesTask = FScopedSlowTask(NumLists,
		FText::Format(
			LOCTEXT("PointsTask", "Adding landscape splines from {0} lines and {1} points"),
			FText::AsNumber(NumLists),
			FText::AsNumber(Points.Num())
		)
	);
	LandscapeSplinesTask.MakeDialog();

	for (auto& PointList : PointLists)
	{
		LandscapeSplinesTask.EnterProgressFrame();
		AddLandscapeSplines(Landscape, CollisionQueryParams, OGRTransform, GlobalCoordinates, LandscapeSplinesComponent, PointList, Points);
	}

	UE_LOG(LogSplineImporter, Log, TEXT("Added %d segments"), LandscapeSplinesComponent->GetSegments().Num());
	LandscapeSplinesComponent->RebuildAllSplines();
	LandscapeSplinesComponent->MarkRenderStateDirty();
	Landscape->RequestSplineLayerUpdate();

	LandscapeSplinesComponent->PostEditChange();
}

void ASplineImporter::AddLandscapeSplinesPoints(
	ALandscape* Landscape,
	FCollisionQueryParams CollisionQueryParams,
	OGRCoordinateTransformation* OGRTransform,
	UGlobalCoordinates* GlobalCoordinates,
	ULandscapeSplinesComponent* LandscapeSplinesComponent,
	FPointList& PointList,
	TMap<FVector2D, ULandscapeSplineControlPoint*>& ControlPoints
)
{
	UWorld* World = Landscape->GetWorld();
	FTransform WorldToComponent = LandscapeSplinesComponent->GetComponentToWorld().Inverse();

	for (OGRPoint& Point : PointList.Points)
	{
		double Longitude = Point.getX();
		double Latitude = Point.getY();

		// copy before converting
		double ConvertedLongitude = Point.getX();
		double ConvertedLatitude = Point.getY();

		if (!GDALInterface::Transform(OGRTransform, &ConvertedLongitude, &ConvertedLatitude)) return;

		FVector2D XY;
		GlobalCoordinates->GetUnrealCoordinatesFromCRS(ConvertedLongitude, ConvertedLatitude, XY);

		double x = XY[0];
		double y = XY[1];
		double z;
		if (LandscapeUtils::GetZ(World, CollisionQueryParams, x, y, z))
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
	ALandscape* Landscape,
	FCollisionQueryParams CollisionQueryParams,
	OGRCoordinateTransformation* OGRTransform,
	UGlobalCoordinates* GlobalCoordinates,
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
		NewSplineImporter->ActorOrLandscapeToPlaceSplinesSelection.ActorTag =
			ULCBlueprintLibrary::ReplaceName(
				ActorOrLandscapeToPlaceSplinesSelection.ActorTag,
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
		ULCReporter::ShowError(LOCTEXT("ASplineImporter::DuplicateActor", "Failed to duplicate actor."));
		return nullptr;
	}
}

#endif

#undef LOCTEXT_NAMESPACE