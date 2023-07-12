// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "Utils/Overpass.h"

FString Overpass::QueryFromShortQuery(double South, double West, double North, double East, FString ShortQuery)
{
	return FString::Format(TEXT("https://overpass-api.de/api/interpreter?data=%5Bbbox%3A{0}%2C{1}%2C{2}%2C{3}%5D%3B%28{4}%29%3B%28%2E%5F%3B%3E%3B%29%3Bout%3B%0A"),
		{
			South, West, North, East,
			ShortQuery
		}
	);
}
