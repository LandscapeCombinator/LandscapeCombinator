// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#if WITH_EDITOR

#include "BuildingsFromSplines/AssetLinkCustomization.h"
#include "BuildingsFromSplines/AssetLink.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "FBuildingsFromSplinesModule"

namespace
{
	UObject* GetObject(const TSharedRef<IPropertyHandle>& Handle)
	{
		UObject* Obj = nullptr;
		return Handle->GetValue(Obj) == FPropertyAccess::Success ? Obj : nullptr;
	}

	class FIdentifier : public IPropertyTypeIdentifier
	{
		virtual bool IsPropertyTypeCustomized(const IPropertyHandle& Handle) const override
		{
			const FObjectProperty* Property = CastField<FObjectProperty>(Handle.GetProperty());
			return Property && AssetLink::IsLinkable(Property->PropertyClass.Get());
		}
	};
}

TSharedPtr<IPropertyTypeIdentifier> FAssetLinkCustomization::Identifier()
{
	static TSharedPtr<IPropertyTypeIdentifier> Instance = MakeShared<FIdentifier>();
	return Instance;
}

void FAssetLinkCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> Handle, FDetailWidgetRow& Row, IPropertyTypeCustomizationUtils& Utils)
{
	TSharedPtr<IPropertyUtilities> PropUtils = Utils.GetPropertyUtilities();
	auto Refresh = [PropUtils]() { if (PropUtils) PropUtils->ForceRefresh(); };

	// picking another class replaces the object, so the children must be rebuilt
	Handle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda(Refresh));

	Row.NameContent()[ Handle->CreatePropertyNameWidget() ];
	Row.ValueContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
		[
			SNew(SButton)
			.Text_Lambda([Handle]()
			{
				return AssetLink::IsLinked(GetObject(Handle)) ? LOCTEXT("Linked", "Linked") : LOCTEXT("Unlinked", "Unlinked");
			})
			.ToolTipText(LOCTEXT("LinkTip", "Linked: edits go to the Blueprint asset and are shared. Unlinked: edits only affect a local copy."))
			.IsEnabled_Lambda([Handle]()
			{
				const UObject* Obj = GetObject(Handle);
				return Obj && Obj->GetClass()->ClassGeneratedBy; // needs a Blueprint asset
			})
			.OnClicked_Lambda([Handle, Refresh]()
			{
				if (UObject* Obj = GetObject(Handle))
				{
					FScopedTransaction Transaction(LOCTEXT("ToggleLink", "Toggle Asset Link"));
					Obj->Modify();
					AssetLink::SetLinked(Obj, !AssetLink::IsLinked(Obj));
					Refresh();
				}
				return FReply::Handled();
			})
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("OpenAsset", "Open"))
			.ToolTipText(LOCTEXT("OpenAssetTip", "Open the linked Blueprint asset"))
			.OnClicked_Lambda([Handle]()
			{
				const UObject* Obj = GetObject(Handle);
				UAssetEditorSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
				if (Obj && Subsystem && Obj->GetClass()->ClassGeneratedBy) Subsystem->OpenEditorForAsset(Obj->GetClass()->ClassGeneratedBy);
				return FReply::Handled();
			})
		]
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[
			Handle->CreatePropertyValueWidget()
		]
	];
}

void FAssetLinkCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> Handle, IDetailChildrenBuilder& Children, IPropertyTypeCustomizationUtils&)
{
	UObject* Obj = GetObject(Handle);
	if (!Obj) return;

	UObject* Target = AssetLink::IsLinked(Obj) ? Obj->GetClass()->GetDefaultObject() : Obj;

	FDetailsViewArgs Args;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	Args.bAllowSearch = false;
	Args.bShowScrollBar = false;
	Args.bShowOptions = false;
	Args.bHideSelectionTip = true;
	Args.bAllowFavoriteSystem = false;

	for (TFieldIterator<FProperty> It(Target->GetClass()); It; ++It)
	{
		if (!It->HasAnyPropertyFlags(CPF_Edit) || !AssetLink::IsLinkable(It->GetOwnerClass())) continue;

		FString Category = It->GetMetaData("Category");
		int32 Bar;
		if (Category.FindChar('|', Bar)) Category.LeftInline(Bar);
		if (!Category.IsEmpty()) Args.InitialCategories.Add(FName(*Category));
	}

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	TSharedRef<IDetailsView> View = PropertyModule.CreateDetailView(Args);
	View->SetObject(Target);

	Children.AddCustomRow(LOCTEXT("Properties", "Properties")).WholeRowContent()[ View ];
}

#undef LOCTEXT_NAMESPACE

#endif
