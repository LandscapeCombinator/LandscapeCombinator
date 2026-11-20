// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/AdvancedSubCategoriesCustomization.h"

#if WITH_EDITOR

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IDetailGroup.h"
#include "PropertyHandle.h"

TSharedRef<IDetailCustomization> FAdvancedSubCategoriesCustomization::MakeInstance()
{
	return MakeShareable(new FAdvancedSubCategoriesCustomization);
}

// The engine does not show AdvancedDisplay properties of "A|B" subcategories,
// so we replace each "A|B" subcategory by a group "B" in "A", with the advanced properties in a nested "Advanced" group.
void FAdvancedSubCategoriesCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	AddAdvancedGroups(DetailBuilder);
}

void FAdvancedSubCategoriesCustomization::AddAdvancedGroups(IDetailLayoutBuilder& DetailBuilder)
{
	TMap<FString, IDetailGroup*> Groups;
	TArray<TPair<FString, TSharedRef<IPropertyHandle>>> Advanced;

	for (TFieldIterator<FProperty> It(DetailBuilder.GetBaseClass()); It; ++It)
	{
		FString Root, Sub;
		if (!It->HasAllPropertyFlags(CPF_Edit) || !It->GetMetaData(TEXT("Category")).Split(TEXT("|"), &Root, &Sub)) continue;
		if (It->GetOwnerClass()->GetOutermost()->GetName().StartsWith(TEXT("/Script/Engine"))) continue;

		TSharedRef<IPropertyHandle> Handle = DetailBuilder.GetProperty(It->GetFName(), It->GetOwnerStruct());
		if (!Handle->IsValidHandle() || !DetailBuilder.IsPropertyVisible(Handle)) continue;

		const FString Key = Root + TEXT("|") + Sub;
		IDetailGroup*& Group = Groups.FindOrAdd(Key);
		if (!Group)
		{
			Group = &DetailBuilder.EditCategory(FName(*Root), FText::GetEmpty(), ECategoryPriority::Important).AddGroup(FName(*Sub), FText::FromString(Sub), false, false);
			Group->SetDisplayMode(EDetailGroupDisplayMode::Category);
		}

		if (It->HasAnyPropertyFlags(CPF_AdvancedDisplay)) Advanced.Emplace(Key, Handle);
		else Group->AddPropertyRow(Handle);
	}

	// Added after the simple rows. A nested group starts expanded iff its parent does (engine), so both start collapsed.
	TMap<FString, IDetailGroup*> AdvancedGroups;
	for (const TPair<FString, TSharedRef<IPropertyHandle>>& Pair : Advanced)
	{
		IDetailGroup*& Group = AdvancedGroups.FindOrAdd(Pair.Key);
		if (!Group) Group = &Groups.FindChecked(Pair.Key)->AddGroup(FName(*(Pair.Key + TEXT("Advanced"))), INVTEXT("Advanced"));
		Group->AddPropertyRow(Pair.Value);
	}
}

#endif
