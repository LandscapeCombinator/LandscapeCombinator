// Copyright LandscapeCombinator. All Rights Reserved.

#include "Elevation/HMInterface.h"
#include "Utils/Logging.h"
#include "Utils/Console.h"
#include "Utils/Concurrency.h"
#include "EngineCode/EngineCode.h"

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

#pragma warning(disable: 4668)
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#pragma warning(default: 4668)




#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

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

bool HMInterface::GetCoordinates(FVector4d& Coordinates, GDALDataset* Dataset)
{
	if (!Dataset) return false;

	double AdfTransform[6];
	if (Dataset->GetGeoTransform(AdfTransform) != CE_None)
		return false;
		
	double LeftCoord    = AdfTransform[0];
	double RightCoord   = AdfTransform[0] + Dataset->GetRasterXSize()*AdfTransform[1];
	double TopCoord     = AdfTransform[3];
	double BottomCoord  = AdfTransform[3] + Dataset->GetRasterYSize()*AdfTransform[5];
	
	Coordinates[0] = LeftCoord;
	Coordinates[1] = RightCoord;
	Coordinates[2] = BottomCoord;
	Coordinates[3] = TopCoord;
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
		if (!GetCoordinates(FileCoordinates, Dataset))
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
	return GetMinMax(MinMax, OriginalFiles);
}

bool HMInterface::GetMinMax(FVector2D &MinMax, TArray<FString> Files) {
	MinMax[0] = DBL_MAX;
	MinMax[1] = -DBL_MAX;

	for (auto& File : Files) {

		GDALDataset *Dataset = (GDALDataset *)GDALOpen(TCHAR_TO_UTF8(*File), GA_ReadOnly);

		if (!Dataset) {
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(LOCTEXT("GetMinMaxError", "Could not open heightmap file '{0}' to compute min/max altitudes."),
					FText::FromString(File)
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

	if (!IPlatformFile::GetPlatformPhysical().CreateDirectory(*ResultDir)) {
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
			  
			GDALDatasetH SrcDataset = GDALOpen(TCHAR_TO_UTF8(*OriginalFile), GA_ReadOnly);

			if (!SrcDataset) {
				FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
					LOCTEXT("ConvertGDALOpenError", "Could not open file {0}."),
					FText::FromString(OriginalFile)
				));
				return false;
			}

			char** TranslateArgv = nullptr;
			
			TranslateArgv = CSLAddString(TranslateArgv, "-scale");
			TranslateArgv = CSLAddString(TranslateArgv, TCHAR_TO_UTF8(*FString::SanitizeFloat(MinAltitude)));
			TranslateArgv = CSLAddString(TranslateArgv, TCHAR_TO_UTF8(*FString::SanitizeFloat(MaxAltitude)));
			TranslateArgv = CSLAddString(TranslateArgv, "0");
			TranslateArgv = CSLAddString(TranslateArgv, "65535");
			TranslateArgv = CSLAddString(TranslateArgv, "-outsize");
			TranslateArgv = CSLAddString(TranslateArgv, TCHAR_TO_UTF8(*FString::Format(TEXT("{0}%"), { Precision })));
			TranslateArgv = CSLAddString(TranslateArgv, TCHAR_TO_UTF8(*FString::Format(TEXT("{0}%"), { Precision })));
			TranslateArgv = CSLAddString(TranslateArgv, "-ot");
			TranslateArgv = CSLAddString(TranslateArgv, "UInt16");
			TranslateArgv = CSLAddString(TranslateArgv, "-of");
			TranslateArgv = CSLAddString(TranslateArgv, "PNG");

			GDALTranslateOptions* Options = GDALTranslateOptionsNew(TranslateArgv, NULL);
			CSLDestroy(TranslateArgv);

			if (!Options) {
				FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
					LOCTEXT("ConvertParseOptionsError", "Internal GDAL error while parsing GDALTranslate options for file {0}."),
					FText::FromString(OriginalFile)
				));
				GDALClose(SrcDataset);
				return false;
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
				return false;
			}
		
			UE_LOG(LogLandscapeCombinator, Log, TEXT("Finished translating: %s to %s"), *OriginalFile, *ScaledFile);
			GDALClose(DstDataset);
		}
		return true;
	},
		OnComplete
	);


	return FReply::Handled();
}

FReply HMInterface::ImportHeightMaps()
{
	FString HeightmapFile = ScaledFiles[0];
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Importing heightmaps for Landscape %s from %s"), *LandscapeLabel, *HeightmapFile);
	GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_Landscape);
	FEdModeLandscape* LandscapeEdMode = (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
	LandscapeEdMode->NewLandscapePreviewMode = ENewLandscapePreviewMode::ImportLandscape;

	ULandscapeEditorObject *UISettings = LandscapeEdMode->UISettings;
	UISettings->LastImportPath = FPaths::GetPath(HeightmapFile);
	FIntPoint OutCoord;
	FString OutBaseFilePattern;
	if (FLandscapeImportHelper::ExtractCoordinates(FPaths::GetBaseFilename(HeightmapFile), OutCoord, OutBaseFilePattern))
	{
		UISettings->ImportLandscape_HeightmapFilename = FString::Format(TEXT("{0}/{1}{2}"), { FPaths::GetPath(HeightmapFile), OutBaseFilePattern, FPaths::GetExtension(HeightmapFile, true) });
	}
	else
	{
		UISettings->ImportLandscape_HeightmapFilename = HeightmapFile;
	}
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Heightmap file: %s"), *HeightmapFile);
	UISettings->OnImportHeightmapFilenameChanged();
	ALandscape *CreatedLandscape = EngineCode::OnCreateButtonClicked();

	
	GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_Default);

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
					AdjustLandscape(WorldWidthKm, WorldHeightKm, ZScale, WorldOriginX, WorldOriginY);
					if (OnComplete) OnComplete(true);
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

bool HMInterface::GetSpatialReference(OGRSpatialReference &InRs, FString File) {

	GDALDataset* Dataset = (GDALDataset *) GDALOpen(TCHAR_TO_UTF8(*File), GA_ReadOnly);
	if (!Dataset)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("GetSpatialReferenceError", "Unable to open file {0} to read its spatial reference."),
			FText::FromString(File)
		));
		return false;
	}

	return GetSpatialReference(InRs, Dataset);
}

bool HMInterface::GetSpatialReference(OGRSpatialReference& InRs, GDALDataset* Dataset)
{
	if (!Dataset)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("GetSpatialReferenceError", "Unable to get spatial reference from dataset (null pointer).")
		);
		return false;
	}

    const char* ProjectionRef = Dataset->GetProjectionRef();
	OGRErr Err = InRs.importFromWkt(ProjectionRef);

    if (Err != OGRERR_NONE)
    {
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("GetSpatialReferenceError2", "Unable to get spatial reference from dataset (error {0})."),
			FText::AsNumber(Err)
		));
		return false;
    }
	InRs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

	return true;
}

bool HMInterface::GetSpatialReferenceFromEPSG(OGRSpatialReference& InRs, int EPSG)
{
	OGRErr Err = InRs.importFromEPSG(EPSG);
	if (Err != OGRERR_NONE)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("StartupModuleError1", "Could not create spatial reference from EPSG {0}: (Error {1})."),
				FText::AsNumber(EPSG),
				FText::AsNumber(Err)
			)
		);
		return false;
	}
	InRs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
	return true;
}

FReply HMInterface::DownloadHeightMaps(TFunction<void(bool)> OnComplete) const {
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

bool HMInterface::GetMinMaxZ(FVector2D &MinMaxZ) {

	FVector LandscapeOrigin;
	FVector LandscapeExtent;
	Landscape->GetActorBounds(false, LandscapeOrigin, LandscapeExtent, true);

	if (LandscapeExtent != FVector(0,0,0))
	{
		UE_LOG(LogLandscapeCombinator, Log, TEXT("GetMinMaxZ: Landscape %s has an extent."), *LandscapeLabel);
		MinMaxZ.X = LandscapeOrigin.Z - LandscapeExtent.Z;
		MinMaxZ.Y = LandscapeOrigin.Z + LandscapeExtent.Z;
		return true;
	}
	else
	{
		UE_LOG(LogLandscapeCombinator, Log, TEXT("GetMinMaxZ: Landscape %s has no extent, so we will look for LandscapeStreamingProxy's."), *LandscapeLabel);
		double MaxZ = -DBL_MAX;
		double MinZ = DBL_MAX;
		for (auto& LandscapeStreamingProxy : LandscapeStreamingProxies)
		{
			FVector LandscapeProxyOrigin;
			FVector LandscapeProxyExtent;
			LandscapeStreamingProxy->GetActorBounds(false, LandscapeProxyOrigin, LandscapeProxyExtent);
			MaxZ = FMath::Max(MaxZ, LandscapeProxyOrigin.Z + LandscapeProxyExtent.Z);
			MinZ = FMath::Min(MinZ, LandscapeProxyOrigin.Z - LandscapeProxyExtent.Z);
		}

		if (MaxZ != -DBL_MAX && MinZ != DBL_MAX)
		{
			MinMaxZ.X = MinZ;
			MinMaxZ.Y = MaxZ;
			return true;
		}
		else {
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("GetMinMaxZError", "Could not compute Min and Max Z values for Landscape {0}."),
				FText::FromString(LandscapeLabel)
			));
			return false;
		}
	}
}

bool HMInterface::SetLandscapeFromLabel()
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	TArray<AActor*> Landscapes;
	UGameplayStatics::GetAllActorsOfClass(World, ALandscape::StaticClass(), Landscapes);

	UE_LOG(LogLandscapeCombinator, Log, TEXT("Searching for Landscape %s among %d landscapes"), *LandscapeLabel, Landscapes.Num());
	for (auto& Landscape0 : Landscapes)
	{
		if (Landscape0->GetActorLabel().Equals(LandscapeLabel))
		{
			Landscape = Cast<ALandscape>(Landscape0);
			SetLandscapeStreamingProxies();

			return true;
		}
	}

	return false;
}

void HMInterface::SetLandscape(ALandscape *Landscape0)
{
	Landscape = Landscape0;
	SetLandscapeStreamingProxies();
}

void HMInterface::SetLandscapeStreamingProxies()
{
	UWorld* World = GEditor->GetEditorWorldContext().World();

	TArray<AActor*> LandscapeStreamingProxiesTemp;
	UGameplayStatics::GetAllActorsOfClass(World, ALandscapeStreamingProxy::StaticClass(), LandscapeStreamingProxiesTemp);
		
	for (auto& LandscapeStreamingProxy0 : LandscapeStreamingProxiesTemp)
	{
		ALandscapeStreamingProxy* LandscapeStreamingProxy = Cast<ALandscapeStreamingProxy>(LandscapeStreamingProxy0);
		if (LandscapeStreamingProxy->GetLandscapeActor() == Landscape)
			LandscapeStreamingProxies.Add(LandscapeStreamingProxy);
	}
}

FReply HMInterface::AdjustLandscape(int WorldWidthKm, int WorldHeightKm, double ZScale, double WorldOriginX, double WorldOriginY)
{
	if (!Landscape)
	{
		if (!SetLandscapeFromLabel())
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("AdjustLandscapeTransformError", "Could not find Landscape {0}"),
				FText::FromString(LandscapeLabel)
			));
			return FReply::Unhandled();
		}
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

bool HMInterface::GetCoordinates4326(FVector4d& Coordinates)
{	FVector4d OriginalCoordinates;	
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