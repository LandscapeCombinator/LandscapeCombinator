// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "BuildingsFromSplines/AssetLink.h"
#include "BuildingsFromSplines/BuildingConfiguration.h"

namespace
{
	constexpr const TCHAR* LinkFlagName = TEXT("bLinkedToAsset");

	TArray<const UClass*> LinkableClasses()
	{
		return { UBuildingConfiguration::StaticClass(), ULevelDescription::StaticClass(), UWallSegment::StaticClass() };
	}
}

bool AssetLink::IsLinkable(const UClass* Class)
{
	if (!Class) return false;
	for (const UClass* Linkable : LinkableClasses())
	{
		if (Class->IsChildOf(Linkable)) return true;
	}
	return false;
}

const TArray<FName>& AssetLink::TypeNames()
{
	static const TArray<FName> Names = []
	{
		TArray<FName> Result;
		for (const UClass* Linkable : LinkableClasses()) Result.Add(Linkable->GetFName());
		return Result;
	}();
	return Names;
}

bool AssetLink::IsLinked(const UObject* Obj)
{
	if (!Obj || Obj->GetClass()->HasAnyClassFlags(CLASS_Native)) return false;
	const FBoolProperty* Flag = FindFProperty<FBoolProperty>(Obj->GetClass(), LinkFlagName);
	return Flag && Flag->GetPropertyValue_InContainer(Obj);
}

void AssetLink::SetLinked(UObject* Obj, bool bLinked)
{
	if (!Obj) return;
	if (const FBoolProperty* Flag = FindFProperty<FBoolProperty>(Obj->GetClass(), LinkFlagName))
		Flag->SetPropertyValue_InContainer(Obj, bLinked);
}
