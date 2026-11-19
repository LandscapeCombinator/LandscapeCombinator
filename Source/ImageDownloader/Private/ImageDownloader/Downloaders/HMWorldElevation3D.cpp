// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Downloaders/HMWorldElevation3D.h"
#include "ImageDownloader/LogImageDownloader.h"

#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"
#include "FileDownloader/Download.h"
#include "GDALInterface/GDALInterface.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"

#define LOCTEXT_NAMESPACE "LandscapeGeneration"

bool HMWorldElevation3D::OnFetch(FString, TArray<FString>)
{
	OutputFiles.Reset();
	OutputCRS = TEXT("EPSG:3857");
	if (Zoom < 0 || Zoom > 16)
	{
		LCReporter::ShowError(LOCTEXT("Zoom", "Terrain3D supports zoom levels 0 through 16."));
		return false;
	}
	if (const int GridSize = 1 << Zoom; MinX < 0 || MinY < 0 || MinX > MaxX || MinY > MaxY || MaxX >= GridSize || MaxY >= GridSize)
	{
		LCReporter::ShowError(LOCTEXT("Bounds", "Invalid ArcGIS tile bounds for the selected XYZ zoom level."));
		return false;
	}
	const int64 Count = (static_cast<int64>(MaxX) - MinX + 1) * (static_cast<int64>(MaxY) - MinY + 1);
	if (Count > MAX_int32)
	{
		LCReporter::ShowError(LOCTEXT("TileLimit", "The requested ArcGIS tile range is too large."));
		return false;
	}
	const int NumTiles = static_cast<int>(Count);
	if (bIsUserInitiated && NumTiles >= 100 && !LCReporter::ShowMessage(
		FText::Format(LOCTEXT("ManyTiles", "Your parameters require downloading and processing {0} elevation tiles. Continue?"), FText::AsNumber(NumTiles)),
		TEXT("SuppressManyTilesWarning"), LOCTEXT("ManyTilesTitle", "Many Tiles Warning"))) return false;

	bool bShowedDialog = false;
	TArray<FString> Results;
	Results.SetNum(NumTiles);
	UE_LOG(LogImageDownloader, Log, TEXT("Downloading and georeferencing %d ArcGIS elevation tiles"), NumTiles);
	const bool bSuccess = Concurrency::RunManyAndWait(bEnableParallelDownload, NumTiles,
		[this, &bShowedDialog, &Results, NumTiles](int Index)
		{
			if (Concurrency::IsCancelRequested()) return false;
			const int X = MinX + Index % (MaxX - MinX + 1);
			const int Y = MinY + Index / (MaxX - MinX + 1);
			const FString URL = FString::Printf(TEXT("https://elevation3d.arcgis.com/arcgis/rest/services/WorldElevation3D/Terrain3D/ImageServer/tile/%d/%d/%d?blankTile=false"), Zoom, Y, X);
			const FString DownloadFile = FPaths::Combine(DownloadDir, FString::Printf(TEXT("ArcGISWorldElevation3D-%d-%d-%d.lerc"), Zoom, X, Y));
			if (!Download::SynchronousFromURL(URL, DownloadFile, bIsUserInitiated && NumTiles < 20)) return bAllowInvalidTiles;
			FString DecodedFile;
			if (!DecodeTiles(DownloadFile, X, Y, DecodedFile, &bShowedDialog)) return bAllowInvalidTiles;
			FString OutputFile;
			if (!GeoreferenceTiles(DecodedFile, X, Y, OutputFile)) return bAllowInvalidTiles;
			Results[Index] = MoveTemp(OutputFile);
			return true;
		});
	if (!bSuccess) return false;
	// Each worker writes its own array element; output ordering is deterministic.
	for (const FString& File : Results) if (!File.IsEmpty()) OutputFiles.Add(File);
	if (OutputFiles.IsEmpty())
	{
		LCReporter::ShowError(LOCTEXT("NoTiles", "No valid ArcGIS elevation tiles were downloaded."));
		return false;
	}
	return true;
}

bool HMWorldElevation3D::DecodeTiles(const FString& DownloadFile, int X, int Y, FString& DecodedFile, bool* BShowedDialog)
{
	const auto fail = [&](const FString& Message)
	{
		UE_LOG(LogImageDownloader, Error, TEXT("%s: %s"), *DownloadFile, *Message);
		if (!bAllowInvalidTiles) LCReporter::ShowOneError(FText::FromString(Message), BShowedDialog);
		return false;
	};
	TArray<uint8> Blob;
	if (!FFileHelper::LoadFileToArray(Blob, *DownloadFile) || Blob.IsEmpty())
	{
		return fail(TEXT("Could not read the LERC tile."));
	}
#if PLATFORM_WINDOWS
	const FString LibraryName = TEXT("Lerc.dll");
	const FString PlatformName = TEXT("Win64");
#elif PLATFORM_LINUX
	const FString LibraryName = TEXT("libLerc.so");
	const FString PlatformName = TEXT("Linux");
#else
	return fail(TEXT("LERC decoding is not supported on this platform."));
#endif
#if PLATFORM_WINDOWS || PLATFORM_LINUX
	// GDALInterface stages its bundled libraries beside the project binaries.
	void* Library = FPlatformProcess::GetDllHandle(*LibraryName);
	if (!Library)
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("LandscapeCombinator"));
		if (Plugin.IsValid())
		{
			const FString LibraryPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Source/ThirdParty/GDAL"), PlatformName, TEXT("bin"), LibraryName);
			Library = FPlatformProcess::GetDllHandle(*LibraryPath);
		}
	}
	if (!Library) return fail(TEXT("Could not load the bundled LERC decoder library."));
	ON_SCOPE_EXIT { FPlatformProcess::FreeDllHandle(Library); };

	using FGetBlobInfo = unsigned int (*)(const unsigned char*, unsigned int, unsigned int*, double*, int, int);
	using FDecode = unsigned int (*)(const unsigned char*, unsigned int, int, unsigned char*, int, int, int, int, unsigned int, void*);
	const FGetBlobInfo GetBlobInfo = reinterpret_cast<FGetBlobInfo>(FPlatformProcess::GetDllExport(Library, TEXT("lerc_getBlobInfo")));
	const FDecode Decode = reinterpret_cast<FDecode>(FPlatformProcess::GetDllExport(Library, TEXT("lerc_decode")));
	if (!GetBlobInfo || !Decode) return fail(TEXT("The LERC library is missing decoder entry points."));

	unsigned int Info[11] = {};
	double Ranges[3] = {};
	if (GetBlobInfo(Blob.GetData(), Blob.Num(), Info, Ranges, 11, 3) != 0)
	{
		return fail(TEXT("Invalid or truncated LERC tile."));
	}
	// Terrain3D stores 257 vertex samples over 256 cells, in one Float32 band.
	if (Info[1] != 6 || Info[2] != 1 || Info[3] != 257 || Info[4] != 257 || Info[5] != 1 || Info[8] > 1 || Info[10] != 0)
	{
		return fail(TEXT("Expected a single-band 257 x 257 Float32 Terrain3D tile."));
	}
	if (Info[6] == 0) return fail(TEXT("The elevation tile contains only NoData."));
	constexpr int Size = 257;
	TArray<float> Heights;
	Heights.SetNumUninitialized(Size * Size);
	TArray<uint8> Valid;
	Valid.Init(1, Size * Size);
	if (Decode(Blob.GetData(), Blob.Num(), Info[8], Info[8] ? Valid.GetData() : nullptr,
		1, Size, Size, 1, 6, Heights.GetData()) != 0)
	{
		return fail(TEXT("LERC elevation decoding failed."));
	}
	constexpr float NoData = -3.402823466e+38F;
	int ValidCount = 0;
	for (int Index = 0; Index < Heights.Num(); ++Index)
	{
		if (!Valid[Index] || !FMath::IsFinite(Heights[Index])) Heights[Index] = NoData;
		else ++ValidCount;
	}
	if (ValidCount == 0) return fail(TEXT("The tile contains no finite valid elevation values."));

	DecodedFile = FPaths::Combine(DownloadDir, FString::Printf(TEXT("ArcGISWorldElevation3D-%d-%d-%d-decoded.tif"), Zoom, X, Y));
	GDALDriver* Driver = GetGDALDriverManager()->GetDriverByName("GTiff");
	GDALDataset* Dataset = Driver ? Driver->Create(TCHAR_TO_UTF8(*DecodedFile), Size, Size, 1, GDT_Float32, nullptr) : nullptr;
	if (!Dataset) return fail(TEXT("Could not create the decoded Float32 TIFF."));
	ON_SCOPE_EXIT { GDALClose(Dataset); };
	GDALRasterBand* Band = Dataset->GetRasterBand(1);
	// A separate NoData value keeps valid sea-level zero samples intact.
	if (Band->SetNoDataValue(NoData) != CE_None ||
		Band->RasterIO(GF_Write, 0, 0, Size, Size, Heights.GetData(), Size, Size, GDT_Float32, 0, 0) != CE_None ||
		Dataset->FlushCache() != CE_None)
	{
		return fail(TEXT("Could not write the decoded elevation TIFF."));
	}
	return true;
#endif
}

bool HMWorldElevation3D::GeoreferenceTiles(const FString& DecodedFile, int X, int Y, FString& OutputFile)
{
	double West, North, East, South;
	GDALInterface::XYZTileToEPSG3857(X, Y, Zoom, West, North);
	GDALInterface::XYZTileToEPSG3857(X + 1, Y + 1, Zoom, East, South);
	OutputFile = FPaths::Combine(OutputDir, FString::Printf(TEXT("%s_x%d_y%d.tif"), *Name, X - MinX, Y - MinY));
	// 257 vertex samples span 256 cells. Half-sample margins align the pixel
	// centers with the mesh vertices and make adjacent tiles share one sample.
	const double HalfSample = (East - West) / 512.0;
	return GDALInterface::Translate(DecodedFile, OutputFile, {
		"-of", "GTiff", "-a_srs", "EPSG:3857", "-a_ullr",
		FString::Printf(TEXT("%.17g"), West - HalfSample),
		FString::Printf(TEXT("%.17g"), North + HalfSample),
		FString::Printf(TEXT("%.17g"), East + HalfSample),
		FString::Printf(TEXT("%.17g"), South - HalfSample)
	});
}

#undef LOCTEXT_NAMESPACE
