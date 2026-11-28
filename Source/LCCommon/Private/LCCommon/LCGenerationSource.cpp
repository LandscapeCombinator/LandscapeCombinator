// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "LCCommon/LCGenerationSource.h"

void ULCGenerationSourceRegistry::Register(UObject* Source)
{
	if (IsValid(Source) && Source->Implements<ULCGenerationSource>()) Sources.AddUnique(Source);
}

void ULCGenerationSourceRegistry::Unregister(UObject* Source)
{
	Sources.RemoveAll([Source](const TWeakObjectPtr<UObject>& Entry) { return !Entry.IsValid() || Entry.Get() == Source; });
}

TArray<FLCGenerationPosition> ULCGenerationSourceRegistry::GetPositions() const
{
	TArray<FLCGenerationPosition> Positions;
	for (const TWeakObjectPtr<UObject>& Entry : Sources)
	{
		const ILCGenerationSource* Source = Cast<ILCGenerationSource>(Entry.Get());
		FLCGenerationPosition Position;
		if (Source && Source->GetGenerationPosition(Position)) Positions.Add(Position);
	}
	return Positions;
}