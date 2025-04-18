#include "LandscapeCombinator/LandscapeCombination.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"
#include "LCReporter/LCReporter.h"
#include "LCCommon/LCBlueprintLibrary.h"
#include "SplineImporter/SplineImporter.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "HeightmapModifier/BlendLandscape.h"
#include "Kismet/GameplayStatics.h"

#if WITH_EDITOR
#include "FileHelpers.h"
#endif


using namespace ConcurrencyOperators;

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void ALandscapeCombination::OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated, TFunction<void(bool)> OnComplete)
{
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Starting Combination with %d Generators"), Generators.Num());

	Concurrency::RunSuccessively<FGeneratorWrapper>(
		Generators,
		
		[this, SpawnedActorsPathOverride, bIsUserInitiated](FGeneratorWrapper GeneratorWrapper, TFunction<void(bool)> OnCompleteOne) -> void
		{
			TSoftObjectPtr<AActor> Generator = GeneratorWrapper.Generator;

			if (!GeneratorWrapper.bIsEnabled)
			{
				if (OnCompleteOne) OnCompleteOne(true);
				return;
			}
			else if (Generator.IsValid())
			{
				if (Generator->Implements<ULCGenerator>())
				{
					UE_LOG(LogLandscapeCombinator, Log, TEXT("Starting generator: %s"), *Generator->GetActorNameOrLabel());

					auto OnCompleteOneSave = [OnCompleteOne, bIsUserInitiated, this](bool bSuccess) {
#if WITH_EDITOR
						if (bSuccess && bSaveAfterEachGenerator)
						{
							UE_LOG(LogLandscapeCombinator, Log, TEXT("Saving Level"));
							Concurrency::RunOnGameThreadAndWait([bIsUserInitiated](){
								const bool bPromptUserToSave = bIsUserInitiated;
								const bool bSaveMapPackages = true;
								const bool bSaveContentPackages = true;
								const bool bFastSave = false;
								const bool bNotifyNoPackagesSaved = false;
								const bool bCanBeDeclined = false;
								FEditorFileUtils::SaveDirtyPackages( bPromptUserToSave, bSaveMapPackages, bSaveContentPackages, bFastSave, bNotifyNoPackagesSaved, bCanBeDeclined );
							});
						}
#endif
						if (OnCompleteOne) OnCompleteOne(bSuccess);
					};

					UE_LOG(LogLandscapeCombinator, Log, TEXT("Starting Generation with %s"), *Generator->GetActorNameOrLabel());
					if (SpawnedActorsPathOverride.IsNone())
						Cast<ILCGenerator>(Generator.Get())->Generate(FName(), bIsUserInitiated, OnCompleteOneSave);
					else
						Cast<ILCGenerator>(Generator.Get())->Generate(
							FName(SpawnedActorsPathOverride.ToString() / Generator->GetActorNameOrLabel()),
							bIsUserInitiated,
							OnCompleteOneSave
						);
				}
				else
				{
					UE_LOG(LogLandscapeCombinator, Error, TEXT("Non-generator actor in combination: %s"), *Generator->GetActorNameOrLabel());
					if (OnCompleteOne) OnCompleteOne(false);
					return;
				}
			}
			else
			{
				UE_LOG(LogLandscapeCombinator, Error, TEXT("Invalid actor in combination"))

				if (OnCompleteOne) OnCompleteOne(false);
				return;
			}
		},
		
		OnComplete
	);
}

bool ALandscapeCombination::Cleanup_Implementation(bool bSkipPrompt)
{
	Modify();

	if (!bSkipPrompt)
	{
		TArray<UObject*> ObjectsToDelete;
		for (auto &GeneratorWrapper : Generators)
		{
			if (GeneratorWrapper.bIsEnabled && GeneratorWrapper.Generator.IsValid() && GeneratorWrapper.Generator->Implements<ULCGenerator>())
			{
				ObjectsToDelete.Append(Cast<ILCGenerator>(GeneratorWrapper.Generator.Get())->GetGeneratedObjects());
			}
		}

		if (ObjectsToDelete.Num() > 0)
		{
			FString ObjectsString;
			for (UObject* Object : ObjectsToDelete)
			{
				if (IsValid(Object)) ObjectsString += Object->GetName() + "\n";
			}
			if (!ULCReporter::ShowMessage(
				FText::Format(
					LOCTEXT("ALandscapeCombination::Cleanup_Implementation",
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
	}

	for (auto &GeneratorWrapper : Generators)
	{
		if (GeneratorWrapper.bIsEnabled && GeneratorWrapper.Generator.IsValid() && GeneratorWrapper.Generator->Implements<ULCGenerator>())
		{
			if (!Cast<ILCGenerator>(GeneratorWrapper.Generator.Get())->Execute_Cleanup(GeneratorWrapper.Generator.Get(), true)) return false;
		}
	}
	return true;
}

#if WITH_EDITOR

AActor* ALandscapeCombination::Duplicate(FName FromName, FName ToName)
{
	if (ALandscapeCombination *NewCombination =
		Cast<ALandscapeCombination>(GEditor->GetEditorSubsystem<UEditorActorSubsystem>()->DuplicateActor(Cast<AActor>(this)))
	)
	{
		NewCombination->Generators.Empty();
		for (auto &GeneratorWrapper : Generators)
		{
			if (!GeneratorWrapper.Generator.IsValid())
			{
				ULCReporter::ShowError(LOCTEXT("FailedDuplicateError1", "Invalid generator found during duplication."));
				continue;
			}

			if (!GeneratorWrapper.Generator->Implements<ULCGenerator>())
			{
				ULCReporter::ShowError(LOCTEXT("FailedDuplicateError2", "Generator does not implement the LCGenerator interface."));
				continue;
			}

			if (AActor *DuplicatedGenerator = Cast<ILCGenerator>(GeneratorWrapper.Generator.Get())->Duplicate(FromName, ToName))
			{
				FGeneratorWrapper NewGeneratorWrapper;
				DuplicatedGenerator->SetFolderPath(ToName);
				NewGeneratorWrapper.bIsEnabled = GeneratorWrapper.bIsEnabled;
				NewGeneratorWrapper.Generator = DuplicatedGenerator;
				NewCombination->Generators.Add(NewGeneratorWrapper);
			}
			else
			{
				ULCReporter::ShowError(LOCTEXT("FailedDuplicateError1", "Failed to duplicate generator."));
			}
		}

		NewCombination->SpawnedActorsPath = ULCBlueprintLibrary::ReplaceName(SpawnedActorsPath, FromName, ToName);
		NewCombination->SetActorLabel(ULCBlueprintLibrary::Replace(GetActorLabel(), FromName.ToString(), ToName.ToString()));
		NewCombination->SetFolderPath(ToName);
		return NewCombination;
	}
	else
	{
		return nullptr;
	}
}

#endif

#undef LOCTEXT_NAMESPACE
