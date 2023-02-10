// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "ActorFactories/ActorFactoryBoxVolume.h"
#include "Math/Transform.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "ActorFactoryOGRProceduralFoliage.generated.h"

class AActor;
class FText;
class UObject;
struct FAssetData;

UCLASS(MinimalAPI, config=Editor)
class UActorFactoryOGRProceduralFoliage : public UActorFactoryBoxVolume
{
	GENERATED_UCLASS_BODY()

	//~ Begin UActorFactory Interface
	virtual bool PreSpawnActor(UObject* Asset, FTransform& InOutLocation);
	virtual bool CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg ) override;
	virtual void PostSpawnActor( UObject* Asset, AActor* NewActor) override;
	virtual void PostCreateBlueprint( UObject* Asset, AActor* CDO ) override;
	virtual UObject* GetAssetFromActorInstance(AActor* ActorInstance) override;
	//~ End UActorFactory Interface
};
