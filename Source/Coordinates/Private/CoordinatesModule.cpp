// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "CoordinatesModule.h"
#include "Misc/EngineVersionComparison.h"
#if WITH_EDITOR
#include "Editor.h"
#endif
#if WITH_EDITOR && !UE_VERSION_OLDER_THAN(5, 6, 0)
#include "Coordinates/LevelCoordinates.h"
#include "ToolMenus.h"
#include "EngineUtils.h"
#endif

#define LOCTEXT_NAMESPACE "FCoordinatesModule"

#if WITH_EDITOR && !UE_VERSION_OLDER_THAN(5, 6, 0)
static ALevelCoordinates* FindLevelCoordinates()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<ALevelCoordinates> It(World); It; ++It) return *It;
	return nullptr;
}
#endif

void FCoordinatesModule::StartupModule()
{
#if WITH_EDITOR && !UE_VERSION_OLDER_THAN(5, 6, 0)
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FCoordinatesModule::RegisterMenus));
#endif
}

void FCoordinatesModule::ShutdownModule()
{
#if WITH_EDITOR && !UE_VERSION_OLDER_THAN(5, 6, 0)
	if (UObjectInitialized())
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}
#endif
}

void FCoordinatesModule::RegisterMenus()
{
#if WITH_EDITOR && !UE_VERSION_OLDER_THAN(5, 6, 0)
	FToolMenuOwnerScoped OwnerScoped(this);
	for (const FName& MenuName : TArray<FName>{ FName("LevelEditor.ViewportToolbar.Camera"), FName("UnrealEd.ViewportToolbar.Camera") })
	{
		FToolMenuSection& Section = UToolMenus::Get()->ExtendMenu(MenuName)->FindOrAddSection("LevelViewportCameraType_Perspective");
		FToolUIAction Action;
		Action.ExecuteAction = FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext&)
		{
			if (ALevelCoordinates* LC = FindLevelCoordinates()) LC->SetSphereViewportNavigation(!LC->bSphereViewportNavigation);
		});
		Action.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda([](const FToolMenuContext&)
		{
			ALevelCoordinates* LC = FindLevelCoordinates();
			return (LC && LC->bSphereViewportNavigation) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		});
		Action.IsActionVisibleDelegate = FToolMenuIsActionButtonVisible::CreateLambda([](const FToolMenuContext&)
		{
			ALevelCoordinates* LC = FindLevelCoordinates();
			return LC && IsValid(LC->GlobalCoordinates) && LC->GlobalCoordinates->IsSphere();
		});
		Section.AddMenuEntry(
			"FollowBodySurface",
			LOCTEXT("FollowBodySurface", "Follow Body Surface"),
			LOCTEXT("FollowBodySurfaceTooltip", "Keeps a constant altitude and a level horizon when flying around a spherical body."),
			FSlateIcon(),
			FToolUIActionChoice(Action),
			EUserInterfaceActionType::ToggleButton
		);
	}
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCoordinatesModule, Coordinates)
