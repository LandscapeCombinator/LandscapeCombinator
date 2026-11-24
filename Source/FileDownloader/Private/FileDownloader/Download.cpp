// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "FileDownloader/Download.h"
#include "FileDownloader/LogFileDownloader.h"
#include "FileDownloader/FileDownloaderStyle.h"

#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/ConcurrencySettings.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Async/Async.h"
#include "Http.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h" 
#include "Widgets/Notifications/SProgressBar.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/MessageDialog.h"
#include "HAL/ThreadManager.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

TMap<FString, int64> ExpectedSizeCache;
FCriticalSection ExpectedSizeCacheLock;

static bool FindExpectedSize(const FString& URL, int64& OutExpectedSize)
{
	FScopeLock Lock(&ExpectedSizeCacheLock);
	if (const int64* Size = ExpectedSizeCache.Find(URL))
	{
		OutExpectedSize = *Size;
		return true;
	}
	return false;
}

FString Shorten(FString Input)
{
	if (Input.Len() > 83)
	{
		return Input.Left(40) + "..." + Input.Right(40);
	}
	else
	{
		return Input;
	}
}

bool Download::SynchronousFromURL(FString URL, FString File, bool bProgress)
{
	if (IsInGameThread())
	{
		LCReporter::ShowError(
			LOCTEXT("Download::SynchronousFromURL", "Synchronous download must be run on a background thread.")
		);
		return false;
	}

	UE_LOG(LogFileDownloader, Log, TEXT("Downloading '%s' to '%s'"), *URL, *File);

	int64 CachedExpectedSize;
	if (FindExpectedSize(URL, CachedExpectedSize))
	{
		UE_LOG(LogFileDownloader, Log, TEXT("Cache says expected size for '%s' is '%lld'"), *URL, CachedExpectedSize);
		return SynchronousFromURLExpecting(URL, File, bProgress, CachedExpectedSize);
	}
	else
	{
		UE_LOG(LogFileDownloader, Log, TEXT("No size is cached for '%s'"), *URL);

		TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(URL);
		Request->SetVerb("HEAD");
		Request->SetHeader("User-Agent", "X-UnrealEngine-Agent");
		Request->SetTimeout(10);

		FEvent* SyncEvent = FPlatformProcess::GetSynchEventFromPool(false);
		if (!SyncEvent)
		{
			LCReporter::ShowError(
				LOCTEXT("SynchronousFromURLSync", "Failed to create sync event for synchronous download.")
			);
			return false;
		}

		TSharedRef<TAtomic<int64>, ESPMode::ThreadSafe> SharedExpectedSize = MakeShared<TAtomic<int64>, ESPMode::ThreadSafe>(0);
		TSharedRef<TAtomic<bool>, ESPMode::ThreadSafe> bTriggered = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);
		Request->OnProcessRequestComplete().BindLambda([SharedExpectedSize, bTriggered, SyncEvent](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
			if (bTriggered->Exchange(true)) return;

			if (bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
			{
				*SharedExpectedSize = FCString::Atoi64(*Response->GetHeader("Content-Length"));
			}
			SyncEvent->Trigger();
		});

		Request->ProcessRequest();
		double StartTime = FPlatformTime::Seconds();
		UE_LOG(LogFileDownloader, Log, TEXT("Waiting for Content-Length request to complete (%.0fs timeout)"), GetDefault<UConcurrencySettings>()->ContentLengthTimeoutSeconds);
		SyncEvent->Wait((uint32)(GetDefault<UConcurrencySettings>()->ContentLengthTimeoutSeconds * 1000));
		UE_LOG(LogFileDownloader, Log, TEXT("Finished waiting for Content-Length request to complete"));
		Request->CancelRequest();

		return SynchronousFromURLExpecting(URL, File, bProgress, SharedExpectedSize->Load());
	}
}

bool Download::SynchronousFromURLExpecting(FString URL, FString File, bool bProgress, int64 ExpectedSize)
{
	if (IsInGameThread())
	{
		LCReporter::ShowError(
			LOCTEXT("Download::SynchronousFromURLExpecting", "Synchronous download must be run on a background thread.")
		);
		return false;
	}

	IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.FileExists(*File) && ExpectedSize != 0 && PlatformFile.FileSize(*File) == ExpectedSize)
	{
		UE_LOG(LogFileDownloader, Log, TEXT("File already exists with the correct size, skipping download of '%s' to '%s' "), *URL, *File);
		return true;
	}

	struct FDownloadState
	{
		TAtomic<bool> bTriggered { false };
		TAtomic<bool> bAbandoned { false };
		TAtomic<bool> bResult { false };
	};
	TSharedRef<FDownloadState, ESPMode::ThreadSafe> State = MakeShared<FDownloadState, ESPMode::ThreadSafe>();

	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(URL);
	Request->SetVerb("GET");
	Request->SetHeader("User-Agent", "X-UnrealEngine-Agent");
	Request->SetTimeout(100);

	FEvent* SyncEvent = FPlatformProcess::GetSynchEventFromPool(false);
	if (!SyncEvent)
	{
		LCReporter::ShowError(
			LOCTEXT("SynchronousFromURLExpectingSync", "Failed to create sync event for synchronous download.")
		);
		return false;
	}

	Request->OnProcessRequestComplete().BindLambda([URL, File, State, SyncEvent](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
	{
		if (State->bTriggered.Exchange(true)) return;
		if (State->bAbandoned) { SyncEvent->Trigger(); return; }
		
		IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		bool DownloadSuccess = bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode());
		bool SavedFile = false;
		if (DownloadSuccess)
		{
			SavedFile = FFileHelper::SaveArrayToFile(Response->GetContent(), *File);
			if (SavedFile)
			{
				AddExpectedSize(URL, PlatformFile.FileSize(*File));
				State->bResult = true;
				UE_LOG(LogFileDownloader, Log, TEXT("Finished downloading '%s' to '%s'"), *URL, *File);
			}
			else
			{
				UE_LOG(LogFileDownloader, Error, TEXT("Error while saving '%s' to '%s'"), *URL, *File);
			}
		}
		else
		{
			UE_LOG(LogFileDownloader, Error, TEXT("Error while downloading '%s' to '%s'"), *URL, *File);
			if (Response)
			{
				UE_LOG(LogFileDownloader, Error, TEXT("Request was not successful. Error %d."), Response->GetResponseCode());
			}
		}
		SyncEvent->Trigger();
	});

	Request->ProcessRequest();
	double StartTime = FPlatformTime::Seconds();
	UE_LOG(LogFileDownloader, Log, TEXT("Waiting for download to complete (%.0fs timeout)"), GetDefault<UConcurrencySettings>()->DownloadTimeoutSeconds);
	bool bAborted = false; while (!SyncEvent->Wait(100)) { if (Concurrency::IsCancelRequested() || IsEngineExitRequested() || FPlatformTime::Seconds() - StartTime > GetDefault<UConcurrencySettings>()->DownloadTimeoutSeconds) { bAborted = true; break; } }
	UE_LOG(LogFileDownloader, Log, TEXT("Finished waiting for download to complete"));
	if (bAborted) State->bAbandoned = true;
	Request->CancelRequest();

	return !bAborted && State->bResult;
}

void Download::FromURL(FString URL, FString File, bool bProgress, TFunction<void(bool)> OnComplete)
{
	UE_LOG(LogFileDownloader, Log, TEXT("Downloading from URL '%s' to '%s'"), *URL, *File);

	int64 CachedExpectedSize;
	if (FindExpectedSize(URL, CachedExpectedSize))
	{
		UE_LOG(LogFileDownloader, Log, TEXT("Cache says expected size for '%s' is '%lld'"), *URL, CachedExpectedSize);
		FromURLExpecting(URL, File, bProgress, CachedExpectedSize, OnComplete);
	}
	else
	{
		UE_LOG(LogFileDownloader, Log, TEXT("No cache for '%s'"), *URL);
		TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(URL);
		Request->SetVerb("HEAD");
		Request->SetHeader("User-Agent", "X-UnrealEngine-Agent");
		bool *bTriggered = new bool(false);

		Request->OnProcessRequestComplete().BindLambda([bProgress, URL, File, OnComplete, bTriggered](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
			if (*bTriggered) return;
			*bTriggered = true;

			if (bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
			{
				int64 ExpectedSize = FCString::Atoi64(*Response->GetHeader("Content-Length"));
				FromURLExpecting(URL, File, bProgress, ExpectedSize, OnComplete);
			}
			else
			{
				FromURLExpecting(URL, File, bProgress, 0, OnComplete);
			}
		});
		Request->ProcessRequest();
	}
}

void Download::DownloadMany(TArray<FString> URLs, FString Directory, TFunction<void(TArray<FString>)> OnComplete)
{
	TArray<FString> Files;
	for (auto& URL : URLs)
	{
		TArray<FString> Elements;
		int NumElements = URL.ParseIntoArray(Elements, TEXT("/"), true);

		if (NumElements == 0)
		{
			LCReporter::ShowError(
				FText::Format(
					LOCTEXT("Download::DownloadMany", "URL could not be parsed to extract a file name: {0}"),
					FText::FromString(URL)
				)
			); 
			if (OnComplete) OnComplete(TArray<FString>());
			return;
		}

		FString FileName = Elements[NumElements - 1];
		FString CleanFileName = FPaths::GetCleanFilename(FileName);
		
		FString File = FPaths::Combine(Directory, FPaths::GetCleanFilename(FileName));
		Files.Add(File);
	}

	return DownloadMany(URLs, Files, OnComplete);
}

void Download::DownloadMany(TArray<FString> URLs, TArray<FString> Files, TFunction<void(TArray<FString>)> OnComplete)
{
	Concurrency::RunMany(
		URLs.Num(),
		[URLs, Files](int i, TFunction<void (bool)> OnCompleteElement)
		{
			Download::FromURL(URLs[i], Files[i], true, OnCompleteElement);
		},
		[OnComplete, Files](bool bSuccess)
		{
			if (bSuccess)
			{
				if (OnComplete) OnComplete(Files);
			}
			else
			{
				if (OnComplete) OnComplete(TArray<FString>());
			}
		}
	);
}

TArray<FString> Download::DownloadManyAndWait(TArray<FString> URLs, FString Directory, bool bProgress)
{
	TArray<FString> Files;
	for (auto& URL : URLs)
	{
		TArray<FString> Elements;
		int NumElements = URL.ParseIntoArray(Elements, TEXT("/"), true);

		if (NumElements == 0)
		{
			LCReporter::ShowError(
				FText::Format(
					LOCTEXT("Download::DownloadMany", "URL could not be parsed to extract a file name: {0}"),
					FText::FromString(URL)
				)
			);
			return TArray<FString>();
		}

		FString FileName = Elements[NumElements - 1];
		FString CleanFileName = FPaths::GetCleanFilename(FileName);
		
		FString File = FPaths::Combine(Directory, FPaths::GetCleanFilename(FileName));
		Files.Add(File);
	}

	return DownloadManyAndWait(URLs, Files, bProgress);
}

TArray<FString> Download::DownloadManyAndWait(TArray<FString> URLs, TArray<FString> Files, bool bProgress)
{
	if (Concurrency::RunManyAndWait(
		true,
		URLs.Num(),
		[URLs, Files, bProgress](int i)
		{
			return Download::SynchronousFromURL(URLs[i], Files[i], bProgress);
		}
	))
	{
		return Files;
	}
	else
	{
		return TArray<FString>();
	}
}

void Download::FromURLExpecting(FString URL, FString File, bool bProgress, int64 ExpectedSize, TFunction<void(bool)> OnComplete)
{
	auto Action = [=]()
	{
		TSharedPtr<int64> Downloaded = MakeShared<int64>(0);

		IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		if (ExpectedSize != 0 && PlatformFile.FileExists(*File))
		{
			int64 FileSize = PlatformFile.FileSize(*File);
			if (FileSize == ExpectedSize)
			{
				UE_LOG(LogFileDownloader, Log, TEXT("File already exists with the correct size, skipping download of '%s' to '%s' "), *URL, *File);
				if (OnComplete) OnComplete(true);
				return;
			}
			else
			{
				UE_LOG(LogFileDownloader, Log, TEXT("File already exists with size %lld but the expected size is %lld. Redownloading"), FileSize, ExpectedSize);
			}
		}

#if WITH_EDITOR
		TSharedPtr<SWindow> Window;
		
		if (bProgress)
		{
			Window = SNew(SWindow)
				.SizingRule(ESizingRule::Autosized)
				.AutoCenter(EAutoCenter::PrimaryWorkArea)
				.Title(LOCTEXT("DownloadProgress", "Download Progress"));
		}
#endif

		TSharedPtr<IHttpRequest> Request = FHttpModule::Get().CreateRequest().ToSharedPtr();
		Request->SetURL(URL);
		Request->SetVerb("GET");
		Request->SetHeader("User-Agent", "X-UnrealEngine-Agent");
		
#if UE_VERSION_OLDER_THAN(5, 4, 0)
		Request->OnRequestProgress().BindLambda([Downloaded](FHttpRequestPtr Request, int Sent, int Received) {
#else
		Request->OnRequestProgress64().BindLambda([Downloaded](FHttpRequestPtr Request, int64 Sent, int64 Received) {
#endif
			*Downloaded = Received;
		});
		bool *bTriggered = new bool(false);

#if WITH_EDITOR
		Request->OnProcessRequestComplete().BindLambda([URL, File, OnComplete, Window, bTriggered, bProgress](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
#else
		Request->OnProcessRequestComplete().BindLambda([URL, File, OnComplete, bTriggered, bProgress](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
#endif
		{
			if (*bTriggered) return;
			*bTriggered = true;

			IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			bool DownloadSuccess = bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode());
			bool SavedFile = false;
			if (DownloadSuccess)
			{
				SavedFile = FFileHelper::SaveArrayToFile(Response->GetContent(), *File);
				if (SavedFile)
				{
					AddExpectedSize(URL, PlatformFile.FileSize(*File));
					UE_LOG(LogFileDownloader, Log, TEXT("Finished downloading '%s' to '%s'"), *URL, *File);
				}
				else
				{
					UE_LOG(LogFileDownloader, Error, TEXT("Error while saving '%s' to '%s'"), *URL, *File);
				}
			}
			else
			{
				UE_LOG(LogFileDownloader, Error, TEXT("Error while downloading '%s' to '%s'"), *URL, *File);
				if (Response.IsValid())
				{
					UE_LOG(LogFileDownloader, Error, TEXT("Request was not successful. Error %d."), Response->GetResponseCode());
				}
			}
			
#if WITH_EDITOR
			if (bProgress)
			{
				Window->RequestDestroyWindow();
			}
#endif

			if (OnComplete) OnComplete(DownloadSuccess && SavedFile);
		});


		Request->ProcessRequest();

#if WITH_EDITOR
		if (bProgress)
		{
			Window->SetContent(
				SNew(SBox).Padding(FMargin(30, 30, 30, 30))
				[
					SNew(SVerticalBox)
						+SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Text(
								FText::Format(
									LOCTEXT("DowloadingURL", "Dowloading {0} to {1}."),
									FText::FromString(Shorten(URL)),
									FText::FromString(Shorten(File))
								)
							).Font(FFileDownloaderStyle::RegularFont())
						]
						+SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 0, 0, 20))
						[
							SNew(SProgressBar)
								.Percent_Lambda([Downloaded, ExpectedSize, Request]() {
										if (ExpectedSize) return (double)*Downloaded / ExpectedSize;
										return (double)*Downloaded / MAX_int32;
									})
						]
						+SVerticalBox::Slot().AutoHeight().HAlign(EHorizontalAlignment::HAlign_Center)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().HAlign(EHorizontalAlignment::HAlign_Center)
								[
									SNew(SButton)
										.OnClicked_Lambda([Window, Request]()->FReply {
											Request->CancelRequest();
											Window->RequestDestroyWindow();
											return FReply::Handled();
										})
										[
											SNew(STextBlock).Font(FFileDownloaderStyle::RegularFont()).Text(FText::FromString(" Cancel "))
										]
								]
						]
				]
			);

			Window->SetOnWindowClosed(FOnWindowClosed::CreateLambda([Request](const TSharedRef<SWindow>& Window) {
				Request->CancelRequest();
			}));

			FSlateApplication::Get().AddWindow(Window.ToSharedRef());
		}


		if (bProgress && Request->GetStatus() != EHttpRequestStatus::Processing)
		{
			Window->RequestDestroyWindow();
		}
#endif
	};

	if (bProgress) Concurrency::RunOnGameThreadThrottled(Action);
	else Action();
}

void Download::AddExpectedSize(FString URL, int64 ExpectedSize)
{
	FScopeLock Lock(&ExpectedSizeCacheLock);
	if (!ExpectedSizeCache.Contains(URL))
	{
		ExpectedSizeCache.Add(URL, ExpectedSize);
		SaveExpectedSizeCache();
	}
}

FString Download::ExpectedSizeCacheFile()
{
	IPlatformFile::GetPlatformPhysical().CreateDirectory(*FPaths::ProjectSavedDir());
	return FPaths::Combine(FPaths::ProjectSavedDir(), "ExpectedSizeCache64"); 
}

void Download::LoadExpectedSizeCache()
{
	FString CacheFile = ExpectedSizeCacheFile();
	//UE_LOG(LogFileDownloader, Log, TEXT("Loading expected size cache from '%s'"), *CacheFile);
		
	TUniquePtr<FArchive> FileReader(IFileManager::Get().CreateFileReader(*CacheFile));
	if (FileReader)
	{
		FScopeLock Lock(&ExpectedSizeCacheLock);
		*FileReader << ExpectedSizeCache;
		FileReader->Close();
	}
}

void Download::SaveExpectedSizeCache()
{
	FString CacheFile = ExpectedSizeCacheFile();
	//UE_LOG(LogFileDownloader, Log, TEXT("Saving expected size cache to '%s'"), *CacheFile);
	TUniquePtr<FArchive> FileWriter(IFileManager::Get().CreateFileWriter(*CacheFile));
	if (FileWriter)
	{
		FScopeLock Lock(&ExpectedSizeCacheLock);
		*FileWriter << ExpectedSizeCache;
		if (FileWriter->Close()) return;
	}

	UE_LOG(LogFileDownloader, Error, TEXT("Failed to save expected size cache to '%s'"), *CacheFile);
}

#undef LOCTEXT_NAMESPACE
