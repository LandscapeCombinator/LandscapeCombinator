// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Delegates/Delegate.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Subsystems/EditorActorSubsystem.h"

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

	virtual void Generate(FName SpawnedActorsPath) { Generate(SpawnedActorsPath, nullptr); }

	UFUNCTION(BlueprintImplementableEvent, Category = "LCGenerator")
	void OnGenerateBP(FName SpawnedActorsPath);
	virtual void OnGenerate(FName SpawnedActorsPathOverride, TFunction<void(bool)> OnComplete) { OnComplete(true); }

	void Generate(FName SpawnedActorsPath, TFunction<void(bool)> OnComplete) {
		Execute_OnGenerateBP(Cast<UObject>(this), SpawnedActorsPath);
		OnGenerate(SpawnedActorsPath, [this, OnComplete](bool bSuccess) {
			if (OnComplete) OnComplete(bSuccess);

#if WITH_EDITOR
			if (GEditor) GEditor->SelectActor(Cast<AActor>(this), true, true, true, true);
#endif
		});
	}

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, CallInEditor, Category = "LCGenerator")
	// return false is the user doesn't want to cleanup
	bool Cleanup(bool bSkipPrompt);

	bool DeleteGeneratedObjects(bool bSkipPrompt);
	void ChangeRootPath(FName FromRootPath, FName ToRootPath);

#if WITH_EDITOR
	virtual AActor* Duplicate(FName FromName, FName ToName)
	{
		return GEditor->GetEditorSubsystem<UEditorActorSubsystem>()->DuplicateActor(Cast<AActor>(this));
	}
#endif
};
