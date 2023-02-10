// Copyright Epic Games, Inc. All Rights Reserved.

#include "EngineCode/EngineCode.h"
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


#include "Editor/FoliageEdit/Private/FoliageEdMode.h"
#include "FoliageHelper.h"
#include "Components/BrushComponent.h"
#include "Components/ModelComponent.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"


/* This file contains functions copied from Unreal Engine 5.1 and 5.1.1, as calling them directly results in "unresolved external symbol" */


/*********************************************************************/
/* Functions copied from Unreal Engine 5.1 with slight modifications */
/*********************************************************************/

namespace EngineCode {

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


/* Functions below this line are copied from UE 5.1.1 */

void FEdModeFoliage::AddInstances(UWorld* InWorld, const TArray<FDesiredFoliageInstance>& DesiredInstances, const FFoliagePaintingGeometryFilter& OverrideGeometryFilter, bool InRebuildFoliageTree)
{
	TMap<const UFoliageType*, TArray<FDesiredFoliageInstance>> SettingsInstancesMap;
	for (const FDesiredFoliageInstance& DesiredInst : DesiredInstances)
	{
		TArray<FDesiredFoliageInstance>& Instances = SettingsInstancesMap.FindOrAdd(DesiredInst.FoliageType);
		Instances.Add(DesiredInst);
	}

	for (auto It = SettingsInstancesMap.CreateConstIterator(); It; ++It)
	{
		const UFoliageType* FoliageType = It.Key();

		const TArray<FDesiredFoliageInstance>& Instances = It.Value();
		AddInstancesImp(InWorld, FoliageType, Instances, TArray<int32>(), 1.f, nullptr, nullptr, &OverrideGeometryFilter, InRebuildFoliageTree);
	}
}

DECLARE_CYCLE_STAT(TEXT("Calculate Potential Instance"), STAT_FoliageCalculatePotentialInstance, STATGROUP_Foliage);
DECLARE_CYCLE_STAT(TEXT("Add Instance Imp"), STAT_FoliageAddInstanceImp, STATGROUP_Foliage);
DECLARE_CYCLE_STAT(TEXT("Add Instance For Brush"), STAT_FoliageAddInstanceBrush, STATGROUP_Foliage);
DECLARE_CYCLE_STAT(TEXT("Remove Instance For Brush"), STAT_FoliageRemoveInstanceBrush, STATGROUP_Foliage);
DECLARE_CYCLE_STAT(TEXT("Spawn Instance"), STAT_FoliageSpawnInstance, STATGROUP_Foliage);

static TSet<AInstancedFoliageActor*> CurrentFoliageTraceBrushAffectedIFAs;
static void SpawnFoliageInstance(UWorld* InWorld, const UFoliageType* Settings, const FFoliageUISettings* UISettings, const TArray<FFoliageInstance>& PlacedInstances, bool InRebuildFoliageTree)
{
	SCOPE_CYCLE_COUNTER(STAT_FoliageSpawnInstance);

	TMap<AInstancedFoliageActor*, TArray<const FFoliageInstance*>> PerIFAPlacedInstances;
	const bool bSpawnInCurrentLevel = UISettings && UISettings->GetIsInSpawnInCurrentLevelMode();
	ULevel* CurrentLevel = InWorld->GetCurrentLevel();
	const bool bCreate = true;
	for (const FFoliageInstance& PlacedInstance : PlacedInstances)
	{
		ULevel* LevelHint = bSpawnInCurrentLevel ? CurrentLevel : PlacedInstance.BaseComponent ? PlacedInstance.BaseComponent->GetComponentLevel() : nullptr;
		if (AInstancedFoliageActor* IFA = AInstancedFoliageActor::Get(InWorld, bCreate, LevelHint, PlacedInstance.Location))
		{
			PerIFAPlacedInstances.FindOrAdd(IFA).Add(&PlacedInstance);
		}
	}

	for (const auto& PlacedLevelInstances : PerIFAPlacedInstances)
	{
		AInstancedFoliageActor* IFA = PlacedLevelInstances.Key;

		CurrentFoliageTraceBrushAffectedIFAs.Add(IFA);

		FFoliageInfo* Info = nullptr;
		UFoliageType* FoliageSettings = IFA->AddFoliageType(Settings, &Info);

		Info->AddInstances(FoliageSettings, PlacedLevelInstances.Value);
		if (InRebuildFoliageTree)
		{
			Info->Refresh(true, false);
		}
	}
}

bool FEdModeFoliage::AddInstancesImp(UWorld* InWorld, const UFoliageType* Settings, const TArray<FDesiredFoliageInstance>& DesiredInstances, const TArray<int32>& ExistingInstanceBuckets, const float Pressure, LandscapeLayerCacheData* LandscapeLayerCachesPtr, const FFoliageUISettings* UISettings, const FFoliagePaintingGeometryFilter* OverrideGeometryFilter, bool InRebuildFoliageTree)
{
	SCOPE_CYCLE_COUNTER(STAT_FoliageAddInstanceImp);

	if (DesiredInstances.Num() == 0)
	{
		return false;
	}

	TArray<FPotentialInstance> PotentialInstanceBuckets[NUM_INSTANCE_BUCKETS];
	if (DesiredInstances[0].PlacementMode == EFoliagePlacementMode::Manual)
	{
		CalculatePotentialInstances(InWorld, Settings, DesiredInstances, PotentialInstanceBuckets, LandscapeLayerCachesPtr, UISettings, OverrideGeometryFilter);
	}
	else
	{
		//@TODO: actual threaded part coming, need parts of this refactor sooner for content team
		CalculatePotentialInstances_ThreadSafe(InWorld, Settings, &DesiredInstances, PotentialInstanceBuckets, nullptr, 0, DesiredInstances.Num() - 1, OverrideGeometryFilter);

		// Existing foliage types in the palette  we want to override any existing mesh settings with the procedural settings.
		TMap<AInstancedFoliageActor*, TArray<const UFoliageType*>> UpdatedTypesByIFA;
		ULevel* CurrentLevel = InWorld->GetCurrentLevel();
		for (TArray<FPotentialInstance>& Bucket : PotentialInstanceBuckets)
		{
			for (auto& PotentialInst : Bucket)
			{
				FFoliageInstance Inst;
				PotentialInst.PlaceInstance(InWorld, Settings, Inst, true);

				ULevel* LevelHint = PotentialInst.HitComponent ? PotentialInst.HitComponent->GetComponentLevel() : CurrentLevel;
				check(LevelHint);
				AInstancedFoliageActor* TargetIFA = AInstancedFoliageActor::Get(InWorld, true, LevelHint, Inst.Location);

				// Update the type in the IFA if needed
				TArray<const UFoliageType*>& UpdatedTypes = UpdatedTypesByIFA.FindOrAdd(TargetIFA);
				if (!UpdatedTypes.Contains(PotentialInst.DesiredInstance.FoliageType))
				{
					UpdatedTypes.Add(PotentialInst.DesiredInstance.FoliageType);
					TargetIFA->AddFoliageType(PotentialInst.DesiredInstance.FoliageType);
				}
			}
		}
	}

	bool bPlacedInstances = false;

	for (int32 BucketIdx = 0; BucketIdx < NUM_INSTANCE_BUCKETS; BucketIdx++)
	{
		TArray<FPotentialInstance>& PotentialInstances = PotentialInstanceBuckets[BucketIdx];
		float BucketFraction = (float)(BucketIdx + 1) / (float)NUM_INSTANCE_BUCKETS;

		// We use the number that actually succeeded in placement (due to parameters) as the target
		// for the number that should be in the brush region.
		const int32 BucketOffset = (ExistingInstanceBuckets.Num() ? ExistingInstanceBuckets[BucketIdx] : 0);
		int32 AdditionalInstances = FMath::Clamp<int32>(FMath::RoundToInt(BucketFraction * (float)(PotentialInstances.Num() - BucketOffset) * Pressure), 0, PotentialInstances.Num());

		{
			SCOPE_CYCLE_COUNTER(STAT_FoliageSpawnInstance);

			TArray<FFoliageInstance> PlacedInstances;
			PlacedInstances.Reserve(AdditionalInstances);

			for (int32 Idx = 0; Idx < AdditionalInstances; Idx++)
			{
				FPotentialInstance& PotentialInstance = PotentialInstances[Idx];
				FFoliageInstance Inst;
				if (PotentialInstance.PlaceInstance(InWorld, Settings, Inst))
				{
					Inst.ProceduralGuid = PotentialInstance.DesiredInstance.ProceduralGuid;
					Inst.BaseComponent = PotentialInstance.HitComponent;
					PlacedInstances.Add(MoveTemp(Inst));
					bPlacedInstances = true;
				}
			}

			SpawnFoliageInstance(InWorld, Settings, UISettings, PlacedInstances, InRebuildFoliageTree);
		}
	}

	return bPlacedInstances;
}

static bool IsWithinSlopeAngle(float NormalZ, float MinAngle, float MaxAngle, float Tolerance = SMALL_NUMBER)
{
	const float MaxNormalAngle = FMath::Cos(FMath::DegreesToRadians(MaxAngle));
	const float MinNormalAngle = FMath::Cos(FMath::DegreesToRadians(MinAngle));
	return !(MaxNormalAngle > (NormalZ + Tolerance) || MinNormalAngle < (NormalZ - Tolerance));
}

bool IsLandscapeLayersArrayValid(const TArray<FName>& LandscapeLayersArray)
{
	bool bValid = false;
	for (FName LayerName : LandscapeLayersArray)
	{
		bValid |= LayerName != NAME_None;
	}

	return bValid;
}

bool GetMaxHitWeight(const FVector& Location, UActorComponent* Component, const TArray<FName>& LandscapeLayersArray, FEdModeFoliage::LandscapeLayerCacheData* LandscapeLayerCaches, float& OutMaxHitWeight)
{
	float MaxHitWeight = 0.f;
	if (ULandscapeHeightfieldCollisionComponent* HitLandscapeCollision = Cast<ULandscapeHeightfieldCollisionComponent>(Component))
	{
		if (ULandscapeComponent* HitLandscape = HitLandscapeCollision->RenderComponent.Get())
		{
			for (const FName& LandscapeLayerName : LandscapeLayersArray)
			{
				// Cache store mapping between component and weight data
				TMap<ULandscapeComponent*, TArray<uint8> >* LandscapeLayerCache = &LandscapeLayerCaches->FindOrAdd(LandscapeLayerName);;
				TArray<uint8>* LayerCache = &LandscapeLayerCache->FindOrAdd(HitLandscape);
				// TODO: FName to LayerInfo?
				const float HitWeight = HitLandscape->GetLayerWeightAtLocation(Location, HitLandscape->GetLandscapeInfo()->GetLayerInfoByName(LandscapeLayerName), LayerCache);
				MaxHitWeight = FMath::Max(MaxHitWeight, HitWeight);
			}

			OutMaxHitWeight = MaxHitWeight;
			return true;
		}
	}

	return false;
}

bool IsFilteredByWeight(float Weight, float TestValue, bool bExclusionTest = false)
{
	if (bExclusionTest)
	{
		// Exclusion always tests 
		const float WeightNeeded = FMath::Max(SMALL_NUMBER, TestValue);
		return Weight >= WeightNeeded;
	}
	else
	{
		const float WeightNeeded = FMath::Max(SMALL_NUMBER, FMath::Max(TestValue, FMath::FRand()));
		return Weight < WeightNeeded;
	}
}

bool LandscapeLayerCheck(const FHitResult& Hit, const UFoliageType* Settings, FEdModeFoliage::LandscapeLayerCacheData& LandscapeLayersCache, float& OutHitWeight)
{
	OutHitWeight = 1.f;
	if (IsLandscapeLayersArrayValid(Settings->LandscapeLayers) && GetMaxHitWeight(Hit.ImpactPoint, Hit.Component.Get(), Settings->LandscapeLayers, &LandscapeLayersCache, OutHitWeight))
	{
		// Reject instance randomly in proportion to weight
		if (IsFilteredByWeight(OutHitWeight, Settings->MinimumLayerWeight))
		{
			return false;
		}
	}

	float HitWeightExclusion = 1.f;
	if (IsLandscapeLayersArrayValid(Settings->ExclusionLandscapeLayers) && GetMaxHitWeight(Hit.ImpactPoint, Hit.Component.Get(), Settings->ExclusionLandscapeLayers, &LandscapeLayersCache, HitWeightExclusion))
	{
		// Reject instance randomly in proportion to weight
		const bool bExclusionTest = true;
		if (IsFilteredByWeight(HitWeightExclusion, Settings->MinimumExclusionLayerWeight, bExclusionTest))
		{
			return false;
		}
	}

	return true;
}

/** This does not check for overlaps or density */
static bool CheckLocationForPotentialInstance_ThreadSafe(const UFoliageType* Settings, const FVector& Location, const FVector& Normal)
{
	// Check height range
	if (!Settings->Height.Contains(Location.Z))
	{
		return false;
	}

	// Check slope
	// ImpactNormal sometimes is slightly non-normalized, so compare slope with some little deviation
	return IsWithinSlopeAngle(Normal.Z, Settings->GroundSlopeAngle.Min, Settings->GroundSlopeAngle.Max, SMALL_NUMBER);
}

static bool CheckForOverlappingSphere(AInstancedFoliageActor* IFA, const UFoliageType* Settings, const FSphere& Sphere)
{
	if (IFA)
	{
		FFoliageInfo* Info = IFA->FindInfo(Settings);
		if (Info)
		{
			return Info->CheckForOverlappingSphere(Sphere);
		}
	}

	return false;
}

// Returns whether or not there is are any instances overlapping the sphere specified
static bool CheckForOverlappingSphere(UWorld* InWorld, const UFoliageType* Settings, const FSphere& Sphere)
{
	bool bIsOverlappingSphere = false;
	auto CheckForOverlap = [&bIsOverlappingSphere, &Sphere](AInstancedFoliageActor* IFA, FFoliageInfo* FoliageInfo, const UFoliageType* FoliageType) {
		bIsOverlappingSphere = FoliageInfo->CheckForOverlappingSphere(Sphere);
		return !bIsOverlappingSphere;
	};

	FEdModeFoliage::ForEachFoliageInfo(InWorld, Settings, Sphere, CheckForOverlap);
	return bIsOverlappingSphere;
}

static bool CheckLocationForPotentialInstance(UWorld* InWorld, const UFoliageType* Settings, const bool bSingleInstanceMode, const FVector& Location, const FVector& Normal, TArray<FVector>& PotentialInstanceLocations, FFoliageInstanceHash& PotentialInstanceHash)
{
	if (CheckLocationForPotentialInstance_ThreadSafe(Settings, Location, Normal) == false)
	{
		return false;
	}

	const float SettingsRadius = Settings->GetRadius(bSingleInstanceMode);

	// Check if we're too close to any other instances
	if (SettingsRadius > 0.f)
	{
		// Check existing instances. Use the Density radius rather than the minimum radius
		if (CheckForOverlappingSphere(InWorld, Settings, FSphere(Location, SettingsRadius)))
		{
			return false;
		}

		// Check with other potential instances we're about to add.
		const float RadiusSquared = FMath::Square(SettingsRadius);
		auto TempInstances = PotentialInstanceHash.GetInstancesOverlappingBox(FBox::BuildAABB(Location, FVector(SettingsRadius)));
		for (int32 Idx : TempInstances)
		{
			if ((PotentialInstanceLocations[Idx] - Location).SizeSquared() < RadiusSquared)
			{
				return false;
			}
		}
	}

	int32 PotentialIdx = PotentialInstanceLocations.Add(Location);
	PotentialInstanceHash.InsertInstance(Location, PotentialIdx);

	return true;
}

void FEdModeFoliage::CalculatePotentialInstances_ThreadSafe(UWorld* InWorld, const UFoliageType* Settings, const TArray<FDesiredFoliageInstance>* DesiredInstances, TArray<FPotentialInstance> OutPotentialInstances[NUM_INSTANCE_BUCKETS], const FFoliageUISettings* UISettings, const int32 StartIdx, const int32 LastIdx, const FFoliagePaintingGeometryFilter* OverrideGeometryFilter)
{
	LandscapeLayerCacheData LocalCache;

	// Reserve space in buckets for a potential instances 
	for (int32 BucketIdx = 0; BucketIdx < NUM_INSTANCE_BUCKETS; ++BucketIdx)
	{
		auto& Bucket = OutPotentialInstances[BucketIdx];
		Bucket.Reserve(DesiredInstances->Num());
	}

	for (int32 InstanceIdx = StartIdx; InstanceIdx <= LastIdx; ++InstanceIdx)
	{
		const FDesiredFoliageInstance& DesiredInst = (*DesiredInstances)[InstanceIdx];
		FHitResult Hit;
		static FName NAME_AddFoliageInstances = FName(TEXT("AddFoliageInstances"));

		FFoliageTraceFilterFunc TraceFilterFunc;
		if (DesiredInst.PlacementMode == EFoliagePlacementMode::Manual && UISettings != nullptr)
		{
			// Enable geometry filters when painting foliage manually
			TraceFilterFunc = FFoliagePaintingGeometryFilter(*UISettings);
		}

		if (OverrideGeometryFilter)
		{
			TraceFilterFunc = *OverrideGeometryFilter;
		}

		if (AInstancedFoliageActor::FoliageTrace(InWorld, Hit, DesiredInst, NAME_AddFoliageInstances, /* bReturnFaceIndex */ true, TraceFilterFunc, /*bAverageNormal*/ true))
		{
			float HitWeight = 1.f;
			const bool bValidInstance = CheckLocationForPotentialInstance_ThreadSafe(Settings, Hit.ImpactPoint, Hit.ImpactNormal)
				&& VertexMaskCheck(Hit, Settings)
				&& LandscapeLayerCheck(Hit, Settings, LocalCache, HitWeight);

			if (bValidInstance)
			{
				const int32 BucketIndex = FMath::RoundToInt(HitWeight * (float)(NUM_INSTANCE_BUCKETS - 1));
				OutPotentialInstances[BucketIndex].Add(FPotentialInstance(Hit.ImpactPoint, Hit.ImpactNormal, Hit.Component.Get(), HitWeight, DesiredInst));
			}
		}
	}
}

void FEdModeFoliage::CalculatePotentialInstances(UWorld* InWorld, const UFoliageType* Settings, const TArray<FDesiredFoliageInstance>& DesiredInstances, TArray<FPotentialInstance> OutPotentialInstances[NUM_INSTANCE_BUCKETS], LandscapeLayerCacheData* LandscapeLayerCachesPtr, const FFoliageUISettings* UISettings, const FFoliagePaintingGeometryFilter* OverrideGeometryFilter)
{
	SCOPE_CYCLE_COUNTER(STAT_FoliageCalculatePotentialInstance);

	LandscapeLayerCacheData LocalCache;
	LandscapeLayerCachesPtr = LandscapeLayerCachesPtr ? LandscapeLayerCachesPtr : &LocalCache;

	// Quick lookup of potential instance locations, used for overlapping check.
	TArray<FVector> PotentialInstanceLocations;
	FFoliageInstanceHash PotentialInstanceHash(7);	// use 128x128 cell size, things like brush radius are typically small
	PotentialInstanceLocations.Empty(DesiredInstances.Num());

	// Reserve space in buckets for a potential instances 
	for (int32 BucketIdx = 0; BucketIdx < NUM_INSTANCE_BUCKETS; ++BucketIdx)
	{
		auto& Bucket = OutPotentialInstances[BucketIdx];
		Bucket.Reserve(DesiredInstances.Num());
	}

	const bool bSingleIntanceMode = UISettings ? UISettings->IsInAnySingleInstantiationMode() : false;
	for (const FDesiredFoliageInstance& DesiredInst : DesiredInstances)
	{
		FFoliageTraceFilterFunc TraceFilterFunc;
		if (DesiredInst.PlacementMode == EFoliagePlacementMode::Manual && UISettings != nullptr)
		{
			// Enable geometry filters when painting foliage manually
			TraceFilterFunc = FFoliagePaintingGeometryFilter(*UISettings);
		}

		if (OverrideGeometryFilter)
		{
			TraceFilterFunc = *OverrideGeometryFilter;
		}

		FHitResult Hit;
		static FName NAME_AddFoliageInstances = FName(TEXT("AddFoliageInstances"));
		if (AInstancedFoliageActor::FoliageTrace(InWorld, Hit, DesiredInst, NAME_AddFoliageInstances, /* bReturnFaceIndex */ true, TraceFilterFunc, /*bAverageNormal*/ true))
		{
			float HitWeight = 1.f;

			UPrimitiveComponent* InstanceBase = Hit.GetComponent();

			if (InstanceBase == nullptr)
			{
				continue;
			}

			ULevel* TargetLevel = InstanceBase->GetComponentLevel();
			// We can paint into new level only if FoliageType is shared
			if (!CanPaint(Settings, TargetLevel))
			{
				continue;
			}

			const bool bValidInstance = CheckLocationForPotentialInstance(InWorld, Settings, bSingleIntanceMode, Hit.ImpactPoint, Hit.ImpactNormal, PotentialInstanceLocations, PotentialInstanceHash)
				&& VertexMaskCheck(Hit, Settings)
				&& LandscapeLayerCheck(Hit, Settings, LocalCache, HitWeight);
			if (bValidInstance)
			{
				const int32 BucketIndex = FMath::RoundToInt(HitWeight * (float)(NUM_INSTANCE_BUCKETS - 1));
				OutPotentialInstances[BucketIndex].Add(FPotentialInstance(Hit.ImpactPoint, Hit.ImpactNormal, InstanceBase, HitWeight, DesiredInst));
			}
		}
	}
}

bool FEdModeFoliage::CanPaint(const ULevel* InLevel)
{
	for (const auto& MeshUIPtr : FoliageMeshList)
	{
		if (MeshUIPtr->Settings->IsSelected && CanPaint(MeshUIPtr->Settings, InLevel))
		{
			return true;
		}
	}

	return false;
}

bool FEdModeFoliage::CanPaint(const UFoliageType* FoliageType, const ULevel* InLevel)
{
	if (FoliageType == nullptr)	//if asset has already been deleted we can't paint
	{
		return false;
	}

	// Non-shared objects can be painted only into their own level
	// Assets can be painted everywhere
	if (FoliageType->IsAsset() || FoliageType->GetOutermost() == InLevel->GetOutermost())
	{
		return true;
	}

	return false;
}

static bool CheckVertexColor(const UFoliageType* Settings, const FColor& VertexColor)
{
	for (uint8 ChannelIdx = 0; ChannelIdx < (uint8)EVertexColorMaskChannel::MAX_None; ++ChannelIdx)
	{
		const FFoliageVertexColorChannelMask& Mask = Settings->VertexColorMaskByChannel[ChannelIdx];

		if (Mask.UseMask)
		{
			uint8 ColorChannel = 0;
			switch ((EVertexColorMaskChannel)ChannelIdx)
			{
			case EVertexColorMaskChannel::Red:
				ColorChannel = VertexColor.R;
				break;
			case EVertexColorMaskChannel::Green:
				ColorChannel = VertexColor.G;
				break;
			case EVertexColorMaskChannel::Blue:
				ColorChannel = VertexColor.B;
				break;
			case EVertexColorMaskChannel::Alpha:
				ColorChannel = VertexColor.A;
				break;
			default:
				// Invalid channel value
				continue;
			}

			if (Mask.InvertMask)
			{
				if (ColorChannel > FMath::RoundToInt(Mask.MaskThreshold * 255.f))
				{
					return false;
				}
			}
			else
			{
				if (ColorChannel < FMath::RoundToInt(Mask.MaskThreshold * 255.f))
				{
					return false;
				}
			}
		}
	}

	return true;
}

void FEdModeFoliage::ForEachFoliageInfo(UWorld* InWorld, const UFoliageType* FoliageType, const FSphere& BrushSphere, TFunctionRef<bool(AInstancedFoliageActor* IFA, FFoliageInfo* FoliageInfo, const UFoliageType* FoliageType)> InOperation)
{
	auto IFAOperation = [&FoliageType, &InOperation](APartitionActor* Actor)
	{
		AInstancedFoliageActor* IFA = Cast<AInstancedFoliageActor>(Actor);
		FFoliageInfo* FoliageInfo = IFA ? IFA->FindInfo(FoliageType) : nullptr;
		if (FoliageInfo)
		{
			return InOperation(IFA, FoliageInfo, FoliageType);
		}
		return true;
	};

	if (UActorPartitionSubsystem* ActorPartitionSubsystem = InWorld->GetSubsystem<UActorPartitionSubsystem>())
	{
		const FBox BrushSphereBounds(BrushSphere.Center - BrushSphere.W, BrushSphere.Center + BrushSphere.W);
		ActorPartitionSubsystem->ForEachRelevantActor(AInstancedFoliageActor::StaticClass(), BrushSphereBounds, IFAOperation);
	}
}

bool FEdModeFoliage::VertexMaskCheck(const FHitResult& Hit, const UFoliageType* Settings)
{
	if (Hit.FaceIndex != INDEX_NONE && IsUsingVertexColorMask(Settings))
	{
		if (UStaticMeshComponent* HitStaticMeshComponent = Cast<UStaticMeshComponent>(Hit.Component.Get()))
		{
			FColor VertexColor;
			if (FEdModeFoliage::GetStaticMeshVertexColorForHit(HitStaticMeshComponent, Hit.FaceIndex, Hit.ImpactPoint, VertexColor))
			{
				if (!CheckVertexColor(Settings, VertexColor))
				{
					return false;
				}
			}
		}
	}

	return true;
}

//
// Painting filtering options
//
bool FFoliagePaintingGeometryFilter::operator() (const UPrimitiveComponent* Component) const
{
	if (Component)
	{
		bool bFoliageOwned = Component->GetOwner() && FFoliageHelper::IsOwnedByFoliage(Component->GetOwner());

		// allow list
		bool bAllowed =
			(bAllowLandscape && Component->IsA(ULandscapeHeightfieldCollisionComponent::StaticClass())) ||
			(bAllowStaticMesh && Component->IsA(UStaticMeshComponent::StaticClass()) && !Component->IsA(UFoliageInstancedStaticMeshComponent::StaticClass()) && !bFoliageOwned) ||
			(bAllowBSP && (Component->IsA(UBrushComponent::StaticClass()) || Component->IsA(UModelComponent::StaticClass()))) ||
			(bAllowFoliage && (Component->IsA(UFoliageInstancedStaticMeshComponent::StaticClass()) || bFoliageOwned));

		// deny list
		bAllowed &=
			(bAllowTranslucent || !(Component->GetMaterial(0) && IsTranslucentBlendMode(Component->GetMaterial(0)->GetBlendMode())));

		return bAllowed;
	}

	return false;
}

bool FEdModeFoliage::GetStaticMeshVertexColorForHit(const UStaticMeshComponent* InStaticMeshComponent, int32 InTriangleIndex, const FVector& InHitLocation, FColor& OutVertexColor)
{
	const UStaticMesh* StaticMesh = InStaticMeshComponent->GetStaticMesh();

	if (StaticMesh == nullptr || StaticMesh->GetRenderData() == nullptr)
	{
		return false;
	}

	const FStaticMeshLODResources& LODModel = StaticMesh->GetRenderData()->LODResources[0];
	bool bHasInstancedColorData = false;
	const FStaticMeshComponentLODInfo* InstanceMeshLODInfo = nullptr;
	if (InStaticMeshComponent->LODData.Num() > 0)
	{
		InstanceMeshLODInfo = InStaticMeshComponent->LODData.GetData();
		bHasInstancedColorData = InstanceMeshLODInfo->PaintedVertices.Num() == LODModel.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices();
	}

	const FColorVertexBuffer& ColorVertexBuffer = LODModel.VertexBuffers.ColorVertexBuffer;

	// no vertex color data
	if (!bHasInstancedColorData && ColorVertexBuffer.GetNumVertices() == 0)
	{
		return false;
	}

	// Get the raw triangle data for this static mesh
	FIndexArrayView Indices = LODModel.IndexBuffer.GetArrayView();
	const FPositionVertexBuffer& PositionVertexBuffer = LODModel.VertexBuffers.PositionVertexBuffer;

	int32 SectionFirstTriIndex = 0;
	for (const FStaticMeshSection& Section : LODModel.Sections)
	{
		if (Section.bEnableCollision)
		{
			int32 NextSectionTriIndex = SectionFirstTriIndex + Section.NumTriangles;
			if (InTriangleIndex >= SectionFirstTriIndex && InTriangleIndex < NextSectionTriIndex)
			{

				int32 IndexBufferIdx = (InTriangleIndex - SectionFirstTriIndex) * 3 + Section.FirstIndex;

				// Look up the triangle vertex indices
				int32 Index0 = Indices[IndexBufferIdx];
				int32 Index1 = Indices[IndexBufferIdx + 1];
				int32 Index2 = Indices[IndexBufferIdx + 2];

				// Lookup the triangle world positions and colors.
				FVector WorldVert0 = InStaticMeshComponent->GetComponentTransform().TransformPosition((FVector)PositionVertexBuffer.VertexPosition(Index0));
				FVector WorldVert1 = InStaticMeshComponent->GetComponentTransform().TransformPosition((FVector)PositionVertexBuffer.VertexPosition(Index1));
				FVector WorldVert2 = InStaticMeshComponent->GetComponentTransform().TransformPosition((FVector)PositionVertexBuffer.VertexPosition(Index2));

				FLinearColor Color0;
				FLinearColor Color1;
				FLinearColor Color2;

				if (bHasInstancedColorData)
				{
					Color0 = InstanceMeshLODInfo->PaintedVertices[Index0].Color.ReinterpretAsLinear();
					Color1 = InstanceMeshLODInfo->PaintedVertices[Index1].Color.ReinterpretAsLinear();
					Color2 = InstanceMeshLODInfo->PaintedVertices[Index2].Color.ReinterpretAsLinear();
				}
				else
				{
					Color0 = ColorVertexBuffer.VertexColor(Index0).ReinterpretAsLinear();
					Color1 = ColorVertexBuffer.VertexColor(Index1).ReinterpretAsLinear();
					Color2 = ColorVertexBuffer.VertexColor(Index2).ReinterpretAsLinear();
				}

				// find the barycentric coordiantes of the hit location, so we can interpolate the vertex colors
				FVector BaryCoords = FMath::GetBaryCentric2D(InHitLocation, WorldVert0, WorldVert1, WorldVert2);

				FLinearColor InterpColor = BaryCoords.X * Color0 + BaryCoords.Y * Color1 + BaryCoords.Z * Color2;

				// convert back to FColor.
				OutVertexColor = InterpColor.ToFColor(false);

				return true;
			}

			SectionFirstTriIndex = NextSectionTriIndex;
		}
	}

	return false;
}

bool FEdModeFoliage::IsUsingVertexColorMask(const UFoliageType* Settings)
{
	for (uint8 ChannelIdx = 0; ChannelIdx < (uint8)EVertexColorMaskChannel::MAX_None; ++ChannelIdx)
	{
		const FFoliageVertexColorChannelMask& Mask = Settings->VertexColorMaskByChannel[ChannelIdx];
		if (Mask.UseMask)
		{
			return true;
		}
	}

	return false;
}


#undef LOCTEXT_NAMESPACE