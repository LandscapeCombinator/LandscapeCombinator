#include "HMInterface.h"
#include "Misc/Paths.h"
#include "Internationalization/Regex.h"
#include "Kismet/GameplayStatics.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"
#include "LandscapeStreamingProxy.h"
#include "Materials/MaterialInstanceDynamic.h"

#pragma warning(disable: 4668)
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#pragma warning(default: 4668)

#include "Logging.h"
#include "Console.h"
#include "Concurrency.h"


#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

HMInterface::HMInterface(FString LandscapeName0, const FText &KindText0, FString Descr0, int Precision0) :
	LandscapeName(LandscapeName0), KindText(KindText0), Descr(Descr0), Precision(Precision0)
{

}

void HMInterface::CouldNotCreateDirectory(FString Dir) {
	FMessageDialog::Open(EAppMsgType::Ok,
		FText::Format(
			LOCTEXT("DirectoryError", "Could not create directory '{0}'."),
			FText::FromString(Dir)
		)
	);
}

bool HMInterface::Initialize()
{	
	if (Descr.IsEmpty()) {
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("HMInterface::Initialize", "Please add coordinate details to Landscape '{0}'."),
				FText::FromString(LandscapeName)
			)
		);
		return false;
	}

	FString Intermediate = FPaths::ConvertRelativePathToFull(FPaths::EngineIntermediateDir());
	LandscapeCombinatorDir = FPaths::Combine(Intermediate, "LandscapeCombinator");
	if (!IPlatformFile::GetPlatformPhysical().CreateDirectory(*LandscapeCombinatorDir)) {
		CouldNotCreateDirectory(LandscapeCombinatorDir);
		return false;
	}

	ResultDir = FPaths::Combine(LandscapeCombinatorDir, FString::Format(TEXT("{0}{1}"), { LandscapeName, Precision }));
	if (!IPlatformFile::GetPlatformPhysical().CreateDirectory(*ResultDir)) {
		CouldNotCreateDirectory(ResultDir);
		return false;
	}

	FString DownloadDir = FPaths::Combine(LandscapeCombinatorDir, "Download");
	if (!IPlatformFile::GetPlatformPhysical().CreateDirectory(*DownloadDir)) {
		CouldNotCreateDirectory(DownloadDir);
		return false;
	}

	InterfaceDir = FPaths::Combine(DownloadDir, KindText.ToString());
	if (!IPlatformFile::GetPlatformPhysical().CreateDirectory(*InterfaceDir)) {
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
		if (!Dataset) {
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("GetCoordinatesError", "Could not open heightmap file '{0}' to read the coordinates."),
					FText::FromString(OriginalFiles[0])
				)
			);
			return false;
		}

		double AdfTransform[6];
		if (Dataset->GetGeoTransform(AdfTransform) != CE_None) {
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("GetCoordinatesError2", "Could not get read coordinates of heightmap file '{0}'."),
					FText::FromString(OriginalFiles[0])
				)
			);
			return false;
		}
		
		double LeftCoord    = AdfTransform[0];
		double RightCoord   = AdfTransform[0] + Dataset->GetRasterXSize()*AdfTransform[1];
		double TopCoord     = AdfTransform[3];
		double BottomCoord  = AdfTransform[3] + Dataset->GetRasterYSize()*AdfTransform[5];

		MinCoordWidth  = FMath::Min(MinCoordWidth, LeftCoord);
		MaxCoordWidth  = FMath::Max(MaxCoordWidth, RightCoord);
		MinCoordHeight = FMath::Min(MinCoordHeight, BottomCoord);
		MaxCoordHeight = FMath::Max(MaxCoordHeight, TopCoord);
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
	MinMax[0] = DBL_MAX;
	MinMax[1] = -DBL_MAX;

	for (auto& File : OriginalFiles) {
		

		GDALDataset *Dataset = (GDALDataset *)GDALOpen(TCHAR_TO_UTF8(*File), GA_ReadOnly);

		if (!Dataset) {
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(LOCTEXT("GetMinMaxError", "Could not open heightmap file '{0}' to compute min/max altitudes. {1}"),
					FText::FromString(File),
					GDALGetDriverByName("SRTMHGT") == NULL
				)
			);
			return false;
		}
		
		double AdfMinMax[2];

		if (GDALComputeRasterMinMax(Dataset->GetRasterBand(1), false, AdfMinMax) != CE_None) {
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(LOCTEXT("GetMinMaxError2", "Could compute min/max altitudes of file '{0}'."),
					FText::FromString(File)
				)
			);
			return false;
		}

		MinMax[0] = FMath::Min(MinMax[0], AdfMinMax[0]);
		MinMax[1] = FMath::Max(MinMax[1], AdfMinMax[1]);
	}

	return true;
}

FReply HMInterface::ConvertHeightMaps() const
{
	FMessageDialog::Open(EAppMsgType::Ok,
		FText::Format(
			LOCTEXT("ConvertingMessage",
				"Heightmaps for {0} will now be converted to PNG format in the folder:\n{1}\n\n"
				"You can monitor the conversions in the Output Log in the LogLandscapeCombinator category. "
				"If there are no errors, you can then go to a level with World Partition enabled to import the PNG files manually in Landscape Mode.\n\n"
				"After you have imported your landscape, please rename it to {0}, and click the `Adjust` button to change its position."
			),
			FText::FromString(LandscapeName),
			FText::FromString(ResultDir)
		)
	);

	if (!IPlatformFile::GetPlatformPhysical().CreateDirectory(*ResultDir)) {
		CouldNotCreateDirectory(ResultDir);
		return FReply::Unhandled();
	}

	FVector2D Altitudes;
	// We use HMInterface::GetMinMax instead of the overridden GetMinMax to access
	// min/max as seen by gdalinfo, rather than the real-world min/max altitudes.
	// Useful when the original files are PNG files rather than GeoTIFF.
	if (!HMInterface::GetMinMax(Altitudes)) return FReply::Unhandled();

	double MinAltitude = Altitudes[0];
	double MaxAltitude = Altitudes[1];

	
	Concurrency::RunAsync([this, MinAltitude, MaxAltitude]() {

		for (int32 i = 0; i < OriginalFiles.Num(); i++) {
			FString OriginalFile = OriginalFiles[i];
			FString ScaledFile = ScaledFiles[i];
			  
			GDALDataset* SrcDataset = (GDALDataset*) GDALOpen(TCHAR_TO_UTF8(*OriginalFile), GA_ReadOnly);

			if (!SrcDataset) {
				FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
					LOCTEXT("ConvertGDALOpenError", "Could not open file {0}."),
					FText::FromString(OriginalFile)
				));
				return;
			}
			
			char** Argv = nullptr;
			Argv = CSLAddString(Argv, "-scale");
			Argv = CSLAddString(Argv, TCHAR_TO_UTF8(*FString::SanitizeFloat(MinAltitude)));
			Argv = CSLAddString(Argv, TCHAR_TO_UTF8(*FString::SanitizeFloat(MaxAltitude)));
			Argv = CSLAddString(Argv, "0");
			Argv = CSLAddString(Argv, "65535");
			Argv = CSLAddString(Argv, "-ot");
			Argv = CSLAddString(Argv, "UInt16");
			Argv = CSLAddString(Argv, "-of");
			Argv = CSLAddString(Argv, "PNG");
			Argv = CSLAddString(Argv, "-outsize");
			Argv = CSLAddString(Argv, TCHAR_TO_UTF8(*FString::Format(TEXT("{0}%"), { Precision })));
			Argv = CSLAddString(Argv, TCHAR_TO_UTF8(*FString::Format(TEXT("{0}%"), { Precision })));
			Argv = CSLAddString(Argv, TCHAR_TO_UTF8(*OriginalFile));
			Argv = CSLAddString(Argv, TCHAR_TO_UTF8(*ScaledFile));

			CPLSetConfigOption("GDAL_PAM_ENABLED", "NO");
			GDALTranslateOptions* Options = GDALTranslateOptionsNew(Argv, NULL);
			CSLDestroy(Argv);

			if (!Options) {
				FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
					LOCTEXT("ConvertParseOptionsError", "Internal GDAL error while parsing GDALTranslate options for file {0}."),
					FText::FromString(OriginalFile)
				));
				GDALClose(SrcDataset);
				return;
			}

			UE_LOG(LogLandscapeCombinator, Log, TEXT("Translating using gdal_translate --config GDAL_PAM_ENABLED NO -scale %s %s 0 65535 -ot UInt16 -of PNG -outsize %d%% %d%% \"%s\" \"%s\""),
				*FString::SanitizeFloat(MinAltitude),
				*FString::SanitizeFloat(MaxAltitude),
				Precision, Precision,
				*OriginalFile,
				*ScaledFile
			);

			GDALDataset* DstDataset = (GDALDataset *) GDALTranslate(TCHAR_TO_UTF8(*ScaledFile), SrcDataset, Options, nullptr);
			GDALClose(SrcDataset);
			GDALTranslateOptionsFree(Options);


			if (!DstDataset) {
				UE_LOG(LogLandscapeCombinator, Error, TEXT("Error while translating: %s to %s"), *OriginalFile, *ScaledFile);
				FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
					LOCTEXT("ConvertGDALTranslateError", "Internal GDALTranslate error while converting dataset from file {0} to PNG."),
					FText::FromString(OriginalFile)
				));
				return;
			}
		
			UE_LOG(LogLandscapeCombinator, Log, TEXT("Finished translating: %s to %s"), *OriginalFile, *ScaledFile);
			GDALClose(DstDataset);
		}
		return;
	});


	return FReply::Handled();
}

bool HMInterface::GetSpatialReference(OGRSpatialReference &InRs, FString File) {

	GDALDataset* Dataset = (GDALDataset *) GDALOpen(TCHAR_TO_UTF8(*File), GA_ReadOnly);
	if (!Dataset) {
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("GetSpatialReferenceError", "Unable to open file {0} to read its spatial reference."),
			FText::FromString(File)
		));
		return false;
	}

    const char* ProjectionRef = Dataset->GetProjectionRef();
    if (InRs.importFromWkt(ProjectionRef) != OGRERR_NONE)
    {
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("GetSpatialReferenceError2", "Unable to get spatial reference for file {0}."),
			FText::FromString(File)
		));
		return false;
    }

	return true;
}

FReply HMInterface::DownloadHeightMaps() const {
	FMessageDialog::Open(EAppMsgType::Ok,
		FText::Format(
			LOCTEXT("DownloadingMessage",
				"Heightmaps for {0} will now start downloading.\n\n"
				"You can monitor the downloads in the Output Log in the LogLandscapeCombinator category. "
				"If there are no error, you may proceed with the `Convert` button to convert (and scale) the heightmaps to PNG files that Unreal Engine can import."
			),
			FText::FromString(LandscapeName)
		)
	);
	return DownloadHeightMapsImpl();
}


FVector2D GetMinMaxZ(TArray<AActor*> LandscapeStreamingProxies, AActor *Landscape) {
	double MaxZ = -DBL_MAX;
	double MinZ = DBL_MAX;

	for (auto& LandscapeStreamingProxy0 : LandscapeStreamingProxies) {
		ALandscapeStreamingProxy* LandscapeStreamingProxy = Cast<ALandscapeStreamingProxy>(LandscapeStreamingProxy0);
		if (LandscapeStreamingProxy->GetLandscapeActor() == Landscape) {
			FVector LandscapeProxyOrigin;
			FVector LandscapeProxyExtent;
			LandscapeStreamingProxy->GetActorBounds(false, LandscapeProxyOrigin, LandscapeProxyExtent);
			MaxZ = FMath::Max(MaxZ, LandscapeProxyOrigin.Z + LandscapeProxyExtent.Z);
			MinZ = FMath::Min(MinZ, LandscapeProxyOrigin.Z - LandscapeProxyExtent.Z);
		}
	}

	return { MinZ, MaxZ };
}

FReply HMInterface::AdjustLandscape(int WorldWidthKm, int WorldHeightKm) const
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	TArray<AActor*> Landscapes;
	TArray<AActor*> LandscapeStreamingProxies;
	UGameplayStatics::GetAllActorsOfClass(World, ALandscape::StaticClass(), Landscapes);
	UGameplayStatics::GetAllActorsOfClass(World, ALandscapeStreamingProxy::StaticClass(), LandscapeStreamingProxies);

	double WorldWidthCm  = ((double) WorldWidthKm) * 1000 * 100;
	double WorldHeightCm = ((double) WorldHeightKm) * 1000 * 100;
	
	// coordinates in the original reference system
	FVector4d Coordinates;
	if (!GetCoordinates(Coordinates)) return FReply::Unhandled();

	double MinCoordWidth0 = Coordinates[0];
	double MaxCoordWidth0 = Coordinates[1];
	double MinCoordHeight0 = Coordinates[2];
	double MaxCoordHeight0 = Coordinates[3];
	double xs[2] = { MinCoordWidth0,  MaxCoordWidth0  };
	double ys[2] = { MaxCoordHeight0, MinCoordHeight0 };
	OGRSpatialReference InRs, OutRs;
	GetSpatialReference(InRs);
	OutRs.importFromEPSG(4326);
	OutRs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

	if (!OGRCreateCoordinateTransformation(&InRs, &OutRs)->Transform(2, xs, ys)) {
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("AjustLandscapeTransformError", "Internal error while transforming coordinates for Landscape {0}."),
			FText::FromString(LandscapeName)
		));
		return FReply::Unhandled();
	}

	// coordinates in EPSG 4326
	double MinCoordWidth = xs[0];
	double MaxCoordWidth = xs[1];
	double MaxCoordHeight = ys[0];
	double MinCoordHeight = ys[1];
	
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

	for (auto& Landscape0 : Landscapes) {
		ALandscape* Landscape = Cast<ALandscape>(Landscape0);
		FString Name = Landscape->GetActorLabel();
		FVector OldScale = Landscape->GetActorScale3D();
		FVector OldLocation = Landscape->GetActorLocation();

		if (Name.Equals(LandscapeName)) {
			UE_LOG(LogLandscapeCombinator, Log, TEXT("I found the landscape %s. Adjusting scale and position."), *LandscapeName);
	
			UE_LOG(LogLandscapeCombinator, Log, TEXT("WorldWidthCm: %f"), WorldWidthCm);
			UE_LOG(LogLandscapeCombinator, Log, TEXT("WorldHeightCm: %f"), WorldHeightCm);
			UE_LOG(LogLandscapeCombinator, Log, TEXT("\nMinCoordWidth0: %f"), MinCoordWidth0);
			UE_LOG(LogLandscapeCombinator, Log, TEXT("MaxCoordWidth0: %f"), MaxCoordWidth0);
			UE_LOG(LogLandscapeCombinator, Log, TEXT("MinCoordHeight0: %f"), MinCoordHeight0);
			UE_LOG(LogLandscapeCombinator, Log, TEXT("MaxCoordHeight0: %f"), MaxCoordHeight0);
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

			FVector2D MinMaxZBeforeScaling = GetMinMaxZ(LandscapeStreamingProxies, Landscape);
			double MinZBeforeScaling = MinMaxZBeforeScaling.X;
			double MaxZBeforeScaling = MinMaxZBeforeScaling.Y;

			UE_LOG(LogLandscapeCombinator, Log, TEXT("\nMinZBeforeScaling: %f"), MinZBeforeScaling);
			UE_LOG(LogLandscapeCombinator, Log, TEXT("MaxZBeforeScaling: %f"), MaxZBeforeScaling);
			UE_LOG(LogLandscapeCombinator, Log, TEXT("MaxAltitude: %f"), MaxAltitude);
			UE_LOG(LogLandscapeCombinator, Log, TEXT("MinAltitude: %f"), MinAltitude);

			FVector NewScale = FVector(
				WorldWidthCm  * CoordWidth  / 360 / InsidePixelWidth,
				WorldHeightCm * CoordHeight / 180 / InsidePixelHeight,
				OldScale.Z * (MaxAltitude - MinAltitude) * 100 / (MaxZBeforeScaling - MinZBeforeScaling)
			);

			Landscape->Modify();
			Landscape->SetActorScale3D(NewScale);
			Landscape->PostEditChange();

			FVector2D MinMaxZAfterScaling = GetMinMaxZ(LandscapeStreamingProxies, Landscape);
			double MinZAfterScaling = MinMaxZAfterScaling.X;
			double MaxZAfterScaling = MinMaxZAfterScaling.Y;
			
			double InsideLocationX = MinCoordWidth * WorldWidthCm / 360;
			double InsideLocationY = - MaxCoordHeight * WorldHeightCm / 180;
			
			double NewLocationX = InsideLocationX - (OutsidePixelWidth - InsidePixelWidth)   * CmPxWidthRatio / 2;
			double NewLocationY = InsideLocationY - (OutsidePixelHeight - InsidePixelHeight) * CmPxHeightRatio / 2;
			double NewLocationZ = OldLocation.Z - MaxZAfterScaling + 100 * MaxAltitude;
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
		}
	}
	
	return FReply::Handled();
}


#undef LOCTEXT_NAMESPACE