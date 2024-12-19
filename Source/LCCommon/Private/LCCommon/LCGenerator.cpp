// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LCCommon/LCGenerator.h"
#include "LCCommon/LCReporter.h"
#include "LCCommon/LCBlueprintLibrary.h"

#if WITH_EDITOR
#include "EditorActorFolders.h"
#endif

#define LOCTEXT_NAMESPACE "FActorGeneratorModule"

bool ILCGenerator::DeleteGeneratedObjects(bool bSkipPrompt)
{
	TArray<UObject*> GeneratedObjects = GetGeneratedObjects();
 
	if (GeneratedObjects.Num() == 0) return true;

	if (!bSkipPrompt)
	{
		FString ObjectsString;
		for (UObject* Object : GeneratedObjects)
		{
			ObjectsString += Object->GetName() + "\n";
		}
		if (!ULCReporter::ShowMessage(
			FText::Format(
				LOCTEXT("ILCGenerator::DeleteGeneratedObjects",
					"The following objects will be deleted:\n{0}\nContinue?"),
				FText::FromString(ObjectsString)
			),
			"SuppressConfirmCleanup",
			LOCTEXT("ConfirmCleanup", "Confirm Cleanup")
		))
		{
			return false;
		}
	}

	for (UObject* Object: GeneratedObjects)
	{
		if (AActor *Actor = Cast<AActor>(Object))
		{
			Actor->Destroy();

#if WITH_EDITOR
			ULCBlueprintLibrary::DeleteFolder(*Actor->GetWorld(), Actor->GetFolder());
#endif
		}
		else if (IsValid(Object)) Object->MarkAsGarbage();
	}
	GeneratedObjects.Empty();
	return true;
}

void ILCGenerator::ChangeRootPath(FName FromRootPath, FName ToRootPath)
{
	TArray<UObject*> GeneratedObjects = GetGeneratedObjects();
	for (auto Object : GeneratedObjects)
	{
		if (AActor *Actor = Cast<AActor>(Object))
		{
			FName CurrentPath = Actor->GetFolderPath();
			if (CurrentPath.ToString().StartsWith(FromRootPath.ToString()))
			{
				FName NewPath = FName(CurrentPath.ToString().Replace(*FromRootPath.ToString(), *ToRootPath.ToString()));
				Actor->SetFolderPath(NewPath);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
