// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeController.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"
#include "LandscapeUtils/LandscapeUtils.h"
#include "Coordinates/LevelCoordinates.h"
#include "GDALInterface/GDALInterface.h"
#include "LCReporter/LCReporter.h"

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
	if (!Landscape)
	{
		ULCReporter::ShowError(
			LOCTEXT("ULandscapeController::AdjustLandscape::1", "Internal error while adjusting landscape scale and position")
		);
		return;
	}
	FString LandscapeLabel = Landscape->GetActorNameOrLabel();

	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(Landscape->GetWorld());
	if (!GlobalCoordinates) return;

	if (CRS != GlobalCoordinates->CRS)
	{
		ULCReporter::ShowError(FText::Format(
			LOCTEXT("ULandscapeController::AdjustLandscape::3", "Adjust landscape requires the CRS of the landscape to be the same as the LevelCoordinates CRS"),
			FText::FromString(LandscapeLabel)
		));
		return;
	}
	
	double MinCoordWidth = Coordinates[0];
	double MaxCoordWidth = Coordinates[1];
	double MinCoordHeight = Coordinates[2];
	double MaxCoordHeight = Coordinates[3];

	double MinAltitude = Altitudes[0];
	double MaxAltitude = Altitudes[1];
	
	int InsidePixelWidth = InsidePixels[0];
	int InsidePixelHeight = InsidePixels[1];

	double CoordWidth = MaxCoordWidth - MinCoordWidth;
	double CoordHeight = MaxCoordHeight - MinCoordHeight;
	
	double CmPxWidthRatio = CoordWidth * FMath::Abs(GlobalCoordinates->CmPerLongUnit) / InsidePixelWidth;	 // cm / px
	double CmPxHeightRatio = CoordHeight * FMath::Abs(GlobalCoordinates->CmPerLatUnit) / InsidePixelHeight;   // cm / px

	FVector OldScale = Landscape->GetActorScale3D();
	FVector OldLocation = Landscape->GetActorLocation();
	UE_LOG(LogLandscapeCombinator, Log, TEXT("I found the landscape %s. Adjusting scale and position."), *LandscapeLabel);
	
	UE_LOG(LogLandscapeCombinator, Log, TEXT("\nMinCoordWidth (Global EPSG): %f"), MinCoordWidth);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("MaxCoordWidth (Global EPSG): %f"), MaxCoordWidth);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("MinCoordHeight (Global EPSG): %f"), MinCoordHeight);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("MaxCoordHeight (Global EPSG): %f"), MaxCoordHeight);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("\nCoordWidth: %f"), CoordWidth);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("CoordHeight: %f"), CoordHeight);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("InsidePixelsWidth: %d"), InsidePixelWidth);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("InsidePixelsHeight: %d"), InsidePixelHeight);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("CmPxWidthRatio: %f cm/px"), CmPxWidthRatio);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("CmPxHeightRatio: %f cm/px"), CmPxHeightRatio);
		
	double OutsidePixelWidth  = Landscape->ComputeComponentCounts().X * Landscape->ComponentSizeQuads + 1;
	double OutsidePixelHeight = Landscape->ComputeComponentCounts().Y * Landscape->ComponentSizeQuads + 1;
	UE_LOG(LogLandscapeCombinator, Log, TEXT("OutsidePixelWidth: %f"), OutsidePixelWidth);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("OutsidePixelHeight: %f"), OutsidePixelHeight);

	FVector2D MinMaxZBeforeScaling;
	if (!LandscapeUtils::GetLandscapeMinMaxZ(Landscape, MinMaxZBeforeScaling)) return;

	double MinZBeforeScaling = MinMaxZBeforeScaling.X;
	double MaxZBeforeScaling = MinMaxZBeforeScaling.Y;

	UE_LOG(LogLandscapeCombinator, Log, TEXT("\nMinZBeforeScaling: %f"), MinZBeforeScaling);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("MaxZBeforeScaling: %f"), MaxZBeforeScaling);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("MaxAltitude: %f"), MaxAltitude);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("MinAltitude: %f"), MinAltitude);

	FVector NewScale = FVector(
		CoordWidth * FMath::Abs(GlobalCoordinates->CmPerLongUnit) / InsidePixelWidth,
		CoordHeight * FMath::Abs(GlobalCoordinates->CmPerLatUnit) / InsidePixelHeight,
		OldScale.Z * (MaxAltitude - MinAltitude) * ZScale * 100 / (MaxZBeforeScaling - MinZBeforeScaling)
	);

	Landscape->Modify();
	Landscape->SetActorScale3D(NewScale);
	Landscape->PostEditChange();
			
	FVector2D MinMaxZAfterScaling;
	if (!LandscapeUtils::GetLandscapeMinMaxZ(Landscape, MinMaxZAfterScaling)) return;

	double MinZAfterScaling = MinMaxZAfterScaling.X;
	double MaxZAfterScaling = MinMaxZAfterScaling.Y;
			
	// expected location of the top-left corner of the data in Unreal coordinates, assuming (0, 0) world origin
	double TopLeftX = MinCoordWidth * GlobalCoordinates->CmPerLongUnit; // cm
	double TopLeftY = MaxCoordHeight * GlobalCoordinates->CmPerLatUnit; // cm
			
	// expected location of the top-left corner of the data in Unreal coordinates
	double AdjustedTopLeftX = TopLeftX - GlobalCoordinates->WorldOriginLong * GlobalCoordinates->CmPerLongUnit; // cm
	double AdjustedTopLeftY = TopLeftY - GlobalCoordinates->WorldOriginLat * GlobalCoordinates->CmPerLatUnit; // cm

	double LeftPadding = (OutsidePixelWidth - InsidePixelWidth)   * CmPxWidthRatio / 2;   // padding to the left of the landscape in cm
	double TopPadding = (OutsidePixelHeight - InsidePixelHeight) * CmPxHeightRatio / 2; // padding to the top of the landscape in cm

	double NewLocationX = AdjustedTopLeftX - LeftPadding;
	double NewLocationY = AdjustedTopLeftY - TopPadding;
	double NewLocationZ = OldLocation.Z - MaxZAfterScaling + 100 * MaxAltitude * ZScale;
	FVector NewLocation = FVector(NewLocationX, NewLocationY, NewLocationZ);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("\nMinZAfterScaling: %f"), MinZAfterScaling);
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
	Landscape->PostEditChange();
	
	return;
}

#endif

#undef LOCTEXT_NAMESPACE