// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "IPropertyTypeCustomization.h"

/* Object picker restricted to the class named by the `WeightedObjectClass` metadata
 * of the property holding the FWeightedObject (or of its parent, e.g. the array), followed by the weight. */
class FWeightedObjectCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() { return MakeShared<FWeightedObjectCustomization>(); }

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> Handle, FDetailWidgetRow& Row, IPropertyTypeCustomizationUtils& Utils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> Handle, IDetailChildrenBuilder& Children, IPropertyTypeCustomizationUtils& Utils) override {}
};

#endif