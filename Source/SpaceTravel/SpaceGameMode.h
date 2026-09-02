// Copyright Epic Games, Inc. All Rights Reserved.
//
// ASpaceGameMode — wires the S1 vertical slice: it makes ASpaceShipPawn the default pawn, spawns
// ASpaceSectorStreamer at BeginPlay, and runs the flight telemetry + first benchmark. Every ~1 s it
// logs elapsed time, pawn world position, logical sector, recenter count and the records digest so a
// reviewer can read the sector-boundary crossing and the origin recenter straight from the log. At
// BenchmarkEndSeconds it dumps median / 1%-low / 0.1%-low frame times (strategy_hl Shape 4).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SpaceGameMode.generated.h"

class ASpaceSectorStreamer;

UCLASS()
class SPACETRAVEL_API ASpaceGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASpaceGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category="Benchmark") float WarmupSeconds = 5.f;
	UPROPERTY(EditAnywhere, Category="Benchmark") float BenchmarkEndSeconds = 40.f;

	/** Toggle the streamer's floating origin for the LWC-vs-rebasing comparison run. */
	UPROPERTY(EditAnywhere, Category="World") bool bUseFloatingOrigin = true;

private:
	UPROPERTY() TObjectPtr<ASpaceSectorStreamer> Streamer = nullptr;
	float Elapsed = 0.f;
	float NextLogAt = 0.f;
	bool bBenchmarkDumped = false;
	TArray<float> FrameTimesMs; // collected after warmup
	FInt64Vector LastLoggedSector;

	void DumpBenchmark();
};
