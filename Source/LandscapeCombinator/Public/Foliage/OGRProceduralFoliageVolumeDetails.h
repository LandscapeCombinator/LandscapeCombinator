// Copyright LandscapeCombinator. All Rights Reserved.

#pragma once

#include "PropertyEditorModule.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "IDetailGroup.h"

class FOGRProceduralFoliageVolumeDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FOGRProceduralFoliageVolumeDetails());
	}

	void CustomizeDetails(IDetailLayoutBuilder&) override;
	FReply OnResimulateWithFilterClicked();

private:
	AOGRProceduralFoliageVolume* Volume;

};