#include "Concurrency.h"
#include "CoreMinimal.h"
#include "Async/Async.h"

void Concurrency::RunAsync(TFunction<void()> Action)
{
	Async(EAsyncExecution::Thread, [Action]() { Action(); });
	return;
}

void Concurrency::RunMany(TArray<FString> Elements, TFunction<void( FString Element, TFunction<void(bool)> )> Action, TFunction<void(bool)> OnComplete) {

	int32 NumberOfTasks = Elements.Num();
	std::atomic<int32> *SuccessfulTasks = new std::atomic<int>(0);
	std::atomic<int32> *FinishedTasks = new std::atomic<int>(0);
	UE_LOG(LogTemp, Display, TEXT("Starting %d tasks asynchronously"), NumberOfTasks);

	TFunction<void(bool)> OnCompleteAction = [FinishedTasks, SuccessfulTasks, NumberOfTasks, OnComplete](bool bWasSuccessful) {
		(*FinishedTasks)++;
		if (bWasSuccessful) (*SuccessfulTasks)++;

		if (*FinishedTasks == NumberOfTasks) {
			OnComplete(*SuccessfulTasks == NumberOfTasks);
		}
	};

	for (const FString& Element : Elements) {
		TFuture<void> Future = Async(EAsyncExecution::Thread,
			[Element, Action, OnCompleteAction]() {
				Action(Element, OnCompleteAction);
			}
		);
	}

	return;
}
