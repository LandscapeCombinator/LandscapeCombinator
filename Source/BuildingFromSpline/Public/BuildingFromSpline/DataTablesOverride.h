#pragma once

#include "CoreMinimal.h"
#include "DataTablesOverride.generated.h"

UCLASS()
class BUILDINGFROMSPLINE_API ADataTablesOverride : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataTables")
	UDataTable* LevelsTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataTables")
	UDataTable* WallSegmentsTable;
};
