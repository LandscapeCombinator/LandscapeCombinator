// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "Splines/SplinesBuilder.h"
#include "Utils/Logging.h"
#include "Utils/Download.h"
#include "Utils/Overpass.h"
#include "Elevation/LandscapeUtils.h"
#include "LandscapeCombinatorStyle.h"

#include "EditorSupportDelegates.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeSplineControlPoint.h" 
#include "LandscapeSplinesComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/SWindow.h"
#include "XmlFile.h"
#include "Misc/CString.h"
#include "Logging/StructuredLog.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Async/Async.h"
#include "Misc/ScopedSlowTask.h" 

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

using namespace GlobalSettings;

ASplinesBuilder::ASplinesBuilder()
{
    PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent")));
	RootComponent->SetMobility(EComponentMobility::Static);
}

void ASplinesBuilder::ToggleLinear()
{
	for (auto& SplineComponent : SplineComponents)
	{
		SplineComponent->Modify();
		
		auto &Points = SplineComponent->SplineCurves.Position.Points;

		if (Points.IsEmpty()) continue;

		TEnumAsByte<EInterpCurveMode> OldInterpMode = Points[0].InterpMode;

		TEnumAsByte<EInterpCurveMode> NewInterpMode = OldInterpMode == CIM_CurveAuto ? CIM_Linear : CIM_CurveAuto;

		for (FInterpCurvePoint<FVector> &Point : SplineComponent->SplineCurves.Position.Points)
		{
			Point.InterpMode = NewInterpMode;
		}
		SplineComponent->UpdateSpline();
		SplineComponent->bSplineHasBeenEdited = true;
	}
}

void ASplinesBuilder::GenerateSplines()
{
	FText IntroMessage;
	if (bUseLandscapeSplines)
	{
		IntroMessage = FText::Format(
			LOCTEXT("LandscapeNotFound",
				"Landscape splines will now be added to Landscape {0}. You can monitor the progress in the Output Log. "
				"After the splines are added, you must go to\n"
				"Landscape Mode > Manage > Splines\n"
				"to manage the splines as usual. By selecting all control points or all segments, and then going to their Details panel, you can choose "
				"to paint a landscape layer for the roads, or you can add spline meshes to form your roads.\n"	
			),
			FText::FromString(TargetLandscapeLabel)
		);
	}
	else
	{
		IntroMessage = FText::Format(
			LOCTEXT("LandscapeNotFound",
				"Splines will now be added to Landscape {0}. You can monitor the progress in the Output Log. "
				"After the splines are added, you may for instance add meshes to them to make roads. "
				"For splines that represent buildings perimeters, you can use the plugin's class ABuildFromSplines to create buildings automatically."
			),
			FText::FromString(TargetLandscapeLabel)
		);
	}
	
	EAppReturnType::Type UserResponse = FMessageDialog::Open(EAppMsgType::OkCancel, IntroMessage);
	if (UserResponse == EAppReturnType::Cancel) 
	{
		UE_LOG(LogLandscapeCombinator, Log, TEXT("User cancelled adding landscape splines."));
		return;
	}
	
	ALandscape *Landscape = LandscapeUtils::GetLandscapeFromLabel(TargetLandscapeLabel);
	if (!Landscape)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("LandscapeNotFound", "Could not find Landscape '{0}'."),
				FText::FromString(TargetLandscapeLabel)
			)
		);
		return;
	}

	UE_LOG(LogLandscapeCombinator, Log, TEXT("Found Landscape %s"), *TargetLandscapeLabel);

	LoadGDALDataset([this, Landscape](GDALDataset* Dataset) {
		if (Dataset)
		{
			GenerateSplinesFromDataset(Landscape, Dataset);
		}
	});

	GDALDataset* Dataset = (GDALDataset*) GDALOpenEx(TCHAR_TO_UTF8(*LocalFile), GDAL_OF_VECTOR, NULL, NULL, NULL);


}

GDALDataset* ASplinesBuilder::LoadGDALDatasetFromFile(FString File)
{
	GDALDataset* Dataset = (GDALDataset*) GDALOpenEx(TCHAR_TO_UTF8(*File), GDAL_OF_VECTOR, NULL, NULL, NULL);

	if (!Dataset)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("LandscapeNotFound", "Could not load vector file '{0}'."),
				FText::FromString(File)
			)
		);
	}

	return Dataset;
}

void ASplinesBuilder::LoadGDALDatasetFromQuery(FString Query, TFunction<void(GDALDataset*)> OnComplete)
{
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Adding roads with Overpass query: '%s'"), *Query);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Decoded URL: '%s'"), *(FGenericPlatformHttp::UrlDecode(Query)));
	FString IntermediateDir = FPaths::ConvertRelativePathToFull(FPaths::EngineIntermediateDir());
	FString LandscapeCombinatorDir = FPaths::Combine(IntermediateDir, "LandscapeCombinator");
	FString DownloadDir = FPaths::Combine(LandscapeCombinatorDir, "Download");
	FString XmlFilePath = FPaths::Combine(DownloadDir, FString::Format(TEXT("overpass_query_{0}.xml"), { FTextLocalizationResource::HashString(Query) }));

	Download::FromURL(Query, XmlFilePath,
		[=, this](bool bWasSuccessful) {
			if (bWasSuccessful)
			{
				// working on splines only works in GameThread
				AsyncTask(ENamedThreads::GameThread, [=, this]() {
					OnComplete(LoadGDALDatasetFromFile(XmlFilePath));
				});
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
					LOCTEXT("GetSpatialReferenceError", "Unable to get the result for the Overpass query: {0}."),
					FText::FromString(Query)
				));
			}
			return;
		}
	);
}

void ASplinesBuilder::LoadGDALDatasetFromShortQuery(FString ShortQuery, TFunction<void(GDALDataset*)> OnComplete)
{
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Adding roads with short Overpass query: '%s'"), *ShortQuery);

	if (!HMTable->Interfaces.Contains(TargetLandscapeLabel))
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("LandscapeNotFound", "Could not find Landscape '{0}' in the elevation table."),
				FText::FromString(TargetLandscapeLabel)
			)
		);
		return;
	}

	HMInterface *Interface = HMTable->Interfaces[TargetLandscapeLabel];
	
	FVector4d Coordinates;
	if (!Interface->GetCoordinates4326(Coordinates)) return;

	double South = Coordinates[2];
	double West = Coordinates[0];
	double North = Coordinates[3];
	double East = Coordinates[1];
	LoadGDALDatasetFromQuery(Overpass::QueryFromShortQuery(South, West, North, East, ShortQuery), OnComplete);
}

void ASplinesBuilder::LoadGDALDataset(TFunction<void(GDALDataset*)> OnComplete)
{
	if (SplinesSource == ESourceKind::LocalFile)
	{
		OnComplete(LoadGDALDatasetFromFile(LocalFile));
	}
	else if (SplinesSource == ESourceKind::OverpassQuery)
	{	
		LoadGDALDatasetFromQuery(OverpassQuery, OnComplete);
	}
	else if (SplinesSource == ESourceKind::OverpassShortQuery)
	{
		LoadGDALDatasetFromShortQuery(OverpassShortQuery, OnComplete);
	}
	else if (SplinesSource == ESourceKind::Roads)
	{
		LoadGDALDatasetFromShortQuery(FString("way[\"highway\"][\"highway\"!~\"path\"][\"highway\"!~\"track\"];"), OnComplete);
	}
	else if (SplinesSource == ESourceKind::Buildings)
	{
		LoadGDALDatasetFromShortQuery(FString("way[\"building\"];"), OnComplete);
	}
	else if (SplinesSource == ESourceKind::Rivers)
	{
		LoadGDALDatasetFromShortQuery(FString("way[\"waterway\"=\"river\"];"), OnComplete);
	}
	else
	{
		check(false);
	}
}

void ASplinesBuilder::AddPointLists(OGRMultiPolygon* MultiPolygon, TArray<TArray<OGRPoint>> &PointLists)
{
	for (OGRPolygon* &Polygon : MultiPolygon)
	{
		for (OGRLinearRing* &LinearRing : Polygon)
		{
			TArray<OGRPoint> NewList;
			for (OGRPoint &Point : LinearRing)
			{
				NewList.Add(Point);
			}
			PointLists.Add(NewList);
		}
	}
}

void ASplinesBuilder::AddPointList(OGRLineString* LineString, TArray<TArray<OGRPoint>> &PointLists)
{
	TArray<OGRPoint> NewList;
	for (OGRPoint &Point : LineString)
	{
		NewList.Add(Point);
	}
	PointLists.Add(NewList);
}

void ASplinesBuilder::GenerateSplinesFromDataset(ALandscape *Landscape, GDALDataset* Dataset)
{
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Got a valid dataset with %d layers to extract splines, continuing..."), Dataset->GetLayerCount());
	TArray<TArray<OGRPoint>> PointLists;
	
	OGRFeature *Feature;
	OGRLayer *Layer;

	Feature = Dataset->GetNextFeature(&Layer, nullptr, nullptr, nullptr);
	while (Feature)
	{
		OGRGeometry* Geometry = Feature->GetGeometryRef();
		if (!Geometry) continue;

		OGRwkbGeometryType GeometryType = wkbFlatten(Geometry->getGeometryType());
			
		if (GeometryType == wkbMultiPolygon)
		{
			AddPointLists(Geometry->toMultiPolygon(), PointLists);
		}
		else if (GeometryType == wkbLineString)
		{
			AddPointList(Geometry->toLineString(), PointLists);
		}
		else if (GeometryType == wkbPoint)
		{
			// ignoring lone point
		}
		else
		{
			UE_LOG(LogLandscapeCombinator, Warning, TEXT("Found an unsupported feature %d"), wkbFlatten(Geometry->getGeometryType()));
		}
		Feature = Dataset->GetNextFeature(&Layer, nullptr, nullptr, nullptr);
	}

	UE_LOG(LogLandscapeCombinator, Log, TEXT("Found %d list of points"), PointLists.Num());

	WorldParametersV1 GlobalParams;
	if (!GetWorldParameters(GlobalParams))
	{
		return;
	}
	
	FCollisionQueryParams CollisionQueryParams = LandscapeUtils::CustomCollisionQueryParams(Landscape);
	double WorldWidthCm  = ((double) GlobalParams.WorldWidthKm) * 1000 * 100;
	double WorldHeightCm = ((double) GlobalParams.WorldHeightKm) * 1000 * 100;
	double WorldOriginX = GlobalParams.WorldOriginX;
	double WorldOriginY = GlobalParams.WorldOriginY;

	if (bUseLandscapeSplines)
    {
        GenerateLandscapeSplines(Landscape, CollisionQueryParams, PointLists, WorldWidthCm, WorldHeightCm, WorldOriginX, WorldOriginY);
    }
    else
    {
        GenerateRegularSplines(Landscape, CollisionQueryParams, PointLists, WorldWidthCm, WorldHeightCm, WorldOriginX, WorldOriginY);
    }
}

void ASplinesBuilder::GenerateLandscapeSplines(
		ALandscape *Landscape,
		FCollisionQueryParams CollisionQueryParams,
		TArray<TArray<OGRPoint>> &PointLists,
		double WorldWidthCm,
		double WorldHeightCm,
		double WorldOriginX,
		double WorldOriginY
)
{
	FString LandscapeLabel = Landscape->GetActorLabel();
	ULandscapeSplinesComponent *LandscapeSplinesComponent = Landscape->GetSplinesComponent();
	if (!LandscapeSplinesComponent)
	{
		UE_LOG(LogLandscapeCombinator, Log, TEXT("Did not find a landscape splines component. Creating one."));
		Landscape->CreateSplineComponent();
		LandscapeSplinesComponent = Landscape->GetSplinesComponent();
	}
	else
	{
		UE_LOG(LogLandscapeCombinator, Log, TEXT("Found a landscape splines component"));
	}

	if (!LandscapeSplinesComponent)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("NoLandscapeSplinesComponent", "Could not create a landscape splines component for Landscape {0}."),
				FText::FromString(LandscapeLabel)
			)
		);
		return;
	}
		
	LandscapeSplinesComponent->Modify();
	LandscapeSplinesComponent->ShowSplineEditorMesh(true);

	TMap<FVector2d, ULandscapeSplineControlPoint*> Points;
	
	const int NumLists = PointLists.Num();
	FScopedSlowTask PointsTask = FScopedSlowTask(NumLists,
		FText::Format(
			LOCTEXT("PointsTask", "Adding points from {0} lines"),
			FText::AsNumber(NumLists)
		)
	);
	PointsTask.MakeDialog();

	for (auto &PointList : PointLists)
	{
		AddLandscapeSplinesPoints(Landscape, CollisionQueryParams, LandscapeSplinesComponent, PointList, Points, WorldWidthCm, WorldHeightCm, WorldOriginX, WorldOriginY);
	}

	UE_LOG(LogLandscapeCombinator, Log, TEXT("Found %d control points"), Points.Num());
	
	FScopedSlowTask LandscapeSplinesTask = FScopedSlowTask(NumLists,
		FText::Format(
			LOCTEXT("PointsTask", "Adding landscape splines from {0} lines"),
			FText::AsNumber(NumLists)
		)
	);
	LandscapeSplinesTask.MakeDialog();
	
	for (auto &PointList : PointLists)
	{
		AddLandscapeSplines(Landscape, CollisionQueryParams, LandscapeSplinesComponent, PointList, Points, WorldWidthCm, WorldHeightCm, WorldOriginX, WorldOriginY);
	}
	
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Added %d segments"), LandscapeSplinesComponent->GetSegments().Num());
			
	LandscapeSplinesComponent->AttachToComponent(Landscape->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	LandscapeSplinesComponent->MarkRenderStateDirty();
	LandscapeSplinesComponent->PostEditChange();

}

void ASplinesBuilder::AddLandscapeSplinesPoints(
	ALandscape* Landscape,
	FCollisionQueryParams CollisionQueryParams,
	ULandscapeSplinesComponent* LandscapeSplinesComponent,
	TArray<OGRPoint> &PointList,
	TMap<FVector2d, ULandscapeSplineControlPoint*> &ControlPoints,
	double WorldWidthCm,
	double WorldHeightCm,
	double WorldOriginX,
	double WorldOriginY
)
{
	UWorld *World = Landscape->GetWorld();

	for (OGRPoint &Point : PointList)
	{
		double longitude = Point.getX();
		double lattitude = Point.getY();

		FVector UnrealCoords = GlobalSettings::EPSG4326ToUnrealCoordinates( { longitude, lattitude }, WorldWidthCm, WorldHeightCm, WorldOriginX, WorldOriginY);
		double x = UnrealCoords[0];
		double y = UnrealCoords[1];
		double z = LandscapeUtils::GetZ(World, CollisionQueryParams, x, y);
		FVector Location = { x, y, z };
		if (z > 0)
		{
			FVector LocalLocation = LandscapeSplinesComponent->GetComponentToWorld().InverseTransformPosition(Location);
			ULandscapeSplineControlPoint* ControlPoint = NewObject<ULandscapeSplineControlPoint>(LandscapeSplinesComponent, NAME_None, RF_Transactional);
			ControlPoint->Location = LocalLocation;
			LandscapeSplinesComponent->GetControlPoints().Add(ControlPoint);
			ControlPoint->LayerName = "Road";
			ControlPoint->Width = 300; // half-width in cm
			ControlPoint->SideFalloff = 200;
			ControlPoints.Add( { longitude, lattitude } , ControlPoint);
		}
	}
}

void ASplinesBuilder::AddLandscapeSplines(
	ALandscape* Landscape,
	FCollisionQueryParams CollisionQueryParams,
	ULandscapeSplinesComponent* LandscapeSplinesComponent,
	TArray<OGRPoint> &PointList,
	TMap<FVector2d, ULandscapeSplineControlPoint*> &Points,
	double WorldWidthCm,
	double WorldHeightCm,
	double WorldOriginX,
	double WorldOriginY
)
{
	int NumPoints = PointList.Num();
	for (int i = 0; i < NumPoints - 1; i++)
	{
		OGRPoint Point1 = PointList[i];
		OGRPoint Point2 = PointList[i + 1];

		FVector2d OGRLocation1 = { Point1.getX(), Point1.getY() };
		FVector2d OGRLocation2 = { Point2.getX(), Point2.getY() };

		// this may happen when GetZ returned 0 in `AddPoints`
		if (!Points.Contains(OGRLocation1) || !Points.Contains(OGRLocation2))
		{
			continue;
		}

		ULandscapeSplineControlPoint *ControlPoint1 = Points[OGRLocation1];
		ULandscapeSplineControlPoint *ControlPoint2 = Points[OGRLocation2];

		ControlPoint1->Modify();
		ControlPoint2->Modify();

		ULandscapeSplineSegment* NewSegment = NewObject<ULandscapeSplineSegment>(LandscapeSplinesComponent, NAME_None, RF_Transactional);
		LandscapeSplinesComponent->GetSegments().Add(NewSegment);
		
		NewSegment->LayerName = "Road";
		NewSegment->Connections[0].ControlPoint = ControlPoint1;
		NewSegment->Connections[1].ControlPoint = ControlPoint2;

		NewSegment->Connections[0].SocketName = ControlPoint1->GetBestConnectionTo(ControlPoint2->Location);
		NewSegment->Connections[1].SocketName = ControlPoint2->GetBestConnectionTo(ControlPoint1->Location);

		FVector Location1, Location2;
		FRotator Rotation1, Rotation2;
		ControlPoint1->GetConnectionLocationAndRotation(NewSegment->Connections[0].SocketName, Location1, Rotation1);
		ControlPoint2->GetConnectionLocationAndRotation(NewSegment->Connections[1].SocketName, Location2, Rotation2);

		float TangentLen = (Location2 - Location1).Size();
		NewSegment->Connections[0].TangentLen = TangentLen;
		NewSegment->Connections[1].TangentLen = TangentLen;
		NewSegment->AutoFlipTangents();

		ControlPoint1->ConnectedSegments.Add(FLandscapeSplineConnection(NewSegment, 0));
		ControlPoint2->ConnectedSegments.Add(FLandscapeSplineConnection(NewSegment, 1));

		ControlPoint1->AutoCalcRotation();
		ControlPoint2->AutoCalcRotation();

		ControlPoint1->UpdateSplinePoints();
		ControlPoint2->UpdateSplinePoints();
	}
}

void ASplinesBuilder::GenerateRegularSplines(
	ALandscape *Landscape,
	FCollisionQueryParams CollisionQueryParams,
	TArray<TArray<OGRPoint>> &PointLists,
	double WorldWidthCm,
	double WorldHeightCm,
	double WorldOriginX,
	double WorldOriginY
)
{
	const int NumLists = PointLists.Num();
	FScopedSlowTask SplinesTask = FScopedSlowTask(NumLists,
		FText::Format(
			LOCTEXT("PointsTask", "Adding landscape splines from {0} lines"),
			FText::AsNumber(NumLists)
		)
	);
	SplinesTask.MakeDialog();

	for (auto &PointList : PointLists)
	{
		AddRegularSpline(Landscape, CollisionQueryParams, PointList, WorldWidthCm, WorldHeightCm, WorldOriginX, WorldOriginY);
	}

	GEditor->NoteSelectionChange();
}

void ASplinesBuilder::AddRegularSpline(
	ALandscape* Landscape,
	FCollisionQueryParams CollisionQueryParams,
	TArray<OGRPoint> &PointList,
	double WorldWidthCm,
	double WorldHeightCm,
	double WorldOriginX,
	double WorldOriginY
)
{
	UWorld *World = Landscape->GetWorld();
	
	int NumPoints = PointList.Num();
	if (NumPoints == 0) return;

	OGRPoint First = PointList[0];
	OGRPoint Last = PointList.Last();
			
	USplineComponent *SplineComponent = NewObject<USplineComponent>(this);
	SplineComponents.Add(SplineComponent);
	SplineComponent->RegisterComponent();
	SplineComponent->ClearSplinePoints();
	SplineComponent->SetMobility(EComponentMobility::Static);

	for (int i = 0; i < NumPoints; i++)
	{
		OGRPoint Point = PointList[i];
		double longitude = Point.getX();
		double lattitude = Point.getY();

		if (Last != First || i < NumPoints - 1) // don't add last point in case the spline is a closed loop
		{
			FVector UnrealCoords = GlobalSettings::EPSG4326ToUnrealCoordinates( { longitude, lattitude }, WorldWidthCm, WorldHeightCm, WorldOriginX, WorldOriginY);
			double x = UnrealCoords[0];
			double y = UnrealCoords[1];
			double z = LandscapeUtils::GetZ(World, CollisionQueryParams, x, y);
			FVector Location = { x, y, z };
			if (z > 0)
			{
				SplineComponent->AddSplinePoint(Location, ESplineCoordinateSpace::World, false);
			}
		}
	}

	if (First == Last) SplineComponent->SetClosedLoop(true);

	SplineComponent->UpdateSpline();
	SplineComponent->bSplineHasBeenEdited = true;

	this->AddInstanceComponent(SplineComponent);
	SplineComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
}


#undef LOCTEXT_NAMESPACE