// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "HeightmapModifier/HeightmapModifier.h"
#include "HeightmapModifier/LogHeightmapModifier.h"


#include "LandscapeUtils/LandscapeUtils.h"
#include "GDALInterface/GDALInterface.h"

#include "Kismet/GameplayStatics.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeDataAccess.h"
#include "Curves/RichCurve.h"
#include "Runtime/Launch/Resources/Version.h"


#define LOCTEXT_NAMESPACE "FHeightmapModifierModule"

UHeightmapModifier::UHeightmapModifier()
{
	ExternalTool = CreateDefaultSubobject<UExternalTool>(TEXT("External Tool"));
}

void UHeightmapModifier::ApplyToolToHeightmap()
{
	ALandscape *Landscape = Cast<ALandscape>(GetOwner());
	if (!Landscape)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::1", "The owner of HeightmapModifier must be a landscape.")
		); 
		return;
	}

	FString LandscapeLabel = Landscape->GetActorLabel();

	if (Landscape->IsMaxLayersReached())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::Layers", "Landscape {0} has too many edit layers, please delete one.")
		); 
		return;
	}

	if (!BoundingActor)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
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
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::28", "The size of the area to edit is too large.")
		);
		return;
	}
	
	uint16* HeightmapData = (uint16*) malloc(SizeX * SizeY * (sizeof uint16));
	if (!HeightmapData)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::29", "Not enough memory to allocate for new heightmap data.")
		);
		return;
	}
	
	ULandscapeInfo *LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
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
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
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


	/* Prepare GDAL Drivers */

	GDALDriver *MEMDriver = GetGDALDriverManager()->GetDriverByName("MEM");
	GDALDriver *TIFDriver = GetGDALDriverManager()->GetDriverByName("GTiff");

	if (!TIFDriver || !MEMDriver)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::3", "Could not load GDAL drivers.")
		);
		free(HeightmapData);
		return;
	}


	/* Write the heightmap data to `InputFile` */

	GDALDataset *Dataset = MEMDriver->Create(
		"",
		SizeX, SizeY,
		1, // nBands
		GDT_UInt16,
		nullptr
	);

	if (!Dataset)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::4", "There was an error while creating a GDAL Dataset."),
			FText::FromString(InputFile)
		));
		free(HeightmapData);
		return;
	}

	if (Dataset->GetRasterCount() != 1)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::4", "There was an error while writing heightmap data to file {0}."),
			FText::FromString(InputFile)
		));
		free(HeightmapData);
		GDALClose(Dataset);
		return;
	}


	CPLErr WriteErr = Dataset->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, SizeX, SizeY, HeightmapData, SizeX, SizeY, GDT_UInt16, 0, 0);
	free(HeightmapData);

	if (WriteErr != CE_None)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::5", "There was an error while writing heightmap data to file {0}. (Error: {1})"),
			FText::FromString(InputFile),
			FText::AsNumber(WriteErr, &FNumberFormattingOptions::DefaultNoGrouping())
		));
		GDALClose(Dataset);
		return;
	}

	GDALDataset *TIFDataset = TIFDriver->CreateCopy(TCHAR_TO_UTF8(*InputFile), Dataset, 1, nullptr, nullptr, nullptr);
	GDALClose(Dataset);

	if (!TIFDataset)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::5", "Could not write heightmap to file {0}."),
			FText::FromString(InputFile),
			FText::AsNumber(WriteErr, &FNumberFormattingOptions::DefaultNoGrouping())
		));
		return;
	}
	
	GDALClose(TIFDataset);


	/* Run the External Tool from `InputFile` to `OutputFile` */

	if (!ExternalTool->Run(InputFile, OutputFile))
	{
		return;
	}

	/* Read the new data from `OutputFile` */

	GDALDataset *NewDataset = (GDALDataset *)GDALOpen(TCHAR_TO_UTF8(*OutputFile), GA_ReadOnly);

	if (!NewDataset)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::6", "Could not read file {0} using GDAL."),
			FText::FromString(OutputFile)
		));
		return;
	}
	
	uint16* NewHeightmapData = (uint16*) malloc(SizeX * SizeY * (sizeof uint16));

	if (!NewHeightmapData)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::7", "Not enough memory to allocate for new heightmap data."),
			FText::FromString(OutputFile)
		));
		GDALClose(NewDataset);
		return;
	}

	CPLErr ReadErr = NewDataset->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, SizeX, SizeY, NewHeightmapData, SizeX, SizeY, GDT_UInt16, 0, 0);
	GDALClose(NewDataset);

	if (ReadErr != CE_None)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("UHeightmapModifier::ModifyHeightmap::7", "There was an error while reading heightmap data from file {0}."),
			FText::FromString(OutputFile)
		));
		free(NewHeightmapData);
		return;
	}


	/* Make the new data to be a difference, so that it can be used on a different edit layer */
	
	for (int i = 0; i < SizeX; i++)
	{
		for (int j = 0; j < SizeY; j++)
		{
			NewHeightmapData[i + j * SizeX] -= HeightmapData[i + j * SizeX];
		}
	}


	/* Write difference data to a new edit layer (>= 5.3 only). */
	
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
	int LayerIndex = Landscape->CreateLayer();
	if (LayerIndex == INDEX_NONE)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
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
	
	FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
		LOCTEXT("UHeightmapModifier::ModifyHeightmap::10", "Finished applying command {0} on the Landscape {1}."),
		FText::FromString(ExternalTool->Command),
		FText::FromString(LandscapeLabel)
	));
}

#undef LOCTEXT_NAMESPACE