#include "Elevation/HMReprojected.h"
#include "Utils/Logging.h"

#pragma warning(disable: 4668)
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#pragma warning(default: 4668)

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

bool HMReprojected::Initialize()
{
	if (!HMInterface::Initialize()) return false;

	int32 NumFiles = Target->OriginalFiles.Num();
	if (NumFiles != 1) {
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("DownloadReprojectError", "Reprojection to 4326 is supported for heightmaps with one file only (found {0} heightmap files)."),
			FText::AsNumber(NumFiles)
		));

		return false;
	}

	FString OriginalFile = Target->OriginalFiles[0];
	FString ReprojectedFile = FPaths::Combine(FPaths::GetPath(OriginalFile), FString::Format(TEXT("{0}-4326.tif"), { FPaths::GetBaseFilename(OriginalFile) } ));
	FString ScaledFile = FPaths::Combine(ResultDir, FString::Format(TEXT("{0}-{1}.png"), { LandscapeLabel, Precision }));
	
	OriginalFiles.Add(ReprojectedFile);
	ScaledFiles.Add(ScaledFile);

	return true;
}


HMReprojected::HMReprojected(FString LandscapeLabel0, const FText& KindText0, FString Descr0, int Precision0, HMInterface* Target0) :
	HMLocalFile(LandscapeLabel0, KindText0, Descr0, Precision0)
{
	Target = Target0;
}

FReply HMReprojected::DownloadHeightMapsImpl(TFunction<void(bool)> OnComplete) const
{
	Target->DownloadHeightMapsImpl([this, OnComplete](bool bWasSuccessful) {
		if (bWasSuccessful)
		{
			int32 i;
			int32 NumFiles = OriginalFiles.Num();
			for (i = 0; i < NumFiles; i++) {
				FString OriginalFile = Target->OriginalFiles[i];
				FString ReprojectedFile = OriginalFiles[i];

				GDALDatasetH SrcDataset = GDALOpen(TCHAR_TO_UTF8(*OriginalFile), GA_ReadOnly);

				if (!SrcDataset)
				{
					FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
						LOCTEXT("GDALWarpOpenError", "Could not open file {0}."),
						FText::FromString(OriginalFile)
					));

					break;
				}

				FVector4d Coordinates;
				if (!GetCoordinates(Coordinates, (GDALDataset*) SrcDataset))
				{
					FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
						LOCTEXT("GDALWarpOpenError", "Could not read coordinates from file {0}."),
						FText::FromString(OriginalFile)
					));

					break;
				}
				
				double MinCoordWidth = Coordinates[0];
				double MaxCoordWidth = Coordinates[1];
				double MinCoordHeight = Coordinates[2];
				double MaxCoordHeight = Coordinates[3];

				double xs[4] = { MinCoordWidth,  MinCoordWidth,  MaxCoordWidth,  MaxCoordWidth };
				double ys[4] = { MinCoordHeight, MaxCoordHeight, MaxCoordHeight, MinCoordHeight };

				OGRSpatialReference InRs, OutRs;	
				if (!GetSpatialReference(InRs, (GDALDataset *) SrcDataset) || !GetSpatialReferenceFromEPSG(OutRs, 4326) || !OGRCreateCoordinateTransformation(&InRs, &OutRs)->Transform(4, xs, ys)) {
					FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
						LOCTEXT("ReprojectionError", "Internal error while transforming coordinates for Landscape {0}."),
						FText::FromString(LandscapeLabel)
					));
					break;
				}
				
				char *inwkt = nullptr;
				InRs.exportToWkt(&inwkt);
				char *outwkt = nullptr;
				OutRs.exportToWkt(&outwkt);

				// for cropping
				double xmin = FMath::Max(xs[0], xs[1]);
				double xmax = FMath::Min(xs[2], xs[3]);
				double ymin = FMath::Max(ys[0], ys[3]);
				double ymax = FMath::Min(ys[1], ys[2]);

				FVector2D Altitudes;
				if (!HMInterface::GetMinMax(Altitudes, Target->OriginalFiles)) break;

				double MinAltitude = Altitudes[0];
				double MaxAltitude = Altitudes[1];

				char** WarpArgv = nullptr;
				
				WarpArgv = CSLAddString(WarpArgv, "-r");
				WarpArgv = CSLAddString(WarpArgv, "bilinear");
				WarpArgv = CSLAddString(WarpArgv, "-t_srs");
				WarpArgv = CSLAddString(WarpArgv, "EPSG:4326");
				WarpArgv = CSLAddString(WarpArgv, "-te");
				WarpArgv = CSLAddString(WarpArgv, TCHAR_TO_UTF8(*FString::SanitizeFloat(xmin)));
				WarpArgv = CSLAddString(WarpArgv, TCHAR_TO_UTF8(*FString::SanitizeFloat(ymin)));
				WarpArgv = CSLAddString(WarpArgv, TCHAR_TO_UTF8(*FString::SanitizeFloat(xmax)));
				WarpArgv = CSLAddString(WarpArgv, TCHAR_TO_UTF8(*FString::SanitizeFloat(ymax)));
				WarpArgv = CSLAddString(WarpArgv, "-dstnodata");
				WarpArgv = CSLAddString(WarpArgv, TCHAR_TO_UTF8(*FString::SanitizeFloat(MinAltitude)));
				GDALWarpAppOptions* WarpOptions = GDALWarpAppOptionsNew(WarpArgv, NULL);
				CSLDestroy(WarpArgv);

				if (!WarpOptions)
				{
					FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
						LOCTEXT("GDALWarpError", "Could not parse gdalwarp options for file {0}."),
						FText::FromString(OriginalFile)
					));

					break;
				}

				UE_LOG(LogLandscapeCombinator, Log, TEXT("Reprojecting using gdalwarp --config GDAL_PAM_ENABLED NO -r bilinear -t_srs EPSG:4326 -te %s %s %s %s \"%s\" \"%s\""),
					*FString::SanitizeFloat(xmin),
					*FString::SanitizeFloat(ymin),
					*FString::SanitizeFloat(xmax),
					*FString::SanitizeFloat(ymax),
					*OriginalFile,
					*ReprojectedFile
				);

				int WarpError = 0;
				GDALDataset* WarpedDataset = (GDALDataset*) GDALWarp(TCHAR_TO_UTF8(*ReprojectedFile), nullptr, 1, &SrcDataset, WarpOptions, &WarpError);
				GDALClose(SrcDataset);

				if (!WarpedDataset)
				{
					FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
						LOCTEXT("GDALWarpError", "Internal GDALWarp error {0} while converting dataset from file {1} to EPSG 4326."),
						FText::AsNumber(WarpError),
						FText::FromString(OriginalFile)
					));

					break;
				}
				
				GDALClose(WarpedDataset);
			}
			OnComplete(i == NumFiles);
		}
		else
		{
			OnComplete(false);
		}
	});

	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE