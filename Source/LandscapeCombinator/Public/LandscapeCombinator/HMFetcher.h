// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class LANDSCAPECOMBINATOR_API HMFetcher
{
public:
	HMFetcher() {};
	virtual ~HMFetcher() {};

	TArray<FString> OutputFiles;
	int OutputEPSG = 0;
	
	virtual void Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) = 0;

	HMFetcher* AndThen(HMFetcher* OtherFetcher);
	HMFetcher* AndRun(TFunction<void(HMFetcher*)> Lambda);
};

class HMAndThenFetcher : public HMFetcher
{
public:
	HMAndThenFetcher(HMFetcher* F1, HMFetcher* F2)
	{
		Fetcher1 = F1;
		Fetcher2 = F2;
	}
	~HMAndThenFetcher() { delete Fetcher1; delete Fetcher2; }
	
	HMFetcher* Fetcher1;
	HMFetcher* Fetcher2;
	
	void Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;
};

class HMAndRunFetcher : public HMFetcher
{
public:
	HMAndRunFetcher(HMFetcher* Fetcher0, TFunction<void(HMFetcher*)> Lambda0)
	{
		Fetcher = Fetcher0;
		Lambda = Lambda0;
	}
	~HMAndRunFetcher() { delete Fetcher; }
	
	HMFetcher* Fetcher;
	TFunction<void(HMFetcher*)> Lambda;
	
	void Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;
};

#undef LOCTEXT_NAMESPACE
