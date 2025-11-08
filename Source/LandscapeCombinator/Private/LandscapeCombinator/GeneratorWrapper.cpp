#if WITH_EDITOR

#include "LandscapeCombinator/GeneratorWrapper.h"
#include "IPropertyUtilities.h"
#include "DetailWidgetRow.h"
#include "Widgets/Images/SThrobber.h"
#include "PropertyHandle.h"
#include "IPropertyTypeCustomization.h"

TSharedRef<IPropertyTypeCustomization> FGeneratorWrapperCustomization::MakeInstance()
{
    return MakeShareable(new FGeneratorWrapperCustomization);
}

void FGeneratorWrapperCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructHandle, FDetailWidgetRow& Row, IPropertyTypeCustomizationUtils& Utils)
{
    auto EnabledHandle          = StructHandle->GetChildHandle("bIsEnabled");
    auto GeneratorHandle        = StructHandle->GetChildHandle("Generator");
    auto GenerratorStatusHandle = StructHandle->GetChildHandle("GeneratorStatus");

    Row.WholeRowContent()
    [
        SNew(SBorder)
        .Padding(4, 4)
        .BorderBackgroundColor(TAttribute<FSlateColor>::Create([EnabledHandle, GenerratorStatusHandle]() {
            bool bIsEnabled = false;
            EnabledHandle->GetValue(bIsEnabled);
            EGeneratorStatus GeneratorStatus = EGeneratorStatus::Idle;
            GenerratorStatusHandle->GetValue((uint8&)GeneratorStatus);

            FColor Color = bIsEnabled ?
                (GeneratorStatus == EGeneratorStatus::Idle ? FColor(0, 0, 16) : 
                GeneratorStatus == EGeneratorStatus::Generating ? FColor(50, 30, 0) : 
                GeneratorStatus == EGeneratorStatus::Error ? FColor(16, 0, 0) : FColor(0, 16, 0)) 
                : FColor(0, 0, 0);

            return FSlateColor(Color);
        }))
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6,0)
            [
                SNew(SBox)
                .WidthOverride(24)
                .HeightOverride(24)
                .Visibility_Lambda([GenerratorStatusHandle]() {
                    EGeneratorStatus GeneratorStatus = EGeneratorStatus::Idle;
                    GenerratorStatusHandle->GetValue((uint8&)GeneratorStatus);
                    return GeneratorStatus == EGeneratorStatus::Generating ? EVisibility::Visible : EVisibility::Collapsed;
                })
                [
                    SNew(SCircularThrobber)
                    .NumPieces(64)
                    .Period(2.5) 
                ]
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4,0)
            [
                EnabledHandle->CreatePropertyValueWidget()
            ]
            + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(4,0)
            [
                GeneratorHandle->CreatePropertyValueWidget()
            ]
        ]
    ];
}

#endif
