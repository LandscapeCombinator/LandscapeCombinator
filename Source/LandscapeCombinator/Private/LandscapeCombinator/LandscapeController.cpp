// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeController.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"
#include "LandscapeUtils/LandscapeUtils.h"
#include "Coordinates/LevelCoordinates.h"
#include "GDALInterface/GDALInterface.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Misc/MessageDialog.h"
#include "Kismet/GameplayStatics.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeDataAccess.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

#if WITH_EDITOR

void ULandscapeController::AdjustLandscape()
{
	ALandscape *Landscape = Cast<ALandscape>(GetOwner());
	if (!IsValid(Landscape))
	{
		LCReporter::ShowError(
			LOCTEXT("ULandscapeController::AdjustLandscape::1", "Internal error while adjusting landscape scale and position")
		);
		return;
	}
	FString LandscapeLabel = Landscape->GetActorNameOrLabel();

	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(Landscape->GetWorld());
	if (!IsValid(GlobalCoordinates)) return;

	if (CRS != GlobalCoordinates->CRS)
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("ULandscapeController::AdjustLandscape::3", "Adjust landscape requires the CRS of the landscape to be the same as the LevelCoordinates CRS"),
			FText::FromString(LandscapeLabel)
		));
		return;
	}
	
	const double MinCoordWidth = Coordinates[0];
	const double MaxCoordWidth = Coordinates[1];
	const double MinCoordHeight = Coordinates[2];
	const double MaxCoordHeight = Coordinates[3];

	const double MinAltitude = Altitudes[0];
	const double MaxAltitude = Altitudes[1];
	
	const int InsidePixelWidth = InsidePixels[0];
	const int InsidePixelHeight = InsidePixels[1];

	const double CoordWidth = MaxCoordWidth - MinCoordWidth;
	const double CoordHeight = MaxCoordHeight - MinCoordHeight;
	
	const double CmPxWidthRatio = CoordWidth * FMath::Abs(GlobalCoordinates->CmPerLongUnit) / InsidePixelWidth;	 // cm / px
	const double CmPxHeightRatio = CoordHeight * FMath::Abs(GlobalCoordinates->CmPerLatUnit) / InsidePixelHeight;   // cm / px

	const FVector OldScale = Landscape->GetActorScale3D();
	const FVector OldLocation = Landscape->GetActorLocation();
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Found Landscape %s. Adjusting scale and position."), *LandscapeLabel);
	
	UE_LOG(LogLandscapeCombinator, Log, TEXT("MinCoordWidth (Global EPSG): %f"), MinCoordWidth);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("MaxCoordWidth (Global EPSG): %f"), MaxCoordWidth);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("MinCoordHeight (Global EPSG): %f"), MinCoordHeight);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("MaxCoordHeight (Global EPSG): %f"), MaxCoordHeight);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("CoordWidth: %f"), CoordWidth);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("CoordHeight: %f"), CoordHeight);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("CmPerLongUnit: %f"), GlobalCoordinates->CmPerLongUnit);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("CmPerLatUnit: %f"), GlobalCoordinates->CmPerLatUnit);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("InsidePixelsWidth: %d"), InsidePixelWidth);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("InsidePixelsHeight: %d"), InsidePixelHeight);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("CmPxWidthRatio: %f cm/px"), CmPxWidthRatio);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("CmPxHeightRatio: %f cm/px"), CmPxHeightRatio);
		
	const double OutsidePixelWidth  = Landscape->ComputeComponentCounts().X * Landscape->ComponentSizeQuads + 1;
	const double OutsidePixelHeight = Landscape->ComputeComponentCounts().Y * Landscape->ComponentSizeQuads + 1;
	UE_LOG(LogLandscapeCombinator, Log, TEXT("OutsidePixelWidth: %f"), OutsidePixelWidth);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("OutsidePixelHeight: %f"), OutsidePixelHeight);

	FVector2D MinMaxZBeforeScaling;
	if (!LandscapeUtils::GetLandscapeMinMaxZ(Landscape, MinMaxZBeforeScaling)) return;

	const double MinZBeforeScaling = MinMaxZBeforeScaling.X;
	const double MaxZBeforeScaling = MinMaxZBeforeScaling.Y;
	const double ZSpan = MaxZBeforeScaling - MinZBeforeScaling;

	UE_LOG(LogLandscapeCombinator, Log, TEXT("MinZBeforeScaling: %f"), MinZBeforeScaling);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("MaxZBeforeScaling: %f"), MaxZBeforeScaling);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("MaxAltitude: %f"), MaxAltitude);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("MinAltitude: %f"), MinAltitude);


	const double NewLandscapeXScale = CoordWidth * FMath::Abs(GlobalCoordinates->CmPerLongUnit) / InsidePixelWidth;
	const double NewLandscapeYScale = CoordHeight * FMath::Abs(GlobalCoordinates->CmPerLatUnit) / InsidePixelHeight;
	double NewLandscapeZScale;
	if (ZSpan <= UE_SMALL_NUMBER)
	{
		UE_LOG(LogLandscapeCombinator, Log, TEXT("Z Span of landscape is too small, so we're using the X and Y scale average for the Z scale."));
		NewLandscapeZScale = (NewLandscapeXScale + NewLandscapeYScale) / 2.0;
	}
	else
	{
		NewLandscapeZScale = OldScale.Z * (MaxAltitude - MinAltitude) * ZScale * 100 / ZSpan;
	}

	FVector NewScale = FVector(NewLandscapeXScale, NewLandscapeYScale, NewLandscapeZScale);

	UE_LOG(LogLandscapeCombinator, Log, TEXT("NewScale: %s"), *NewScale.ToString());

	Landscape->Modify();
	Landscape->SetActorScale3D(NewScale);
	Landscape->PostEditChange();
			
	FVector2D MinMaxZAfterScaling;
	if (!LandscapeUtils::GetLandscapeMinMaxZ(Landscape, MinMaxZAfterScaling)) return;

	const double MinZAfterScaling = MinMaxZAfterScaling.X;
	const double MaxZAfterScaling = MinMaxZAfterScaling.Y;
			
	// expected location of the top-left corner of the data in Unreal coordinates, assuming (0, 0) world origin
	const double TopLeftX = MinCoordWidth * GlobalCoordinates->CmPerLongUnit; // cm
	const double TopLeftY = MaxCoordHeight * GlobalCoordinates->CmPerLatUnit; // cm
			
	// expected location of the top-left corner of the data in Unreal coordinates
	const double AdjustedTopLeftX = TopLeftX - GlobalCoordinates->WorldOriginLong * GlobalCoordinates->CmPerLongUnit; // cm
	const double AdjustedTopLeftY = TopLeftY - GlobalCoordinates->WorldOriginLat * GlobalCoordinates->CmPerLatUnit; // cm

	const double LeftPadding = (OutsidePixelWidth - InsidePixelWidth)  * CmPxWidthRatio / 2;  // padding to the left of the landscape in cm
	const double TopPadding = (OutsidePixelHeight - InsidePixelHeight) * CmPxHeightRatio / 2; // padding to the top of the landscape in cm

	const double NewLocationX = AdjustedTopLeftX - LeftPadding;
	const double NewLocationY = AdjustedTopLeftY - TopPadding;
	
	double NewLocationZ;
	if (ZSpan <= UE_SMALL_NUMBER) NewLocationZ = 0;
	else NewLocationZ = OldLocation.Z - MaxZAfterScaling + 100 * MaxAltitude * ZScale;
	const FVector NewLocation = FVector(NewLocationX, NewLocationY, NewLocationZ);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("MinZAfterScaling: %f"), MinZAfterScaling);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("MaxZAfterScaling: %f"), MaxZAfterScaling);

	UE_LOG(LogLandscapeCombinator, Log, TEXT("TopLeftX: %f"), TopLeftX);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("TopLeftY: %f"), TopLeftY);

	UE_LOG(LogLandscapeCombinator, Log, TEXT("GlobalCoordinates->WorldOriginLong * GlobalCoordinates->CmPerLongUnit: %f"), GlobalCoordinates->WorldOriginLong * GlobalCoordinates->CmPerLongUnit);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("GlobalCoordinates->WorldOriginLat * GlobalCoordinates->CmPerLatUnit: %f"), GlobalCoordinates->WorldOriginLat * GlobalCoordinates->CmPerLatUnit);

	UE_LOG(LogLandscapeCombinator, Log, TEXT("AdjustedTopLeftX: %f"), AdjustedTopLeftX);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("AdjustedTopLeftY: %f"), AdjustedTopLeftY);

	UE_LOG(LogLandscapeCombinator, Log, TEXT("LeftPadding: %f"), LeftPadding);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("TopPadding: %f"), TopPadding);

	UE_LOG(LogLandscapeCombinator, Log, TEXT("OldLocation.X: %f"), OldLocation.X);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("OldLocation.Y: %f"), OldLocation.Y);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("OldLocation.Z: %f"), OldLocation.Z);

	UE_LOG(LogLandscapeCombinator, Log, TEXT("NewLocationX: %f"), NewLocationX);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("NewLocationY: %f"), NewLocationY);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("NewLocationZ: %f"), NewLocationZ);

	Landscape->Modify();
	Landscape->SetActorLocation(NewLocation);
	Landscape->GetRootComponent()->SetRelativeLocation(NewLocation);

	Landscape->PostEditChange();
	
	return;
}

#endif

#undef LOCTEXT_NAMESPACE