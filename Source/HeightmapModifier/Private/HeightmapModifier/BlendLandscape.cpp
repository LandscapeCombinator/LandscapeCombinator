// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "HeightmapModifier/BlendLandscape.h"
#include "HeightmapModifier/LogHeightmapModifier.h"
#include "LCReporter/LCReporter.h"
#include "LCCommon/LCSettings.h"
#include "LandscapeUtils/LandscapeUtils.h"

#include "Kismet/GameplayStatics.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeDataAccess.h"
#include "Curves/RichCurve.h"
#include "Misc/MessageDialog.h"

#if WITH_EDITOR

#include "ScopedTransaction.h"

#endif

#define LOCTEXT_NAMESPACE "FHeightmapModifierModule"

UBlendLandscape::UBlendLandscape()
{
	{
		FRichCurveKey ZeroKey, OneKey;
		ZeroKey.Time = 0;
		ZeroKey.Value = 0;
		ZeroKey.InterpMode = ERichCurveInterpMode::RCIM_Cubic;
		ZeroKey.TangentMode = ERichCurveTangentMode::RCTM_User;
		ZeroKey.TangentWeightMode = ERichCurveTangentWeightMode::RCTWM_WeightedBoth;
		ZeroKey.LeaveTangent = FLT_MAX;
		ZeroKey.LeaveTangentWeight = 1;

		OneKey.Time = 0.1;
		OneKey.Value = 1;
		OneKey.InterpMode = ERichCurveInterpMode::RCIM_Cubic;
		OneKey.TangentMode = ERichCurveTangentMode::RCTM_User;
		OneKey.TangentWeightMode = ERichCurveTangentWeightMode::RCTWM_WeightedBoth;
		OneKey.ArriveTangent = 0;
		OneKey.ArriveTangentWeight = 1;
	
		FRichCurve *ThisCurve = new FRichCurve();
		ThisCurve->SetKeys( { ZeroKey, OneKey });
		DegradeThisData = CreateDefaultSubobject<UCurveFloat>(TEXT("DegradeThisData"));
		DegradeThisData->FloatCurve = *ThisCurve;
	}

	{
		FRichCurveKey ZeroKey, OneKey;
		ZeroKey.Time = 0;
		ZeroKey.Value = 1;
		ZeroKey.InterpMode = ERichCurveInterpMode::RCIM_Cubic;
		ZeroKey.TangentMode = ERichCurveTangentMode::RCTM_User;
		ZeroKey.TangentWeightMode = ERichCurveTangentWeightMode::RCTWM_WeightedBoth;
		ZeroKey.LeaveTangent = 0;
		ZeroKey.LeaveTangentWeight = 1;

		OneKey.Time = 1;
		OneKey.Value = 0;
		OneKey.InterpMode = ERichCurveInterpMode::RCIM_Cubic;
		OneKey.TangentMode = ERichCurveTangentMode::RCTM_User;
		OneKey.TangentWeightMode = ERichCurveTangentWeightMode::RCTWM_WeightedBoth;
		OneKey.ArriveTangent = 0;
		OneKey.ArriveTangentWeight = 1;
	
		FRichCurve *OtherCurve = new FRichCurve();
		OtherCurve->SetKeys( { ZeroKey, OneKey });
		DegradeOtherData = CreateDefaultSubobject<UCurveFloat>(TEXT("DegradeOtherData"));
		DegradeOtherData->FloatCurve = *OtherCurve;
	}

}

#if WITH_EDITOR

void UBlendLandscape::BlendWithLandscape(bool bIsUserInitiated)
{
	ALandscape *Landscape = Cast<ALandscape>(GetOwner());
	if (!Landscape) return;

	TArray<AActor*> LandscapeToBlendWiths;
	UGameplayStatics::GetAllActorsOfClass(this->GetWorld(), ALandscape::StaticClass(), LandscapeToBlendWiths);

	FVector2D MinMaxX, MinMaxY, UnusedMinMaxZ;
	if (!LandscapeUtils::GetLandscapeBounds(Landscape, MinMaxX, MinMaxY, UnusedMinMaxZ))
	{
		return;
	}

	double ExtentX = MinMaxX[1] - MinMaxX[0];
	double ExtentY = MinMaxY[1] - MinMaxY[0];


	FVector GlobalTopLeft = FVector(MinMaxX[0], MinMaxY[0], 0);
	FVector GlobalBottomRight = FVector(MinMaxX[1], MinMaxY[1], 0);

	FTransform ThisToGlobal = Landscape->GetTransform();
	FTransform GlobalToThis = ThisToGlobal.Inverse();

	const FScopedTransaction Transaction(LOCTEXT("BlendWithLandscapeToBlendWiths", "Blending With Other Landscapes"));

	if (!LandscapeToBlendWith)
	{
		ULCReporter::ShowError(
			LOCTEXT("UBlendLandscape::BlendWithLandscape", "Please select a LandscapeToBlendWith")
		);
		return;
	}

	if (LandscapeToBlendWith == Landscape)
	{
		ULCReporter::ShowError(
			LOCTEXT("UBlendLandscape::BlendWithLandscape", "Please select a LandscapeToBlendWith different than this one")
		);
		return;
	}

	FVector2D OtherMinMaxX, OtherMinMaxY, UnusedOtherMinMaxZ;
	if (!LandscapeUtils::GetLandscapeBounds(LandscapeToBlendWith, OtherMinMaxX, OtherMinMaxY, UnusedOtherMinMaxZ))
	{
		ULCReporter::ShowError(FText::Format(
			LOCTEXT("UBlendLandscape::BlendWithLandscape::2", "Could not compute landscape bounds of Landscape {0}."),
			FText::FromString(LandscapeToBlendWith->GetActorNameOrLabel())
		));
		return;
	}

	if (
		MinMaxX[0] <= OtherMinMaxX[1] && OtherMinMaxX[0] <= MinMaxX[1] &&
		MinMaxY[0] <= OtherMinMaxY[1] && OtherMinMaxY[0] <= MinMaxY[1]
	)
	{
		int32 SizeX = Landscape->ComputeComponentCounts().X * Landscape->ComponentSizeQuads + 1;
		int32 SizeY = Landscape->ComputeComponentCounts().Y * Landscape->ComponentSizeQuads + 1;
		uint16* OldHeightmapData = (uint16*) malloc(SizeX * SizeY * (sizeof (uint16)));
		FHeightmapAccessor<false> HeightmapAccessor(Landscape->GetLandscapeInfo());
		HeightmapAccessor.GetDataFast(0, 0, SizeX - 1, SizeY - 1, OldHeightmapData);

		FTransform OtherToGlobal = LandscapeToBlendWith->GetTransform();
		FTransform GlobalToOther = OtherToGlobal.Inverse();
		FVector OtherTopLeft = GlobalToOther.TransformPosition(GlobalTopLeft);
		FVector OtherBottomRight = GlobalToOther.TransformPosition(GlobalBottomRight);
			
		FHeightmapAccessor<false> OtherHeightmapAccessor(LandscapeToBlendWith->GetLandscapeInfo());

		// We are only interested in the heightmap data from `LandscapeToBlendWith` in the rectangle delimited by
		// the `TopLeft` and `BottomRight` corners 
			
		int32 OtherTotalSizeX = LandscapeToBlendWith->ComputeComponentCounts().X * LandscapeToBlendWith->ComponentSizeQuads + 1;
		int32 OtherTotalSizeY = LandscapeToBlendWith->ComputeComponentCounts().Y * LandscapeToBlendWith->ComponentSizeQuads + 1;
		int32 OtherX1 = FMath::Min(OtherTotalSizeX - 1, FMath::Max(0, OtherTopLeft.X));
		int32 OtherX2 = FMath::Min(OtherTotalSizeX - 1, FMath::Max(0, OtherBottomRight.X));
		int32 OtherY1 = FMath::Min(OtherTotalSizeY - 1, FMath::Max(0, OtherTopLeft.Y));
		int32 OtherY2 = FMath::Min(OtherTotalSizeY - 1, FMath::Max(0, OtherBottomRight.Y));
		int32 OtherSizeX = OtherX2 - OtherX1 + 1;
		int32 OtherSizeY = OtherY2 - OtherY1 + 1;

		if (OtherSizeX <= 0 || OtherSizeY <= 0)
		{
			ULCReporter::ShowError(FText::Format(
				LOCTEXT("UBlendLandscape::BlendWithLandscape::2", "Could not blend with Landscape{0}. Its resolution might be too small."),
				FText::FromString(LandscapeToBlendWith->GetActorNameOrLabel())
			));
			return;
		}

		UE_LOG(LogHeightmapModifier, Log, TEXT("Blending with Landscape %s (MinX: %d, MaxX: %d, MinY: %d, MaxY: %d)"),
			*LandscapeToBlendWith->GetActorNameOrLabel(), OtherX1, OtherX2, OtherY1, OtherY2
		);

		uint16* OtherOldHeightmapData = (uint16*) malloc(OtherSizeX * OtherSizeY * (sizeof (uint16)));
		OtherHeightmapAccessor.GetDataFast(OtherX1, OtherY1, OtherX2, OtherY2, OtherOldHeightmapData);


		/* Modify the data of the other landscape */
		
		double MaxDistance = ((double) FMath::Min(OtherSizeX, OtherSizeY)) / 2;
		uint16* OtherNewHeightmapData = (uint16*) malloc(OtherSizeX * OtherSizeY * (sizeof (uint16)));
		for (int X = 0; X < OtherSizeX; X++)
		{
			for (int Y = 0; Y < OtherSizeY; Y++)
			{
				FVector OtherPosition = FVector(OtherX1 + X, OtherY1 + Y, 0);
				FVector GlobalPosition = OtherToGlobal.TransformPosition(OtherPosition);
				FVector ThisPosition = GlobalToThis.TransformPosition(GlobalPosition);

				int ThisX = ThisPosition.X;
				int ThisY = ThisPosition.Y;

				// if this landscape has data at this position
				if (ThisX >= 0 && ThisY >= 0 && ThisX < SizeX && ThisY < SizeY && OldHeightmapData[ThisX + ThisY * SizeX] != ThisLandscapeNoData)
				{
					// we transform the data according to the curve
					double DistanceFromBorder = FMath::Min(X, FMath::Min(Y, FMath::Min(OtherSizeX - X - 1, OtherSizeY - Y - 1)));
					double DistanceRatio = DistanceFromBorder / MaxDistance;
					double Alpha = DegradeOtherData->GetFloatValue(DistanceRatio);
					OtherNewHeightmapData[X + Y * OtherSizeX] = Alpha * OtherOldHeightmapData[X + Y * OtherSizeX] + (1 - Alpha) * OtherLandscapeNoData;
				}
				else
				{					
					// otherwise, we keep the old data
					OtherNewHeightmapData[X + Y * OtherSizeX] = OtherOldHeightmapData[X + Y * OtherSizeX];
				}
			}
		}

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
		
		if (bUseEditLayers)
		{
			/* Make the new data to be a difference, so that it can be used on a different edit layer (>= 5.3 only) */
	
			LandscapeUtils::MakeDataRelativeTo(OtherSizeX, OtherSizeY, OtherNewHeightmapData, OtherOldHeightmapData);

			/* Write difference data to a new edit layer (>= 5.3 only) */

			int OtherLayerIndex = LandscapeToBlendWith->CreateLayer();
			if (OtherLayerIndex == INDEX_NONE)
			{
				ULCReporter::ShowError(FText::Format(
					LOCTEXT("UHeightmapModifier::ModifyHeightmap::10", "Could not create landscape layer. Make sure that edit layers are enabled on Landscape {0}."),
					FText::FromString(LandscapeToBlendWith->GetActorNameOrLabel())
				));
				free(OldHeightmapData);
				free(OtherOldHeightmapData);
				free(OtherNewHeightmapData);
				return;
			}

			OtherHeightmapAccessor.SetEditLayer(LandscapeToBlendWith->GetLayer(OtherLayerIndex)->Guid);
		}

#endif


		OtherHeightmapAccessor.SetData(OtherX1, OtherY1, OtherX2, OtherY2, OtherNewHeightmapData);
		free(OtherNewHeightmapData);

		
		/* Modify the data of this landscape */
		
		double MaxDistance2 = ((double) FMath::Min(SizeX, SizeY)) / 2;
		uint16* NewHeightmapData = (uint16*) malloc(SizeX * SizeY * (sizeof (uint16)));
		for (int X = 0; X < SizeX; X++)
		{
			for (int Y = 0; Y < SizeY; Y++)
			{
				FVector ThisPosition = FVector(X, Y, 0);
				FVector GlobalPosition = ThisToGlobal.TransformPosition(ThisPosition);
				FVector OtherPosition = GlobalToOther.TransformPosition(GlobalPosition);

				int OtherX = OtherPosition.X;
				int OtherY = OtherPosition.Y;

				int OtherXOffset = FMath::Max(FMath::Min(OtherX2, OtherX - OtherX1), 0);
				int OtherYOffset = FMath::Max(FMath::Min(OtherY2, OtherY - OtherY1), 0);
				
				// if the other landscape has data at this position
				if (OtherOldHeightmapData[OtherXOffset + OtherYOffset * OtherSizeX] != OtherLandscapeNoData)
				{
					// we transform the data according to the curve
					double DistanceFromBorder = FMath::Min(X, FMath::Min(Y, FMath::Min(SizeX - X - 1, SizeY - Y - 1)));
					double DistanceRatio = DistanceFromBorder / MaxDistance2;
					double Alpha = DegradeThisData->GetFloatValue(DistanceRatio);
					NewHeightmapData[X + Y * SizeX] = Alpha * OldHeightmapData[X + Y * SizeX] + (1 - Alpha) * OtherLandscapeNoData;
				}
				else
				{
					// otherwise, we keep the old data
					NewHeightmapData[X + Y * SizeX] = OldHeightmapData[X + Y * SizeX];
				}
			}
		}



#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)

		/* Make the new data to be a difference, so that it can be used on a different edit layer (>= 5.3 only) */
	
		if (bUseEditLayers)
		{
			LandscapeUtils::MakeDataRelativeTo(SizeX, SizeY, NewHeightmapData, OldHeightmapData);
	
			int LayerIndex = Landscape->CreateLayer();
			if (LayerIndex == INDEX_NONE)
			{
				ULCReporter::ShowError(FText::Format(
					LOCTEXT("UHeightmapModifier::ModifyHeightmap::10", "Could not create landscape layer. Make sure that edit layers are enabled on Landscape {0}."),
					FText::FromString(Landscape->GetActorNameOrLabel())
				));
				free(OldHeightmapData);
				free(NewHeightmapData);
				free(OtherOldHeightmapData);
				return;
			}

			OtherHeightmapAccessor.SetEditLayer(Landscape->GetLayer(LayerIndex)->Guid);
		}
#endif

		free(OldHeightmapData);
		free(OtherOldHeightmapData);

		HeightmapAccessor.SetData(0, 0, SizeX - 1, SizeY - 1, NewHeightmapData);

		free(NewHeightmapData);

		UE_LOG(LogHeightmapModifier, Log, TEXT("Finished blending with Landscape %s (MinX: %d, MaxX: %d, MinY: %d, MaxY: %d)"),
			*LandscapeToBlendWith->GetActorNameOrLabel(), OtherX1, OtherX2, OtherY1, OtherY2
		);

		if (bIsUserInitiated)
		{
			ULCReporter::ShowMessage(
				FText::Format(
					LOCTEXT(
						"UBlendLandscape::BlendWithLandscape::Finished",
						"Finished blending with Landscape {0}."
					),
					FText::FromString(LandscapeToBlendWith->GetActorNameOrLabel()),
					false
				),
				"SuppressBlendInfoMessages",
				LOCTEXT("BlendLandscapeFinishedTitle", "Landscape Blending"),
				false,
				FAppStyle::GetBrush("Icons.InfoWithColor.Large")
			);
		}
		return;
	}
	else
	{
		UE_LOG(LogHeightmapModifier, Log, TEXT("Skipping blending with Landscape %s, which does not overlap with Landscape %s"),
			*LandscapeToBlendWith->GetActorNameOrLabel(),
			*Landscape->GetActorNameOrLabel()
		);
		ULCReporter::ShowError(FText::Format(
			LOCTEXT("UBlendLandscape::BlendWithLandscape::Skip", "Skipping blending with Landscape {0}, which does not overlap with Landscape {1}."),
			FText::FromString(LandscapeToBlendWith->GetActorNameOrLabel()),
			FText::FromString(Landscape->GetActorNameOrLabel())
		));
		return;
	}

	return;
}

#endif

#undef LOCTEXT_NAMESPACE
