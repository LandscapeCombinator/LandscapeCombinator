#include "LandscapeCombinator/NodeGDALImport.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"

UNodeGDALImport* UNodeGDALImport::Import(const UObject* WorldContextObject, AGDALImporter *GDALImporter)
{
    UNodeGDALImport* Node = NewObject<UNodeGDALImport>();
    Node->GDALImporter = GDALImporter;
    Node->RegisterWithGameInstance(WorldContextObject);
    Node->SetFlags(RF_StrongRefOnFrame);
    return Node;
}

void UNodeGDALImport::Activate()
{
    if (GDALImporter.IsValid())
    {
        GDALImporter->Import([this](bool bSuccess) { TaskComplete(bSuccess); });
    }
}

void UNodeGDALImport::TaskComplete(bool bSuccess)
{
    if (bSuccess) OnImportFinished.Broadcast();
    else OnFailure.Broadcast();
    SetReadyToDestroy();
}
