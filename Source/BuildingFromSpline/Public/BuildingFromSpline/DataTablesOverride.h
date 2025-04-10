#pragma once

#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "CoreMinimal.h"
#include "DataTablesOverride.generated.h"

UCLASS()
class BUILDINGFROMSPLINE_API ADataTablesOverride : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataTables")
	TObjectPtr<UDataTable> LevelsTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataTables")
	TObjectPtr<UDataTable> WallSegmentsTable;
};
