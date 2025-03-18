// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "HeightmapModifier/HeightmapModifier.h"
#include "HeightmapModifier/LogHeightmapModifier.h"
#include "LCReporter/LCReporter.h"

#include "LandscapeUtils/LandscapeUtils.h"
#include "GDALInterface/GDALInterface.h"

#include "Kismet/GameplayStatics.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeDataAccess.h"
#include "Curves/RichCurve.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Misc/MessageDialog.h"


#define LOCTEXT_NAMESPACE "FHeightmapModifierModule"

UHeightmapModifier::UHeightmapModifier()
{
	ExternalTool = CreateDefaultSubobject<UExternalTool>(TEXT("External Tool"));
}

#if WITH_EDITOR

void UHeightmapModifier::ApplyToolToHeightmap()
{
	ALandscape *Landscape = Cast<ALandscape>(GetOwner());
	if (!Landscape)
	{
		ULCReporter::ShowError(
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::1", "The owner of HeightmapModifier must be a landscape.")
		); 
		return;
	}

	FString LandscapeLabel = Landscape->GetActorNameOrLabel();

	if (Landscape->IsMaxLayersReached())
	{
		ULCReporter::ShowError(
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::Layers", "Landscape {0} has too many edit layers, please delete one.")
		); 
		return;
	}

	if (!BoundingActor)
	{
		ULCReporter::ShowError(
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::2", "Please select a bounding actor.")
		); 
		return;
	}

	
	/* Get the heightmap data from `Landscape` using `BoundingActor` as bounds */

	FTransform GlobalToThis = Landscape->GetTransform().Inverse();

	FVector Origin, Extent;
	BoundingActor->GetActorBounds(false, Origin, Extent);

	int Top = Origin.Y - Extent.Y;
	int Bottom = Origin.Y + Extent.Y;
	int Left = Origin.X - Extent.X;
	int Right = Origin.X + Extent.X;

	FVector TopLeft(Left, Top, 0);
	FVector BottomRight(Right, Bottom, 0);
	
	FVector LocalTopLeft = GlobalToThis.TransformPosition(TopLeft);
	FVector LocalBottomRight = GlobalToThis.TransformPosition(BottomRight);
	
	int32 TotalSizeX = Landscape->ComputeComponentCounts().X * Landscape->ComponentSizeQuads + 1;
	int32 TotalSizeY = Landscape->ComputeComponentCounts().Y * Landscape->ComponentSizeQuads + 1;

	int32 X1 = FMath::Min(TotalSizeX - 1, FMath::Max(0, LocalTopLeft.X));
	int32 X2 = FMath::Min(TotalSizeX - 1, FMath::Max(0, LocalBottomRight.X));
	int32 Y1 = FMath::Min(TotalSizeY - 1, FMath::Max(0, LocalTopLeft.Y));
	int32 Y2 = FMath::Min(TotalSizeY - 1, FMath::Max(0, LocalBottomRight.Y));
	int32 SizeX = X2 - X1 + 1;
	int32 SizeY = Y2 - Y1 + 1;

	if (SizeX > INT32_MAX / SizeY)
	{
		ULCReporter::ShowError(
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::28", "The size of the area to edit is too large.")
		);
		return;
	}
	
	uint16* HeightmapData = (uint16*) malloc(SizeX * SizeY * (sizeof (uint16)));
	if (!HeightmapData)
	{
		ULCReporter::ShowError(
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::29", "Not enough memory to allocate for new heightmap data.")
		);
		return;
	}
	
	ULandscapeInfo *LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		ULCReporter::ShowError(FText::Format(
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::3", "Could not get LandscapeInfo for Landscape {0}."),
			FText::FromString(LandscapeLabel)
		)); 

		free(HeightmapData);
		return;
	}

	FHeightmapAccessor<false> HeightmapAccessor(LandscapeInfo);
	HeightmapAccessor.GetDataFast(X1, Y1, X2, Y2, HeightmapData);


	/* Prepare the directories */
	
	FString Intermediate = FPaths::ConvertRelativePathToFull(FPaths::EngineIntermediateDir());
	FString TempDir = FPaths::Combine(Intermediate, "Temp");
	if (!IPlatformFile::GetPlatformPhysical().CreateDirectory(*TempDir))
	{
		ULCReporter::ShowError(FText::Format(
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::4", "Could not initialize directory {0}."),
			FText::FromString(TempDir)
		));

		free(HeightmapData);
		return;
	}


	/* Prepare the filenames */

	FString InputFile = FPaths::Combine(TempDir, "input.tif");
	FString Extension = ExternalTool->bChangeExtension ? ExternalTool->NewExtension : "tif";
	FString OutputFile = FPaths::Combine(TempDir, "output." + Extension);

	if (!GDALInterface::WriteHeightmapDataToTIF(InputFile, SizeX, SizeY, HeightmapData))
	{
		free(HeightmapData);
		return;
	}

	/* Run the External Tool from `InputFile` to `OutputFile` */

	if (!ExternalTool->Run(InputFile, OutputFile))
	{
		free(HeightmapData);
		return;
	}

	/* Read the new data from `OutputFile` */

	GDALDataset *NewDataset = (GDALDataset *)GDALOpen(TCHAR_TO_UTF8(*OutputFile), GA_ReadOnly);

	if (!NewDataset)
	{
		ULCReporter::ShowError(FText::Format(
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::6", "Could not read file {0} using GDAL."),
			FText::FromString(OutputFile)
		));
		free(HeightmapData);
		return;
	}
	
	uint16* NewHeightmapData = (uint16*) malloc(SizeX * SizeY * (sizeof (uint16)));

	if (!NewHeightmapData)
	{
		ULCReporter::ShowError(FText::Format(
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::7", "Not enough memory to allocate for new heightmap data."),
			FText::FromString(OutputFile)
		));
		GDALClose(NewDataset);
		free(HeightmapData);
		return;
	}

	CPLErr ReadErr = NewDataset->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, SizeX, SizeY, NewHeightmapData, SizeX, SizeY, GDT_UInt16, 0, 0);
	GDALClose(NewDataset);

	if (ReadErr != CE_None)
	{
		ULCReporter::ShowError(FText::Format(
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::7", "There was an error while reading heightmap data from file {0}."),
			FText::FromString(OutputFile)
		));
		free(NewHeightmapData);
		free(HeightmapData);
		return;
	}

	
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)

	/* Make the new data to be a difference, so that it can be used on a different edit layer (>= 5.3 only) */

	LandscapeUtils::MakeDataRelativeTo(SizeX, SizeY, NewHeightmapData, HeightmapData);
	free(HeightmapData);

	/* Write difference data to a new edit layer (>= 5.3 only) */
	
	int LayerIndex = Landscape->CreateLayer();
	if (LayerIndex == INDEX_NONE)
	{
		ULCReporter::ShowError(FText::Format(
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::9", "Could not create landscape layer. Make sure that edit layers are enabled on Landscape {0}."),
			FText::FromString(LandscapeLabel)
		));
		free(NewHeightmapData);
		return;
	}

	HeightmapAccessor.SetEditLayer(Landscape->GetLayer(LayerIndex)->Guid);
#endif

	HeightmapAccessor.SetData(X1, Y1, X2, Y2, NewHeightmapData);
	free(NewHeightmapData);
	
	ULCReporter::ShowError(FText::Format(
		LOCTEXT("UHeightmapModifier::ModifyHeightmap::10", "Finished applying command {0} on the Landscape {1}."),
		FText::FromString(ExternalTool->Command),
		FText::FromString(LandscapeLabel)
	));
}

#endif

#undef LOCTEXT_NAMESPACE