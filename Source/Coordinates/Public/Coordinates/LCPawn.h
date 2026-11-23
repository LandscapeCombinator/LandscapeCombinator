// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DefaultPawn.h"
#include "LCPawn.generated.h"

class ALevelCoordinates;
class UGlobalCoordinates;

UCLASS(Config = Game)
class COORDINATES_API ALCPawn : public ADefaultPawn
{
	GENERATED_BODY()

public:
	ALCPawn(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
	bool bSphereNavigation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Navigation")
	bool bAzerty = false;

	virtual void Tick(float DeltaTime) override;
	virtual void MoveUp_World(float Value) override;

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	TWeakObjectPtr<ALevelCoordinates> LevelCoordinates;
	FVector LastLocation = FVector::ZeroVector;
	FRotator LastRotation = FRotator::ZeroRotator;
	bool bHasLast = false;

	UGlobalCoordinates* GetSphereCoordinates() const;
	ALevelCoordinates* FindNearestSphereBody() const;
	void ScaleSpeed(float Factor);
	void SpeedUp();
	void SpeedDown();
	void LookUp(float Value);
	void MoveBack(float Value);
	void KeyA(float Value);
	void KeyQ(float Value);
};
