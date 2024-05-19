#pragma once

#include "SplineImporter/GDALImporter.h"

#include "Kismet/BlueprintAsyncActionBase.h"
#include "NodeGDALImport.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGDALImportDelegate);

UCLASS()
class LANDSCAPECOMBINATOR_API UNodeGDALImport : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FGDALImportDelegate OnImportFinished;

    UPROPERTY(BlueprintAssignable)
    FGDALImportDelegate OnFailure;
    
    UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "LandscapeCombinator")
    static UNodeGDALImport* Import(const UObject* WorldContextObject, AGDALImporter *GDALImporter);

    // UBlueprintAsyncActionBase interface
    virtual void Activate() override;
    //~UBlueprintAsyncActionBase interface

    virtual void TaskComplete(bool bSuccess);

private:

    UPROPERTY()
    TWeakObjectPtr<AGDALImporter> GDALImporter = nullptr;
};
