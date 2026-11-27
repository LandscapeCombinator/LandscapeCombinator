// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "Coordinates/LCPawn.h"
#include "Coordinates/LevelCoordinates.h"
#include "Coordinates/BodyNavigation.h"
#include "Coordinates/GlobalCoordinates.h"

#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "Components/InputComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

ALCPawn::ALCPawn(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bAddDefaultMovementBindings = false;

	const FString Locale = FPlatformMisc::GetDefaultLocale().Replace(TEXT("_"), TEXT("-"));
	bAzerty = Locale.StartsWith(TEXT("fr-FR")) || Locale.StartsWith(TEXT("fr-BE")) || Locale.StartsWith(TEXT("nl-BE"));

	if (UFloatingPawnMovement* Movement = Cast<UFloatingPawnMovement>(GetMovementComponent()))
	{
		Movement->MaxSpeed = 1500;
		Movement->Acceleration = 1e12f;
		Movement->Deceleration = 1e12f;
	}
}

void ALCPawn::BeginPlay()
{
	Super::BeginPlay();
	LevelCoordinates = Cast<ALevelCoordinates>(UGameplayStatics::GetActorOfClass(this, ALevelCoordinates::StaticClass()));
}

void ALCPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	APlayerController* PC = Cast<APlayerController>(NewController);
	if (PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->ViewRollMin = -179.9f;
		PC->PlayerCameraManager->ViewRollMax = 179.9f;
	}
}

UGlobalCoordinates* ALCPawn::GetSphereCoordinates() const
{
	const ALevelCoordinates* LC = LevelCoordinates.Get();
	if (!bSphereNavigation || !IsValid(LC) || !IsValid(LC->GlobalCoordinates) || !LC->GlobalCoordinates->IsSphere()) return nullptr;
	return LC->GlobalCoordinates;
}

ALevelCoordinates* ALCPawn::FindNearestSphereBody() const
{
	for (TActorIterator<ALevelCoordinates> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && !It->IsHidden() && It->IsNearestSphereBody(GetActorLocation())) return *It;
	}
	return LevelCoordinates.Get();
}

void ALCPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ALevelCoordinates* Nearest = FindNearestSphereBody();
	if (Nearest != LevelCoordinates.Get())
	{
		LevelCoordinates = Nearest;
		bHasLast = false;
	}

	const UGlobalCoordinates* Coordinates = GetSphereCoordinates();
	if (!IsValid(Controller) || !IsValid(Coordinates))
	{
		bHasLast = false;
		return;
	}

	FVector Location = GetActorLocation();
	FRotator Rotation = Controller->GetControlRotation();
	FBodyNavigation::Apply(*Coordinates, bHasLast ? &LastLocation : nullptr, bHasLast ? &LastRotation : nullptr, Location, Rotation);
	SetActorLocation(Location);
	Controller->SetControlRotation(Rotation);

	LastLocation = GetActorLocation();
	LastRotation = Controller->GetControlRotation();
	bHasLast = true;
}

void ALCPawn::MoveUp_World(float Value)
{
	const UGlobalCoordinates* Coordinates = GetSphereCoordinates();
	const FVector Up = Coordinates ? Coordinates->Up(GetActorLocation()) : FVector::UpVector;
	if (Value != 0.f) AddMovementInput(Up, Value);
}

void ALCPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxisKey(EKeys::W, this, &ADefaultPawn::MoveForward);
	PlayerInputComponent->BindAxisKey(EKeys::Z, this, &ADefaultPawn::MoveForward);
	PlayerInputComponent->BindAxisKey(EKeys::S, this, &ALCPawn::MoveBack);
	PlayerInputComponent->BindAxisKey(EKeys::D, this, &ADefaultPawn::MoveRight);
	PlayerInputComponent->BindAxisKey(EKeys::A, this, &ALCPawn::KeyA);
	PlayerInputComponent->BindAxisKey(EKeys::Q, this, &ALCPawn::KeyQ);
	PlayerInputComponent->BindAxisKey(EKeys::E, this, &ALCPawn::MoveUp_World);
	PlayerInputComponent->BindAxisKey(EKeys::MouseX, this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxisKey(EKeys::MouseY, this, &ALCPawn::LookUp);
	PlayerInputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ALCPawn::SpeedUp);
	PlayerInputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ALCPawn::SpeedDown);
}

void ALCPawn::ScaleSpeed(float Factor)
{
	UFloatingPawnMovement* Movement = Cast<UFloatingPawnMovement>(GetMovementComponent());
	if (!Movement) return;
	Movement->MaxSpeed *= Factor;
	Movement->Acceleration *= Factor;
	Movement->Deceleration *= Factor;
}

void ALCPawn::SpeedUp() { ScaleSpeed(1.4f); }
void ALCPawn::SpeedDown() { ScaleSpeed(1.f / 1.4f); }
void ALCPawn::LookUp(float Value) { AddControllerPitchInput(-Value); }
void ALCPawn::MoveBack(float Value) { MoveForward(-Value); }

void ALCPawn::KeyA(float Value)
{
	if (bAzerty) MoveUp_World(-Value);
	else MoveRight(-Value);
}

void ALCPawn::KeyQ(float Value)
{
	if (bAzerty) MoveRight(-Value);
	else MoveUp_World(-Value);
}
