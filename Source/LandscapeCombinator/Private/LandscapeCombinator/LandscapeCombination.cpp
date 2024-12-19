#include "LandscapeCombinator/LandscapeCombination.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"
#include "LCCommon/LCReporter.h"
#include "LCCommon/LCBlueprintLibrary.h"
#include "SplineImporter/SplineImporter.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "HeightmapModifier/BlendLandscape.h"

#include "Kismet/GameplayStatics.h"

using namespace ConcurrencyOperators;

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void ALandscapeCombination::OnGenerate(FName SpawnedActorsPathOverride, TFunction<void(bool)> OnComplete)
{
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Starting Combination with %d Generators"), Generators.Num());

	Concurrency::RunSuccessively<AActor*>(
		Generators,
		
		[this, SpawnedActorsPathOverride](AActor* Generator, TFunction<void(bool)> OnCompleteOne) -> void
		{
			if (IsValid(Generator))
			{
				if (Generator->Implements<ULCGenerator>())
				{
					UE_LOG(LogLandscapeCombinator, Log, TEXT("Starting generator: %s"), *Generator->GetActorNameOrLabel());

					if (SpawnedActorsPathOverride.IsNone())
						Cast<ILCGenerator>(Generator)->Generate(FName(), OnCompleteOne);
					else
						Cast<ILCGenerator>(Generator)->Generate(
							FName(SpawnedActorsPathOverride.ToString() / Generator->GetActorNameOrLabel()),
							OnCompleteOne
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
	if (!bSkipPrompt)
	{
		TArray<UObject*> ObjectsToDelete;
		for (auto &Generator : Generators)
		{
			if (IsValid(Generator) && Generator->Implements<ULCGenerator>())
			{
				ObjectsToDelete.Append(Cast<ILCGenerator>(Generator)->GetGeneratedObjects());
			}
		}

		if (ObjectsToDelete.Num() > 0)
		{
			FString ObjectsString;
			for (UObject* Object : ObjectsToDelete)
			{
				ObjectsString += Object->GetName() + "\n";
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

	for (auto &Generator : Generators)
	{
		if (IsValid(Generator) && Generator->Implements<ULCGenerator>())
		{
			if (!Cast<ILCGenerator>(Generator)->Execute_Cleanup(Generator, true)) return false;
		}
	}
	return true;
}

AActor* ALandscapeCombination::Duplicate(FName FromName, FName ToName)
{
	if (ALandscapeCombination *NewCombination =
		Cast<ALandscapeCombination>(GEditor->GetEditorSubsystem<UEditorActorSubsystem>()->DuplicateActor(Cast<AActor>(this)))
	)
	{
		for (auto &Generator : Generators)
		{
			if (!IsValid(Generator))
			{
				ULCReporter::ShowError(LOCTEXT("FailedDuplicateError1", "Invalid generator found during duplication."));
				continue;
			}

			if (!Generator->Implements<ULCGenerator>())
			{
				ULCReporter::ShowError(LOCTEXT("FailedDuplicateError2", "Generator does not implement the LCGenerator interface."));
				continue;
			}

			if (AActor *DuplicatedGenerator = Cast<ILCGenerator>(Generator)->Duplicate(FromName, ToName))
			{
				DuplicatedGenerator->SetFolderPath(ToName);
				NewCombination->Generators.Add(DuplicatedGenerator);
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

#undef LOCTEXT_NAMESPACE
