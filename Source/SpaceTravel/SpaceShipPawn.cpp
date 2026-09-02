// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpaceShipPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"

ASpaceShipPawn::ASpaceShipPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->bConstrainAspectRatio = false;
	Camera->SetFieldOfView(90.f);

	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void ASpaceShipPawn::BeginPlay()
{
	Super::BeginPlay();
	ElapsedFlight = 0.f;
	Heading = Heading.GetSafeNormal(1e-4f, FVector(1.f, 0.f, 0.f));
	// Face the heading so the camera looks where we fly.
	SetActorRotation(Heading.Rotation());
}

void ASpaceShipPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bAutoFly)
	{
		return;
	}
	ElapsedFlight += DeltaSeconds;

	float Speed = CruiseSpeed;
	if (ElapsedFlight >= BoostStart && ElapsedFlight < BoostEnd)
	{
		Speed *= BoostMultiplier;
	}
	const FVector Delta = Heading * Speed * DeltaSeconds;
	AddActorWorldOffset(Delta, /*bSweep*/ false);
}
