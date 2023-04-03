// Copyright LandscapeCombinator. All Rights Reserved.

#include "Utils/Time.h"
#include "Utils/Logging.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

namespace Time
{
	template<typename T>
	T Time(FString Label, TFunction<T()> Code)
	{
		double Before = FPlatformTime::Seconds();
		T Result = Code();
		double After = FPlatformTime::Seconds();
		
		if (After - Before > 1)
		{
			UE_LOG(LogLandscapeCombinator, Log, TEXT("%s finished in %f"), *Label, After - Before);
		}

		if (TimeSpent.Contains(Label))
		{
			TimeSpent[Label] += (After - Before);
		}
		else
		{
			TimeSpent.Add(Label, After - Before);
		}
		return Result;
	}

	void DumpTable()
	{
		UE_LOG(LogLandscapeCombinator, Log, TEXT("Timers"));
		for (auto& Pair : TimeSpent)
		{
			UE_LOG(LogLandscapeCombinator, Log, TEXT("%s: %f s"), *(Pair.Key), Pair.Value);
		}
	}
}

#undef LOCTEXT_NAMESPACE