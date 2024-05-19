#include "LandscapeCombinator/NodeGenerateBuildings.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"

UNodeGenerateBuildings* UNodeGenerateBuildings::GenerateBuildings(const UObject* WorldContextObject, ABuildingsFromSplines *BuildingsFromSplines)
{
    UNodeGenerateBuildings* Node = NewObject<UNodeGenerateBuildings>();
    Node->BuildingsFromSplines = BuildingsFromSplines;
    Node->RegisterWithGameInstance(WorldContextObject);
    Node->SetFlags(RF_StrongRefOnFrame);
    return Node;
}

void UNodeGenerateBuildings::Activate()
{
    if (BuildingsFromSplines.IsValid())
    {
        BuildingsFromSplines->GenerateBuildings();
        TaskComplete();
    }
}

void UNodeGenerateBuildings::TaskComplete()
{
    OnBuildingsGenerated.Broadcast();
    SetReadyToDestroy();
}
