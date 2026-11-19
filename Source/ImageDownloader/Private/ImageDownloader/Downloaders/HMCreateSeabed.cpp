#include "ImageDownloader/Downloaders/HMCreateSeabed.h"

#include "GDALInterface/GDALInterface.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "Misc/ScopeExit.h"

FString HMCreateSeabed::GetOutputDir()
{
	return FPaths::Combine(ImageDownloaderDir, FString::Printf(TEXT("%s-CreateSeabed"), *Name));
}

bool HMCreateSeabed::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	OutputCRS = MoveTemp(InputCRS);
	OutputFiles.Reset();
	if (!FMath::IsFinite(MaxDepthMeters) || MaxDepthMeters <= 0 || ShoreTransitionPixels < 1)
	{
		UE_LOG(LogImageDownloader, Error, TEXT("Create Seabed requires positive maximum depth and at least one shore transition pixel."));
		return false;
	}

	GDALDriver* MemoryDriver = GetGDALDriverManager()->GetDriverByName("MEM");
	if (!MemoryDriver) return false;

	for (const FString& InputFile : InputFiles)
	{
		GDALDataset* Source = static_cast<GDALDataset*>(GDALOpen(TCHAR_TO_UTF8(*InputFile), GA_ReadOnly));
		if (!Source)
		{
			UE_LOG(LogImageDownloader, Error, TEXT("Could not open seabed input: %s"), *InputFile);
			return false;
		}
		ON_SCOPE_EXIT { GDALClose(Source); };
		if (Source->GetRasterCount() != 1)
		{
			UE_LOG(LogImageDownloader, Error, TEXT("Create Seabed needs a single-band heightmap: %s"), *InputFile);
			return false;
		}

		GDALRasterBand* SourceBand = Source->GetRasterBand(1);
		int HasNoData = false;
		const double NoData = SourceBand->GetNoDataValue(&HasNoData);
		if (HasNoData && NoData == 0)
		{
			UE_LOG(LogImageDownloader, Error, TEXT("Create Seabed cannot distinguish water from NoData when the input uses zero as NoData: %s"), *InputFile);
			return false;
		}

		const int Width = Source->GetRasterXSize();
		const int Height = Source->GetRasterYSize();
		GDALDataset* LandMask = MemoryDriver->Create("", Width, Height, 1, GDT_Byte, nullptr);
		GDALDataset* Proximity = MemoryDriver->Create("", Width, Height, 1, GDT_Float32, nullptr);
		if (!LandMask || !Proximity)
		{
			if (LandMask) GDALClose(LandMask);
			if (Proximity) GDALClose(Proximity);
			return false;
		}
		ON_SCOPE_EXIT { GDALClose(LandMask); GDALClose(Proximity); };
		TArray<float> Heights;
		TArray<float> Distances;
		TArray<uint8> Land;
		Heights.SetNumUninitialized(Width);
		Distances.SetNumUninitialized(Width);
		Land.SetNumUninitialized(Width);
		GDALRasterBand* MaskBand = LandMask->GetRasterBand(1);
		GDALRasterBand* ProximityBand = Proximity->GetRasterBand(1);
		for (int Y = 0; Y < Height; ++Y)
		{
			if (SourceBand->RasterIO(GF_Read, 0, Y, Width, 1, Heights.GetData(), Width, 1, GDT_Float32, 0, 0) != CE_None) return false;
			for (int X = 0; X < Width; ++X)
			{
				const float Value = Heights[X];
				Land[X] = FMath::IsFinite(Value) && (!HasNoData || Value != NoData) && Value > 0.0f ? 1 : 0;
			}
			if (MaskBand->RasterIO(GF_Write, 0, Y, Width, 1, Land.GetData(), Width, 1, GDT_Byte, 0, 0) != CE_None) return false;
		}
		if (GDALComputeProximity(MaskBand, ProximityBand, nullptr, nullptr, nullptr) != CE_None) return false;

		const FString OutputFile = FPaths::Combine(OutputDir, FPaths::GetBaseFilename(InputFile) + TEXT(".tif"));
		if (!GDALInterface::Translate(InputFile, OutputFile, {TEXT("-of"), TEXT("GTiff"), TEXT("-ot"), TEXT("Float32")})) return false;
		GDALDataset* Output = static_cast<GDALDataset*>(GDALOpen(TCHAR_TO_UTF8(*OutputFile), GA_Update));
		if (!Output) return false;
		bool bSuccess = true;
		GDALRasterBand* OutputBand = Output->GetRasterBand(1);
		for (int Y = 0; Y < Height; ++Y)
		{
			if (SourceBand->RasterIO(GF_Read, 0, Y, Width, 1, Heights.GetData(), Width, 1, GDT_Float32, 0, 0) != CE_None ||
				ProximityBand->RasterIO(GF_Read, 0, Y, Width, 1, Distances.GetData(), Width, 1, GDT_Float32, 0, 0) != CE_None)
			{
				bSuccess = false;
				break;
			}
			for (int X = 0; X < Width; ++X)
			{
				const float Value = Heights[X];
				if (!FMath::IsFinite(Value) || (HasNoData && Value == NoData) || Value > 0.0f) continue;
				const float Distance = Distances[X];
				const double Fraction = Distance < 0 ? 1.0 : FMath::Clamp(static_cast<double>(Distance) / ShoreTransitionPixels, 0.0, 1.0);
				Heights[X] = static_cast<float>(-MaxDepthMeters * Fraction);
			}
			if (OutputBand->RasterIO(GF_Write, 0, Y, Width, 1, Heights.GetData(), Width, 1, GDT_Float32, 0, 0) != CE_None)
			{
				bSuccess = false;
				break;
			}
		}
		if (Output->FlushCache() != CE_None) bSuccess = false;
		GDALClose(Output);
		if (!bSuccess) return false;
		OutputFiles.Add(OutputFile);
	}
	return true;
}
