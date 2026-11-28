// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Subsystems/WorldSubsystem.h"
#include "LCGenerationSource.generated.h"

struct FLCGenerationPosition
{
	double Longitude = 0;
	double Latitude = 0;
	double AltitudeCm = 0;
};

UINTERFACE()
class LCCOMMON_API ULCGenerationSource : public UInterface
{
	GENERATED_BODY()
};

class LCCOMMON_API ILCGenerationSource
{
	GENERATED_BODY()

public:
	virtual bool GetGenerationPosition(FLCGenerationPosition& OutPosition) const = 0;
};

UCLASS()
class LCCOMMON_API ULCGenerationSourceRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void Register(UObject* Source);
	void Unregister(UObject* Source);
	TArray<FLCGenerationPosition> GetPositions() const;

private:
	TArray<TWeakObjectPtr<UObject>> Sources;
};