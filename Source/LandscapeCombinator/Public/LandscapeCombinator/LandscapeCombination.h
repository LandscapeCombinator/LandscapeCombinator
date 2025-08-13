// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "LandscapeCombinator/LandscapeSpawner.h"
#include "LandscapeCombinator/LandscapePCGVolume.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"
#include "BuildingsFromSplines/BuildingsFromSplines.h"
#include "SplineImporter/GDALImporter.h"
#include "LCCommon/LCGenerator.h"
#include "LCCommon/LCContinuousGeneration.h"

#include "LandscapeCombination.generated.h"

USTRUCT(BlueprintType)
struct FGeneratorWrapper
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeneratorWrapper", meta = (DisplayPriority = "0"))
	bool bIsEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeneratorWrapper", meta = (DisplayPriority = "1", MustImplement = "/Script/LCCommon.LCGenerator"))
	TSoftObjectPtr<AActor> Generator;
};


UCLASS(BlueprintType)
class LANDSCAPECOMBINATOR_API ALandscapeCombination : public AActor, public ILCGenerator
{
	GENERATED_BODY()

public:
	ALandscapeCombination()
	{
		ContinuousGeneration = CreateDefaultSubobject<ULCContinuousGeneration >(TEXT("ContinuousGeneration"));
	};

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeCombination",
		meta = (DisplayPriority = "0")
	)
	/* Folder used to spawn the actors. If set, overrides the `Spawned Actors Path` settings from generators */
	FName SpawnedActorsPath = FName("GeneratedActors");

	UFUNCTION(CallInEditor, Category = "LandscapeCombination", meta = (DisplayPriority = "0"))
	virtual void GenerateActors() { GenerateFromGameThread(SpawnedActorsPath, true); }

	UFUNCTION(CallInEditor, Category = "LandscapeCombination", meta = (DisplayPriority = "0"))
	virtual void GenerateActorsNoPrompt() { GenerateFromGameThread(SpawnedActorsPath, false); }

	UFUNCTION(CallInEditor, Category = "LandscapeCombination", meta = (DisplayPriority = "1"))
	virtual void DeleteActors() { Execute_Cleanup(this, false); };

	// Combinations hold pointers of generated objects through the child generators, so there is
	// no need to keep them separately
	virtual TArray<UObject*> GetGeneratedObjects() const override { return TArray<UObject*>(); }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeCombination", meta = (DisplayPriority = "5"))
	TArray<FGeneratorWrapper> Generators;
	
	virtual bool OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated) override;
	virtual bool Cleanup_Implementation(bool bSkipPrompt) override;

#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category = "Duplication", meta = (DisplayPriority = "0"))
	void Duplicate() { Duplicate(RenameLabelsAndTagsContaining, NewCombinationName); }

	virtual AActor* Duplicate(FName FromName, FName ToName) override;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeCombination",meta = (DisplayPriority = "0"))
	/* Save level when one generator finishes successfully */
	bool bSaveAfterEachGenerator = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duplication", meta = (DisplayPriority = "1"))
	FName RenameLabelsAndTagsContaining = "OldCombination";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duplication", meta = (DisplayPriority = "2"))
	FName NewCombinationName = "NewCombination";
#endif

	UPROPERTY(
		VisibleAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner",
		meta = (DisplayPriority = "11", ShowOnlyInnerProperties)
	)
	TObjectPtr<ULCContinuousGeneration> ContinuousGeneration = nullptr;

protected:
	TMap<AActor*, double> TimeSpent;
};
