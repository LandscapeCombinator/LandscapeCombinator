#include "LCCommon/NodeGenerator.h"

UNodeGenerator* UNodeGenerator::Generate(
		const UObject* WorldContextObject,
		TScriptInterface<ILCGenerator> Generator,
		FName SpawnedActorsPath)
{
	UNodeGenerator* Node = NewObject<UNodeGenerator>();
	Node->SpawnedActorsPath = SpawnedActorsPath;
	Node->Generator = Generator;
	Node->RegisterWithGameInstance(WorldContextObject);
	Node->SetFlags(RF_StrongRefOnFrame);
	return Node;
}

void UNodeGenerator::Activate()
{
	if (Generator.GetInterface())
	{
		Generator.GetInterface()->Generate(SpawnedActorsPath, [this](bool bSuccess) { TaskComplete(bSuccess); });
	}
}

void UNodeGenerator::TaskComplete(bool bSuccess)
{
	if (bSuccess) OnSuccess.Broadcast();
	else OnFailure.Broadcast();
	SetReadyToDestroy();
}
