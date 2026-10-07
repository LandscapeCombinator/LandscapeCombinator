// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "BuildingsFromSplines/OpeningsVisualizer.h"

#if WITH_EDITOR

#include "SceneManagement.h"
#include "EditorViewportClient.h"
#include "ScopedTransaction.h"
#include "InputCoreTypes.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "FBuildingsFromSplinesModule"

struct HOpeningProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();
	HOpeningProxy(const UActorComponent* InComponent, ULevelDescription* InLevel, int32 InIndex)
		: HComponentVisProxy(InComponent, HPP_UI), Level(InLevel), Index(InIndex) {}

	TWeakObjectPtr<ULevelDescription> Level;
	int32 Index;
};
IMPLEMENT_HIT_PROXY(HOpeningProxy, HComponentVisProxy)

struct HStairsProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();
	HStairsProxy(const UActorComponent* InComponent) : HComponentVisProxy(InComponent, HPP_UI) {}
};
IMPLEMENT_HIT_PROXY(HStairsProxy, HComponentVisProxy)

ABuilding* FOpeningsVisualizer::GetBuilding() const
{
	return EditedComponent.IsValid() ? Cast<ABuilding>(EditedComponent->GetOwner()) : nullptr;
}

void FOpeningsVisualizer::TrackingStopped(FEditorViewportClient* VC, bool bDidMove)
{
	bAllowDuplication = true;
}

bool FOpeningsVisualizer::IsSelectionValid() const
{
	ULevelDescription* Level = SelectedLevel.Get();
	return GetBuilding() && IsValid(Level) && Level->Openings.IsValidIndex(SelectedIndex) && Level->HasSegment(Level->Openings[SelectedIndex].SegmentKey);
}

void FOpeningsVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const ABuilding* Building = Cast<ABuilding>(Component->GetOwner());
	if (!Building) return;

	TArray<FOpeningHandle> Handles;
	Building->GetOpeningHandles(Handles);

	if (Building->BCfg && Building->BCfg->bAutoStairs && Building->StairsHandle)
	{
		PDI->SetHitProxy(new HStairsProxy(Component));
		PDI->DrawPoint(Building->StairsHandle->GetComponentLocation(), bStairsSelected ? FLinearColor::Yellow : FLinearColor(0.2f, 0.6f, 1.f), bStairsSelected ? 32.f : 26.f, SDPG_Foreground);
		PDI->SetHitProxy(nullptr);
	}
	for (const FOpeningHandle& H : Handles)
	{
		const bool bSelected = H.Level == SelectedLevel && H.OpeningIndex == SelectedIndex;
		PDI->SetHitProxy(new HOpeningProxy(Component, H.Level.Get(), H.OpeningIndex));
		PDI->DrawPoint(H.WorldLocation, bSelected ? FLinearColor::Yellow : FLinearColor(0.2f, 0.6f, 1.f), bSelected ? 32.f : 26.f, SDPG_Foreground);
		PDI->SetHitProxy(nullptr);
	}
}

bool FOpeningsVisualizer::VisProxyHandleClick(FEditorViewportClient* VC, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	if (VisProxy && VisProxy->IsA(HStairsProxy::StaticGetType()))
	{
		EditedComponent = const_cast<UOpeningsVisualizerComponent*>(Cast<UOpeningsVisualizerComponent>(VisProxy->Component.Get()));
		SelectedLevel.Reset();
		SelectedIndex = INDEX_NONE;
		bStairsSelected = true;
		return true;
	}
	if (!VisProxy || !VisProxy->IsA(HOpeningProxy::StaticGetType())) return false;
	const HOpeningProxy* Proxy = static_cast<HOpeningProxy*>(VisProxy);

	EditedComponent = const_cast<UOpeningsVisualizerComponent*>(Cast<UOpeningsVisualizerComponent>(VisProxy->Component.Get()));
	SelectedLevel = Proxy->Level;
	SelectedIndex = Proxy->Index;
	bStairsSelected = false;
	return IsSelectionValid();
}

bool FOpeningsVisualizer::GetWidgetLocation(const FEditorViewportClient* VC, FVector& OutLocation) const
{
	if (bStairsSelected && GetBuilding() && GetBuilding()->StairsHandle)
	{
		OutLocation = GetBuilding()->StairsHandle->GetComponentLocation();
		return true;
	}
	if (!IsSelectionValid()) return false;

	TArray<FOpeningHandle> Handles;
	GetBuilding()->GetOpeningHandles(Handles);
	for (const FOpeningHandle& H : Handles)
	{
		if (H.Level == SelectedLevel && H.OpeningIndex == SelectedIndex)
		{
			OutLocation = H.WorldLocation;
			return true;
		}
	}
	return false;
}

bool FOpeningsVisualizer::HandleInputDelta(FEditorViewportClient* VC, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
	if (bStairsSelected && GetBuilding() && GetBuilding()->StairsHandle && VC->GetCurrentWidgetAxis() != EAxisList::None)
	{
		ABuilding* StairsBuilding = GetBuilding();
		StairsBuilding->StairsHandle->Modify();
		StairsBuilding->StairsHandle->AddWorldOffset(FVector(DeltaTranslate.X, DeltaTranslate.Y, 0));
		StairsBuilding->StairsHandle->AddWorldRotation(FRotator(0, DeltaRotate.Yaw, 0));
		StairsBuilding->OnOpeningsEdited();
		return true;
	}
	if (!IsSelectionValid() || VC->GetCurrentWidgetAxis() == EAxisList::None) return false;

	ABuilding* Building = GetBuilding();
	ULevelDescription* Level = SelectedLevel.Get();
	Level->Modify();

	if (VC->IsAltPressed() && bAllowDuplication) // Alt-drag: leave a copy at the old position
	{
		bAllowDuplication = false;
		Level->Openings.Add(FWallOpening(Level->Openings[SelectedIndex]));
	}

	FWallOpening& Opening = Level->Openings[SelectedIndex];
	UWallSegment* Segment = Level->GetSegment(Opening.SegmentKey);
	Segment->Modify(); // shared definition: all openings/floors using this key change

	const USplineComponent* Spline = Building->BaseClockwiseSplineComponent;
	const double ScaleZ = FMath::Max(KINDA_SMALL_NUMBER, (double)Building->GetActorScale3D().Z);
	Segment->HoleDistanceToFloor = FMath::Max(0.0, Segment->HoleDistanceToFloor + DeltaTranslate.Z / ScaleZ);

	const FVector LocalTangent = Spline->GetTangentAtDistanceAlongSpline(Opening.Position, ESplineCoordinateSpace::Local).GetSafeNormal();
	const FVector WorldTangent = Spline->GetComponentTransform().TransformVector(LocalTangent); // length = world units per local unit
	const double Ratio = FMath::Max(KINDA_SMALL_NUMBER, WorldTangent.Size());
	const double MaxPosition = FMath::Max(0.0, Spline->GetSplineLength() - Segment->SegmentLength);
	Opening.Position = FMath::Clamp(Opening.Position + FVector::DotProduct(DeltaTranslate, WorldTangent / Ratio) / Ratio, 0.0, MaxPosition);

	Building->OnOpeningsEdited();
	return true;
}

bool FOpeningsVisualizer::HandleInputKey(FEditorViewportClient* VC, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (bStairsSelected) return Key == EKeys::Delete;
	if (Key != EKeys::Delete || !IsSelectionValid()) return false;
	if (Event != IE_Pressed) return true;

	const FScopedTransaction Transaction(LOCTEXT("DeleteOpening", "Delete Opening"));
	ULevelDescription* Level = SelectedLevel.Get();
	Level->Modify();
	Level->Openings.RemoveAt(SelectedIndex);
	EndEditing();

	GetBuilding()->OnOpeningsEdited();
	return true;
}

#undef LOCTEXT_NAMESPACE
#endif
