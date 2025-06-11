// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "GameFramework/Pawn.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameFramework/Actor.h"
#include "LCBlueprintLibrary.generated.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

UCLASS()
class LCCOMMON_API ULCBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category="LandscapeCombinator")
	static void SortByLabel(UPARAM(ref) TArray<AActor*>& Actors);

	/* Similar to UGameplayStatics::GetAllActorsOfClassWithTag, but with the correct type */
	template<typename T>
	static void GetSortedActorsOfClassWithTag(const UWorld* World, FName Tag, TArray<T*>& OutActors);

	UFUNCTION(BlueprintCallable, Category="LandscapeCombinator")
	static FString Replace(FString Original, FString String1, FString String2);

	UFUNCTION(BlueprintCallable, Category="LandscapeCombinator")
	static FName ReplaceName(FName Original, FName String1, FName String2);

	UFUNCTION(BlueprintCallable, Category="LandscapeCombinator")
	static bool GetCmPerPixelForCRS(FString CRS, int &CmPerPixel);

	UFUNCTION(BlueprintCallable, Category="LandscapeCombinator")
	static bool GetEditorViewClientPosition(FVector &OutPosition);

	UFUNCTION(BlueprintCallable, Category="LandscapeCombinator")
	static bool GetFirstPlayerPosition(FVector &OutPosition);

	template<class T>
	static int GetRandomIndex(const TArray<T> &WeightedElements)
	{
		int NumWeights = WeightedElements.Num();
		if (NumWeights == 0) return -1;

		double TotalWeight = 0.0;
		for (const auto &Element: WeightedElements) TotalWeight += Element.Weight;

		double RandomValue = FMath::RandRange(0.0, TotalWeight);
		double CurrentWeight = 0.0;
		for (int i = 0; i < NumWeights; i++)
		{
			CurrentWeight += WeightedElements[i].Weight;
			if (RandomValue <= CurrentWeight) return i;
		}
		return -1;
	}

#if WITH_EDITOR

	UFUNCTION(BlueprintCallable, Category="LandscapeCombinator")
	static void SetFolderPath2(AActor *Actor, FName FolderPathOverride, FName FolderPath);

	static bool HasActor(UWorld &World, FFolder Folder);

	static void DeleteFolder(UWorld &World, FFolder Folder);

#endif
};

#undef LOCTEXT_NAMESPACE
