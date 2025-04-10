// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/Interface.h"
#include "Delegates/Delegate.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "LCCommon/LogLCCommon.h"

#if WITH_EDITOR
#include "Subsystems/EditorActorSubsystem.h"
#endif

#include "LCPositionBasedGeneration.h"

#include "LCGenerator.generated.h"


UINTERFACE(Blueprintable)
class LCCOMMON_API ULCGenerator : public UInterface
{
	GENERATED_BODY()

};

class LCCOMMON_API ILCGenerator
{	
	GENERATED_BODY()

public:
	
	virtual TArray<UObject*> GetGeneratedObjects() const = 0;

	virtual bool ConfigureForTiles(int Zoom, int MinX, int MaxX, int MinY, int MaxY) {
		return false;
	}

	void Generate(FName SpawnedActorsPath, bool bIsUserInitiated) { Generate(SpawnedActorsPath, bIsUserInitiated, nullptr); }

	UFUNCTION(BlueprintImplementableEvent, Category = "LCGenerator")
	void OnGenerateBP(FName SpawnedActorsPath, bool bIsUserInitiated);
	virtual void OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated, TFunction<void(bool)> OnComplete) { OnComplete(true); }

	void Generate(FName SpawnedActorsPath, bool bIsUserInitiated,TFunction<void(bool)> OnComplete);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, CallInEditor, Category = "LCGenerator")
	// return false if the user doesn't want to cleanup
	bool Cleanup(bool bSkipPrompt);

	bool DeleteGeneratedObjects(bool bSkipPrompt);

#if WITH_EDITOR

	void ChangeRootPath(FName FromRootPath, FName ToRootPath);

	virtual AActor* Duplicate(FName FromName, FName ToName)
	{
		return GEditor->GetEditorSubsystem<UEditorActorSubsystem>()->DuplicateActor(Cast<AActor>(this));
	}
#endif

protected:
	void GenerationFinished(bool bSuccess)
	{
		if (AActor *Self = Cast<AActor>(this))
		{
			if (bSuccess)
			{
				UE_LOG(LogLCCommon, Log, TEXT("Generation for %s finished successfully"), *Cast<AActor>(this)->GetActorNameOrLabel())
			}
			else
			{
				UE_LOG(LogLCCommon, Error, TEXT("Generation for %s failed"), *Cast<AActor>(this)->GetActorNameOrLabel())
			}
		}
		else
		{
			UE_LOG(LogLCCommon, Error, TEXT("ILCGenerator::GenerationFinished called on an non-actor class (success: %d)"), bSuccess);
		}
	}

};
