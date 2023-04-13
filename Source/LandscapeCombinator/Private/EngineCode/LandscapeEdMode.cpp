// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/LandscapeEditor/Private/LandscapeEdMode.h"

#include "Algo/Accumulate.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "SceneView.h"
#include "Engine/Texture2D.h"
#include "EditorViewportClient.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Engine/Light.h"
#include "Engine/Selection.h"
#include "EditorModeManager.h"
#include "LandscapeFileFormatInterface.h"
#include "LandscapeEditorModule.h"
#include "LandscapeEditorObject.h"
#include "Landscape.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeSubsystem.h"
#include "LandscapeSettings.h"

#include "EditorSupportDelegates.h"
#include "ScopedTransaction.h"
#include "LandscapeEdit.h"
#include "LandscapeEditorUtils.h"
#include "LandscapeRender.h"
#include "LandscapeDataAccess.h"
#include "Framework/Commands/UICommandList.h"
#include "LevelEditor.h"
#include "Toolkits/ToolkitManager.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "InstancedFoliageActor.h"
#include "EditorWorldExtension.h"
#include "Editor/LandscapeEditor/Private/LandscapeEdModeTools.h"
#include "LandscapeInfoMap.h"
#include "LandscapeImportHelper.h"
#include "LandscapeConfigHelper.h"
#include "UObject/ObjectSaveContext.h"

//Slate dependencies
#include "Misc/FeedbackContext.h"
#include "IAssetViewport.h"
#include "SLevelViewport.h"
#include "Editor/LandscapeEditor/Private/SLandscapeEditor.h"
#include "Framework/Application/SlateApplication.h"

// Classes
#include "LandscapeMaterialInstanceConstant.h"
#include "LandscapeSplinesComponent.h"
#include "ComponentReregisterContext.h"
#include "EngineUtils.h"
#include "Misc/ScopedSlowTask.h"
#include "Editor/LandscapeEditor/Private/LandscapeEditorCommands.h"
#include "Framework/Commands/InputBindingManager.h"
#include "MouseDeltaTracker.h"
#include "Interfaces/IMainFrameModule.h"
#include "LandscapeBlueprintBrushBase.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Settings/EditorExperimentalSettings.h"
#include "ComponentRecreateRenderStateContext.h"
#include "VisualLogger/VisualLogger.h"

#define LOCTEXT_NAMESPACE "Landscape"

DEFINE_LOG_CATEGORY(LogLandscapeEdMode);

void FLandscapeTool::SetEditRenderType()
{
	GLandscapeEditRenderMode = ELandscapeEditRenderMode::SelectRegion | (GLandscapeEditRenderMode & ELandscapeEditRenderMode::BitMaskForMask);
}

namespace LandscapeTool
{
	UMaterialInstance* CreateMaterialInstance(UMaterialInterface* BaseMaterial)
	{
		UObject* Outer = GetTransientPackage();
		// Use the base material's name as the base of our MIC to help debug: 
		FString MICName(FString::Format(TEXT("LandscapeMaterialInstanceConstant_{0}"), { *BaseMaterial->GetName() }));
		ULandscapeMaterialInstanceConstant* MaterialInstance = NewObject<ULandscapeMaterialInstanceConstant>(Outer, MakeUniqueObjectName(Outer, ULandscapeMaterialInstanceConstant::StaticClass(), FName(MICName)));
		MaterialInstance->bEditorToolUsage = true;
		MaterialInstance->SetParentEditorOnly(BaseMaterial);
		MaterialInstance->PostEditChange();
		return MaterialInstance;
	}

	/** Indicates the user is currently moving the landscape gizmo object by dragging the mouse. */
	bool GIsGizmoDragging = false;
	/** Indicates the user is currently changing the landscape brush radius/falloff by dragging the mouse. */
	bool GIsAdjustingBrush = false;
}

/** FGCObject interface */
void FEdModeLandscape::AddReferencedObjects(FReferenceCollector& Collector)
{
	// Call parent implementation
	FEdMode::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(UISettings);

	Collector.AddReferencedObject(GLayerDebugColorMaterial);
	Collector.AddReferencedObject(GSelectionColorMaterial);
	Collector.AddReferencedObject(GSelectionRegionMaterial);
	Collector.AddReferencedObject(GMaskRegionMaterial);
	Collector.AddReferencedObject(GColorMaskRegionMaterial);
	Collector.AddReferencedObject(GLandscapeBlackTexture);
	Collector.AddReferencedObject(GLandscapeLayerUsageMaterial);
	Collector.AddReferencedObject(GLandscapeDirtyMaterial);
}

bool FEdModeLandscape::UsesToolkits() const
{
	return true;
}

TSharedRef<FUICommandList> FEdModeLandscape::GetUICommandList() const
{
	check(Toolkit.IsValid());
	return Toolkit->GetToolkitCommands();
}

ELandscapeToolTargetType::Type FEdModeLandscape::GetLandscapeToolTargetType() const
{
	if (CurrentToolMode)
	{
		if (CurrentToolMode->ToolModeName == "ToolMode_Sculpt")
		{
			return CurrentToolTarget.TargetType == ELandscapeToolTargetType::Visibility ? ELandscapeToolTargetType::Visibility : ELandscapeToolTargetType::Heightmap;
		}
		else if (CurrentToolMode->ToolModeName == "ToolMode_Paint")
		{
			return ELandscapeToolTargetType::Weightmap;
		}
	}
	return ELandscapeToolTargetType::Invalid;
}

const FLandscapeLayer* FEdModeLandscape::GetLandscapeSelectedLayer() const
{
	return GetCurrentLayer();
}

ULandscapeLayerInfoObject* FEdModeLandscape::GetSelectedLandscapeLayerInfo() const
{
	return CurrentToolTarget.LayerInfo.Get();
}

int32 FEdModeLandscape::GetAccumulatedAllLandscapesResolution() const
{
	int32 TotalResolution = 0;

	TotalResolution = Algo::Accumulate(LandscapeList, TotalResolution, [](int32 Accum, const FLandscapeListInfo& LandscapeListInfo)
	{
		return Accum + ((LandscapeListInfo.Width > 0) ? LandscapeListInfo.Width : 1)
					 * ((LandscapeListInfo.Height > 0) ? LandscapeListInfo.Height : 1);
	});

	return TotalResolution;
}

bool FEdModeLandscape::IsLandscapeResolutionCompliant() const
{
	const TObjectPtr<const ULandscapeSettings> Settings = GetDefault<ULandscapeSettings>();
	check(Settings);

	if (Settings->IsLandscapeResolutionRestricted())
	{
		int32 TotalResolution = (CurrentTool != nullptr) ? CurrentTool->GetToolActionResolutionDelta() : 0;
		TotalResolution += GetAccumulatedAllLandscapesResolution();
		
		return TotalResolution <= Settings->GetTotalResolutionLimit();
	}

	return true;
}

bool FEdModeLandscape::DoesCurrentToolAffectEditLayers() const
{
	return (CurrentTool != nullptr) ? CurrentTool->AffectsEditLayers() : false;
}

FText FEdModeLandscape::GetLandscapeResolutionErrorText() const
{
	const TObjectPtr<const ULandscapeSettings> Settings = GetDefault<ULandscapeSettings>();
	check(Settings);

	return FText::Format(LOCTEXT("LandscapeResolutionError", "Total resolution for all Landscape actors cannot exceed the equivalent of {0} x {0}."), Settings->GetSideResolutionLimit());
}

int32 FEdModeLandscape::GetNewLandscapeResolutionX() const
{
	return UISettings->NewLandscape_ComponentCount.X * UISettings->NewLandscape_SectionsPerComponent * UISettings->NewLandscape_QuadsPerSection + 1;
}

int32 FEdModeLandscape::GetNewLandscapeResolutionY() const
{
	return UISettings->NewLandscape_ComponentCount.Y * UISettings->NewLandscape_SectionsPerComponent * UISettings->NewLandscape_QuadsPerSection + 1;
}

void FEdModeLandscape::OnPreSaveWorld(UWorld* InWorld, FObjectPreSaveContext ObjectSaveContext)
{
	// If the mode is pending deletion, don't run the presave routine.
	if (!Owner->IsModeActive(GetID()))
	{
		return;
	}

	// Avoid doing this during procedural saves to keep determinism and we don't want to do this on GameWorlds.
	if (!InWorld->IsGameWorld() && !ObjectSaveContext.IsProceduralSave())
	{
		ULandscapeInfoMap& LandscapeInfoMap = ULandscapeInfoMap::GetLandscapeInfoMap(InWorld);
		for (const TPair<FGuid, ULandscapeInfo*>& Pair : LandscapeInfoMap.Map)
		{
			if (const ULandscapeInfo* LandscapeInfo = Pair.Value)
			{
				if (ALandscape* LandscapeActor = LandscapeInfo->LandscapeActor.Get())
				{
					LandscapeActor->OnPreSave();
				}
			}
		}
	}
}

bool FEdModeLandscape::GetOverrideCursorVisibility(bool& bWantsOverride, bool& bHardwareCursorVisible, bool bSoftwareCursorVisible) const
{
	if (!IsEditingEnabled())
	{
		return false;
	}

	bool Result = false;
	if (NewLandscapePreviewMode == ENewLandscapePreviewMode::None)
	{
		if (CurrentTool)
		{
			Result = CurrentTool->GetOverrideCursorVisibility(bWantsOverride, bHardwareCursorVisible, bSoftwareCursorVisible);
		}
	}

	return Result;
}

bool FEdModeLandscape::PreConvertMouseMovement(FEditorViewportClient* InViewportClient)
{
	if (!IsEditingEnabled())
	{
		return false;
	}

	bool Result = false;
	if (NewLandscapePreviewMode == ENewLandscapePreviewMode::None)
	{
		if (CurrentTool)
		{
			Result = CurrentTool->PreConvertMouseMovement(InViewportClient);
		}
	}

	return Result;
}

bool FEdModeLandscape::PostConvertMouseMovement(FEditorViewportClient* InViewportClient)
{
	if (!IsEditingEnabled())
	{
		return false;
	}

	bool Result = false;
	if (NewLandscapePreviewMode == ENewLandscapePreviewMode::None)
	{
		if (CurrentTool)
		{
			Result = CurrentTool->PostConvertMouseMovement(InViewportClient);
		}
	}

	return Result;
}

bool FEdModeLandscape::DisallowMouseDeltaTracking() const
{
	// We never want to use the mouse delta tracker while painting
	return (ToolActiveViewport != nullptr);
}

/**
 * Called when the mouse is moved while a window input capture is in effect
 *
 * @param	InViewportClient	Level editor viewport client that captured the mouse input
 * @param	InViewport			Viewport that captured the mouse input
 * @param	InMouseX			New mouse cursor X coordinate
 * @param	InMouseY			New mouse cursor Y coordinate
 *
 * @return	true if input was handled
 */
bool FEdModeLandscape::CapturedMouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 MouseX, int32 MouseY)
{
	return MouseMove(ViewportClient, Viewport, MouseX, MouseY);
}

/** FEdMode: Called when a mouse button is pressed */
bool FEdModeLandscape::StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if (CurrentGizmoActor.IsValid() && CurrentGizmoActor->IsSelected() && GLandscapeEditRenderMode & ELandscapeEditRenderMode::Gizmo)
	{
		LandscapeTool::GIsGizmoDragging = true;
		return true;
	}
	else if (IsAdjustingBrush(InViewportClient))
	{ 
		LandscapeTool::GIsAdjustingBrush = true;
		// We're adjusting the brush via mouse tracking, return true in order to prevent the viewport client from doing any mouse dragging operation while we're doing it
		return true;
	}
	return false;
}



/** FEdMode: Called when the a mouse button is released */
bool FEdModeLandscape::EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if (LandscapeTool::GIsGizmoDragging)
	{
		LandscapeTool::GIsGizmoDragging = false;
		return true;
	}
	if (LandscapeTool::GIsAdjustingBrush)
	{
		LandscapeTool::GIsAdjustingBrush = false;
		return true;
	}
	return false;
}

/** Trace under the mouse cursor and return the landscape hit and the hit location (in landscape quad space) */
bool FEdModeLandscape::LandscapeMouseTrace(FEditorViewportClient* ViewportClient, float& OutHitX, float& OutHitY)
{
	int32 MouseX = ViewportClient->Viewport->GetMouseX();
	int32 MouseY = ViewportClient->Viewport->GetMouseY();

	return LandscapeMouseTrace(ViewportClient, MouseX, MouseY, OutHitX, OutHitY);
}

bool FEdModeLandscape::LandscapeMouseTrace(FEditorViewportClient* ViewportClient, FVector& OutHitLocation)
{
	int32 MouseX = ViewportClient->Viewport->GetMouseX();
	int32 MouseY = ViewportClient->Viewport->GetMouseY();

	return LandscapeMouseTrace(ViewportClient, MouseX, MouseY, OutHitLocation);
}

/** Trace under the specified coordinates and return the landscape hit and the hit location (in landscape quad space) */
bool FEdModeLandscape::LandscapeMouseTrace(FEditorViewportClient* ViewportClient, int32 MouseX, int32 MouseY, float& OutHitX, float& OutHitY)
{
	FVector HitLocation;
	bool bResult = LandscapeMouseTrace(ViewportClient, MouseX, MouseY, HitLocation);
	OutHitX = HitLocation.X;
	OutHitY = HitLocation.Y;
	return bResult;
}

bool FEdModeLandscape::LandscapeMouseTrace(FEditorViewportClient* ViewportClient, int32 MouseX, int32 MouseY, FVector& OutHitLocation)
{
	// Cache a copy of the world pointer	
	UWorld* World = ViewportClient->GetWorld();

	// Compute a world space ray from the screen space mouse coordinates
	FSceneViewFamilyContext ViewFamily(FSceneViewFamilyContext::ConstructionValues(ViewportClient->Viewport, ViewportClient->GetScene(), ViewportClient->EngineShowFlags)
		.SetRealtimeUpdate(ViewportClient->IsRealtime()));

	FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);
	FViewportCursorLocation MouseViewportRay(View, ViewportClient, MouseX, MouseY);
	FVector MouseViewportRayDirection = MouseViewportRay.GetDirection();

	FVector Start = MouseViewportRay.GetOrigin();
	FVector End = Start + WORLD_MAX * MouseViewportRayDirection;
	if (ViewportClient->IsOrtho())
	{
		Start -= WORLD_MAX * MouseViewportRayDirection;
	}

	return LandscapeTrace(Start, End, MouseViewportRayDirection,OutHitLocation);
}

struct FEdModeLandscape::FProcessLandscapeTraceHitsResult
{
	FVector HitLocation;
	ULandscapeHeightfieldCollisionComponent* HeightfieldComponent;
	ALandscapeProxy* LandscapeProxy;
};

bool FEdModeLandscape::ProcessLandscapeTraceHits(const TArray<FHitResult>& InResults, FProcessLandscapeTraceHitsResult& OutLandscapeTraceHitsResult)
{
	for (int32 i = 0; i < InResults.Num(); i++)
	{
		const FHitResult& Hit = InResults[i];
		ULandscapeHeightfieldCollisionComponent* CollisionComponent = Cast<ULandscapeHeightfieldCollisionComponent>(Hit.Component.Get());
		if (CollisionComponent)
		{
			ALandscapeProxy* HitLandscape = CollisionComponent->GetLandscapeProxy();
			if (HitLandscape &&
				CurrentToolTarget.LandscapeInfo.IsValid() &&
				CurrentToolTarget.LandscapeInfo->LandscapeGuid == HitLandscape->GetLandscapeGuid())
			{
				OutLandscapeTraceHitsResult.HeightfieldComponent = CollisionComponent;
				OutLandscapeTraceHitsResult.HitLocation = Hit.Location;
				OutLandscapeTraceHitsResult.LandscapeProxy = HitLandscape;
				return true;
			}
		}
	}
	return false;
}

bool FEdModeLandscape::LandscapeTrace(const FVector& InRayOrigin, const FVector& InRayEnd, const FVector& InDirection, FVector& OutHitLocation)
{
	if (!CurrentTool || !CurrentToolTarget.LandscapeInfo.IsValid())
	{
		return false;
	}

	FVector Start = InRayOrigin;
	FVector End = InRayEnd;

	// Cache a copy of the world pointer
	UWorld* World = GetWorld();

	// Check Tool Trace first
	if (CurrentTool->HitTrace(Start, End, OutHitLocation))
	{
		return true;
	}
	
	TArray<FHitResult> Results;
	// Each landscape component has 2 collision shapes, 1 of them is specific to landscape editor
	// Trace only ECC_Visibility channel, so we do hit only Editor specific shape
	if (World->LineTraceMultiByObjectType(Results, Start, End, FCollisionObjectQueryParams(ECollisionChannel::ECC_Visibility), FCollisionQueryParams(SCENE_QUERY_STAT(LandscapeTrace), true)))
	{
		if (FProcessLandscapeTraceHitsResult ProcessResult; ProcessLandscapeTraceHits(Results, ProcessResult))
		{
			OutHitLocation = ProcessResult.LandscapeProxy->LandscapeActorToWorld().InverseTransformPosition(ProcessResult.HitLocation);
			
			UE_VLOG_SEGMENT_THICK(World, LogLandscapeEdMode, VeryVerbose, InRayOrigin, InRayOrigin + 20000.0f * InDirection, FColor(100,255,100), 4, TEXT("landscape:ray-hit"));
			UE_VLOG_LOCATION(World, LogLandscapeEdMode, VeryVerbose,  ProcessResult.LandscapeProxy->LandscapeActorToWorld().TransformPosition(OutHitLocation), 20.0,  FColor(100,100,255), TEXT("landscape:point-hit"));
			return true;
		}
	}

	UE_VLOG_SEGMENT_THICK(World, LogLandscapeEdMode, VeryVerbose, InRayOrigin, InRayOrigin + 20000.0f * InDirection, FColor(255,100,100), 4, TEXT("landscape:ray-miss"));
		
	if (CurrentTool->UseSphereTrace())
	{
		// If there is no landscape directly under the mouse search for a landscape collision
		// under the shape of the brush.
		FCollisionShape SphereShape;
		SphereShape.SetSphere(UISettings->GetCurrentToolBrushRadius());
		if (World->SweepMultiByObjectType(Results, Start, End, FQuat::Identity, FCollisionObjectQueryParams(ECollisionChannel::ECC_Visibility), SphereShape, FCollisionQueryParams(SCENE_QUERY_STAT(LandscapeTrace), true)))
		{
			if (FProcessLandscapeTraceHitsResult SweepProcessResult; ProcessLandscapeTraceHits(Results, SweepProcessResult))
			{
				UE_VLOG_LOCATION(World, LogLandscapeEdMode, VeryVerbose, SweepProcessResult.HitLocation, UISettings->GetCurrentToolBrushRadius(), FColor(255, 100, 100), TEXT("landscape:sweep-hit-location"));
				FSphere HitSphere(SweepProcessResult.HitLocation, UISettings->GetCurrentToolBrushRadius());

				float MeanHeight = 0.0f;
				int32 Count = 0;
				const int32 NumHeightSamples = 16;

				for (int32 Y = 0; Y < NumHeightSamples; ++Y)
				{
					float HY = (Y / (float)NumHeightSamples) * HitSphere.W * 2.0f + HitSphere.Center.Y - HitSphere.W;
					for (int32 X = 0; X < NumHeightSamples; ++X)
					{
						float HX = (X / (float)NumHeightSamples) * HitSphere.W * 2.0f + HitSphere.Center.X - HitSphere.W;

						FVector HeightSampleLocation(HX, HY, HitSphere.Center.Z);

						TOptional<float> Height = SweepProcessResult.LandscapeProxy->GetHeightAtLocation(HeightSampleLocation, EHeightfieldSource::Editor);

						if (!Height.IsSet())
						{
							continue;
						}

						UE_VLOG_LOCATION(World, LogLandscapeEdMode, VeryVerbose, FVector(HX, HY, Height.GetValue()), 2.0f, FColor(100, 100, 255), TEXT(""));

						MeanHeight += Height.GetValue();
						Count++;
					}
				}

				FVector PointOnPlane(HitSphere.Center.X, HitSphere.Center.Y, HitSphere.Center.Z);

				if (Count > 0)
				{
					MeanHeight /= (float)Count;
					PointOnPlane.Z = MeanHeight;
				}

				UE_VLOG_LOCATION(World, LogLandscapeEdMode, VeryVerbose, PointOnPlane, 10.0, FColor(100, 100, 255), TEXT("landscape:point-on-plane"));

				if (FMath::Abs(FVector::DotProduct(InDirection, FVector::ZAxisVector)) < SMALL_NUMBER)
				{
					// the ray and plane are nearly parallel, and won't intersect (or if they do it will be wildly far away)
					return false;
				}

				const FPlane Plane(PointOnPlane, FVector::ZAxisVector);
				FVector EstimatedHitLocation = FMath::RayPlaneIntersection(Start, InDirection, Plane);
				check(!EstimatedHitLocation.ContainsNaN());

				UE_VLOG_LOCATION(World, LogLandscapeEdMode, VeryVerbose, EstimatedHitLocation, 10.0, FColor(100, 100, 255), TEXT("landscape:estimated-hit-location"));

				OutHitLocation = SweepProcessResult.LandscapeProxy->LandscapeActorToWorld().InverseTransformPosition(EstimatedHitLocation);
				return true;
			}
		}
	}

	return false;
}

bool FEdModeLandscape::LandscapePlaneTrace(FEditorViewportClient* ViewportClient, const FPlane& Plane, FVector& OutHitLocation)
{
	int32 MouseX = ViewportClient->Viewport->GetMouseX();
	int32 MouseY = ViewportClient->Viewport->GetMouseY();

	return LandscapePlaneTrace(ViewportClient, MouseX, MouseY, Plane, OutHitLocation);
}

bool FEdModeLandscape::LandscapePlaneTrace(FEditorViewportClient* ViewportClient, int32 MouseX, int32 MouseY, const FPlane& Plane, FVector& OutHitLocation)
{
	// Compute a world space ray from the screen space mouse coordinates
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		ViewportClient->Viewport,
		ViewportClient->GetScene(),
		ViewportClient->EngineShowFlags)
		.SetRealtimeUpdate(ViewportClient->IsRealtime()));
	FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);
	FViewportCursorLocation MouseViewportRay(View, ViewportClient, MouseX, MouseY);

	FVector Start = MouseViewportRay.GetOrigin();
	FVector End = Start + WORLD_MAX * MouseViewportRay.GetDirection();

	OutHitLocation = FMath::LinePlaneIntersection(Start, End, Plane);

	return true;
}

namespace
{
	const int32 SelectionSizeThresh = 2 * 256 * 256;
	FORCEINLINE bool IsSlowSelect(ULandscapeInfo* LandscapeInfo)
	{
		if (LandscapeInfo)
		{
			int32 MinX = MAX_int32, MinY = MAX_int32, MaxX = MIN_int32, MaxY = MIN_int32;
			LandscapeInfo->GetSelectedExtent(MinX, MinY, MaxX, MaxY);
			return (MinX != MAX_int32 && ((MaxX - MinX) * (MaxY - MinY)));
		}
		return false;
	}
};

EEditAction::Type FEdModeLandscape::GetActionEditDuplicate()
{
	EEditAction::Type Result = EEditAction::Skip;

	if (NewLandscapePreviewMode == ENewLandscapePreviewMode::None)
	{
		if (CurrentTool != nullptr)
		{
			Result = CurrentTool->GetActionEditDuplicate();
		}
	}

	return Result;
}

EEditAction::Type FEdModeLandscape::GetActionEditDelete()
{
	EEditAction::Type Result = EEditAction::Skip;

	if (NewLandscapePreviewMode == ENewLandscapePreviewMode::None)
	{
		if (CurrentTool != nullptr)
		{
			Result = CurrentTool->GetActionEditDelete();
		}

		if (Result == EEditAction::Skip)
		{
			// Prevent deleting Gizmo during LandscapeEdMode
			if (CurrentGizmoActor.IsValid())
			{
				if (CurrentGizmoActor->IsSelected())
				{
					if (GEditor->GetSelectedActors()->Num() > 1)
					{
						GEditor->GetSelectedActors()->Deselect(CurrentGizmoActor.Get());
						Result = EEditAction::Skip;
					}
					else
					{
						Result = EEditAction::Halt;
					}
				}
			}
		}
	}

	return Result;
}

EEditAction::Type FEdModeLandscape::GetActionEditCut()
{
	EEditAction::Type Result = EEditAction::Skip;

	if (NewLandscapePreviewMode == ENewLandscapePreviewMode::None)
	{
		if (CurrentTool != nullptr)
		{
			Result = CurrentTool->GetActionEditCut();
		}
	}

	if (Result == EEditAction::Skip)
	{
		// Special case: we don't want the 'normal' cut operation to be possible at all while in this mode, 
		// so we need to stop evaluating the others in-case they come back as true.
		return EEditAction::Halt;
	}

	return Result;
}

EEditAction::Type FEdModeLandscape::GetActionEditCopy()
{
	EEditAction::Type Result = EEditAction::Skip;

	if (NewLandscapePreviewMode == ENewLandscapePreviewMode::None)
	{
		if (CurrentTool != nullptr)
		{
			Result = CurrentTool->GetActionEditCopy();
		}

		if (Result == EEditAction::Skip)
		{
			if (GLandscapeEditRenderMode & ELandscapeEditRenderMode::Gizmo || GLandscapeEditRenderMode & (ELandscapeEditRenderMode::Select))
			{
				if (CurrentGizmoActor.IsValid() && GizmoBrush && CurrentGizmoActor->TargetLandscapeInfo)
				{
					Result = EEditAction::Process;
				}
			}
		}
	}

	return Result;
}

EEditAction::Type FEdModeLandscape::GetActionEditPaste()
{
	EEditAction::Type Result = EEditAction::Skip;

	if (NewLandscapePreviewMode == ENewLandscapePreviewMode::None)
	{
		if (CurrentTool != nullptr)
		{
			Result = CurrentTool->GetActionEditPaste();
		}

		if (Result == EEditAction::Skip)
		{
			if (GLandscapeEditRenderMode & ELandscapeEditRenderMode::Gizmo || GLandscapeEditRenderMode & (ELandscapeEditRenderMode::Select))
			{
				if (CurrentGizmoActor.IsValid() && GizmoBrush && CurrentGizmoActor->TargetLandscapeInfo)
				{
					Result = EEditAction::Process;
				}
			}
		}
	}

	return Result;
}

bool FEdModeLandscape::ProcessEditDuplicate()
{
	if (!IsEditingEnabled())
	{
		return true;
	}

	bool Result = false;

	if (NewLandscapePreviewMode == ENewLandscapePreviewMode::None)
	{
		if (CurrentTool != nullptr)
		{
			Result = CurrentTool->ProcessEditDuplicate();
		}
	}

	return Result;
}

bool FEdModeLandscape::ProcessEditDelete()
{
	if (!IsEditingEnabled())
	{
		return true;
	}

	bool Result = false;

	if (NewLandscapePreviewMode == ENewLandscapePreviewMode::None)
	{
		if (CurrentTool != nullptr)
		{
			Result = CurrentTool->ProcessEditDelete();
		}
	}

	return Result;
}

bool FEdModeLandscape::ProcessEditCut()
{
	if (!IsEditingEnabled())
	{
		return true;
	}

	bool Result = false;

	if (NewLandscapePreviewMode == ENewLandscapePreviewMode::None)
	{
		if (CurrentTool != nullptr)
		{
			Result = CurrentTool->ProcessEditCut();
		}
	}

	return Result;
}

bool FEdModeLandscape::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	if (!IsEditingEnabled())
	{
		return false;
	}

	if (NewLandscapePreviewMode != ENewLandscapePreviewMode::None)
	{
		return false;
	}

	// Override Click Input for Splines Tool
	if (CurrentTool && CurrentTool->HandleClick(HitProxy, Click))
	{
		return true;
	}

	return false;
}

bool FEdModeLandscape::IsAdjustingBrush(FEditorViewportClient* InViewportClient) const
{
	const FLandscapeEditorCommands& LandscapeActions = FLandscapeEditorCommands::Get();
	if (InViewportClient->IsCommandChordPressed(LandscapeActions.DragBrushSizeAndFalloff))
	{
		return true;
	}
	if (InViewportClient->IsCommandChordPressed(LandscapeActions.DragBrushSize))
	{
		return true;
	}
	if (InViewportClient->IsCommandChordPressed(LandscapeActions.DragBrushFalloff))
	{
		return true;
	}
	if (InViewportClient->IsCommandChordPressed(LandscapeActions.DragBrushStrength))
	{
		return true;
	}
	return false;
}

void FEdModeLandscape::ChangeAlphaBrushRotation(bool bIncrease)
{
	const float SliderMin = -180.f;
	const float SliderMax = 180.f;
	const float DefaultDiffValue = 10.f;
	const float Diff = bIncrease ? DefaultDiffValue : -DefaultDiffValue;
	UISettings->Modify();
	UISettings->AlphaBrushRotation = FMath::Clamp(UISettings->AlphaBrushRotation + Diff, SliderMin, SliderMax);
}


/** FEdMode: Called when mouse drag input is applied */
bool FEdModeLandscape::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	if (!IsEditingEnabled())
	{
		return false;
	}

	if (CurrentTool && CurrentTool->InputDelta(InViewportClient, InViewport, InDrag, InRot, InScale))
	{
		return true;
	}

	return false;
}


void FEdModeLandscape::SetCurrentTargetLayer(FName TargetLayerName, TWeakObjectPtr<ULandscapeLayerInfoObject> LayerInfo)
{
	if (CurrentToolMode)
	{
		// Cache current Layer Name so we can set it back when switching between Modes
		CurrentToolMode->CurrentTargetLayerName = TargetLayerName;
	}
	CurrentToolTarget.LayerName = TargetLayerName;
	CurrentToolTarget.LayerInfo = LayerInfo;
}

const TArray<ALandscapeBlueprintBrushBase*>& FEdModeLandscape::GetBrushList() const
{
	return BrushList;
}

const TArray<TSharedRef<FLandscapeTargetListInfo>>& FEdModeLandscape::GetTargetList() const
{
	return LandscapeTargetList;
}

const TArray<FLandscapeListInfo>& FEdModeLandscape::GetLandscapeList()
{
	return LandscapeList;
}

bool FEdModeLandscape::IsGridBased() const
{
	return GetWorld()->GetSubsystem<ULandscapeSubsystem>()->IsGridBased();
}

bool FEdModeLandscape::HasValidLandscapeEditLayerSelection() const
{
	return !CanHaveLandscapeLayersContent() || GetCurrentLayer();
}

bool FEdModeLandscape::CanEditCurrentTarget(FText* Reason) const
{
	static FText DummyReason;
	FText& LocalReason = Reason ? *Reason : DummyReason;

	if (!CurrentToolTarget.LandscapeInfo.IsValid())
	{
		LocalReason = NSLOCTEXT("UnrealEd", "LandscapeInvalidTarget", "No landscape selected.");
		return false;
	}

	ALandscapeProxy* Proxy = CurrentToolTarget.LandscapeInfo->GetLandscapeProxy();

	// Landscape Layer Editing not available without a loaded Landscape Actor
	if (GetLandscape() == nullptr)
	{
		if (!Proxy)
		{
            LocalReason = NSLOCTEXT("UnrealEd", "LandscapeNotFound", "No Landscape found.");
			return false;
		}
		
		if (Proxy->HasLayersContent())
		{
			LocalReason = NSLOCTEXT("UnrealEd", "LandscapeActorNotLoaded", "Landscape actor is not loaded. It is needed to do layer editing.");
			return false;
		}

	}
	
	// To edit a Landscape with layers all components need to be registered
	if(Proxy && Proxy->HasLayersContent() && !CurrentToolTarget.LandscapeInfo->AreAllComponentsRegistered())
	{
		LocalReason = NSLOCTEXT("UnrealEd", "LandscapeMissingComponents", "Landscape has some unregistered components (hidden levels).");
		return false;
	}

	return true;
}

bool FEdModeLandscape::ShouldShowLayer(TSharedRef<FLandscapeTargetListInfo> Target) const
{
	if (!UISettings->ShowUnusedLayers)
	{
		return Target->LayerInfoObj.IsValid() && Target->LayerInfoObj.Get()->IsReferencedFromLoadedData;
	}

	return true;
}

const TArray<FName>& FEdModeLandscape::GetTargetShownList() const
{
	return ShownTargetLayerList;
}

int32 FEdModeLandscape::GetTargetLayerStartingIndex() const
{
	return TargetLayerStartingIndex;
}

const TArray<FName>* FEdModeLandscape::GetTargetDisplayOrderList() const
{
	if (!CurrentToolTarget.LandscapeInfo.IsValid())
	{
		return nullptr;
	}

	ALandscapeProxy* LandscapeProxy = CurrentToolTarget.LandscapeInfo->GetLandscapeProxy();

	if (LandscapeProxy == nullptr)
	{
		return nullptr;
	}

	return &LandscapeProxy->TargetDisplayOrderList;
}

FEdModeLandscape::FTargetsListUpdated FEdModeLandscape::TargetsListUpdated;


/** FEdMode: Render the mesh paint tool */
void FEdModeLandscape::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	/** Call parent implementation */
	FEdMode::Render(View, Viewport, PDI);

	if (!IsEditingEnabled())
	{
		return;
	}

	// Override Rendering for Splines Tool
	if (CurrentTool)
	{
		CurrentTool->Render(View, Viewport, PDI);
	}
}

/** FEdMode: Render HUD elements for this tool */
void FEdModeLandscape::DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
}

bool FEdModeLandscape::UsesTransformWidget() const
{
	if (NewLandscapePreviewMode != ENewLandscapePreviewMode::None)
	{
		return true;
	}

	// Override Widget for Splines Tool
	if (CurrentTool && CurrentTool->UsesTransformWidget())
	{
		return true;
	}

	return (CurrentGizmoActor.IsValid() && CurrentGizmoActor->IsSelected() && (GLandscapeEditRenderMode & ELandscapeEditRenderMode::Gizmo));
}

bool FEdModeLandscape::ShouldDrawWidget() const
{
	return UsesTransformWidget();
}

EAxisList::Type FEdModeLandscape::GetWidgetAxisToDraw(UE::Widget::EWidgetMode InWidgetMode) const
{
	if (NewLandscapePreviewMode == ENewLandscapePreviewMode::None)
	{
		// Override Widget for Splines Tool
		if (CurrentTool)
		{
			return CurrentTool->GetWidgetAxisToDraw(InWidgetMode);
		}
	}

	switch (InWidgetMode)
	{
	case UE::Widget::WM_Translate:
		return EAxisList::XYZ;
	case UE::Widget::WM_Rotate:
		return EAxisList::Z;
	case UE::Widget::WM_Scale:
		return EAxisList::XYZ;
	default:
		return EAxisList::None;
	}
}

FVector FEdModeLandscape::GetWidgetLocation() const
{
	if (NewLandscapePreviewMode != ENewLandscapePreviewMode::None)
	{
		return UISettings->NewLandscape_Location;
	}

	if (CurrentGizmoActor.IsValid() && (GLandscapeEditRenderMode & ELandscapeEditRenderMode::Gizmo) && CurrentGizmoActor->IsSelected())
	{
		if (CurrentGizmoActor->TargetLandscapeInfo && CurrentGizmoActor->TargetLandscapeInfo->GetLandscapeProxy())
		{
			// Apply Landscape transformation when it is available
			ULandscapeInfo* LandscapeInfo = CurrentGizmoActor->TargetLandscapeInfo;
			return CurrentGizmoActor->GetActorLocation()
				+ FQuatRotationMatrix(LandscapeInfo->GetLandscapeProxy()->GetActorQuat()).TransformPosition(FVector(0, 0, CurrentGizmoActor->GetLength()));
		}
		return CurrentGizmoActor->GetActorLocation();
	}

	// Override Widget for Splines Tool
	if (CurrentTool && CurrentTool->OverrideWidgetLocation())
	{
		return CurrentTool->GetWidgetLocation();
	}

	return FEdMode::GetWidgetLocation();
}

bool FEdModeLandscape::GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData)
{
	if (NewLandscapePreviewMode != ENewLandscapePreviewMode::None)
	{
		InMatrix = FRotationMatrix(UISettings->NewLandscape_Rotation);
		return true;
	}

	// Override Widget for Splines Tool
	if (CurrentTool && CurrentTool->OverrideWidgetRotation())
	{
		InMatrix = CurrentTool->GetWidgetRotation();
		return true;
	}

	return false;
}

bool FEdModeLandscape::GetCustomInputCoordinateSystem(FMatrix& InMatrix, void* InData)
{
	return GetCustomDrawingCoordinateSystem(InMatrix, InData);
}


/** FEdMode: Check to see if an actor can be selected in this mode - no side effects */
bool FEdModeLandscape::IsSelectionAllowed(AActor* InActor, bool bInSelection) const
{
	if (!bInSelection)
	{
		// always allow de-selection
		return true;
	}

	if (!IsEditingEnabled())
	{
		return false;
	}

	// Override Selection for Splines Tool
	if (CurrentTool && CurrentTool->OverrideSelection())
	{
		return CurrentTool->IsSelectionAllowed(InActor, bInSelection);
	}

	if (InActor->IsA(ALandscapeProxy::StaticClass()))
	{
		return true;
	}
	else if (InActor->IsA(ALandscapeGizmoActor::StaticClass()))
	{
		return true;
	}
	else if (InActor->IsA(ALandscapeBlueprintBrushBase::StaticClass()))
	{
		return true;
	}

	return false;
}

/** FEdMode: Called when the currently selected actor has changed */
void FEdModeLandscape::ActorSelectionChangeNotify()
{
	if (CurrentGizmoActor.IsValid() && CurrentGizmoActor->IsSelected() && (GEditor->GetSelectedActors()->CountSelections<AActor>() != 1))
	{
		GEditor->SelectNone(false, true);
		GEditor->SelectActor(CurrentGizmoActor.Get(), true, false, true);
	}
}

void FEdModeLandscape::ActorMoveNotify()
{
	//GUnrealEd->UpdateFloatingPropertyWindows();
}

/** Forces all level editor viewports to realtime mode */
void FEdModeLandscape::ForceRealTimeViewports(const bool bEnable)
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();
	if (LevelEditor.IsValid())
	{
		TArray<TSharedPtr<SLevelViewport>> Viewports = LevelEditor->GetViewports();
		for (const TSharedPtr<SLevelViewport>& ViewportWindow : Viewports)
		{
			if (ViewportWindow.IsValid())
			{
				FEditorViewportClient& Viewport = ViewportWindow->GetAssetViewportClient();
				const FText SystemDisplayName = LOCTEXT("RealtimeOverrideMessage_Landscape", "Landscape Mode");
				if (bEnable)
				{
					Viewport.AddRealtimeOverride(bEnable, SystemDisplayName);
				}
				else
				{
					Viewport.RemoveRealtimeOverride(SystemDisplayName, false);
				}
			}
		}
	}
}

void FEdModeLandscape::ReimportData(const FLandscapeTargetListInfo& TargetInfo)
{
	const FString& SourceFilePath = TargetInfo.GetReimportFilePath();
	if (SourceFilePath.Len())
	{
		FScopedSetLandscapeEditingLayer Scope(GetLandscape(), GetCurrentLayerGuid(), [&] { RequestLayersContentUpdateForceAll(); });
		ImportData(TargetInfo, SourceFilePath);
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "LandscapeReImport_BadFileName", "Reimport Source Filename is invalid"));
	}
}

template<class T>
void ImportDataInternal(ULandscapeInfo* LandscapeInfo, const FString& Filename, FName LayerName, bool bSingleFile, bool bFlipYAxis, const FIntRect& ImportRegionVerts, ELandscapeImportTransformType TransformType, FIntPoint Offset, TFunctionRef<void(int32, int32, int32, int32, const TArray<T>&)> SetDataFunc)
{
	if (LandscapeInfo)
	{
		const FLandscapeImportResolution LandscapeResolution = { (uint32)(ImportRegionVerts.Width()), (uint32)(ImportRegionVerts.Height()) };
		FLandscapeImportDescriptor OutImportDescriptor;
		FText OutMessage;

		ELandscapeImportResult ImportResult = FLandscapeImportHelper::GetImportDescriptor<T>(Filename, bSingleFile, bFlipYAxis, LayerName, OutImportDescriptor, OutMessage);
		if (ImportResult == ELandscapeImportResult::Error)
		{
			FMessageDialog::Open(EAppMsgType::Ok, OutMessage);
			return;
		}

		check(OutImportDescriptor.ImportResolutions.Num() > 0);

		int32 DescriptorIndex = 0;
		// If there is more than one possible res it needs to match perfectly (this constraint could probably removed)
		if (OutImportDescriptor.ImportResolutions.Num() > 1)
		{
			DescriptorIndex = OutImportDescriptor.FindDescriptorIndex(LandscapeResolution.Width, LandscapeResolution.Height);
			// if the file is a raw format with multiple possibly resolutions, only attempt import if one matches the current landscape
			if (DescriptorIndex == INDEX_NONE)
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("LandscapeSizeX"), LandscapeResolution.Width);
				Args.Add(TEXT("LandscapeSizeY"), LandscapeResolution.Height);

				FMessageDialog::Open(EAppMsgType::Ok,
					FText::Format(NSLOCTEXT("LandscapeEditor.Import", "Import_SizeMismatchRaw", "The file resolution does not match the current Landscape extent ({LandscapeSizeX}\u00D7{LandscapeSizeY}), and its exact resolution could not be determined"), Args));

				return;
			}
		}
				
		// display warning message if there is one and allow user to cancel
		if (ImportResult == ELandscapeImportResult::Warning)
		{
			auto Result = FMessageDialog::Open(EAppMsgType::OkCancel, OutMessage);

			if (Result != EAppReturnType::Ok)
			{
				return;
			}
		}

		const FLandscapeImportResolution ImportResolution = OutImportDescriptor.ImportResolutions[DescriptorIndex];
		bool bResolutionMismatch = false;
		// if the file is a format with resolution information, warn the user if the resolution doesn't match the current landscape
		// unlike for raw this is only a warning as we can pad/clip the data if we know what resolution it is
		if (ImportResolution != LandscapeResolution)
		{
			bResolutionMismatch = true;

			FFormatNamedArguments Args;
			Args.Add(TEXT("ImportSizeX"), ImportResolution.Width);
			Args.Add(TEXT("ImportSizeY"), ImportResolution.Height);
			Args.Add(TEXT("LandscapeSizeX"), LandscapeResolution.Width);
			Args.Add(TEXT("LandscapeSizeY"), LandscapeResolution.Height);

			auto Result = FMessageDialog::Open(EAppMsgType::OkCancel,
				FText::Format(NSLOCTEXT("LandscapeEditor.Import", "Import_SizeMismatch", "The import size ({ImportSizeX}\u00D7{ImportSizeY}) does not match the current Landscape extent ({LandscapeSizeX}\u00D7{LandscapeSizeY}), if you continue it will be padded/clipped to fit"), Args));

			if (Result != EAppReturnType::Ok)
			{
				return;
			}
		}

		TArray<T> ImportData;
		ImportResult = FLandscapeImportHelper::GetImportData<T>(OutImportDescriptor, DescriptorIndex, LayerName, ImportData, OutMessage);
		if (ImportResult == ELandscapeImportResult::Error)
		{
			FMessageDialog::Open(EAppMsgType::Ok, OutMessage);
			return;
		}

		// Expand if necessary...
		{
			TArray<T> FinalData;
			if (bResolutionMismatch)
			{
				FLandscapeImportHelper::TransformImportData<T>(ImportData, FinalData, ImportResolution, LandscapeResolution, TransformType, Offset);
			}
			else
			{
				FinalData = MoveTemp(ImportData);
			}
									
			// Set Data is in Quads (so remove 1)
			SetDataFunc(ImportRegionVerts.Min.X, ImportRegionVerts.Min.Y, ImportRegionVerts.Max.X-1, ImportRegionVerts.Max.Y-1, FinalData);
		}
	}			
}

void FEdModeLandscape::ImportHeightData(ULandscapeInfo* LandscapeInfo, const FGuid& LayerGuid, const FString& Filename, const FIntRect& ImportRegionVerts, ELandscapeImportTransformType TransformType, FIntPoint Offset, const ELandscapeLayerPaintingRestriction& PaintRestriction, bool bFlipYAxis)
{
	ImportDataInternal<uint16>(LandscapeInfo, Filename, NAME_None, UseSingleFileImport(), bFlipYAxis, ImportRegionVerts, TransformType, Offset, [LandscapeInfo, LayerGuid, PaintRestriction](int32 MinX, int32 MinY, int32 MaxX, int32 MaxY, const TArray<uint16>& Data)
	{
		ALandscape* Landscape = LandscapeInfo->LandscapeActor.Get();
		FScopedSetLandscapeEditingLayer Scope(Landscape, LayerGuid, [&] { check(Landscape); Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_Heightmap_All); });

		FScopedTransaction Transaction(LOCTEXT("Undo_ImportHeightmap", "Importing Landscape Heightmap"));

		FHeightmapAccessor<false> HeightmapAccessor(LandscapeInfo);
		HeightmapAccessor.SetData(MinX, MinY, MaxX, MaxY, Data.GetData());
	});
}

void FEdModeLandscape::ImportWeightData(ULandscapeInfo* LandscapeInfo, const FGuid& LayerGuid, ULandscapeLayerInfoObject* LayerInfo, const FString& Filename, const FIntRect& ImportRegionVerts, ELandscapeImportTransformType TransformType, FIntPoint Offset, const ELandscapeLayerPaintingRestriction& PaintRestriction, bool bFlipYAxis)
{
	if (LayerInfo)
	{
		ImportDataInternal<uint8>(LandscapeInfo, Filename, LayerInfo->LayerName, UseSingleFileImport(), bFlipYAxis, ImportRegionVerts, TransformType, Offset, [LandscapeInfo, LayerGuid, LayerInfo, PaintRestriction](int32 MinX, int32 MinY, int32 MaxX, int32 MaxY, const TArray<uint8>& Data)
		{
			ALandscape* Landscape = LandscapeInfo->LandscapeActor.Get();
			FScopedSetLandscapeEditingLayer Scope(Landscape, LayerGuid, [&] { check(Landscape); Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_Weightmap_All); });

			FScopedTransaction Transaction(LOCTEXT("Undo_ImportWeightmap", "Importing Landscape Layer"));

			FAlphamapAccessor<false, false> AlphamapAccessor(LandscapeInfo, LayerInfo);
			AlphamapAccessor.SetData(MinX, MinY, MaxX, MaxY, Data.GetData(), PaintRestriction);
		});
	}
}

void FEdModeLandscape::ImportData(const FLandscapeTargetListInfo& TargetInfo, const FString& Filename)
{
	ULandscapeInfo* LandscapeInfo = TargetInfo.LandscapeInfo.Get();
	FIntRect ImportExtent;
	if (LandscapeInfo && LandscapeInfo->GetLandscapeExtent(ImportExtent))
	{
		// Import is in Verts (Extent is in Quads)
		ImportExtent.Max.X += 1;
		ImportExtent.Max.Y += 1;

		if (TargetInfo.TargetType == ELandscapeToolTargetType::Heightmap)
		{
			FScopedSlowTask Progress(1, LOCTEXT("ImportingLandscapeHeightmapTask", "Importing Landscape Heightmap..."));
			Progress.MakeDialog();
			ImportHeightData(LandscapeInfo, GetCurrentLayerGuid(), Filename, ImportExtent, ELandscapeImportTransformType::ExpandCentered);
		}
		else
		{
			FScopedSlowTask Progress(1, LOCTEXT("ImportingLandscapeWeightmapTask", "Importing Landscape Layer Weightmap..."));
			Progress.MakeDialog();
			ImportWeightData(LandscapeInfo, GetCurrentLayerGuid(), TargetInfo.LayerInfoObj.Get(), Filename, ImportExtent, ELandscapeImportTransformType::ExpandCentered);
		}
	}
}

ELandscapeEditingState FEdModeLandscape::GetEditingState() const
{
	UWorld* World = GetWorld();

	if (GEditor->bIsSimulatingInEditor)
	{
		return ELandscapeEditingState::SIEWorld;
	}
	else if (GEditor->PlayWorld != nullptr)
	{
		return ELandscapeEditingState::PIEWorld;
	}
	else if (World == nullptr)
	{
		return ELandscapeEditingState::Unknown;
	}
	else if (World->FeatureLevel < ERHIFeatureLevel::SM5)
	{
		return ELandscapeEditingState::BadFeatureLevel;
	}
	else if (NewLandscapePreviewMode == ENewLandscapePreviewMode::None &&
		!CurrentToolTarget.LandscapeInfo.IsValid())
	{
		return ELandscapeEditingState::NoLandscape;
	}

	return ELandscapeEditingState::Enabled;
}

bool FEdModeLandscape::CanHaveLandscapeLayersContent() const
{
	ALandscape* Landscape = GetLandscape();
	return Landscape != nullptr ? Landscape->CanHaveLayersContent() : false;
}

bool FEdModeLandscape::HasLandscapeLayersContent() const
{
	ALandscape* Landscape = GetLandscape();	
	return Landscape != nullptr ? Landscape->HasLayersContent() : false;
}

int32 FEdModeLandscape::GetLayerCount() const
{
	ALandscape* Landscape = GetLandscape();
	return Landscape ? Landscape->GetLayerCount() : 0;
}


int32 FEdModeLandscape::GetCurrentLayerIndex() const
{
	return UISettings ? UISettings->CurrentLayerIndex : INDEX_NONE;
}

ALandscape* FEdModeLandscape::GetLandscape() const
{
	return CurrentToolTarget.LandscapeInfo.IsValid() ? CurrentToolTarget.LandscapeInfo->LandscapeActor.Get() : nullptr;
}

FLandscapeLayer* FEdModeLandscape::GetLayer(int32 InLayerIndex) const
{
	ALandscape* Landscape = GetLandscape();
	return Landscape ? Landscape->GetLayer(InLayerIndex) : nullptr;
}

FName FEdModeLandscape::GetLayerName(int32 InLayerIndex) const
{
	FLandscapeLayer* Layer = GetLayer(InLayerIndex);
	return Layer ? Layer->Name : NAME_None;
}

bool FEdModeLandscape::CanRenameLayerTo(int32 InLayerIndex, const FName& InNewName)
{
	ALandscape* Landscape = GetLandscape();
	if (Landscape)
	{
		int32 LayerCount = GetLayerCount();
		for (int32 LayerIdx = 0; LayerIdx < LayerCount; ++LayerIdx)
		{
			if (LayerIdx != InLayerIndex && GetLayerName(LayerIdx) == InNewName)
			{
				return false;
			}
		}
	}
	return true;
}

void FEdModeLandscape::SetLayerName(int32 InLayerIndex, const FName& InName)
{
	ALandscape* Landscape = GetLandscape();
	if (Landscape)
	{
		Landscape->SetLayerName(InLayerIndex, InName);
	}
}

bool FEdModeLandscape::IsLayerAlphaVisible(int32 InLayerIndex) const
{
	return (CurrentToolTarget.TargetType == ELandscapeToolTargetType::Heightmap || CurrentToolTarget.TargetType == ELandscapeToolTargetType::Weightmap);
}

float FEdModeLandscape::GetClampedLayerAlpha(float InLayerAlpha) const
{
	ALandscape* Landscape = GetLandscape();
	if (Landscape)
	{
		if (CurrentToolTarget.TargetType == ELandscapeToolTargetType::Heightmap || CurrentToolTarget.TargetType == ELandscapeToolTargetType::Weightmap)
		{
			return Landscape->GetClampedLayerAlpha(InLayerAlpha, CurrentToolTarget.TargetType == ELandscapeToolTargetType::Heightmap);
		}
	}
	return InLayerAlpha;
}

float FEdModeLandscape::GetLayerAlpha(int32 InLayerIndex) const
{
	ALandscape* Landscape = GetLandscape();
	if (Landscape)
	{
		if (CurrentToolTarget.TargetType == ELandscapeToolTargetType::Heightmap || CurrentToolTarget.TargetType == ELandscapeToolTargetType::Weightmap)
		{
			return Landscape->GetLayerAlpha(InLayerIndex, CurrentToolTarget.TargetType == ELandscapeToolTargetType::Heightmap);
		}
	}
	return 1.0f;
}

void FEdModeLandscape::SetLayerAlpha(int32 InLayerIndex, float InAlpha)
{
	ALandscape* Landscape = GetLandscape();
	if (Landscape)
	{
		if (CurrentToolTarget.TargetType == ELandscapeToolTargetType::Heightmap || CurrentToolTarget.TargetType == ELandscapeToolTargetType::Weightmap)
		{
			Landscape->SetLayerAlpha(InLayerIndex, InAlpha, CurrentToolTarget.TargetType == ELandscapeToolTargetType::Heightmap);
		}
	}
}

bool FEdModeLandscape::IsLayerVisible(int32 InLayerIndex) const
{
	FLandscapeLayer* Layer = GetLayer(InLayerIndex);
	return Layer ? Layer->bVisible : false;
}

void FEdModeLandscape::SetLayerVisibility(bool bInVisible, int32 InLayerIndex)
{
	ALandscape* Landscape = GetLandscape();
	if (Landscape)
	{
		Landscape->SetLayerVisibility(InLayerIndex, bInVisible);
	}
}

bool FEdModeLandscape::IsLayerLocked(int32 InLayerIndex) const
{
	const FLandscapeLayer* Layer = GetLayer(InLayerIndex);
	return Layer && Layer->bLocked;
}

void FEdModeLandscape::SetLayerLocked(int32 InLayerIndex, bool bInLocked)
{
	ALandscape* Landscape = GetLandscape();
	if (Landscape)
	{
		Landscape->SetLayerLocked(InLayerIndex, bInLocked);
	}
}

void FEdModeLandscape::RequestLayersContentUpdate(ELandscapeLayerUpdateMode InUpdateMode)
{
	if (ALandscape* Landscape = GetLandscape())
	{
		Landscape->RequestLayersContentUpdate(InUpdateMode);
	}
}

void FEdModeLandscape::RequestLayersContentUpdateForceAll(ELandscapeLayerUpdateMode InUpdateMode)
{
	if (ALandscape* Landscape = GetLandscape())
	{
		Landscape->RequestLayersContentUpdateForceAll(InUpdateMode);
	}
}

ALandscapeBlueprintBrushBase* FEdModeLandscape::GetBrushForCurrentLayer(int32 InBrushIndex) const
{
	if (ALandscape* Landscape = GetLandscape())
	{
		return Landscape->GetBrushForLayer(GetCurrentLayerIndex(), InBrushIndex);
	}
	return nullptr;
}

TArray<ALandscapeBlueprintBrushBase*> FEdModeLandscape::GetBrushesForCurrentLayer()
{
	TArray<ALandscapeBlueprintBrushBase*> Brushes;
	if (ALandscape* Landscape = GetLandscape())
	{
		Brushes = Landscape->GetBrushesForLayer(GetCurrentLayerIndex());
	}
	return Brushes;
}

void FEdModeLandscape::ShowOnlySelectedBrush(class ALandscapeBlueprintBrushBase* InBrush)
{
	if (ALandscape * Landscape = GetLandscape())
	{
		int32 BrushLayer = Landscape->GetBrushLayer(InBrush);
		TArray<ALandscapeBlueprintBrushBase*> Brushes = Landscape->GetBrushesForLayer(BrushLayer);
		for (ALandscapeBlueprintBrushBase* Brush : Brushes)
		{
			Brush->SetIsVisible(Brush == InBrush);
		}
	}
}

void FEdModeLandscape::DuplicateBrush(class ALandscapeBlueprintBrushBase* InBrush)
{
	GEditor->SelectNone(false, true);
	GEditor->SelectActor(InBrush, true, false, false);

	GEditor->edactDuplicateSelected(InBrush->GetLevel(), false);
}

bool FEdModeLandscape::IsCurrentLayerBlendSubstractive(const TWeakObjectPtr<ULandscapeLayerInfoObject>& InLayerInfoObj) const
{
	ALandscape* Landscape = GetLandscape();

	if (Landscape != nullptr)
	{
		return Landscape->IsLayerBlendSubstractive(GetCurrentLayerIndex(), InLayerInfoObj);
	}

	return false;
}

void FEdModeLandscape::SetCurrentLayerSubstractiveBlendStatus(bool InStatus, const TWeakObjectPtr<ULandscapeLayerInfoObject>& InLayerInfoObj)
{
	ALandscape* Landscape = GetLandscape();

	if (Landscape != nullptr)
	{
		return Landscape->SetLayerSubstractiveBlendStatus(GetCurrentLayerIndex(), InStatus, InLayerInfoObj);
	}
}

FLandscapeLayer* FEdModeLandscape::GetCurrentLayer() const
{
	return GetLayer(GetCurrentLayerIndex());
}

void FEdModeLandscape::AutoUpdateDirtyLandscapeSplines()
{
	if (HasLandscapeLayersContent() && GEditor->IsTransactionActive())
	{
		// Only auto-update if a layer is reserved for landscape splines
		ALandscape* Landscape = GetLandscape();
		if (Landscape && Landscape->GetLandscapeSplinesReservedLayer())
		{
			// TODO : Only update dirty regions
			Landscape->RequestSplineLayerUpdate();
		}
	}
}

bool FEdModeLandscape::CanEditLayer(FText* Reason /*=nullptr*/, FLandscapeLayer* InLayer /*= nullptr*/)
{
	if (CanHaveLandscapeLayersContent())
	{
		ALandscape* Landscape = GetLandscape();
		FLandscapeLayer* TargetLayer = InLayer ? InLayer : GetCurrentLayer();
		if (!TargetLayer)
		{
			if (Reason)
			{
				*Reason = NSLOCTEXT("UnrealEd", "LandscapeInvalidLayer", "No layer selected. You must first chose a layer to work on.");
			}
			return false;
		}
		else if (!TargetLayer->bVisible)
		{
			if (Reason)
			{
				*Reason = NSLOCTEXT("UnrealEd", "LandscapeLayerHidden", "Painting or sculping in a hidden layer is not allowed.");
			}
			return false;
		}
		else if (TargetLayer->bLocked)
		{
			if (Reason)
			{
				*Reason = NSLOCTEXT("UnrealEd", "LandscapeLayerLocked", "This layer is locked. You must unlock it before you can work on this layer.");
			}
			return false;
		}
		else if (CurrentTool)
		{
			int32 TargetLayerIndex = Landscape ? Landscape->LandscapeLayers.IndexOfByPredicate([TargetLayeyGuid = TargetLayer->Guid](const FLandscapeLayer& OtherLayer) { return OtherLayer.Guid == TargetLayeyGuid; }) : INDEX_NONE;

			if ((CurrentTool != (FLandscapeTool*)SplinesTool) && Landscape && (TargetLayer == Landscape->GetLandscapeSplinesReservedLayer()))
			{
				if (Reason)
				{
					*Reason = NSLOCTEXT("UnrealEd", "LandscapeLayerReservedForSplines", "This layer is reserved for Landscape Splines.");
				}
				return false;
			}
			else if (CurrentTool->GetToolName() == FName("Retopologize"))
			{
				if (Reason)
				{
					*Reason = FText::Format(NSLOCTEXT("UnrealEd", "LandscapeLayersNoSupportForRetopologize", "{0} Tool is not available with the Landscape Layer System."), CurrentTool->GetDisplayName());
				}
				return false;
			}
		}
	}

	if (CurrentToolTarget.TargetType == ELandscapeToolTargetType::Weightmap && CurrentToolTarget.LayerInfo == nullptr && (CurrentTool && CurrentTool->GetToolName() != FName("BlueprintBrush")))
	{
		if (Reason)
		{
			*Reason = NSLOCTEXT("UnrealEd", "LandscapeNeedToCreateLayerInfo", "This layer has no layer info assigned yet. You must create or assign a layer info before you can paint this layer.");
		}
		return false;
		// TODO: FName to LayerInfo: do we want to create the layer info here?
		//if (FMessageDialog::Open(EAppMsgType::YesNo, NSLOCTEXT("UnrealEd", "LandscapeNeedToCreateLayerInfo", "This layer has no layer info assigned yet. You must create or assign a layer info before you can paint this layer.")) == EAppReturnType::Yes)
		//{
		//	CurrentToolTarget.LandscapeInfo->LandscapeProxy->CreateLayerInfo(*CurrentToolTarget.PlaceholderLayerName.ToString());
		//}
	}
	return true;
}

void FEdModeLandscape::UpdateLandscapeSplines(bool bUpdateOnlySelected /* = false*/)
{
	if (HasLandscapeLayersContent())
	{
		ALandscape* Landscape = GetLandscape();
		if (Landscape)
		{
			Landscape->UpdateLandscapeSplines(GetCurrentLayerGuid(), bUpdateOnlySelected);
		}
	}
	else
	{
		if (CurrentToolTarget.LandscapeInfo.IsValid())
		{
			CurrentToolTarget.LandscapeInfo->ApplySplines(bUpdateOnlySelected);
		}
	}
}

FGuid FEdModeLandscape::GetCurrentLayerGuid() const
{
	FLandscapeLayer* CurrentLayer = GetCurrentLayer();
	return CurrentLayer ? CurrentLayer->Guid : FGuid();
}

bool FEdModeLandscape::NeedToFillEmptyMaterialLayers() const
{
	if (!CurrentToolTarget.LandscapeInfo.IsValid() || !CurrentToolTarget.LandscapeInfo->LandscapeActor.IsValid())
	{
		return false;
	}

	bool bCanFill = true;

	CurrentToolTarget.LandscapeInfo->ForAllLandscapeProxies([&](ALandscapeProxy* Proxy)
	{
		if (!bCanFill)
		{
			return;
		}

		ALandscape* Landscape = Proxy->GetLandscapeActor();

		if (Landscape != nullptr)
		{
			for (FLandscapeLayer& Layer : Landscape->LandscapeLayers)
			{
				for (ULandscapeComponent* Component : Proxy->LandscapeComponents)
				{
					const FLandscapeLayerComponentData* LayerComponentData = Component->GetLayerData(Layer.Guid);

					if (LayerComponentData != nullptr)
					{
						for (const FWeightmapLayerAllocationInfo& Alloc : LayerComponentData->WeightmapData.LayerAllocations)
						{
							if (Alloc.LayerInfo != nullptr)
							{
								bCanFill = false;
								return;
							}
						}
					}
				}
			}
		}
	});	

	return bCanFill;
}

void FEdModeLandscape::UpdateBrushList()
{
	BrushList.Empty();
	for (TObjectIterator<ALandscapeBlueprintBrushBase> BrushIt(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject, true, EInternalObjectFlags::Garbage); BrushIt; ++BrushIt)
	{
		ALandscapeBlueprintBrushBase* Brush = *BrushIt;
		if (Brush->GetPackage() != GetTransientPackage())
		{
			BrushList.Add(Brush);
		}
	}
}

FName FLandscapeTargetListInfo::GetLayerName() const
{
	return LayerInfoObj.IsValid() ? LayerInfoObj->LayerName : LayerName;
}

#undef LOCTEXT_NAMESPACE
