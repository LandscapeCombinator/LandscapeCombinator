#include "LandscapeCombinator/LandscapeCombination.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"
#include "LCCommon/LCBlueprintLibrary.h"
#include "SplineImporter/SplineImporter.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "HeightmapModifier/BlendLandscape.h"
#include "Kismet/GameplayStatics.h"

#if WITH_EDITOR
#include "FileHelpers.h"
#endif

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

bool ALandscapeCombination::OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated)
{
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Starting Combination with %d Generators"), Generators.Num());

	Modify();

	for (auto &GeneratorWrapper: Generators)
		GeneratorWrapper.GeneratorStatus = EGeneratorStatus::Idle;

	for (auto &GeneratorWrapper: Generators)
	{
		TSoftObjectPtr<AActor> Generator = GeneratorWrapper.Generator;
		const FString GeneratorName = Generator->GetActorNameOrLabel();

		if (!GeneratorWrapper.bIsEnabled) continue;

		GeneratorWrapper.GeneratorStatus = EGeneratorStatus::Generating;

#if WITH_EDITOR
		Concurrency::RunOnGameThreadAndWait([this](){
			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.NotifyCustomizationModuleChanged();
			return true;
		});
#endif

		if (!Generator.IsValid())
		{
			LCReporter::ShowError(LOCTEXT("InvalidActor", "Invalid actor in combination"));
			GeneratorWrapper.GeneratorStatus = EGeneratorStatus::Error;
			return false;
		}
		
		if (!Generator->Implements<ULCGenerator>())
		{
			LCReporter::ShowError(
				FText::Format(
					LOCTEXT("NonGeneratorActor", "Non-generator actor in combination: {0}"),
					FText::FromString(GeneratorName)
				)
			);
			GeneratorWrapper.GeneratorStatus = EGeneratorStatus::Error;
			return false;
		}

		double StartTime = FPlatformTime::Seconds();
		UE_LOG(LogLandscapeCombinator, Log, TEXT("Starting Generation with %s"), *GeneratorName);

		FName Path = SpawnedActorsPathOverride.IsNone() ? FName() : FName(SpawnedActorsPathOverride.ToString() / GeneratorName);

		bool bGeneratorSuccess = Cast<ILCGenerator>(Generator.Get())->Generate(Path, bIsUserInitiated);
		GeneratorWrapper.GeneratorStatus = bGeneratorSuccess ? EGeneratorStatus::Success : EGeneratorStatus::Error;

#if WITH_EDITOR
		Concurrency::RunOnGameThreadAndWait([this](){
			if (GEditor) GEditor->SelectActor(this, true, true);
			return true;
		});
#endif

		if (!bGeneratorSuccess) return false;

#if WITH_EDITOR
		if (bSaveAfterEachGenerator)
		{
			UE_LOG(LogLandscapeCombinator, Log, TEXT("Saving Level"));
			if (!Concurrency::RunOnGameThreadAndWait([bIsUserInitiated](){
				const bool bPromptUserToSave = bIsUserInitiated;
				const bool bSaveMapPackages = true;
				const bool bSaveContentPackages = true;
				const bool bFastSave = false;
				const bool bNotifyNoPackagesSaved = false;
				const bool bCanBeDeclined = false;
				return FEditorFileUtils::SaveDirtyPackages( bPromptUserToSave, bSaveMapPackages, bSaveContentPackages, bFastSave, bNotifyNoPackagesSaved, bCanBeDeclined );
			}))
			{
				GeneratorWrapper.GeneratorStatus = EGeneratorStatus::Error;
				return false;
			}
		}
#endif
		double Time = FPlatformTime::Seconds() - StartTime;

		if (!TimeSpent.Contains(Generator.Get())) TimeSpent.Add(Generator.Get(), Time);
		else TimeSpent[Generator.Get()] += Time;
	}

	TArray<TPair<AActor*, double>> Times;
	for (auto &Time : TimeSpent)
	{
		Times.Add(TPair<AActor*, double>(Time.Key, Time.Value));
	}
	Times.Sort([](const TPair<AActor*, double> &A, const TPair<AActor*, double> &B) { return A.Value < B.Value; });

	for (auto &[Actor, Time] : Times)
	{
		if (IsValid(Actor))
		{
			const FString GeneratorName = Actor->GetActorNameOrLabel();
			UE_LOG(LogLandscapeCombinator, Log, TEXT("Total generation time for %s: %f seconds"), *GeneratorName, Time);
		}
	}
	return true;
}

bool ALandscapeCombination::Cleanup_Implementation(bool bSkipPrompt)
{
	Modify();

	for (auto &GeneratorWrapper: Generators)
		GeneratorWrapper.GeneratorStatus = EGeneratorStatus::Idle;

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

		FString ObjectsString;
		for (UObject* Object : ObjectsToDelete)
		{
			if (IsValid(Object)) ObjectsString += Object->GetName() + "\n";
		}

		if (!ObjectsString.IsEmpty())
		{
			if (!LCReporter::ShowMessage(
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
				LCReporter::ShowError(LOCTEXT("FailedDuplicateError1", "Invalid generator found during duplication."));
				continue;
			}

			if (!GeneratorWrapper.Generator->Implements<ULCGenerator>())
			{
				LCReporter::ShowError(LOCTEXT("FailedDuplicateError2", "Generator does not implement the LCGenerator interface."));
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
				LCReporter::ShowError(LOCTEXT("FailedDuplicateError1", "Failed to duplicate generator."));
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
