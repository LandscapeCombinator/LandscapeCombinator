// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintAsyncActionBase.h"
#include "LCCommon/LCGenerator.h"
#include "NodeGenerator.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSuccessDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFailureDelegate);

UCLASS()
class LCCOMMON_API UNodeGenerator : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FSuccessDelegate OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FFailureDelegate OnFailure;
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "LCGenerator")
	static UNodeGenerator* Generate(
		const UObject* WorldContextObject,
		TScriptInterface<ILCGenerator> Generator,
		FName SpawnedActorsPath
	);

	// UBlueprintAsyncActionBase interface
	virtual void Activate() override;
	//~UBlueprintAsyncActionBase interface

	virtual void TaskComplete(bool bSuccess);

private:

	UPROPERTY()
	TScriptInterface<ILCGenerator> Generator;

	UPROPERTY()
	FName SpawnedActorsPath;
};