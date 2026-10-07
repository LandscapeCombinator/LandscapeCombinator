// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/Interface.h"
#include "Delegates/Delegate.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Async/Async.h"
#include "ConcurrencyHelpers/Concurrency.h"

#if WITH_EDITOR
#include "Subsystems/EditorActorSubsystem.h"
#include "Editor/EditorEngine.h"
#include "Editor.h"
#endif

#include "LCCommon/LogLCCommon.h"
#include "LCPositionBasedGeneration.h"

#include <atomic>
#include "LCGenerator.generated.h"

UENUM(BlueprintType, meta = (ScriptName = "LCGeneratorStatus"))
enum class EGeneratorStatus : uint8
{
	Idle,
	Generating,
	Error,
	Success
};

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

	virtual bool OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated) { return true; }

	bool Generate(FName SpawnedActorsPath, bool bIsUserInitiated);

	void GenerateFromGameThread(FName SpawnedActorsPath, bool bIsUserInitiated, TFunction<void(bool)> OnComplete = nullptr)
	{
		Async(EAsyncExecution::Thread, [this, SpawnedActorsPath, bIsUserInitiated, OnComplete]() {
			bool bSuccess = Generate(SpawnedActorsPath, bIsUserInitiated);
			if (OnComplete) OnComplete(bSuccess);
		});
	}

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, CallInEditor, Category = "LCGenerator")
	// return false if the user doesn't want to cleanup
	bool Cleanup(bool bSkipPrompt);

	void CancelGeneration() { Concurrency::SetCancelRequested(true); }

	bool DeleteGeneratedObjects(bool bSkipPrompt);
	bool DeleteGeneratedObjects_GameThread(bool bSkipPrompt);

#if WITH_EDITOR

	void ChangeRootPath(FName FromRootPath, FName ToRootPath);

	virtual AActor* Duplicate(FName FromName, FName ToName)
	{
		return GEditor->GetEditorSubsystem<UEditorActorSubsystem>()->DuplicateActor(Cast<AActor>(this));
	}
#endif

	EGeneratorStatus GetGeneratorStatus() const { return CurrentStatus; }
	void SetGeneratorStatus(EGeneratorStatus NewStatus) { CurrentStatus = NewStatus; }
	void ResetGeneratorStatus() { CurrentStatus = EGeneratorStatus::Idle; }

protected:
	EGeneratorStatus CurrentStatus = EGeneratorStatus::Idle;
	std::atomic<bool> bCancelRequested{false};
	EGeneratorStatus FailedStatus() const { return Concurrency::IsCancelRequested() ? EGeneratorStatus::Idle : EGeneratorStatus::Error; }
	TWeakObjectPtr<AActor> Self;

	void GenerationFinished(bool bSuccess)
	{
        TWeakObjectPtr<AActor> WeakSelf = Self;
		Concurrency::RunOnGameThread([this, WeakSelf, bSuccess]() {
			if (WeakSelf.IsValid())
			{
				if (bSuccess)
				{
					UE_LOG(LogLCCommon, Log, TEXT("Generation for %s finished successfully"), *WeakSelf->GetActorNameOrLabel())
				}
				else
				{
					UE_LOG(LogLCCommon, Error, TEXT("Generation for %s failed"), *WeakSelf->GetActorNameOrLabel())
				}
			}
			else
			{
				UE_LOG(LogLCCommon, Error, TEXT("ILCGenerator::GenerationFinished, calling actor became invalid (success: %d)"), bSuccess);
			}
		});
	}

};
