// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "LandscapeCombinator/LandscapeSpawner.h"
#include "LandscapeCombinator/LandscapePCGVolume.h"
#include "BuildingFromSpline/BuildingsFromSplines.h"
#include "SplineImporter/GDALImporter.h"
#include "LCCommon/LCGenerator.h"

#include "LandscapeCombination.generated.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

UCLASS(BlueprintType)
class LANDSCAPECOMBINATOR_API ALandscapeCombination : public AActor, public ILCGenerator
{
	GENERATED_BODY()

public:
	ALandscapeCombination() {};

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeCombination",
		meta = (DisplayPriority = "0")
	)
	/* Folder used to spawn the actors. If set, overrides the `Spawned Actors Path` settings from generators */
	FName SpawnedActorsPath = FName("GeneratedActors");

	UFUNCTION(CallInEditor, Category = "LandscapeCombination", meta = (DisplayPriority = "0"))
	virtual void GenerateActors() { Generate(SpawnedActorsPath); };

	UFUNCTION(CallInEditor, Category = "LandscapeCombination", meta = (DisplayPriority = "1"))
	virtual void DeleteActors() { Execute_Cleanup(this, false); };

	// Combinations hold pointers of generated objects through the child generators, so there is
	// no need to keep them separately
	virtual TArray<UObject*> GetGeneratedObjects() const override { return TArray<UObject*>(); }

	UPROPERTY(EditAnywhere, DuplicateTransient, BlueprintReadWrite, Category = "LandscapeCombination", meta = (DisplayPriority = "5", MustImplement = "/Script/LCCommon.LCGenerator"))
	TArray<AActor*> Generators;
	
	virtual void OnGenerate(FName SpawnedActorsPathOverride, TFunction<void(bool)> OnComplete) override;
	virtual bool Cleanup_Implementation(bool bSkipPrompt) override;

#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category = "Duplication", meta = (DisplayPriority = "0"))
	void Duplicate() { Duplicate(RenameLabelsAndTagsContaining, NewCombinationName); }

	virtual AActor* Duplicate(FName FromName, FName ToName) override;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duplication", meta = (DisplayPriority = "1"))
	FName RenameLabelsAndTagsContaining = "OldCombination";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duplication", meta = (DisplayPriority = "2"))
	FName NewCombinationName = "NewCombination";
#endif

};
