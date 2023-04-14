// Copyright LandscapeCombinator. All Rights Reserved.

#include "Utils/Download.h"
#include "Utils/Logging.h"

#include "Http.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

namespace Download {

	float SleepSeconds = 0.05;
	float TimeoutSeconds = 10;

	bool SynchronousFromURL(FString URL, FString File)
	{
		UE_LOG(LogLandscapeCombinator, Log, TEXT("Downloading '%s' to '%s'"), *URL, *File);

		if (ExpectedSizeCache.Contains(URL))
		{
			UE_LOG(LogLandscapeCombinator, Log, TEXT("Cache says expected size for '%s' is '%d'"), *URL, ExpectedSizeCache[URL]);
			return SynchronousFromURLExpecting(URL, File, ExpectedSizeCache[URL]);
		}
		else
		{
			TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
			Request->SetURL(URL);
			Request->SetVerb("HEAD");
			Request->SetHeader("User-Agent", "X-UnrealEngine-Agent");

			bool bIsComplete = false;
			int32 ExpectedSize = 0;

			Request->OnProcessRequestComplete().BindLambda([URL, File, &ExpectedSize, &bIsComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
				if (bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
				{
					ExpectedSize = FCString::Atoi(*Response->GetHeader("Content-Length"));
				}
				bIsComplete = true;
			});
			Request->ProcessRequest();

			float StartTime = FPlatformTime::Seconds();
			while (!bIsComplete && FPlatformTime::Seconds() - StartTime <= TimeoutSeconds)
			{
				FPlatformProcess::Sleep(SleepSeconds);
			}

			Request->CancelRequest();

			return SynchronousFromURLExpecting(URL, File, ExpectedSize);
		}
	}

	bool SynchronousFromURLExpecting(FString URL, FString File, int32 ExpectedSize)
	{
		IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (PlatformFile.FileExists(*File) && ExpectedSize != 0 && PlatformFile.FileSize(*File) == ExpectedSize)
		{
			UE_LOG(LogLandscapeCombinator, Log, TEXT("File already exists with the correct size, skipping download of '%s' to '%s' "), *URL, *File);
			return true;
		}

		bool bDownloadResult = false;
		bool bIsComplete = false;

		TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(URL);
		Request->SetVerb("GET");
		Request->SetHeader("User-Agent", "X-UnrealEngine-Agent");
		Request->OnProcessRequestComplete().BindLambda([URL, File, &bDownloadResult, &bIsComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
		{
			IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			bool DownloadSuccess = bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode());
			bool SavedFile = false;
			if (DownloadSuccess)
			{
				SavedFile = FFileHelper::SaveArrayToFile(Response->GetContent(), *File);
				if (SavedFile)
				{
					AddExpectedSize(URL, PlatformFile.FileSize(*File));
					bDownloadResult = true;
					bIsComplete = true;

					UE_LOG(LogLandscapeCombinator, Log, TEXT("Finished downloading '%s' to '%s'"), *URL, *File);
				}
				else
				{
					bDownloadResult = false;
					bIsComplete = true;
					UE_LOG(LogLandscapeCombinator, Error, TEXT("Error while saving '%s' to '%s'"), *URL, *File);
				}
			}
			else
			{
				UE_LOG(LogLandscapeCombinator, Error, TEXT("Error while downloading '%s' to '%s'"), *URL, *File);
				UE_LOG(LogLandscapeCombinator, Error, TEXT("Request was not successful. Error %d."), Response->GetResponseCode());
				bDownloadResult = false;
				bIsComplete = true;
			}
		});
		Request->ProcessRequest();
		

		float StartTime = FPlatformTime::Seconds();
		while (!bIsComplete && FPlatformTime::Seconds() - StartTime <= TimeoutSeconds)
		{
			FPlatformProcess::Sleep(SleepSeconds);
		}
		
		Request->CancelRequest();

		return bDownloadResult;
	}

	void FromURL(FString URL, FString File, TFunction<void(bool)> OnComplete)
	{
		UE_LOG(LogLandscapeCombinator, Log, TEXT("Downloading '%s' to '%s'"), *URL, *File);

		if (ExpectedSizeCache.Contains(URL))
		{
			UE_LOG(LogLandscapeCombinator, Log, TEXT("Cache says expected size for '%s' is '%d'"), *URL, ExpectedSizeCache[URL]);
			FromURLExpecting(URL, File, ExpectedSizeCache[URL], OnComplete);
		}
		else
		{
			TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
			Request->SetURL(URL);
			Request->SetVerb("HEAD");
			Request->SetHeader("User-Agent", "X-UnrealEngine-Agent");
			Request->OnProcessRequestComplete().BindLambda([URL, File, OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
				if (bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
				{
					int32 ExpectedSize = FCString::Atoi(*Response->GetHeader("Content-Length"));
					FromURLExpecting(URL, File, ExpectedSize, OnComplete);
				}
				else
				{
					FromURLExpecting(URL, File, 0, OnComplete);
				}
			});
			Request->ProcessRequest();
		}

	}

	void FromURLExpecting(FString URL, FString File, int32 ExpectedSize, TFunction<void(bool)> OnComplete)
	{
		IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (PlatformFile.FileExists(*File) && ExpectedSize != 0 && PlatformFile.FileSize(*File) == ExpectedSize)
		{
			UE_LOG(LogLandscapeCombinator, Log, TEXT("File already exists with the correct size, skipping download of '%s' to '%s' "), *URL, *File);
			if (OnComplete) OnComplete(true);
			return;
		}

		TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(URL);
		Request->SetVerb("GET");
		Request->SetHeader("User-Agent", "X-UnrealEngine-Agent");
		Request->OnProcessRequestComplete().BindLambda([URL, File, OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
		{
			IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			bool DownloadSuccess = bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode());
			bool SavedFile = false;
			if (DownloadSuccess)
			{
				SavedFile = FFileHelper::SaveArrayToFile(Response->GetContent(), *File);
				if (SavedFile)
				{
					AddExpectedSize(URL, PlatformFile.FileSize(*File));
					UE_LOG(LogLandscapeCombinator, Log, TEXT("Finished downloading '%s' to '%s'"), *URL, *File);
				}
				else
				{
					UE_LOG(LogLandscapeCombinator, Error, TEXT("Error while saving '%s' to '%s'"), *URL, *File);
				}
			}
			else
			{
				UE_LOG(LogLandscapeCombinator, Error, TEXT("Error while downloading '%s' to '%s'"), *URL, *File);
				UE_LOG(LogLandscapeCombinator, Error, TEXT("Request was not successful. Error %d."), Response->GetResponseCode());
			}
			if (OnComplete) OnComplete(DownloadSuccess && SavedFile);
		});
		Request->ProcessRequest();
	}

	void AddExpectedSize(FString URL, int32 ExpectedSize)
	{
		if (!ExpectedSizeCache.Contains(URL))
		{
			ExpectedSizeCache.Add(URL, ExpectedSize);
			SaveExpectedSizeCache();
		}
	}

	FString ExpectedSizeCacheFile()
	{
		IPlatformFile::GetPlatformPhysical().CreateDirectory(*FPaths::ProjectSavedDir());
		return FPaths::Combine(FPaths::ProjectSavedDir(), "ExpectedSizeCache"); 
	}

	void LoadExpectedSizeCache()
	{
		FString CacheFile = ExpectedSizeCacheFile();
		UE_LOG(LogLandscapeCombinator, Log, TEXT("Loading expected size cache from '%s'"), *CacheFile);
		
		FArchive* FileReader = IFileManager::Get().CreateFileReader(*CacheFile);
		if (FileReader)
		{
			*FileReader << ExpectedSizeCache;
			FileReader->Close();
		}
	}

	void SaveExpectedSizeCache()
	{
		FString CacheFile = ExpectedSizeCacheFile();
		UE_LOG(LogLandscapeCombinator, Log, TEXT("Saving expected size cache to '%s'"), *CacheFile);
		FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*CacheFile);
		if (FileWriter)
		{
			*FileWriter << ExpectedSizeCache;
			if (FileWriter->Close()) return;
		}

		UE_LOG(LogLandscapeCombinator, Error, TEXT("Failed to save expected size cache to '%s'"), *CacheFile);
	}
}

#undef LOCTEXT_NAMESPACE
