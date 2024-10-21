#include "LandscapeCombinator/NodeSpawnLandscape.h"

#if WITH_EDITOR

#include "LandscapeCombinator/LogLandscapeCombinator.h"

UNodeSpawnLandscape* UNodeSpawnLandscape::SpawnLandscape(const UObject* WorldContextObject, ALandscapeSpawner *LandscapeSpawnerIn)
{
    UNodeSpawnLandscape* Node = NewObject<UNodeSpawnLandscape>();
    Node->LandscapeSpawner = LandscapeSpawnerIn;
    Node->RegisterWithGameInstance(WorldContextObject);
    Node->SetFlags(RF_StrongRefOnFrame);
    return Node;
}

void UNodeSpawnLandscape::Activate()
{
    if (LandscapeSpawner.IsValid())
    {
        LandscapeSpawner->SpawnLandscape([this](ALandscape *CreatedLandscape) { TaskComplete(CreatedLandscape); });
    }
}

void UNodeSpawnLandscape::TaskComplete(ALandscape *CreatedLandscape)
{
    if (IsValid(CreatedLandscape)) OnLandscapeCreated.Broadcast(CreatedLandscape);
    else OnFailure.Broadcast();
    SetReadyToDestroy();
}

#endif
