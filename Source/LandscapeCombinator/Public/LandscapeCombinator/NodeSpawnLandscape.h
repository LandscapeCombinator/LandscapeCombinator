#pragma once

#include "LandscapeSpawner.h"

#include "Kismet/BlueprintAsyncActionBase.h"
#include "NodeSpawnLandscape.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLandscapeDelegate, ALandscape*, CreatedLandscape);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLandscapeFailureDelegate);

UCLASS()
class LANDSCAPECOMBINATOR_API UNodeSpawnLandscape : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()

#if WITH_EDITORONLY_DATA

public:

    UPROPERTY(BlueprintAssignable)
    FLandscapeDelegate OnLandscapeCreated;

    UPROPERTY(BlueprintAssignable)
    FLandscapeFailureDelegate OnFailure;

private:

    UPROPERTY()
    TWeakObjectPtr<ALandscapeSpawner> LandscapeSpawner = nullptr;

#endif

#if WITH_EDITOR

public:
    
    UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "LandscapeCombinator")
    static UNodeSpawnLandscape* SpawnLandscape(const UObject* WorldContextObject, ALandscapeSpawner *LandscapeSpawnerIn);

    // UBlueprintAsyncActionBase interface
    virtual void Activate() override;
    //~UBlueprintAsyncActionBase interface

    virtual void TaskComplete(ALandscape *CreatedLandscape);

#endif

};
