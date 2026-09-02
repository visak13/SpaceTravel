// Copyright Epic Games, Inc. All Rights Reserved.
//
// ASpaceShipPawn — the flown viewpoint. 6-DoF is supported, but for the S1 vertical-slice benchmark
// the pawn follows a deterministic SCRIPTED route (constant cruise velocity along a fixed heading,
// optionally a boost segment), so the benchmark route is repeatable (strategy_hl Shape 4) and the
// flight crosses a known sector boundary and triggers a known origin recenter. No collision gameplay.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "SpaceShipPawn.generated.h"

class UCameraComponent;

UCLASS()
class SPACETRAVEL_API ASpaceShipPawn : public APawn
{
	GENERATED_BODY()

public:
	ASpaceShipPawn();

	virtual void Tick(float DeltaSeconds) override;

	/** Cruise speed in uu/s (1 uu = 1 cm). Design scale contract: cruise ~ 2 km/s. */
	UPROPERTY(EditAnywhere, Category="Flight")
	float CruiseSpeed = 200000.f;

	/** Boost multiplier applied during the boost window. */
	UPROPERTY(EditAnywhere, Category="Flight")
	float BoostMultiplier = 6.f;

	/** Heading (unit) the scripted route flies along, in world space at BeginPlay. */
	UPROPERTY(EditAnywhere, Category="Flight")
	FVector Heading = FVector(1.f, 0.f, 0.f);

	/** When true, Tick advances the pawn along the scripted route automatically. */
	UPROPERTY(EditAnywhere, Category="Flight")
	bool bAutoFly = true;

	/** Boost is active between these elapsed-time marks (seconds). */
	UPROPERTY(EditAnywhere, Category="Flight")
	float BoostStart = 8.f;
	UPROPERTY(EditAnywhere, Category="Flight")
	float BoostEnd = 14.f;

	/** Total elapsed flight time since BeginPlay (seconds). */
	float ElapsedFlight = 0.f;

private:
	UPROPERTY(VisibleAnywhere, Category="Flight")
	TObjectPtr<UCameraComponent> Camera;

protected:
	virtual void BeginPlay() override;
};
