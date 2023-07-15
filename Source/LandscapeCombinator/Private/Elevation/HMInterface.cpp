// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "Elevation/HMInterface.h"
#include "Elevation/LandscapeUtils.h"
#include "Utils/Logging.h"
#include "Utils/Console.h"
#include "Utils/Concurrency.h"
#include "Utils/GDALUtils.h"

#include "Misc/Paths.h"
#include "Internationalization/Regex.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeEditorObject.h"
#include "Async/Async.h"
#include "Editor/LandscapeEditor/Private/LandscapeEditorDetailCustomization_NewLandscape.h"
#include "EditorModes.h"
#include "EditorModeManager.h" 
#include "LandscapeEditorObject.h"
#include "LandscapeSettings.h" 
#include "LandscapeSubsystem.h"
#include "LandscapeDataAccess.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

using namespace GDALUtils;

HMInterface::HMInterface(FString LandscapeLabel0, const FText &KindText0, FString Descr0, int Precision0) :
	LandscapeLabel(LandscapeLabel0), KindText(KindText0), Descr(Descr0), Precision(Precision0)
{
	Landscape = nullptr;
}

void HMInterface::CouldNotCreateDirectory(FString Dir)
{
	FMessageDialog::Open(EAppMsgType::Ok,
		FText::Format(
			LOCTEXT("DirectoryError", "Could not create directory '{0}'."),
			FText::FromString(Dir)
		)
	);
}

bool HMInterface::Initialize()
{	
	if (Descr.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("HMInterface::Initialize", "Please add coordinate details to Landscape '{0}'."),
				FText::FromString(LandscapeLabel)
			)
		);
		return false;
	}

	FString Intermediate = FPaths::ConvertRelativePathToFull(FPaths::EngineIntermediateDir());
	LandscapeCombinatorDir = FPaths::Combine(Intermediate, "LandscapeCombinator");
	if (!IPlatformFile::GetPlatformPhysical().CreateDirectory(*LandscapeCombinatorDir))
	{
		CouldNotCreateDirectory(LandscapeCombinatorDir);
		return false;
	}

	ResultDir = FPaths::Combine(LandscapeCombinatorDir, FString::Format(TEXT("{0}-{1}"), { LandscapeLabel, Precision }));
	if (!IPlatformFile::GetPlatformPhysical().CreateDirectory(*ResultDir))
	{
		CouldNotCreateDirectory(ResultDir);
		return false;
	}

	FString DownloadDir = FPaths::Combine(LandscapeCombinatorDir, "Download");
	if (!IPlatformFile::GetPlatformPhysical().CreateDirectory(*DownloadDir))
	{
		CouldNotCreateDirectory(DownloadDir);
		return false;
	}

	InterfaceDir = FPaths::Combine(DownloadDir, KindText.ToString());
	if (!IPlatformFile::GetPlatformPhysical().CreateDirectory(*InterfaceDir))
	{
		CouldNotCreateDirectory(InterfaceDir);
		return false;
	}

	return true;
}

bool HMInterface::GetCoordinates(FVector4d &Coordinates) const {
	double MinCoordWidth = DBL_MAX;
	double MaxCoordWidth = -DBL_MAX;
	double MinCoordHeight = DBL_MAX;
	double MaxCoordHeight = -DBL_MAX;

	for (auto& File : OriginalFiles) {
		GDALDataset *Dataset = (GDALDataset *)GDALOpen(TCHAR_TO_UTF8(*File), GA_ReadOnly);
		if (!Dataset)
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("GetCoordinatesError", "Could not open heightmap file '{0}' to read the coordinates."),
					FText::FromString(File)
				)
			);
			return false;
		}

		FVector4d FileCoordinates;
		if (!GDALUtils::GetCoordinates(FileCoordinates, Dataset))
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("GetCoordinatesError", "Could not read coordinates from heightmap file '{0}'."),
					FText::FromString(File)
				)
			);
			return false;
		}
		MinCoordWidth  = FMath::Min(MinCoordWidth, FileCoordinates[0]);
		MaxCoordWidth  = FMath::Max(MaxCoordWidth, FileCoordinates[1]);
		MinCoordHeight = FMath::Min(MinCoordHeight, FileCoordinates[2]);
		MaxCoordHeight = FMath::Max(MaxCoordHeight, FileCoordinates[3]);
	}
	
	Coordinates[0] = MinCoordWidth;
	Coordinates[1] = MaxCoordWidth;
	Coordinates[2] = MinCoordHeight;
	Coordinates[3] = MaxCoordHeight;

	return true;
}

bool HMInterface::GetInsidePixels(FIntPoint &InsidePixels) const
{
	GDALDataset *Dataset = (GDALDataset *)GDALOpen(TCHAR_TO_UTF8(*(OriginalFiles[0])), GA_ReadOnly);

	if (!Dataset) {
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(LOCTEXT("GetInsidePixelsError", "Could not open heightmap file '{0}' to read the size."),
				FText::FromString(OriginalFiles[0])
			)
		);
		return false;
	}

	InsidePixels[0] = Dataset->GetRasterXSize();
	InsidePixels[1] = Dataset->GetRasterYSize();
	GDALClose(Dataset);
	return true;
}

bool HMInterface::GetMinMax(FVector2D &MinMax) const {
	return GDALUtils::GetMinMax(MinMax, OriginalFiles);
}

FReply HMInterface::ConvertHeightMaps(TFunction<void(bool)> OnComplete) const
{
	//FMessageDialog::Open(EAppMsgType::Ok,
	//	FText::Format(
	//		LOCTEXT("ConvertingMessage",
	//			"Heightmaps for {0} will now be converted to PNG format in the folder:\n{1}\n\n"
	//			"You can monitor the conversions in the Output Log in the LogLandscapeCombinator category. "
	//			"If there are no errors, you can then go to a level with World Partition enabled to import the PNG files manually in Landscape Mode.\n\n"
	//			"After you have imported your landscape, please rename it to {0}, and click the `Adjust` button to change its position."
	//		),
	//		FText::FromString(LandscapeLabel),
	//		FText::FromString(ResultDir)
	//	)
	//);

	if (!IPlatformFile::GetPlatformPhysical().CreateDirectory(*ResultDir))
	{
		CouldNotCreateDirectory(ResultDir);
		return FReply::Unhandled();
	}

	
	Concurrency::RunOne([this](){

		FVector2D Altitudes;
		// We use HMInterface::GetMinMax instead of the overridden GetMinMax to access
		// min/max as seen by gdalinfo, rather than the real-world min/max altitudes.
		// Useful when the original files are PNG files rather than GeoTIFF.
		if (!HMInterface::GetMinMax(Altitudes)) return false;

		double MinAltitude = Altitudes[0];
		double MaxAltitude = Altitudes[1];

		for (int32 i = 0; i < OriginalFiles.Num(); i++) {
			FString OriginalFile = OriginalFiles[i];
			FString ScaledFile = ScaledFiles[i];

			if (!GDALUtils::Translate(OriginalFile, ScaledFile, MinAltitude, MaxAltitude, Precision))
			{
				return false;
			}
		}
		return true;
	},
		OnComplete
	);


	return FReply::Handled();
}

FReply HMInterface::ImportHeightMaps()
{
	ALandscape *CreatedLandscape = LandscapeUtils::SpawnLandscape(ScaledFiles, LandscapeLabel);

	if (CreatedLandscape)
	{
		CreatedLandscape->SetActorLabel(LandscapeLabel);
		SetLandscape(CreatedLandscape);
		UE_LOG(LogLandscapeCombinator, Log, TEXT("Created Landscape %s successfully."), *LandscapeLabel);
		return FReply::Handled();
	}
	else
	{
		UE_LOG(LogLandscapeCombinator, Error, TEXT("Could not create Landscape %s."), *LandscapeLabel);
		return FReply::Unhandled();
	}
	
}

FReply HMInterface::CreateLandscape(int WorldWidthKm, int WorldHeightKm, double ZScale, double WorldOriginX, double WorldOriginY, TFunction<void(bool)> OnComplete)
{
	if (GetLandscapeFromLabel())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("CreateLandscapeError", "There already exists a landscape labeled {0} in your level."),
				FText::FromString(LandscapeLabel)
			)
		);

		if (OnComplete) OnComplete(false);

		return FReply::Handled();
	}

	EAppReturnType::Type UserResponse = FMessageDialog::Open(EAppMsgType::OkCancel,
		FText::Format(
			LOCTEXT("CreateLandscapeMessage",
				"Landscape {0} will now be created. Heightmaps need to be downloaded and converted to PNG. "
				"You can monitor the progress in the Output Log in the LogLandscapeCombinator category.\n\n"
				"Do not forget to add a material to your landscape and paint layers once it is imported."
			),
			FText::FromString(LandscapeLabel)
		)
	);

	if (UserResponse == EAppReturnType::Cancel) 
	{
		UE_LOG(LogLandscapeCombinator, Log, TEXT("User cancelled landscape creation."));
		if (OnComplete) OnComplete(false);

		return FReply::Handled();
	}

	DownloadHeightMaps([this, OnComplete, WorldWidthKm, WorldHeightKm, ZScale, WorldOriginX, WorldOriginY](bool bWasSuccessfulDownload)
	{
		if (!bWasSuccessfulDownload)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("DownloadError", "There was an error while downloading heightmap files for Landscape {0}."),
				FText::FromString(LandscapeLabel)
			));
			if (OnComplete) OnComplete(false);
			return;
		}
		ConvertHeightMaps([this, OnComplete, WorldWidthKm, WorldHeightKm, ZScale, WorldOriginX, WorldOriginY](bool bWasSuccessfulConvert)
		{
			if (!bWasSuccessfulConvert)
			{
				FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
					LOCTEXT("ConvertError", "There was an error while converting heightmap files for Landscape {0}."),
					FText::FromString(LandscapeLabel)
				));
				if (OnComplete) OnComplete(false);
				return;
			}

			// importing Landscapes only works in GameThread
			AsyncTask(ENamedThreads::GameThread, [=]()
			{
				if (ImportHeightMaps().IsEventHandled())
				{
					bool bAdjustSuccess = AdjustLandscape(WorldWidthKm, WorldHeightKm, ZScale, WorldOriginX, WorldOriginY).IsEventHandled();
					if (OnComplete) OnComplete(bAdjustSuccess);
					return;
				}
				else
				{
					FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
						LOCTEXT("ImportError", "There was an error while importing heightmap files for Landscape {0}."),
						FText::FromString(LandscapeLabel)
					));
					if (OnComplete) OnComplete(false);
					return;
				}
			});
		});
	});

	return FReply::Handled();
}

FReply HMInterface::DownloadHeightMaps(TFunction<void(bool)> OnComplete)
{
	//FMessageDialog::Open(EAppMsgType::Ok,
	//	FText::Format(
	//		LOCTEXT("DownloadingMessage",
	//			"Heightmaps for {0} will now start downloading.\n\n"
	//			"You can monitor the downloads in the Output Log in the LogLandscapeCombinator category. "
	//			"If there are no error, you may proceed with the `Convert` button to convert (and scale) the heightmaps to PNG files that Unreal Engine can import."
	//		),
	//		FText::FromString(LandscapeLabel)
	//	)
	//);
	return DownloadHeightMapsImpl(OnComplete);
}

bool HMInterface::GetMinMaxZ(FVector2D &MinMaxZ)
{
	FVector2D UnusedMinMaxX, UnusedMinMaxY;
	return LandscapeUtils::GetLandscapeBounds(Landscape, LandscapeStreamingProxies, UnusedMinMaxX, UnusedMinMaxY, MinMaxZ);
}

ALandscape* HMInterface::GetLandscapeFromLabel()
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	TArray<AActor*> Landscapes;
	UGameplayStatics::GetAllActorsOfClass(World, ALandscape::StaticClass(), Landscapes);

	for (auto& Landscape0 : Landscapes)
	{
		if (Landscape0->GetActorLabel().Equals(LandscapeLabel))
		{
			return Cast<ALandscape>(Landscape0); 
		}
	}

	return NULL;
}

bool HMInterface::SetLandscapeFromLabel()
{
	Landscape = NULL;
	LandscapeStreamingProxies.Empty();
	ALandscape* Landscape0 = GetLandscapeFromLabel();
	if (Landscape0)
	{
		Landscape = Landscape0;
		SetLandscapeStreamingProxies();
		return true;
	}
	else
	{
		return false;
	}
}

void HMInterface::SetLandscape(ALandscape *Landscape0)
{
	Landscape = Landscape0;
	SetLandscapeStreamingProxies();
}

void HMInterface::SetLandscapeStreamingProxies()
{
	LandscapeStreamingProxies = LandscapeUtils::GetLandscapeStreamingProxies(Landscape);
}

FReply HMInterface::AdjustLandscape(int WorldWidthKm, int WorldHeightKm, double ZScale, double WorldOriginX, double WorldOriginY)
{
	if (!SetLandscapeFromLabel())
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("LandscapeNotFoundError", "Could not find Landscape {0}"),
			FText::FromString(LandscapeLabel)
		));
		return FReply::Unhandled();
	}

	double WorldWidthCm  = ((double) WorldWidthKm)  * 1000 * 100;
	double WorldHeightCm = ((double) WorldHeightKm) * 1000 * 100;

	FVector4d Coordinates;	
	if (!GetCoordinates4326(Coordinates)) return FReply::Unhandled();
	
	double MinCoordWidth = Coordinates[0];
	double MaxCoordWidth = Coordinates[1];
	double MinCoordHeight = Coordinates[2];
	double MaxCoordHeight = Coordinates[3];

	FVector2D Altitudes;
	if (!GetMinMax(Altitudes)) return FReply::Unhandled();
	double MinAltitude = Altitudes[0];
	double MaxAltitude = Altitudes[1];
	
	FIntPoint InsidePixels;
	if (!GetInsidePixels(InsidePixels)) return FReply::Unhandled();
	int InsidePixelWidth = InsidePixels[0];
	int InsidePixelHeight = InsidePixels[1];

	double CoordWidth = MaxCoordWidth - MinCoordWidth;
	double CoordHeight = MaxCoordHeight - MinCoordHeight;
	
	double CmPxWidthRatio = (CoordWidth / 360) * WorldWidthCm / InsidePixelWidth;     // cm / px
	double CmPxHeightRatio = (CoordHeight / 180) * WorldHeightCm / InsidePixelHeight; // cm / px

	FVector OldScale = Landscape->GetActorScale3D();
	FVector OldLocation = Landscape->GetActorLocation();
	UE_LOG(LogLandscapeCombinator, Log, TEXT("I found the landscape %s. Adjusting scale and position."), *LandscapeLabel);
	
	UE_LOG(LogLandscapeCombinator, Log, TEXT("WorldWidthCm: %f"), WorldWidthCm);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("WorldHeightCm: %f"), WorldHeightCm);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("\nMinCoordWidth(ESPG4326): %f"), MinCoordWidth);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("MaxCoordWidth(ESPG4326): %f"), MaxCoordWidth);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("MinCoordHeight(ESPG4326): %f"), MinCoordHeight);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("MaxCoordHeight(ESPG4326): %f"), MaxCoordHeight);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("\nCoordWidth: %f"), CoordWidth);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("CoordHeight: %f"), CoordHeight);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("InsidePixelsWidth: %d"), InsidePixelWidth);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("InsidePixelsHeight: %d"), InsidePixelHeight);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("CmPxWidthRatio: %f cm/px"), CmPxWidthRatio);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("CmPxHeightRatio: %f cm/px"), CmPxHeightRatio);
		
	double OutsidePixelWidth  = Landscape->ComputeComponentCounts().X * Landscape->ComponentSizeQuads + 1;
	double OutsidePixelHeight = Landscape->ComputeComponentCounts().Y * Landscape->ComponentSizeQuads + 1;
	UE_LOG(LogLandscapeCombinator, Log, TEXT("OutsidePixelWidth: %f"), OutsidePixelWidth);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("OutsidePixelHeight: %f"), OutsidePixelHeight);

	FVector2D MinMaxZBeforeScaling;
	if (!GetMinMaxZ(MinMaxZBeforeScaling)) return FReply::Unhandled();
	double MinZBeforeScaling = MinMaxZBeforeScaling.X;
	double MaxZBeforeScaling = MinMaxZBeforeScaling.Y;

	UE_LOG(LogLandscapeCombinator, Log, TEXT("\nMinZBeforeScaling: %f"), MinZBeforeScaling);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("MaxZBeforeScaling: %f"), MaxZBeforeScaling);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("MaxAltitude: %f"), MaxAltitude);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("MinAltitude: %f"), MinAltitude);

	FVector NewScale = FVector(
		WorldWidthCm  * CoordWidth  / 360 / InsidePixelWidth,
		WorldHeightCm * CoordHeight / 180 / InsidePixelHeight,
		OldScale.Z * (MaxAltitude - MinAltitude) * ZScale * 100 / (MaxZBeforeScaling - MinZBeforeScaling)
	);

	Landscape->Modify();
	Landscape->SetActorScale3D(NewScale);
	Landscape->PostEditChange();
			
	FVector2D MinMaxZAfterScaling;
	if (!GetMinMaxZ(MinMaxZAfterScaling)) return FReply::Unhandled();
	double MinZAfterScaling = MinMaxZAfterScaling.X;
	double MaxZAfterScaling = MinMaxZAfterScaling.Y;
			
	double InsideLocationX = MinCoordWidth * WorldWidthCm / 360;
	double InsideLocationY = - MaxCoordHeight * WorldHeightCm / 180;
			
	double NewLocationX = InsideLocationX - (OutsidePixelWidth - InsidePixelWidth)   * CmPxWidthRatio / 2  - WorldOriginX * WorldWidthCm / 360;
	double NewLocationY = InsideLocationY - (OutsidePixelHeight - InsidePixelHeight) * CmPxHeightRatio / 2 + WorldOriginY * WorldHeightCm / 180;
	double NewLocationZ = OldLocation.Z - MaxZAfterScaling + 100 * MaxAltitude * ZScale;
	FVector NewLocation = FVector(NewLocationX, NewLocationY, NewLocationZ);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("\nMinZAfterScaling: %f"), MinZAfterScaling);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("MaxZAfterScaling: %f"), MaxZAfterScaling);

	UE_LOG(LogLandscapeCombinator, Log, TEXT("InsideLocationX: %f"), InsideLocationX);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("InsideLocationY: %f"), InsideLocationY);

	UE_LOG(LogLandscapeCombinator, Log, TEXT("OldLocation.X: %f"), OldLocation.X);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("OldLocation.Y: %f"), OldLocation.Y);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("OldLocation.Z: %f"), OldLocation.Z);

	UE_LOG(LogLandscapeCombinator, Log, TEXT("NewLocationX: %f"), NewLocationX);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("NewLocationY: %f"), NewLocationY);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("NewLocationZ: %f"), NewLocationZ);

	Landscape->Modify();
	Landscape->SetActorLocation(NewLocation);
	Landscape->PostEditChange();
	
	return FReply::Handled();
}

FReply HMInterface::DigOtherLandscapes(int WorldWidthKm, int WorldHeightKm, double ZScale, double WorldOriginX, double WorldOriginY)
{
	if (!SetLandscapeFromLabel())
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("LandscapeNotFoundError", "Could not find Landscape {0}"),
			FText::FromString(LandscapeLabel)
		));
		return FReply::Unhandled();
	}

	FGlobalTabmanager::Get()->TryInvokeTab(FTabId("LevelEditor"));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	TArray<AActor*> OtherLandscapes;
	UGameplayStatics::GetAllActorsOfClass(World, ALandscape::StaticClass(), OtherLandscapes);

	FVector2d MinMaxX, MinMaxY, UnusedMinMaxZ;
	if (!LandscapeUtils::GetLandscapeBounds(Landscape, LandscapeStreamingProxies, MinMaxX, MinMaxY, UnusedMinMaxZ))
	{
		return FReply::Unhandled();
	}

	double ExtentX = MinMaxX[1] - MinMaxX[0];
	double ExtentY = MinMaxY[1] - MinMaxY[0];
	
	FHeightmapAccessor<false> HeightmapAccessor(Landscape->GetLandscapeInfo());

	int32 SizeX = Landscape->ComputeComponentCounts().X * Landscape->ComponentSizeQuads + 1;
	int32 SizeY = Landscape->ComputeComponentCounts().Y * Landscape->ComponentSizeQuads + 1;
	uint16* HeightmapData = (uint16*) malloc(SizeX * SizeY * (sizeof uint16));
	HeightmapAccessor.GetDataFast(0, 0, SizeX - 1, SizeY - 1, HeightmapData);


	FVector GlobalTopLeft = FVector(MinMaxX[0], MinMaxY[0], 0);
	FVector GlobalBottomRight = FVector(MinMaxX[1], MinMaxY[1], 0);

	FTransform ThisToGlobal = Landscape->GetTransform();
	FTransform GlobalToThis = ThisToGlobal.Inverse();

	const FScopedTransaction Transaction(LOCTEXT("DigOtherLandscapes", "Digging Other Landscapes"));

	for (auto& OtherLandscape0 : OtherLandscapes)
	{
		ALandscape *OtherLandscape = Cast<ALandscape>(OtherLandscape0);
		if (OtherLandscape == Landscape) continue;

		TArray<ALandscapeStreamingProxy*> OtherLandscapeStreamingProxies = LandscapeUtils::GetLandscapeStreamingProxies(OtherLandscape);

		FVector2d OtherMinMaxX, OtherMinMaxY, UnusedOtherMinMaxZ;
		if (!LandscapeUtils::GetLandscapeBounds(OtherLandscape, OtherLandscapeStreamingProxies, OtherMinMaxX, OtherMinMaxY, UnusedOtherMinMaxZ))
		{
			return FReply::Unhandled();
		}

		if (
			MinMaxX[0] <= OtherMinMaxX[1] && OtherMinMaxX[0] <= MinMaxX[1] &&
			MinMaxY[0] <= OtherMinMaxY[1] && OtherMinMaxY[0] <= MinMaxY[1]
		)
		{
			FTransform OtherToGlobal = OtherLandscape->GetTransform();
			FTransform GlobalToOther = OtherToGlobal.Inverse();
			FVector OtherTopLeft = GlobalToOther.TransformPosition(GlobalTopLeft);
			FVector OtherBottomRight = GlobalToOther.TransformPosition(GlobalBottomRight);
			
			FHeightmapAccessor<false> OtherHeightmapAccessor(OtherLandscape->GetLandscapeInfo());

			// We are only interested in the heightmap data from `OtherLandscape` in the rectangle delimited by
			// the `TopLeft` and `BottomRight` corners 
			
			int32 OtherTotalSizeX = OtherLandscape->ComputeComponentCounts().X * OtherLandscape->ComponentSizeQuads + 1;
			int32 OtherTotalSizeY = OtherLandscape->ComputeComponentCounts().Y * OtherLandscape->ComponentSizeQuads + 1;
			int32 OtherX1 = FMath::Min(OtherTotalSizeX - 1, FMath::Max(0, OtherTopLeft.X + 2));
			int32 OtherX2 = FMath::Min(OtherTotalSizeX - 1, FMath::Max(0, OtherBottomRight.X - 2));
			int32 OtherY1 = FMath::Min(OtherTotalSizeY - 1, FMath::Max(0, OtherTopLeft.Y + 2));
			int32 OtherY2 = FMath::Min(OtherTotalSizeY - 1, FMath::Max(0, OtherBottomRight.Y - 2));
			int32 OtherSizeX = OtherX2 - OtherX1 + 1;
			int32 OtherSizeY = OtherY2 - OtherY1 + 1;

			if (OtherSizeX <= 0 || OtherSizeY <= 0)
			{
				UE_LOG(LogLandscapeCombinator, Log, TEXT("Could not dig into Landscape %s"), *OtherLandscape->GetActorLabel());
				continue;
			}

			UE_LOG(LogLandscapeCombinator, Log, TEXT("Digging into Landscape %s (MinX: %d, MaxX: %d, MinY: %d, MaxY: %d)"),
				*OtherLandscape->GetActorLabel(), OtherX1, OtherX2, OtherY1, OtherY2
			);

			uint16* OtherOldHeightmapData = (uint16*) malloc(OtherSizeX * OtherSizeY * (sizeof uint16));
			uint16* OtherNewHeightmapData = (uint16*) malloc(OtherSizeX * OtherSizeY * (sizeof uint16));
			OtherHeightmapAccessor.GetDataFast(OtherX1, OtherY1, OtherX2, OtherY2, OtherOldHeightmapData);

			for (int X = 0; X < OtherSizeX; X++)
			{
				for (int Y = 0; Y < OtherSizeY; Y++)
				{
					FVector OtherPosition = FVector(OtherX1 + X, OtherY1 + Y, 0);
					FVector GlobalPosition = OtherToGlobal.TransformPosition(OtherPosition);
					FVector ThisPosition = GlobalToThis.TransformPosition(GlobalPosition);

					int ThisX = ThisPosition.X;
					int ThisY = ThisPosition.Y;

					// if this landscape has non-zero data at this position
					if (ThisX >= 0 && ThisY >= 0 && ThisX < SizeX && ThisY < SizeY && HeightmapData[ThisX + ThisY * SizeX] > 0)
					{
						// we delete the data from the other landscape
						OtherNewHeightmapData[X + Y * OtherSizeX] = 0;
					}
					else
					{
						// otherwise, we keep the old data
						OtherNewHeightmapData[X + Y * OtherSizeX] = OtherOldHeightmapData[X + Y * OtherSizeX];
					}
					
				}
			}

			OtherHeightmapAccessor.SetData(OtherX1, OtherY1, OtherX2, OtherY2, OtherNewHeightmapData);

			UE_LOG(LogLandscapeCombinator, Log, TEXT("Finished digging into Landscape %s (MinX: %d, MaxX: %d, MinY: %d, MaxY: %d)"),
				*OtherLandscape->GetActorLabel(), OtherX1, OtherX2, OtherY1, OtherY2
			);
		}
		else
		{
			UE_LOG(LogLandscapeCombinator, Log, TEXT("Skipping digging into Landscape %s, which does not overlap with Landscape %s"),
				*OtherLandscape->GetActorLabel(),
				*Landscape->GetActorLabel()
			);
		}

	}

	return FReply::Handled();
}

bool HMInterface::GetCoordinates4326(FVector4d& Coordinates)
{
	FVector4d OriginalCoordinates;	
	if (!GetCoordinates(OriginalCoordinates)) return false;
	
	double MinCoordWidth0 = OriginalCoordinates[0];
	double MaxCoordWidth0 = OriginalCoordinates[1];
	double MinCoordHeight0 = OriginalCoordinates[2];
	double MaxCoordHeight0 = OriginalCoordinates[3];
	double xs[2] = { MinCoordWidth0,  MaxCoordWidth0  };
	double ys[2] = { MaxCoordHeight0, MinCoordHeight0 };
	OGRSpatialReference InRs, OutRs;
	
	if (!GetCoordinatesSpatialReference(InRs) || !GetSpatialReferenceFromEPSG(OutRs, 4326) || !OGRCreateCoordinateTransformation(&InRs, &OutRs)->Transform(2, xs, ys)) {
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("AdjustLandscapeTransformError", "Internal error while transforming coordinates for Landscape {0}."),
			FText::FromString(LandscapeLabel)
		));
		return false;
	}
		
	// coordinates in EPSG 4326
	Coordinates[0] = xs[0];
	Coordinates[1] = xs[1];
	Coordinates[2] = ys[1];
	Coordinates[3] = ys[0];
	return true;
}

FCollisionQueryParams HMInterface::CustomCollisionQueryParams(UWorld* World)
{
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Creating custom collusion query parameters for Landscape %s"), *LandscapeLabel);
	SetLandscapeStreamingProxies();
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), Actors);

	FCollisionQueryParams CollisionQueryParams;
	for (auto &Actor : Actors)
	{
		if (Actor != Landscape && !LandscapeStreamingProxies.Contains(Actor))
		{
			UE_LOG(LogLandscapeCombinator, Log, TEXT("Ignoring actor %s"), *Actor->GetActorLabel());
			CollisionQueryParams.AddIgnoredActor(Actor);
		}
	}
	return CollisionQueryParams;
}

double HMInterface::GetZ(UWorld* World, FCollisionQueryParams CollisionQueryParams, double x, double y)
{
	FVector StartLocation = FVector(x, y, -HALF_WORLD_MAX);
	FVector EndLocation = FVector(x, y, HALF_WORLD_MAX);

	FHitResult HitResult;
	World->LineTraceSingleByObjectType(
		OUT HitResult,
		StartLocation,
		EndLocation,
		FCollisionObjectQueryParams(ECollisionChannel::ECC_WorldStatic),
		CollisionQueryParams
	);

	if (HitResult.GetActor()) return HitResult.ImpactPoint.Z;
	else return 0;
}

#undef LOCTEXT_NAMESPACE