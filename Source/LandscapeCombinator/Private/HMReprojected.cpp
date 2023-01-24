#include "HMReprojected.h"
#include "Logging.h"

#pragma warning(disable: 4668)
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#pragma warning(default: 4668)

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

bool HMReprojected::Initialize() {
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
	FString ScaledFile = FPaths::Combine(ResultDir, FString::Format(TEXT("{0}-{1}.png"), { LandscapeName, Precision }));
	
	OriginalFiles.Add(ReprojectedFile);
	ScaledFiles.Add(ScaledFile);

	return true;
}


HMReprojected::HMReprojected(FString LandscapeName0, const FText& KindText0, FString Descr0, int Precision0, HMInterface* Target0) :
	HMLocalFile(LandscapeName0, KindText0, Descr0, Precision0)
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

				FVector2D Altitudes;
				if (!HMInterface::GetMinMax(Altitudes, Target->OriginalFiles)) return;

				double MinAltitude = Altitudes[0];
				double MaxAltitude = Altitudes[1];

				char** WarpArgv = nullptr;
				
				WarpArgv = CSLAddString(WarpArgv, "-r");
				WarpArgv = CSLAddString(WarpArgv, "bilinear");
				WarpArgv = CSLAddString(WarpArgv, "-t_srs");
				WarpArgv = CSLAddString(WarpArgv, "EPSG:4326");
				WarpArgv = CSLAddString(WarpArgv, "-dstnodata");
				WarpArgv = CSLAddString(WarpArgv, TCHAR_TO_UTF8(*FString::SanitizeFloat(MinAltitude)));
				GDALWarpAppOptions* WarpOptions = GDALWarpAppOptionsNew(WarpArgv, NULL);
				CSLDestroy(WarpArgv);

				UE_LOG(LogLandscapeCombinator, Log, TEXT("Reprojecting using gdalwarp --config GDAL_PAM_ENABLED NO -r bilinear -t_srs EPSG:4326 \"%s\" \"%s\""),
					*OriginalFile,
					*ReprojectedFile
				);

				int WarpError = 0;
				GDALDataset* WarpedDataset = (GDALDataset*) GDALWarp(TCHAR_TO_UTF8(*ReprojectedFile), nullptr, 1, &SrcDataset, WarpOptions, &WarpError);
				GDALClose(SrcDataset);

				if (!WarpedDataset)
				{
					UE_LOG(LogLandscapeCombinator, Error, TEXT("Error while using gdalwarp on file %s"), *OriginalFile);
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