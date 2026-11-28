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
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

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

	PlayerInputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ALCPawn::SpeedUp);
	PlayerInputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ALCPawn::SpeedDown);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!Input || !PC) return;

	UInputMappingContext* Context = NewObject<UInputMappingContext>(this);

	auto MakeAxis = [&](TFunction<void(float)> Handler)
	{
		UInputAction* Action = NewObject<UInputAction>(this);
		Action->ValueType = EInputActionValueType::Axis1D;
		Input->BindActionValueLambda(Action, ETriggerEvent::Triggered, [Handler](const FInputActionValue& Value) { Handler(Value.Get<float>()); });
		return Action;
	};

	auto Map = [Context](UInputAction* Action, FKey Key, bool bNegate)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		if (bNegate) Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
	};

	UInputAction* Forward = MakeAxis([this](float Value) { MoveForward(Value); });
	UInputAction* Right = MakeAxis([this](float Value) { MoveRight(Value); });
	UInputAction* Up = MakeAxis([this](float Value) { MoveUp_World(Value); });

	const FKey Left = bAzerty ? EKeys::Q : EKeys::A;
	const FKey Down = bAzerty ? EKeys::A : EKeys::Q;
	Map(Forward, bAzerty ? EKeys::Z : EKeys::W, false);
	Map(Forward, EKeys::S, true);
	Map(Right, EKeys::D, false);
	Map(Right, Left, true);
	Map(Up, EKeys::E, false);
	Map(Up, Down, true);

	UInputAction* Look = NewObject<UInputAction>(this);
	Look->ValueType = EInputActionValueType::Axis2D;
	Context->MapKey(Look, EKeys::Mouse2D);
	Input->BindActionValueLambda(Look, ETriggerEvent::Triggered, [this](const FInputActionValue& Value)
	{
		const FVector2D Delta = Value.Get<FVector2D>();
		AddControllerYawInput(Delta.X);
		AddControllerPitchInput(-Delta.Y);
	});

	ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer())->AddMappingContext(Context, 0);
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
