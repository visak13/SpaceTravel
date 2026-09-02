// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpaceGameMode.h"
#include "SpaceSectorStreamer.h"
#include "SpaceShipPawn.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogSpaceBench, Log, All);

ASpaceGameMode::ASpaceGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = ASpaceShipPawn::StaticClass();
}

void ASpaceGameMode::BeginPlay()
{
	Super::BeginPlay();
	FActorSpawnParameters Params;
	Streamer = GetWorld()->SpawnActor<ASpaceSectorStreamer>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (Streamer)
	{
		Streamer->bUseFloatingOrigin = bUseFloatingOrigin;
	}
	UE_LOG(LogSpaceBench, Log, TEXT("[bench] BEGIN floatingOrigin=%d warmup=%.1fs end=%.1fs"),
		bUseFloatingOrigin ? 1 : 0, WarmupSeconds, BenchmarkEndSeconds);
}

void ASpaceGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Elapsed += DeltaSeconds;

	if (Elapsed >= WarmupSeconds && Elapsed < BenchmarkEndSeconds)
	{
		FrameTimesMs.Add(DeltaSeconds * 1000.f);
	}

	if (Streamer && Elapsed >= NextLogAt)
	{
		NextLogAt += 1.f;
		const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
		const FVector P = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
		const FInt64Vector LS = Streamer->PlayerLogicalSector();
		const bool bCrossed = !(LS == LastLoggedSector);
		LastLoggedSector = LS;
		UE_LOG(LogSpaceBench, Log,
			TEXT("[flight] t=%5.1f fps=%5.1f pawnWorld=(%.0f,%.0f,%.0f) logicalSector=(%lld,%lld,%lld)%s recenters=%d %s"),
			Elapsed, DeltaSeconds > 0 ? 1.f / DeltaSeconds : 0.f, P.X, P.Y, P.Z,
			LS.X, LS.Y, LS.Z, bCrossed ? TEXT(" <BOUNDARY>") : TEXT(""),
			Streamer->RecenterCount, *Streamer->RecordsDigest());
	}

	if (!bBenchmarkDumped && Elapsed >= BenchmarkEndSeconds)
	{
		DumpBenchmark();
		bBenchmarkDumped = true;
	}
}

void ASpaceGameMode::DumpBenchmark()
{
	if (FrameTimesMs.Num() == 0)
	{
		UE_LOG(LogSpaceBench, Warning, TEXT("[bench] no frames collected"));
		return;
	}
	TArray<float> S = FrameTimesMs;
	S.Sort();
	const int32 N = S.Num();
	auto Pct = [&S, N](float P) { return S[FMath::Clamp((int32)(P * N), 0, N - 1)]; };
	const float MedianMs = Pct(0.5f);
	const float P99Ms = Pct(0.99f);   // 1% worst
	const float P999Ms = Pct(0.999f); // 0.1% worst
	auto Fps = [](float Ms) { return Ms > 0 ? 1000.f / Ms : 0.f; };
	UE_LOG(LogSpaceBench, Log,
		TEXT("[bench] RESULT frames=%d median=%.2fms(%.1ffps) 1%%low=%.2fms(%.1ffps) 0.1%%low=%.2fms(%.1ffps) floatingOrigin=%d recenters=%d finalDigest=%s"),
		N, MedianMs, Fps(MedianMs), P99Ms, Fps(P99Ms), P999Ms, Fps(P999Ms),
		bUseFloatingOrigin ? 1 : 0, Streamer ? Streamer->RecenterCount : -1,
		Streamer ? *Streamer->RecordsDigest() : TEXT("n/a"));
}
