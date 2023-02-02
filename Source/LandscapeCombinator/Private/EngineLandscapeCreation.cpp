// Copyright Epic Games, Inc. All Rights Reserved.

#include "EngineLandscapeCreation.h"
#include "Utils/Logging.h"

#include "Editor/LandscapeEditor/Private/LandscapeEditorDetailCustomization_NewLandscape.h"
#include "EditorModes.h"
#include "EditorModeManager.h" 
#include "LandscapeEditorObject.h"
#include "LandscapeSettings.h" 
#include "LandscapeSubsystem.h"
#include "LandscapeDataAccess.h"
#include "SlateOptMacros.h"

#include "Editor/LandscapeEditor/Private/LandscapeEditorDetailCustomization_ImportExport.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "SlateOptMacros.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Notifications/SErrorText.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "SWarningOrErrorBox.h"
#include "LandscapeEditorModule.h"
#include "LandscapeEditorObject.h"
#include "Landscape.h"
#include "LandscapeConfigHelper.h"
#include "LandscapeImportHelper.h"
#include "LandscapeSettings.h"
#include "LandscapeSplinesComponent.h"
#include "LandscapeSplineControlPoint.h" 

#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "PropertyCustomizationHelpers.h"

#include "Dialogs/DlgPickAssetPath.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Input/SRotatorInputBox.h"
#include "ScopedTransaction.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "TutorialMetaData.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "LandscapeDataAccess.h"
#include "Settings/EditorExperimentalSettings.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "LandscapeSubsystem.h"
#include "SPrimaryButton.h"
#include "Widgets/Input/SSegmentedControl.h"

#include "Editor/LandscapeEditor/Private/LandscapeEdMode.h"

#include "Algo/Accumulate.h"
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

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"


/* This file contains functions copied from Unreal Engine 5.1, as calling them directly results in "unresolved external symbol" */


/*********************************************************************/
/* Functions copied from Unreal Engine 5.1 with slight modifications */
/*********************************************************************/

namespace EngineLandscapeCreation {

	ALandscape* OnCreateButtonClicked()
	{
		GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_Landscape);
		FEdModeLandscape* LandscapeEdMode = (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
		if (LandscapeEdMode != nullptr && 
			LandscapeEdMode->GetWorld() != nullptr && 
			LandscapeEdMode->GetWorld()->GetCurrentLevel()->bIsVisible)
		{
			ULandscapeEditorObject* UISettings = LandscapeEdMode->UISettings;
			const int32 ComponentCountX = UISettings->NewLandscape_ComponentCount.X;
			const int32 ComponentCountY = UISettings->NewLandscape_ComponentCount.Y;
			const int32 QuadsPerComponent = UISettings->NewLandscape_SectionsPerComponent * UISettings->NewLandscape_QuadsPerSection;
			const int32 SizeX = ComponentCountX * QuadsPerComponent + 1;
			const int32 SizeY = ComponentCountY * QuadsPerComponent + 1;

			TArray<FLandscapeImportLayerInfo> MaterialImportLayers;
			ELandscapeImportResult LayerImportResult = LandscapeEdMode->NewLandscapePreviewMode == ENewLandscapePreviewMode::NewLandscape ? UISettings->CreateNewLayersInfo(MaterialImportLayers) : UISettings->CreateImportLayersInfo(MaterialImportLayers);

			if (LayerImportResult == ELandscapeImportResult::Error)
			{
				return nullptr;
			}

			TMap<FGuid, TArray<uint16>> HeightDataPerLayers;
			TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayers;

			TArray<uint16> OutHeightData;
			if (LandscapeEdMode->NewLandscapePreviewMode == ENewLandscapePreviewMode::NewLandscape)
			{
				UISettings->InitializeDefaultHeightData(OutHeightData);
			}
			else
			{
				UISettings->ExpandImportData(OutHeightData, MaterialImportLayers);
			}

			HeightDataPerLayers.Add(FGuid(), OutHeightData);
			// ComputeHeightData will also modify/expand material layers data, which is why we create MaterialLayerDataPerLayers after calling ComputeHeightData
			MaterialLayerDataPerLayers.Add(FGuid(), MoveTemp(MaterialImportLayers));

			FScopedTransaction Transaction(LOCTEXT("Undo", "Creating New Landscape"));

			const FVector Offset = FTransform(UISettings->NewLandscape_Rotation, FVector::ZeroVector, UISettings->NewLandscape_Scale).TransformVector(FVector(-ComponentCountX * QuadsPerComponent / 2, -ComponentCountY * QuadsPerComponent / 2, 0));
		
			ALandscape* Landscape = LandscapeEdMode->GetWorld()->SpawnActor<ALandscape>(UISettings->NewLandscape_Location + Offset, UISettings->NewLandscape_Rotation);
			Landscape->bCanHaveLayersContent = UISettings->bCanHaveLayersContent;
			Landscape->LandscapeMaterial = UISettings->NewLandscape_Material.Get();
			Landscape->SetActorRelativeScale3D(UISettings->NewLandscape_Scale);

			// automatically calculate a lighting LOD that won't crash lightmass (hopefully)
			// < 2048x2048 -> LOD0
			// >=2048x2048 -> LOD1
			// >= 4096x4096 -> LOD2
			// >= 8192x8192 -> LOD3
			Landscape->StaticLightingLOD = FMath::DivideAndRoundUp(FMath::CeilLogTwo((SizeX * SizeY) / (2048 * 2048) + 1), (uint32)2);

			FString ReimportHeightmapFilePath;
			if (LandscapeEdMode->NewLandscapePreviewMode == ENewLandscapePreviewMode::ImportLandscape)
			{
				ReimportHeightmapFilePath = UISettings->ImportLandscape_HeightmapFilename;
			}

			Landscape->Import(FGuid::NewGuid(), 0, 0, SizeX - 1, SizeY - 1, UISettings->NewLandscape_SectionsPerComponent, UISettings->NewLandscape_QuadsPerSection, HeightDataPerLayers, *ReimportHeightmapFilePath, MaterialLayerDataPerLayers, UISettings->ImportLandscape_AlphamapType);

			ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
			check(LandscapeInfo);

			FActorLabelUtilities::SetActorLabelUnique(Landscape, ALandscape::StaticClass()->GetName());

			LandscapeInfo->UpdateLayerInfoMap(Landscape);

			// Import doesn't fill in the LayerInfo for layers with no data, do that now
			const TArray<FLandscapeImportLayer>& ImportLandscapeLayersList = UISettings->ImportLandscape_Layers;
			const ULandscapeSettings* Settings = GetDefault<ULandscapeSettings>();
			TSoftObjectPtr<ULandscapeLayerInfoObject> DefaultLayerInfoObject = Settings->GetDefaultLayerInfoObject().LoadSynchronous();

			for (int32 i = 0; i < ImportLandscapeLayersList.Num(); i++)
			{
				ULandscapeLayerInfoObject* LayerInfo = ImportLandscapeLayersList[i].LayerInfo;
				FName LayerName = ImportLandscapeLayersList[i].LayerName;

				// If DefaultLayerInfoObject is set and LayerInfo does not exist, we will try to create the new LayerInfo by cloning DefaultLayerInfoObject. Except for VisibilityLayer which doesn't require an asset.
				if (DefaultLayerInfoObject.IsValid() && (LayerInfo == nullptr) && (LayerName != ALandscapeProxy::VisibilityLayer->LayerName))
				{
					LayerInfo = Landscape->CreateLayerInfo(*LayerName.ToString(), DefaultLayerInfoObject.Get());

					if (LayerInfo != nullptr)
					{
						LayerInfo->LayerUsageDebugColor = LayerInfo->GenerateLayerUsageDebugColor();
						LayerInfo->MarkPackageDirty();
					}
				}

				if (LayerInfo != nullptr)
				{
					if (LandscapeEdMode->NewLandscapePreviewMode == ENewLandscapePreviewMode::ImportLandscape)
					{
						Landscape->EditorLayerSettings.Add(FLandscapeEditorLayerSettings(LayerInfo, ImportLandscapeLayersList[i].SourceFilePath));
					}
					else
					{
						Landscape->EditorLayerSettings.Add(FLandscapeEditorLayerSettings(LayerInfo));
					}

					int32 LayerInfoIndex = LandscapeInfo->GetLayerInfoIndex(ImportLandscapeLayersList[i].LayerName);
					if (ensure(LayerInfoIndex != INDEX_NONE))
					{
						FLandscapeInfoLayerSettings& LayerSettings = LandscapeInfo->Layers[LayerInfoIndex];
						LayerSettings.LayerInfoObj = LayerInfo;
					}
				}
			}

			//LandscapeEdMode->UpdateLandscapeList();
			//LandscapeEdMode->SetLandscapeInfo(LandscapeInfo);
			//LandscapeEdMode->CurrentToolTarget.TargetType = ELandscapeToolTargetType::Heightmap;
			//LandscapeEdMode->SetCurrentTargetLayer(NAME_None, nullptr);
			//LandscapeEdMode->SetCurrentTool("Select"); // change tool so switching back to the manage mode doesn't give "New Landscape" again
			//LandscapeEdMode->SetCurrentTool("Sculpt"); // change to sculpting mode and tool
			//LandscapeEdMode->SetCurrentLayer(0);

			LandscapeEdMode->GetWorld()->GetSubsystem<ULandscapeSubsystem>()->ChangeGridSize(LandscapeInfo, UISettings->WorldPartitionGridSize);

			//if (LandscapeEdMode->CurrentToolTarget.LandscapeInfo.IsValid())
			//{
			//	ALandscapeProxy* LandscapeProxy = LandscapeEdMode->CurrentToolTarget.LandscapeInfo->GetLandscapeProxy();
			//	LandscapeProxy->OnMaterialChangedDelegate().AddRaw(LandscapeEdMode, &FEdModeLandscape::OnLandscapeMaterialChangedDelegate);
			//}
			return Landscape;
		}
		return nullptr;

	}

	// copied, simplified, (and slightly modified) from Unreal Engine's file LandscapeEdModeSplineTools.cpp
	ULandscapeSplineControlPoint* AddControlPoint(ULandscapeSplinesComponent* SplinesComponent, const FVector& LocalLocation)
	{
		FScopedTransaction Transaction(LOCTEXT("LandscapeSpline_AddControlPoint", "Add Landscape Spline Control Point"));

		SplinesComponent->Modify();

		ULandscapeSplineControlPoint* NewControlPoint = NewObject<ULandscapeSplineControlPoint>(SplinesComponent, NAME_None, RF_Transactional);
		SplinesComponent->GetControlPoints().Add(NewControlPoint);

		NewControlPoint->Location = LocalLocation;
		NewControlPoint->UpdateSplinePoints();

		if (!SplinesComponent->IsRegistered())
		{
			SplinesComponent->RegisterComponent();
		}
		else
		{
			SplinesComponent->MarkRenderStateDirty();
		}
		return NewControlPoint;
	}

	// copied, simplified, (and slightly modified) from Unreal Engine's file LandscapeEdModeSplineTools.cpp
	ULandscapeSplineSegment* AddSegment(ULandscapeSplineControlPoint* Start, ULandscapeSplineControlPoint* End, bool bAutoRotateStart, bool bAutoRotateEnd)
	{
		FScopedTransaction Transaction(LOCTEXT("LandscapeSpline_AddSegment", "Add Landscape Spline Segment"));


		ULandscapeSplinesComponent* SplinesComponent = Start->GetOuterULandscapeSplinesComponent();
		SplinesComponent->Modify();
		Start->Modify();
		End->Modify();

		ULandscapeSplineSegment* NewSegment = NewObject<ULandscapeSplineSegment>(SplinesComponent, NAME_None, RF_Transactional);
		SplinesComponent->GetSegments().Add(NewSegment);

		NewSegment->Connections[0].ControlPoint = Start;
		NewSegment->Connections[1].ControlPoint = End;

		NewSegment->Connections[0].SocketName = Start->GetBestConnectionTo(End->Location);
		NewSegment->Connections[1].SocketName = End->GetBestConnectionTo(Start->Location);

		FVector StartLocation; FRotator StartRotation;
		Start->GetConnectionLocationAndRotation(NewSegment->Connections[0].SocketName, StartLocation, StartRotation);
		FVector EndLocation; FRotator EndRotation;
		End->GetConnectionLocationAndRotation(NewSegment->Connections[1].SocketName, EndLocation, EndRotation);

		// Set up tangent lengths
		NewSegment->Connections[0].TangentLen = (EndLocation - StartLocation).Size();
		NewSegment->Connections[1].TangentLen = NewSegment->Connections[0].TangentLen;

		NewSegment->AutoFlipTangents();

		Start->ConnectedSegments.Add(FLandscapeSplineConnection(NewSegment, 0));
		End->ConnectedSegments.Add(FLandscapeSplineConnection(NewSegment, 1));

		bool bUpdatedStart = false;
		bool bUpdatedEnd = false;
		if (bAutoRotateStart)
		{
			Start->AutoCalcRotation();
			Start->UpdateSplinePoints();
			bUpdatedStart = true;
		}
		if (bAutoRotateEnd)
		{
			End->AutoCalcRotation();
			End->UpdateSplinePoints();
			bUpdatedEnd = true;
		}

		// Control points' points are currently based on connected segments, so need to be updated.
		if (!bUpdatedStart && Start->Mesh)
		{
			Start->UpdateSplinePoints();
		}
		if (!bUpdatedEnd && End->Mesh)
		{
			End->UpdateSplinePoints();
		}

		// If we've called UpdateSplinePoints on either control point it will already have called UpdateSplinePoints on the new segment
		if (!(bUpdatedStart || bUpdatedEnd))
		{
			NewSegment->UpdateSplinePoints();
		}

		return NewSegment;
	}
}


/*****************************************************************/
/* Functions copied from Unreal Engine 5.1 without modifications */
/*****************************************************************/

void ULandscapeEditorObject::InitializeDefaultHeightData(TArray<uint16>& OutData)
{
	const int32 QuadsPerComponent = NewLandscape_SectionsPerComponent * NewLandscape_QuadsPerSection;
	const int32 SizeX = NewLandscape_ComponentCount.X * QuadsPerComponent + 1;
	const int32 SizeY = NewLandscape_ComponentCount.Y * QuadsPerComponent + 1;
	const int32 TotalSize = SizeX * SizeY;
	// Initialize heightmap data
	OutData.Reset();
	OutData.AddUninitialized(TotalSize);
	
	TArray<uint16> StrideData;
	StrideData.AddUninitialized(SizeX);
	// Initialize blank heightmap data
	for (int32 X = 0; X < SizeX; ++X)
	{
		StrideData[X] = LandscapeDataAccess::MidValue;
	}
	for (int32 Y = 0; Y < SizeY; ++Y)
	{
		FMemory::Memcpy(&OutData[Y * SizeX], StrideData.GetData(), sizeof(uint16) * SizeX);
	}
}

ELandscapeImportResult ULandscapeEditorObject::CreateNewLayersInfo(TArray<FLandscapeImportLayerInfo>& OutNewLayerInfos)
{
	const int32 QuadsPerComponent = NewLandscape_SectionsPerComponent * NewLandscape_QuadsPerSection;
	const int32 SizeX = NewLandscape_ComponentCount.X * QuadsPerComponent + 1;
	const int32 SizeY = NewLandscape_ComponentCount.Y * QuadsPerComponent + 1;

	OutNewLayerInfos.Reset(ImportLandscape_Layers.Num());

	// Fill in LayerInfos array and allocate data
	for (const FLandscapeImportLayer& UIImportLayer : ImportLandscape_Layers)
	{
		FLandscapeImportLayerInfo ImportLayer = FLandscapeImportLayerInfo(UIImportLayer.LayerName);
		ImportLayer.LayerInfo = UIImportLayer.LayerInfo;
		ImportLayer.SourceFilePath = "";
		ImportLayer.LayerData = TArray<uint8>();
		OutNewLayerInfos.Add(MoveTemp(ImportLayer));
	}

	// Fill the first weight-blended layer to 100%
	if (FLandscapeImportLayerInfo* FirstBlendedLayer = OutNewLayerInfos.FindByPredicate([](const FLandscapeImportLayerInfo& ImportLayer) { return ImportLayer.LayerInfo && !ImportLayer.LayerInfo->bNoWeightBlend; }))
	{
		const int32 DataSize = SizeX * SizeY;
		FirstBlendedLayer->LayerData.AddUninitialized(DataSize);

		uint8* ByteData = FirstBlendedLayer->LayerData.GetData();
		FMemory::Memset(ByteData, 255, DataSize);
	}

	return ELandscapeImportResult::Success;
}

void ULandscapeEditorObject::ExpandImportData(TArray<uint16>& OutHeightData, TArray<FLandscapeImportLayerInfo>& OutImportLayerInfos)
{
	const TArray<uint16>& ImportData = GetImportLandscapeData();
	if (ImportData.Num())
	{
		const int32 QuadsPerComponent = NewLandscape_SectionsPerComponent * NewLandscape_QuadsPerSection;
		FLandscapeImportResolution RequiredResolution(NewLandscape_ComponentCount.X * QuadsPerComponent + 1, NewLandscape_ComponentCount.Y * QuadsPerComponent + 1);
		FLandscapeImportResolution ImportResolution(ImportLandscape_Width, ImportLandscape_Height);

		FLandscapeImportHelper::TransformHeightmapImportData(ImportData, OutHeightData, ImportResolution, RequiredResolution, ELandscapeImportTransformType::ExpandCentered);

		for (int32 LayerIdx = 0; LayerIdx < OutImportLayerInfos.Num(); ++LayerIdx)
		{
			TArray<uint8>& OutImportLayerData = OutImportLayerInfos[LayerIdx].LayerData;
			TArray<uint8> OutLayerData;
			if (OutImportLayerData.Num())
			{
				FLandscapeImportHelper::TransformWeightmapImportData(OutImportLayerData, OutLayerData, ImportResolution, RequiredResolution, ELandscapeImportTransformType::ExpandCentered);
				OutImportLayerData = MoveTemp(OutLayerData);
			}
		}
	}
}

ELandscapeImportResult ULandscapeEditorObject::CreateImportLayersInfo(TArray<FLandscapeImportLayerInfo>& OutImportLayerInfos)
{
	const uint32 ImportSizeX = ImportLandscape_Width;
	const uint32 ImportSizeY = ImportLandscape_Height;

	if (ImportLandscape_HeightmapImportResult == ELandscapeImportResult::Error)
	{
		// Cancel import
		return ELandscapeImportResult::Error;
	}

	OutImportLayerInfos.Reserve(ImportLandscape_Layers.Num());

	// Fill in LayerInfos array and allocate data
	for (FLandscapeImportLayer& UIImportLayer : ImportLandscape_Layers)
	{
		OutImportLayerInfos.Add((const FLandscapeImportLayer&)UIImportLayer); //slicing is fine here
		FLandscapeImportLayerInfo& ImportLayer = OutImportLayerInfos.Last();

		if (ImportLayer.LayerInfo != nullptr && !ImportLayer.SourceFilePath.IsEmpty())
		{
			UIImportLayer.ImportResult = FLandscapeImportHelper::GetWeightmapImportDescriptor(ImportLayer.SourceFilePath, UseSingleFileImport(), bFlipYAxis, ImportLayer.LayerName, UIImportLayer.ImportDescriptor, UIImportLayer.ErrorMessage);
			if (UIImportLayer.ImportResult == ELandscapeImportResult::Error)
			{
				FMessageDialog::Open(EAppMsgType::Ok, UIImportLayer.ErrorMessage);
				return ELandscapeImportResult::Error;
			}

			// Use same import index as Heightmap
			int32 FoundIndex = INDEX_NONE;
			for (int32 Index = 0; Index < UIImportLayer.ImportDescriptor.FileResolutions.Num(); ++Index)
			{
				if (UIImportLayer.ImportDescriptor.ImportResolutions[Index] == HeightmapImportDescriptor.ImportResolutions[HeightmapImportDescriptorIndex])
				{
					FoundIndex = Index;
					break;
				}
			}

			if (FoundIndex == INDEX_NONE)
			{
				UIImportLayer.ImportResult = ELandscapeImportResult::Error;
				UIImportLayer.ErrorMessage = NSLOCTEXT("LandscapeEditor.NewLandscape", "Import_WeightHeightResolutionMismatch", "Weightmap import resolution isn't same as Heightmap resolution.");
				FMessageDialog::Open(EAppMsgType::Ok, UIImportLayer.ErrorMessage);
				return ELandscapeImportResult::Error;
			}

			UIImportLayer.ImportResult = FLandscapeImportHelper::GetWeightmapImportData(UIImportLayer.ImportDescriptor, FoundIndex, ImportLayer.LayerName, ImportLayer.LayerData, UIImportLayer.ErrorMessage);
			if (UIImportLayer.ImportResult == ELandscapeImportResult::Error)
			{
				FMessageDialog::Open(EAppMsgType::Ok, UIImportLayer.ErrorMessage);
				return ELandscapeImportResult::Error;
			}
		}
	}

	return ELandscapeImportResult::Success;
}

void ULandscapeEditorObject::ChooseBestComponentSizeForImport()
{
	FLandscapeImportHelper::ChooseBestComponentSizeForImport(ImportLandscape_Width, ImportLandscape_Height, NewLandscape_QuadsPerSection, NewLandscape_SectionsPerComponent, NewLandscape_ComponentCount);
}

void ULandscapeEditorObject::RefreshImports()
{
	ClearImportLandscapeData();
	HeightmapImportDescriptorIndex = 0;
	HeightmapImportDescriptor.Reset();
	ImportLandscape_Width = 0;
	ImportLandscape_Height = 0;

	ImportLandscape_HeightmapImportResult = ELandscapeImportResult::Success;
	ImportLandscape_HeightmapErrorMessage = FText();

	if (!ImportLandscape_HeightmapFilename.IsEmpty())
	{
		ImportLandscape_HeightmapImportResult =
			FLandscapeImportHelper::GetHeightmapImportDescriptor(ImportLandscape_HeightmapFilename, UseSingleFileImport(), bFlipYAxis, HeightmapImportDescriptor, ImportLandscape_HeightmapErrorMessage);
		if (ImportLandscape_HeightmapImportResult != ELandscapeImportResult::Error)
		{
			ImportLandscape_Width = HeightmapImportDescriptor.ImportResolutions[HeightmapImportDescriptorIndex].Width;
			ImportLandscape_Height = HeightmapImportDescriptor.ImportResolutions[HeightmapImportDescriptorIndex].Height;
			ChooseBestComponentSizeForImport();
			ImportLandscapeData();
		}
	}

	RefreshLayerImports();
}

void ULandscapeEditorObject::ImportLandscapeData()
{
	ImportLandscape_HeightmapImportResult = FLandscapeImportHelper::GetHeightmapImportData(HeightmapImportDescriptor, HeightmapImportDescriptorIndex, ImportLandscape_Data, ImportLandscape_HeightmapErrorMessage);
	if (ImportLandscape_HeightmapImportResult == ELandscapeImportResult::Error)
	{
		ImportLandscape_Data.Empty();
	}
}

bool ULandscapeEditorObject::UseSingleFileImport() const
{
	if (ParentMode)
	{
		return ParentMode->UseSingleFileImport();
	}

	return true;
}

bool FEdModeLandscape::IsGridBased() const
{
	return GetWorld()->GetSubsystem<ULandscapeSubsystem>()->IsGridBased();
}

void ULandscapeEditorObject::RefreshLayerImport(FLandscapeImportLayer& ImportLayer)
{
	ImportLayer.ErrorMessage = FText();
	ImportLayer.ImportResult = ELandscapeImportResult::Success;

	if (ImportLayer.LayerName == ALandscapeProxy::VisibilityLayer->LayerName)
	{
		ImportLayer.LayerInfo = ALandscapeProxy::VisibilityLayer;
	}

	if (!ImportLayer.SourceFilePath.IsEmpty())
	{
		if (!ImportLayer.LayerInfo)
		{
			ImportLayer.ImportResult = ELandscapeImportResult::Error;
			ImportLayer.ErrorMessage = NSLOCTEXT("LandscapeEditor.NewLandscape", "Import_LayerInfoNotSet", "Can't import a layer file without a layer info");
		}
		else
		{
			ImportLayer.ImportResult = FLandscapeImportHelper::GetWeightmapImportDescriptor(ImportLayer.SourceFilePath, UseSingleFileImport(), bFlipYAxis, ImportLayer.LayerName, ImportLayer.ImportDescriptor, ImportLayer.ErrorMessage);
			if (ImportLayer.ImportResult != ELandscapeImportResult::Error)
			{
				if (ImportLandscape_Height != 0 || ImportLandscape_Width != 0)
				{
					// Use same import index as Heightmap
					int32 FoundIndex = INDEX_NONE;
					for (int32 Index = 0; Index < ImportLayer.ImportDescriptor.FileResolutions.Num(); ++Index)
					{
						if (ImportLayer.ImportDescriptor.ImportResolutions[Index] == HeightmapImportDescriptor.ImportResolutions[HeightmapImportDescriptorIndex])
						{
							FoundIndex = Index;
							break;
						}
					}

					if (FoundIndex == INDEX_NONE)
					{
						ImportLayer.ImportResult = ELandscapeImportResult::Error;
						ImportLayer.ErrorMessage = NSLOCTEXT("LandscapeEditor.ImportLandscape", "Import_WeightHeightResolutionMismatch", "Weightmap import resolution isn't same as Heightmap resolution.");
					}
				}
			}
		}
	}
}

void ULandscapeEditorObject::RefreshLayerImports()
{
	// Make sure to reset import width and height if we don't have a Heightmap to import
	if (ImportLandscape_HeightmapFilename.IsEmpty())
	{
		HeightmapImportDescriptorIndex = 0;
		ImportLandscape_Width = 0;
		ImportLandscape_Height = 0;
	}

	for (FLandscapeImportLayer& UIImportLayer : ImportLandscape_Layers)
	{
		RefreshLayerImport(UIImportLayer);
	}
}

//int32 FEdModeLandscape::UpdateLandscapeList()
//{
//	LandscapeList.Empty();
//
//	if (!CurrentGizmoActor.IsValid())
//	{
//		ALandscapeGizmoActiveActor* GizmoActor = nullptr;
//		for (TActorIterator<ALandscapeGizmoActiveActor> It(GetWorld()); It; ++It)
//		{
//			GizmoActor = *It;
//			break;
//		}
//	}
//
//	int32 CurrentIndex = INDEX_NONE;
//	UWorld* World = GetWorld();
//	
//	if (World)
//	{
//		int32 Index = 0;
//		auto& LandscapeInfoMap = ULandscapeInfoMap::GetLandscapeInfoMap(World);
//
//		for (auto It = LandscapeInfoMap.Map.CreateIterator(); It; ++It)
//		{
//			ULandscapeInfo* LandscapeInfo = It.Value();
//			if (IsValid(LandscapeInfo) && LandscapeInfo->SupportsLandscapeEditing())
//			{
//				if (ALandscape* Landscape = LandscapeInfo->LandscapeActor.Get())
//				{
//					Landscape->RegisterLandscapeEdMode(this);
//				}
//
//				ALandscapeProxy* LandscapeProxy = LandscapeInfo->GetLandscapeProxy();
//				if (LandscapeProxy)
//				{
//					if (CurrentToolTarget.LandscapeInfo == LandscapeInfo)
//					{
//						CurrentIndex = Index;
//
//						// Update GizmoActor Landscape Target (is this necessary?)
//						if (CurrentGizmoActor.IsValid())
//						{
//							CurrentGizmoActor->SetTargetLandscape(LandscapeInfo);
//						}
//					}
//
//					int32 MinX, MinY, MaxX, MaxY;
//					int32 Width = 0, Height = 0;
//					if (LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
//					{
//						Width = MaxX - MinX + 1;
//						Height = MaxY - MinY + 1;
//					}
//
//					LandscapeList.Add(FLandscapeListInfo(*LandscapeProxy->GetName(), LandscapeInfo,
//						LandscapeInfo->ComponentSizeQuads, LandscapeInfo->ComponentNumSubsections, Width, Height));
//					Index++;
//				}
//			}
//		}
//	}
//
//	if (CurrentIndex == INDEX_NONE)
//	{
//		if (LandscapeList.Num() > 0)
//		{
//			FName CurrentToolName = CurrentTool != nullptr ? CurrentTool->GetToolName() : FName();
//			SetLandscapeInfo(LandscapeList[0].Info);
//			CurrentIndex = 0;
//
//			SetCurrentLayer(0);
//
//			UpdateShownLayerList();
//						
//			if (!CurrentToolName.IsNone())
//			{
//				SetCurrentTool(CurrentToolName);
//			}
//		}
//		else
//		{
//			// no landscape, switch to "new landscape" tool
//			SetLandscapeInfo(nullptr);
//			SetCurrentToolMode("ToolMode_Manage", false);
//			SetCurrentTool("NewLandscape");
//		}
//	}
//	else if (!HasValidLandscapeEditLayerSelection())
//	{
//		SetCurrentLayer(0);
//	}
//
//	if (!CanEditCurrentTarget())
//	{
//		SetCurrentToolMode("ToolMode_Manage", false);
//		SetCurrentTool("NewLandscape");
//	}
//
//	return CurrentIndex;
//}

#undef LOCTEXT_NAMESPACE