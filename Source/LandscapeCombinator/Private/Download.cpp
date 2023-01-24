#include "Download.h"
#include "Http.h"
#include "Logging.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void Download::FromURL(FString URL, FString File, TFunction<void(bool)> OnComplete)
{
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Downloading '%s' to '%s'"), *URL, *File);
	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(URL);
    Request->SetVerb("HEAD");
    Request->SetHeader("User-Agent", "X-UnrealEngine-Agent");
    Request->OnProcessRequestComplete().BindLambda([URL, File, OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
        if (bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode())) {
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

void Download::FromURLExpecting(FString URL, FString File, int32 ExpectedSize, TFunction<void(bool)> OnComplete)
{
    IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (PlatformFile.FileExists(*File) && ExpectedSize != 0 && PlatformFile.FileSize(*File) == ExpectedSize) {
        UE_LOG(LogLandscapeCombinator, Log, TEXT("File already exists, skipping download of '%s' to '%s' "), *URL, *File);
        if (OnComplete) OnComplete(true);
        return;
    }

	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(URL);
    Request->SetVerb("GET");
    Request->SetHeader("User-Agent", "X-UnrealEngine-Agent");
    Request->OnProcessRequestComplete().BindLambda([URL, File, OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
        bool FullSuccess = bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()) && FFileHelper::SaveArrayToFile(Response->GetContent(), *File);
        if (FullSuccess)
        {
    	    UE_LOG(LogLandscapeCombinator, Log, TEXT("Finished downloading '%s' to '%s'"), *URL, *File);
        }
        else
        {
	        UE_LOG(LogLandscapeCombinator, Error, TEXT("Error while downloading '%s' to '%s'"), *URL, *File);
        }
        OnComplete(FullSuccess);
    });
    Request->ProcessRequest();
}

#undef LOCTEXT_NAMESPACE
