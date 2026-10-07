// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

/* Asset link: UBuildingConfiguration, ULevelDescription and UWallSegment carry a bLinkedToAsset flag.
 * Linked: the object follows its Blueprint asset (edits go to the Blueprint's default object).
 * Unlinked: the object is a local copy. */
namespace AssetLink
{
	BUILDINGSFROMSPLINES_API bool IsLinkable(const UClass* Class);
	BUILDINGSFROMSPLINES_API const TArray<FName>& TypeNames();
	BUILDINGSFROMSPLINES_API bool IsLinked(const UObject* Obj);
	BUILDINGSFROMSPLINES_API void SetLinked(UObject* Obj, bool bLinked);

	/* the object to read or edit: the Blueprint's default object when linked, the object itself otherwise */
	template<class T> T* Resolve(T* Obj) { return IsLinked(Obj) ? static_cast<T*>(Obj->GetClass()->GetDefaultObject()) : Obj; }
}

template<class TMapType>
bool HasKeyInMap(const TMapType& Map, const FString& Key)
{
	const auto* Value = Map.Find(Key);
	return Value && IsValid(Value->Get());
}
