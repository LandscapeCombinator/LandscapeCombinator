// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#if WITH_EDITOR

#include "LandscapeCombinator/SGeneratorsDock.h"
#include "LandscapeCombinator/SGeneratorsStatus.h"

#include "DetailLayoutBuilder.h"
#include "IDetailCustomization.h"
#include "IDetailsView.h"
#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleDefaults.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "UObject/Package.h"

namespace
{
	const FLinearColor PanelColor = FLinearColor::FromSRGBColor(FColor(62, 62, 72));

	const TCHAR* ConfigSection = TEXT("LandscapeCombinator");
	const TCHAR* ConfigSimpleKey = TEXT("SimpleDetails");

	bool IsPluginProperty(const FPropertyAndParent& PropertyAndParent)
	{
		const UClass* Owner = PropertyAndParent.Property.GetOwnerClass();
		if (!Owner) return true;

		const FString Package = Owner->GetOutermost()->GetName();
		return !Package.StartsWith(TEXT("/Script/Engine")) && !Package.StartsWith(TEXT("/Script/CoreUObject"));
	}

	class FGeneratorDetailsCustomization : public IDetailCustomization
	{
	public:
		static TSharedRef<IDetailCustomization> MakeInstance() { return MakeShared<FGeneratorDetailsCustomization>(); }

		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
		{
			for (const TCHAR* Category : {
				TEXT("TransformCommon"), TEXT("Actor"), TEXT("Rendering"), TEXT("Replication"), TEXT("Collision"), TEXT("HLOD"),
				TEXT("Input"), TEXT("LOD"), TEXT("Cooking"), TEXT("DataLayers"), TEXT("WorldPartition"), TEXT("Physics"), TEXT("Tick"), TEXT("PCG") })
			{
				DetailBuilder.HideCategory(Category);
			}
		}
	};
}

void SGeneratorsDock::Construct(const FArguments& InArgs)
{
	GConfig->GetBool(ConfigSection, ConfigSimpleKey, bSimple, GEditorPerProjectIni);

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs Args;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	Args.bUpdatesFromSelection = false;
	Args.bHideSelectionTip = true;
	DetailsView = PropertyModule.CreateDetailView(Args);
	DetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([this](const FPropertyAndParent& PropertyAndParent)
	{
		if (!IsPluginProperty(PropertyAndParent)) return false;
		return !(bSimple && PropertyAndParent.Property.HasAnyPropertyFlags(CPF_AdvancedDisplay));
	}));
	DetailsView->SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance::CreateStatic(&FGeneratorDetailsCustomization::MakeInstance));

	ChildSlot
	[
		SNew(SDPIScaler)
		.DPIScale_Lambda([]() { return SGeneratorsStatus::GetUIScale(); })
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			+ SSplitter::Slot().Value(0.35f)
			[
				SNew(SGeneratorsStatus)
				.OnGeneratorPicked_Lambda([this](AActor* Actor)
				{
					Picked = Actor;
					DetailsView->SetObject(Actor);
				})
			]
			+ SSplitter::Slot().Value(0.65f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
				.BorderBackgroundColor_Lambda([this]() -> FSlateColor
				{
					return FMath::Lerp(PanelColor, SGeneratorsStatus::GetStatusColor(Picked.Get()), 0.2f);
				})
				.Padding(4.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SNew(SBorder)
							.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
							.BorderBackgroundColor_Lambda([this]() -> FSlateColor { return SGeneratorsStatus::GetStatusColor(Picked.Get()); })
							.Padding(FMargin(8.f, 3.f))
							.Visibility_Lambda([this]() { return Picked.IsValid() ? EVisibility::Visible : EVisibility::Hidden; })
							[
								SNew(STextBlock)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
								.Text_Lambda([this]() -> FText
								{
									AActor* Actor = Picked.Get();
									return Actor ? FText::FromString(Actor->GetActorNameOrLabel()) : FText::GetEmpty();
								})
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
						[
							SNew(SButton)
							.ContentPadding(FMargin(8.f, 1.f))
							.ToolTipText(INVTEXT("Simple shows only the essential settings, Advanced shows everything"))
							.Text_Lambda([this]() -> FText { return bSimple ? INVTEXT("Simple") : INVTEXT("Advanced"); })
							.OnClicked_Lambda([this]()
							{
								bSimple = !bSimple;
								GConfig->SetBool(ConfigSection, ConfigSimpleKey, bSimple, GEditorPerProjectIni);
								DetailsView->ForceRefresh();
								return FReply::Handled();
							})
						]
					]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SNew(SBorder)
						.BorderImage(FStyleDefaults::GetNoBrush())
						.Padding(0.f)
						.ColorAndOpacity(TAttribute<FLinearColor>::CreateLambda([this]() -> FLinearColor
						{
							return FMath::Lerp(FLinearColor::White, SGeneratorsStatus::GetStatusColor(Picked.Get()), 0.3f);
						}))
						[
							DetailsView.ToSharedRef()
						]
					]
				]
			]
		]
	];
}

#endif