#include "Concurrency.h"
#include "CoreMinimal.h"
#include "Async/Async.h"

void Concurrency::RunAsync(TFunction<void()> Action)
{
	Async(EAsyncExecution::Thread, [Action]() { Action(); });
	return;
}
