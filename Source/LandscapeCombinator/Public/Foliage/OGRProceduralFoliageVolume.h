#pragma once

#include "ProceduralFoliageVolume.h"
#include "GlobalSettings.h"

#include "Landscape.h"
#include "Kismet/GameplayStatics.h"
#include "OGRProceduralFoliageVolume.generated.h"


#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"


UENUM(BlueprintType)
enum class EFoliageSourceType : uint8 {
	LocalVectorFile,
	OverpassShortQuery,
	Forests
};


UCLASS()
class LANDSCAPECOMBINATOR_API AOGRProceduralFoliageVolume : public AProceduralFoliageVolume
{
	GENERATED_BODY()
public:

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "OGRProceduralFoliage",
		meta = (DisplayPriority = "0")
	)
	EFoliageSourceType FoliageSourceType = EFoliageSourceType::Forests;


	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "OGRProceduralFoliage",
		meta = (EditCondition = "FoliageSourceType == EFoliageSourceType::LocalVectorFile", EditConditionHides, DisplayPriority = "1")
	)
	FString OSMPath;


	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "OGRProceduralFoliage",
		meta = (EditCondition = "FoliageSourceType == EFoliageSourceType::OverpassShortQuery", EditConditionHides, DisplayPriority = "2")
	)
	FString OverpassShortQuery = "nwr[\"landuse\"=\"forest\"];nwr[\"natural\"=\"wood\"];";
	

	void ResimulateWithFilterFromShortQuery(const FString& OverpassShortQuery);
	void ResimulateWithFilterFromQuery(const FString& OverpassQuery);
	void ResimulateWithFilterFromFile(const FString& Path);

	void ResimulateWithFilter();
};
