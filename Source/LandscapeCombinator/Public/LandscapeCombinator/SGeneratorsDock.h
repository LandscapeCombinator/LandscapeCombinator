// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class IDetailsView;

class SGeneratorsDock : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGeneratorsDock) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedPtr<IDetailsView> DetailsView;
	TWeakObjectPtr<AActor> Picked;
	bool bSimple = true;
};

#endif