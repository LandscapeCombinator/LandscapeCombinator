// Copyright LandscapeCombinator. All Rights Reserved.

#include "Foliage/OGRProceduralFoliageVolumeDetails.h"
#include "Foliage/OGRProceduralFoliageVolume.h"
#include "Utils/Logging.h"

#include "PropertyEditorModule.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void FOGRProceduralFoliageVolumeDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	const FName OGRProceduralFoliageCategoryName("OGRProceduralFoliage");
	IDetailCategoryBuilder& OGRProceduralFoliageCategory = DetailBuilder.EditCategory(OGRProceduralFoliageCategoryName);

	TArray< TWeakObjectPtr<UObject> > ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

	if (ObjectsBeingCustomized.IsEmpty()) return;
	Volume = Cast<AOGRProceduralFoliageVolume>(ObjectsBeingCustomized[0].Get());

	// Customization similar to the Resimulate button in ProceduralFoliageComponentDetails.cpp
	FDetailWidgetRow& NewRow = OGRProceduralFoliageCategory.AddCustomRow(FText::GetEmpty());
	NewRow.ValueContent()
		.MaxDesiredWidth(60.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()			
			.Padding(4.0f)
			[
				SNew(SButton)
				.OnClicked(this, &FOGRProceduralFoliageVolumeDetails::OnResimulateWithFilterClicked)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("ResimulateWithFilter", "Resimulate With Filter"))
				]
			]
		];
}

FReply FOGRProceduralFoliageVolumeDetails::OnResimulateWithFilterClicked()
{
	Volume->ResimulateWithFilter();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
