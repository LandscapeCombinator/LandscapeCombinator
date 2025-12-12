// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "SplineImporter/Overpass.h"

FString Overpass::QueryFromShortQuery(FString OverpassServer, double South, double West, double North, double East, FString ShortQuery)
{
	return FString::Format(TEXT("{0}?data=%5Bbbox%3A{1}%2C{2}%2C{3}%2C{4}%5D%3B%28{5}%29%3B%28%2E%5F%3B%3E%3B%29%3Bout%3B%0A"),
		{
			OverpassServer,
			South, West, North, East,
			ShortQuery
		}
	);
}
