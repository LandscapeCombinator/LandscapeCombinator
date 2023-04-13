// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorObject.h"
#include "LandscapeDataAccess.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "UObject/ConstructorHelpers.h"
#include "LandscapeEditorModule.h"
#include "Editor/LandscapeEditor/Private/LandscapeEditorPrivate.h"
#include "LandscapeRender.h"
#include "LandscapeSettings.h"
#include "LandscapeImportHelper.h"
#include "LandscapeMaterialInstanceConstant.h"
#include "Misc/ConfigCacheIni.h"
#include "EngineUtils.h"
#include "RenderUtils.h"
#include "Misc/MessageDialog.h"

static TAutoConsoleVariable<bool> CVarLandscapeSimulateAlphaBrushTextureLoadFailure(
	TEXT("landscape.SimulateAlphaBrushTextureLoadFailure"),
	false,
	TEXT("Debug utility to simulate a loading failure (e.g. invalid source data, which can happen in cooked editor or with a badly virtualized texture) when loading the alpha brush texture"));

// Region
void ULandscapeEditorObject::SetbUseSelectedRegion(bool InbUseSelectedRegion)
{ 
	bUseSelectedRegion = InbUseSelectedRegion;
	if (bUseSelectedRegion)
	{
		GLandscapeEditRenderMode |= ELandscapeEditRenderMode::Mask;
	}
	else
	{
		GLandscapeEditRenderMode &= ~(ELandscapeEditRenderMode::Mask);
	}
}
void ULandscapeEditorObject::SetbUseNegativeMask(bool InbUseNegativeMask) 
{ 
	bUseNegativeMask = InbUseNegativeMask;
	if (bUseNegativeMask)
	{
		GLandscapeEditRenderMode |= ELandscapeEditRenderMode::InvertedMask;
	}
	else
	{
		GLandscapeEditRenderMode &= ~(ELandscapeEditRenderMode::InvertedMask);
	}
}

void ULandscapeEditorObject::SetPasteMode(ELandscapeToolPasteMode InPasteMode)
{
	PasteMode = InPasteMode;
}

void ULandscapeEditorObject::SetGizmoSnapMode(ELandscapeGizmoSnapType InSnapMode)
{
	SnapMode = InSnapMode;

	if (ParentMode->CurrentGizmoActor.IsValid())
	{
		ParentMode->CurrentGizmoActor->SnapType = SnapMode;
	}

	if (SnapMode != ELandscapeGizmoSnapType::None)
	{
		if (ParentMode->CurrentGizmoActor.IsValid())
		{
			check(ParentMode->CurrentGizmoActor->TargetLandscapeInfo);

			const FVector WidgetLocation = ParentMode->CurrentGizmoActor->GetActorLocation();
			const FRotator WidgetRotation = ParentMode->CurrentGizmoActor->GetActorRotation();

			const FVector SnappedWidgetLocation = ParentMode->CurrentGizmoActor->SnapToLandscapeGrid(WidgetLocation);
			const FRotator SnappedWidgetRotation = ParentMode->CurrentGizmoActor->SnapToLandscapeGrid(WidgetRotation);

			ParentMode->CurrentGizmoActor->SetActorLocation(SnappedWidgetLocation, false);
			ParentMode->CurrentGizmoActor->SetActorRotation(SnappedWidgetRotation);
		}
	}
}

bool ULandscapeEditorObject::HasValidAlphaTextureData() const
{
	return ((AlphaTextureSizeX > 0) && (AlphaTextureSizeY > 0) && !AlphaTextureData.IsEmpty());
}

void ULandscapeEditorObject::ChooseBestComponentSizeForImport()
{
	FLandscapeImportHelper::ChooseBestComponentSizeForImport(ImportLandscape_Width, ImportLandscape_Height, NewLandscape_QuadsPerSection, NewLandscape_SectionsPerComponent, NewLandscape_ComponentCount);
}

bool ULandscapeEditorObject::UseSingleFileImport() const
{
	if (ParentMode)
	{
		return ParentMode->UseSingleFileImport();
	}

	return true;
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

void ULandscapeEditorObject::OnChangeImportLandscapeResolution(int32 DescriptorIndex)
{
	check(DescriptorIndex >= 0 && DescriptorIndex < HeightmapImportDescriptor.ImportResolutions.Num());
	HeightmapImportDescriptorIndex = DescriptorIndex;
	ImportLandscape_Width = HeightmapImportDescriptor.ImportResolutions[HeightmapImportDescriptorIndex].Width;
	ImportLandscape_Height = HeightmapImportDescriptor.ImportResolutions[HeightmapImportDescriptorIndex].Height;
	ClearImportLandscapeData();
	ImportLandscapeData();
	ChooseBestComponentSizeForImport();
}

void ULandscapeEditorObject::ImportLandscapeData()
{
	ImportLandscape_HeightmapImportResult = FLandscapeImportHelper::GetHeightmapImportData(HeightmapImportDescriptor, HeightmapImportDescriptorIndex, ImportLandscape_Data, ImportLandscape_HeightmapErrorMessage);
	if (ImportLandscape_HeightmapImportResult == ELandscapeImportResult::Error)
	{
		ImportLandscape_Data.Empty();
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

void ULandscapeEditorObject::UpdateComponentLayerAllowList()
{
	if (ParentMode->CurrentToolTarget.LandscapeInfo.IsValid())
	{
		ParentMode->CurrentToolTarget.LandscapeInfo->UpdateComponentLayerAllowList();
	}
}


float ULandscapeEditorObject::GetCurrentToolStrength() const
{
	if (IsWeightmapTarget())
	{
		return PaintToolStrength;
	}
	return ToolStrength;
}

void ULandscapeEditorObject::SetCurrentToolStrength(float NewToolStrength)
{
	if (IsWeightmapTarget())
	{
		PaintToolStrength = NewToolStrength;		
	}
	else
	{
		ToolStrength = NewToolStrength;
	}
}

float ULandscapeEditorObject::GetCurrentToolBrushRadius() const
{
	if (IsWeightmapTarget())
	{
		return PaintBrushRadius;
	}
	return BrushRadius;
	
	
}

void ULandscapeEditorObject::SetCurrentToolBrushRadius(float NewBrushStrength)
{
	if (IsWeightmapTarget())
	{
		PaintBrushRadius = NewBrushStrength;
	}
	else
	{
		BrushRadius = NewBrushStrength;
	}
}

float ULandscapeEditorObject::GetCurrentToolBrushFalloff() const
{
	if (IsWeightmapTarget())
	{
		return PaintBrushFalloff;
		
	}
	return BrushFalloff;
	
}

void ULandscapeEditorObject::SetCurrentToolBrushFalloff(float NewBrushFalloff)
{
	if (IsWeightmapTarget())
	{
		PaintBrushFalloff = NewBrushFalloff;
	}
	else
	{
		BrushFalloff = NewBrushFalloff;
	}
}
