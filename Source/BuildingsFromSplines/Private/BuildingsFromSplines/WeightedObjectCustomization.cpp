// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#if WITH_EDITOR

#include "BuildingsFromSplines/WeightedObjectCustomization.h"
#include "BuildingsFromSplines/BuildingConfiguration.h"

#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

namespace
{
	const FName ClassMeta("WeightedObjectClass");

	UClass* GetAllowedClass(const TSharedRef<IPropertyHandle>& Handle)
	{
		for (TSharedPtr<IPropertyHandle> H = Handle; H.IsValid(); H = H->GetParentHandle())
		{
			const FProperty* Property = H->GetProperty();
			if (Property && Property->HasMetaData(ClassMeta))
			{
				UClass* Class = FindObject<UClass>(nullptr, *Property->GetMetaData(ClassMeta));
				if (Class) return Class;
			}
		}
		return UObject::StaticClass();
	}
}

void FWeightedObjectCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> Handle, FDetailWidgetRow& Row, IPropertyTypeCustomizationUtils& Utils)
{
	TSharedPtr<IPropertyHandle> ObjectHandle = Handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FWeightedObject, Object));
	TSharedPtr<IPropertyHandle> WeightHandle = Handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FWeightedObject, Weight));
	if (!ObjectHandle.IsValid() || !WeightHandle.IsValid()) return;

	Row.NameContent()[ Handle->CreatePropertyNameWidget() ];
	Row.ValueContent().MinDesiredWidth(300.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
		[
			SNew(STextBlock)
			.Text(INVTEXT("Weight"))
			.ToolTipText(INVTEXT("Relative probability of picking this object"))
			.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
		[
			SNew(SBox).WidthOverride(70.f)
			[
				WeightHandle->CreatePropertyValueWidget()
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[
			SNew(SObjectPropertyEntryBox)
			.PropertyHandle(ObjectHandle)
			.AllowedClass(GetAllowedClass(Handle))
			.ThumbnailPool(Utils.GetThumbnailPool())
		]
	];
}

#endif