// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Downloaders/HMXYZ.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"

#include "ConsoleHelpers/Console.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"
#include "FileDownloader/Download.h"
#include "GDALInterface/GDALInterface.h"
#include "MapboxHelpers/MapboxHelpers.h"
#include "LCCommon/LCSettings.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManagerGeneric.h"
#include "Logging/StructuredLog.h"
#include "Containers/Queue.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

bool HMXYZ::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	if (!bGeoreferenceSlippyTiles && CRS.IsEmpty())
	{
		LCReporter::ShowError(
			LOCTEXT("HMXYZ::Fetch::CRS", "Please provide a valid CRS for your XYZ tiles.")
		);
		return false;
	}
	
	if (bGeoreferenceSlippyTiles)
	{
		OutputCRS = "EPSG:3857";
	}
	else
	{
		OutputCRS = CRS;
	}
	
	if (MinX > MaxX || MinY > MaxY)
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("HMXYZ::Fetch::Tiles", "For XYZ tiles, MinX ({0}) must be <= than MaxX ({1}), and MinY ({2}) must be <= MaxY ({3})."),
			FText::AsNumber(MinX),
			FText::AsNumber(MaxX),
			FText::AsNumber(MinY),
			FText::AsNumber(MaxY)
		));
		return false;
	}

	int NumTiles = (MaxX - MinX + 1) * (MaxY - MinY + 1);

	if (bIsUserInitiated && NumTiles >= 100)
	{
		if (!LCReporter::ShowMessage(
			FText::Format(
				LOCTEXT(
					"ManyTiles",
					"Your parameters require downloading and processing a lot of tiles ({0}).\nContinue?"
				),
				FText::AsNumber(NumTiles)
			),
			"SuppressManyTilesWarning",
			LOCTEXT("ManyTilesTitle", "Many Tiles Warning")
		))
		{
			return false;
		}
	}
	
	if (Format.Contains("."))
	{
		if (!Console::ExecProcess(TEXT("7z"), TEXT(""), false, false))
		{
			LCReporter::ShowError(
				LOCTEXT(
					"MissingRequirement",
					"Please make sure 7z is installed on your computer and available in your PATH if you want to use a compressed format."
				)
			);

			return false;
		}
	}

	bool *bShowedDialog = new bool(false);
	TQueue<FString> OutputFilesQueue; // thread-safe

	UE_LOG(LogImageDownloader, Log, TEXT("Downloading and Georeferencing %d tiles"), NumTiles);
	bool bSuccess = Concurrency::RunManyAndWait(
		bEnableParallelDownload,
		NumTiles,

		[this, bShowedDialog, NumTiles, &OutputFilesQueue](int i)
		{
			int X = i % (MaxX - MinX + 1) + MinX;
			int Y = i / (MaxX - MinX + 1) + MinY;

			FString ReplacedURL =
				URL.Replace(TEXT("{z}"), *FString::FromInt(Zoom))
				   .Replace(TEXT("{x}"), *FString::FromInt(X))
				   .Replace(TEXT("{y}"), *FString::FromInt(Y));
			FString DownloadFile = FPaths::Combine(DownloadDir, FString::Format(TEXT("{0}-{1}-{2}-{3}.{4}"), { Layer, Zoom, X, Y, Format }));
			int XOffset = X - MinX;
			int YOffset = bMaxY_IsNorth ? MaxY - Y : Y - MinY;

			FString FileName = FString::Format(TEXT("{0}_x{1}_y{2}"), { Name, XOffset, YOffset });

			if (!Download::SynchronousFromURL(ReplacedURL, DownloadFile, bIsUserInitiated && NumTiles < 20)) return false;
			
			FString DecodedFile = DownloadFile;

			if (bDecodeMapbox)
			{
				DecodedFile = FPaths::Combine(DownloadDir, FString::Format(TEXT("{0}-{1}-{2}-{3}-decoded.{4}"), { Layer, Zoom, X, Y, Format }));
				if (!MapboxHelpers::DecodeMapboxThreeBands(DownloadFile, DecodedFile, bUseTerrariumFormula, bShowedDialog))
				{
					LCReporter::ShowOneError(
						FText::Format(
							LOCTEXT("HMXYZ::Fetch::Decode", "Could not decode file {0}."),
							FText::FromString(DownloadFile)
						),
						bShowedDialog
					);
					return bAllowInvalidTiles;
				}
			}

			if (Format.Contains("."))
			{
				FString ExtractionDir = FPaths::Combine(DownloadDir, FString::Format(TEXT("{0}-{1}-{2}-{3}"), { Layer, Zoom, X, Y }));
				FString ExtractParams = FString::Format(TEXT("x -aos \"{0}\" -o\"{1}\""), { DownloadFile, ExtractionDir });

				if (!Console::ExecProcess(TEXT("7z"), *ExtractParams)) return bAllowInvalidTiles;

				TArray<FString> TileFiles;
				FString ImageFormat = Format.Left(Format.Find(FString(".")));

				FFileManagerGeneric::Get().FindFilesRecursive(TileFiles, *ExtractionDir, *(FString("*.") + ImageFormat), true, false);

				if (TileFiles.Num() != 1)
				{
					LCReporter::ShowOneError(
						FText::Format(
							LOCTEXT("HMXYZ::Fetch::Extract", "Expected one {0} file inside the archive {1}, but found {2}."),
							FText::FromString(ImageFormat),
							FText::FromString(DownloadFile),
							FText::AsNumber(TileFiles.Num())
						),
						bShowedDialog
					);
					return bAllowInvalidTiles;
				}

				DecodedFile = TileFiles[0];
			}
			
			if (bGeoreferenceSlippyTiles)
			{
				double MinLong, MaxLong, MinLat, MaxLat;
				GDALInterface::XYZTileToEPSG3857(X, Y, Zoom, MinLong, MaxLat);
				GDALInterface::XYZTileToEPSG3857(X+1, Y+1, Zoom, MaxLong, MinLat);

				FString OutputFile = FPaths::Combine(OutputDir, FileName + ".tif");
				if (!GDALInterface::AddGeoreference(DecodedFile, OutputFile, "EPSG:3857", MinLong, MaxLong, MinLat, MaxLat)) return bAllowInvalidTiles;

				UE_LOG(LogImageDownloader, Log, TEXT("Adding file: %s"), *OutputFile);
				OutputFilesQueue.Enqueue(OutputFile);
			}
			else
			{
				FString OutputFile = FPaths::Combine(OutputDir, FileName + FPaths::GetExtension(DecodedFile, true));
				if (IFileManager::Get().Copy(*OutputFile, *DecodedFile) != COPY_OK) return bAllowInvalidTiles;

				UE_LOG(LogImageDownloader, Log, TEXT("Adding file: %s"), *OutputFile);
				OutputFilesQueue.Enqueue(OutputFile);
			}

			return true;
		}
	);

	if (bShowedDialog) delete(bShowedDialog);

	if (bSuccess)
	{
		FString Element;
		while (OutputFilesQueue.Dequeue(Element)) OutputFiles.Add(Element);
	}

	return bSuccess;
}

#undef LOCTEXT_NAMESPACE
