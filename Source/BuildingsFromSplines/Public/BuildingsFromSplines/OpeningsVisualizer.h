// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "ComponentVisualizer.h"
#include "BuildingsFromSplines/Building.h"

class FOpeningsVisualizer : public FComponentVisualizer
{
public:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* VC, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	virtual bool GetWidgetLocation(const FEditorViewportClient* VC, FVector& OutLocation) const override;
	virtual bool HandleInputDelta(FEditorViewportClient* VC, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;
	virtual bool HandleInputKey(FEditorViewportClient* VC, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	virtual void TrackingStopped(FEditorViewportClient* VC, bool bDidMove) override;
	virtual void EndEditing() override { SelectedLevel.Reset(); SelectedIndex = INDEX_NONE; bStairsSelected = false; }
	virtual UActorComponent* GetEditedComponent() const override { return EditedComponent.Get(); }

protected:
	TWeakObjectPtr<UOpeningsVisualizerComponent> EditedComponent;
	TWeakObjectPtr<ULevelDescription> SelectedLevel;
	int32 SelectedIndex = INDEX_NONE;
	bool bStairsSelected = false;
	bool bAllowDuplication = true; // making sure alt-drag duplicates only once per drag

	ABuilding* GetBuilding() const;
	bool IsSelectionValid() const;
};

#endif
