#include "Road/FRoadBuilderOverpass.h"
#include "Utils/Download.h"

#include "Async/Async.h"
#include "Internationalization/TextLocalizationResource.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

FString FRoadBuilderOverpass::DetailsString()
{
	return OverpassQuery;
}

void FRoadBuilderOverpass::AddRoads(int WorldWidthKm, int WorldHeightKm, double ZScale, double WorldOriginX, double WorldOriginY)
{
	FString IntermediateDir = FPaths::ConvertRelativePathToFull(FPaths::EngineIntermediateDir());
	FString LandscapeCombinatorDir = FPaths::Combine(IntermediateDir, "LandscapeCombinator");
	FString DownloadDir = FPaths::Combine(LandscapeCombinatorDir, "Download");
	XmlFilePath = FPaths::Combine(DownloadDir, FString::Format(TEXT("overpass_query_{0}.xml"), { FTextLocalizationResource::HashString(OverpassQuery) % 1000000 }));

	Download::FromURL(OverpassQuery, XmlFilePath,
		[=](bool bWasSuccessful) {
			if (bWasSuccessful)
			{
				// working on splines only works in GameThread
				AsyncTask(ENamedThreads::GameThread, [=]() {
					FRoadBuilder::AddRoads(WorldWidthKm, WorldHeightKm, ZScale, WorldOriginX, WorldOriginY);
				});
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
					LOCTEXT("GetSpatialReferenceError", "Unable to get the result for the Overpass query: {0}."),
					FText::FromString(OverpassQuery)
				));
			}
			return;
		}
	);	
}

#undef LOCTEXT_NAMESPACE
