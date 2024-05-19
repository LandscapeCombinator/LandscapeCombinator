#pragma once

#include "BuildingFromSpline/BuildingsFromSplines.h"

#include "Kismet/BlueprintAsyncActionBase.h"
#include "NodeGenerateBuildings.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGenerateBuildingsDelegate);

UCLASS()
class LANDSCAPECOMBINATOR_API UNodeGenerateBuildings : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FGenerateBuildingsDelegate OnBuildingsGenerated;
    
    UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "LandscapeCombinator")
    static UNodeGenerateBuildings* GenerateBuildings(const UObject* WorldContextObject, ABuildingsFromSplines *BuildingsFromSplines);

    // UBlueprintAsyncActionBase interface
    virtual void Activate() override;
    //~UBlueprintAsyncActionBase interface

    virtual void TaskComplete();

private:

    UPROPERTY()
    TWeakObjectPtr<ABuildingsFromSplines> BuildingsFromSplines = nullptr;
};
