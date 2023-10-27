// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "SplineImporter/SplineCollection.h"
#include "SplineImporter/LogSplineImporter.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

ASplineCollection::ASplineCollection()
{
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent")));
	RootComponent->SetMobility(EComponentMobility::Static);
}

void ASplineCollection::ToggleLinear()
{
	for (auto& SplineComponent : SplineComponents)
	{
		if (IsValid(SplineComponent))
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
}

#undef LOCTEXT_NAMESPACE
