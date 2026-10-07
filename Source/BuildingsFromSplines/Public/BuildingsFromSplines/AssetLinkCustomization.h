// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "IPropertyTypeCustomization.h"

/* Header: Linked/Unlinked toggle + the usual class picker.
 * Children: the Blueprint asset's properties when linked, the local copy's properties otherwise. */
class FAssetLinkCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() { return MakeShared<FAssetLinkCustomization>(); }
	static TSharedPtr<IPropertyTypeIdentifier> Identifier();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> Handle, FDetailWidgetRow& Row, IPropertyTypeCustomizationUtils& Utils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> Handle, IDetailChildrenBuilder& Children, IPropertyTypeCustomizationUtils& Utils) override;
};

#endif
