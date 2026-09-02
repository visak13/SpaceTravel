// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpaceSectorStreamer.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogSpaceStreamer, Log, All);

// Family generation ranges (bodies per sector). Kept sparse for the S1 slice / perf gate.
static const int32 GFamilyMin[(int32)ESpaceFamily::Count] = { 0, 1, 0, 1 };
static const int32 GFamilyMax[(int32)ESpaceFamily::Count] = { 2, 3, 1, 2 };

ASpaceSectorStreamer::ASpaceSectorStreamer()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ASpaceSectorStreamer::BeginPlay()
{
	Super::BeginPlay();
	SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	BodyMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/Materials/M_Nebula.M_Nebula"));
	UE_LOG(LogSpaceStreamer, Log, TEXT("[streamer] begin: CellSize=%.0f LoadRadius=%d seed=0x%llX floatingOrigin=%d mesh=%d mat=%d"),
		CellSize, LoadRadius, WorldSeed(), bUseFloatingOrigin ? 1 : 0, SphereMesh != nullptr, BodyMaterial != nullptr);
}

FInt64Vector ASpaceSectorStreamer::WorldToSector(const FVector& World) const
{
	return FInt64Vector(
		OriginSector.X + (int64)FMath::FloorToDouble(World.X / CellSize),
		OriginSector.Y + (int64)FMath::FloorToDouble(World.Y / CellSize),
		OriginSector.Z + (int64)FMath::FloorToDouble(World.Z / CellSize));
}

FVector ASpaceSectorStreamer::SectorToWorld(const FInt64Vector& Sector, const FVector& LocalOffset) const
{
	return FVector(
		(double)(Sector.X - OriginSector.X) * CellSize + LocalOffset.X,
		(double)(Sector.Y - OriginSector.Y) * CellSize + LocalOffset.Y,
		(double)(Sector.Z - OriginSector.Z) * CellSize + LocalOffset.Z);
}

APawn* ASpaceSectorStreamer::GetTrackedPawn() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

FInt64Vector ASpaceSectorStreamer::PlayerLogicalSector() const
{
	if (const APawn* P = GetTrackedPawn())
	{
		return WorldToSector(P->GetActorLocation());
	}
	return OriginSector;
}

void ASpaceSectorStreamer::GenerateSector(const FInt64Vector& Sector, FSectorState& Out)
{
	// Generation reads ONLY the logical sector coordinate — never world position.
	for (int32 Fam = 0; Fam < (int32)ESpaceFamily::Count; ++Fam)
	{
		const ESpaceFamily Family = (ESpaceFamily)Fam;
		const int32 Count = USpaceGenLibrary::BodyCount(WorldSeed(), Sector, Family, GFamilyMin[Fam], GFamilyMax[Fam]);
		for (int32 i = 0; i < Count; ++i)
		{
			FBodyRecord R;
			R.Sector = Sector;
			R.Family = (uint8)Fam;
			R.ObjectIndex = i;
			// Properties: 1..3 position, 4 radius. Domain-separated, order-independent.
			R.LocalOffset = FVector(
				USpaceGenLibrary::RandRange(WorldSeed(), Sector, Family, i, 1, 0.05f * CellSize, 0.95f * CellSize),
				USpaceGenLibrary::RandRange(WorldSeed(), Sector, Family, i, 2, 0.05f * CellSize, 0.95f * CellSize),
				USpaceGenLibrary::RandRange(WorldSeed(), Sector, Family, i, 3, 0.05f * CellSize, 0.95f * CellSize));
			R.LogicalRadius = USpaceGenLibrary::RandRange(WorldSeed(), Sector, Family, i, 4, 20000.f, 120000.f);
			R.Seed = USpaceGenLibrary::HashSeed(WorldSeed(), Sector, Family, i, 0xFF);
			Out.Bodies.Add(R);
		}
	}
}

static float TierScale(EBodyTier Tier, float LogicalRadius)
{
	// Sphere base radius is 50 uu at scale 1. Near-field = true logical size; coarser tiers shrink.
	const float NearScale = LogicalRadius / 50.f;
	switch (Tier)
	{
	case EBodyTier::NearField:      return NearScale;
	case EBodyTier::MidProxy:       return NearScale * 0.5f;
	case EBodyTier::HorizonImpostor:return NearScale * 0.2f;
	default:                        return NearScale * 0.2f;
	}
}

void ASpaceSectorStreamer::EnsureSectorLoaded(const FInt64Vector& Sector)
{
	if (Loaded.Contains(Sector))
	{
		return;
	}
	FSectorState State;
	GenerateSector(Sector, State);

	UWorld* World = GetWorld();
	for (FBodyRecord& R : State.Bodies)
	{
		const FVector WorldPos = SectorToWorld(R.Sector, R.LocalOffset);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(WorldPos, FRotator::ZeroRotator, Params);
		if (A)
		{
			A->SetMobility(EComponentMobility::Movable);
			if (UStaticMeshComponent* SMC = A->GetStaticMeshComponent())
			{
				if (SphereMesh)   { SMC->SetStaticMesh(SphereMesh); }
				if (BodyMaterial) { SMC->SetMaterial(0, BodyMaterial); }
			}
			R.Tier = EBodyTier::HorizonImpostor;
			A->SetActorScale3D(FVector(TierScale(R.Tier, R.LogicalRadius)));
			R.Actor = A;
		}
	}
	UE_LOG(LogSpaceStreamer, Log, TEXT("[streamer] load sector (%lld,%lld,%lld): %d bodies"),
		Sector.X, Sector.Y, Sector.Z, State.Bodies.Num());
	Loaded.Add(Sector, MoveTemp(State));
}

void ASpaceSectorStreamer::UnloadSector(const FInt64Vector& Sector)
{
	if (FSectorState* State = Loaded.Find(Sector))
	{
		for (FBodyRecord& R : State->Bodies)
		{
			if (R.Actor) { R.Actor->Destroy(); }
		}
		Loaded.Remove(Sector);
	}
}

void ASpaceSectorStreamer::UpdateTiers(const FVector& CameraWorld)
{
	const double NearD = 0.5 * CellSize;
	const double MidD = 2.0 * CellSize;
	for (auto& Pair : Loaded)
	{
		for (FBodyRecord& R : Pair.Value.Bodies)
		{
			if (!R.Actor) { continue; }
			const double Dist = FVector::Dist(R.Actor->GetActorLocation(), CameraWorld);
			EBodyTier NewTier = (Dist < NearD) ? EBodyTier::NearField
			                  : (Dist < MidD)  ? EBodyTier::MidProxy
			                                   : EBodyTier::HorizonImpostor;
			if (NewTier != R.Tier)
			{
				UE_LOG(LogSpaceStreamer, Log, TEXT("[streamer] body S(%lld,%lld,%lld)#%d fam%d tier %d->%d dist=%.0f"),
					R.Sector.X, R.Sector.Y, R.Sector.Z, R.ObjectIndex, R.Family, (int)R.Tier, (int)NewTier, Dist);
				R.Tier = NewTier;
				R.Actor->SetActorScale3D(FVector(TierScale(R.Tier, R.LogicalRadius)));
			}
		}
	}
}

void ASpaceSectorStreamer::MaybeRecenter(APawn* Pawn)
{
	if (!bUseFloatingOrigin || !Pawn) { return; }
	const FVector P = Pawn->GetActorLocation();
	if (P.Length() < RecenterThreshold) { return; }

	const FInt64Vector Shift(
		(int64)FMath::FloorToDouble(P.X / CellSize),
		(int64)FMath::FloorToDouble(P.Y / CellSize),
		(int64)FMath::FloorToDouble(P.Z / CellSize));
	if (Shift == FInt64Vector::ZeroValue) { return; }

	const FVector WorldShift((double)Shift.X * CellSize, (double)Shift.Y * CellSize, (double)Shift.Z * CellSize);
	// Translate everything back toward origin; logical positions are untouched.
	for (auto& Pair : Loaded)
	{
		for (FBodyRecord& R : Pair.Value.Bodies)
		{
			if (R.Actor) { R.Actor->AddActorWorldOffset(-WorldShift, false); }
		}
	}
	Pawn->AddActorWorldOffset(-WorldShift, false);
	OriginSector += Shift;
	++RecenterCount;
	UE_LOG(LogSpaceStreamer, Log, TEXT("[streamer] RECENTER #%d shift(%lld,%lld,%lld) -> OriginSector(%lld,%lld,%lld) pawnWas=%.0f"),
		RecenterCount, Shift.X, Shift.Y, Shift.Z, OriginSector.X, OriginSector.Y, OriginSector.Z, P.Length());
}

void ASpaceSectorStreamer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APawn* Pawn = GetTrackedPawn();
	if (!Pawn) { return; }

	MaybeRecenter(Pawn);

	const FInt64Vector Center = WorldToSector(Pawn->GetActorLocation());
	LastPlayerLogicalSector = Center;

	// Load the neighbourhood (a few per frame is enough at this density; spawn all in-range here).
	for (int64 dz = -LoadRadius; dz <= LoadRadius; ++dz)
	for (int64 dy = -LoadRadius; dy <= LoadRadius; ++dy)
	for (int64 dx = -LoadRadius; dx <= LoadRadius; ++dx)
	{
		EnsureSectorLoaded(FInt64Vector(Center.X + dx, Center.Y + dy, Center.Z + dz));
	}

	// Unload sectors outside the neighbourhood.
	TArray<FInt64Vector> ToUnload;
	for (const auto& Pair : Loaded)
	{
		const FInt64Vector& S = Pair.Key;
		if (FMath::Abs(S.X - Center.X) > LoadRadius || FMath::Abs(S.Y - Center.Y) > LoadRadius || FMath::Abs(S.Z - Center.Z) > LoadRadius)
		{
			ToUnload.Add(S);
		}
	}
	for (const FInt64Vector& S : ToUnload) { UnloadSector(S); }

	UpdateTiers(Pawn->GetActorLocation());
}

FString ASpaceSectorStreamer::RecordsDigest() const
{
	TArray<FString> Lines;
	for (const auto& Pair : Loaded)
	{
		for (const FBodyRecord& R : Pair.Value.Bodies)
		{
			Lines.Add(FString::Printf(TEXT("S(%lld,%lld,%lld) fam%d #%d off(%.1f,%.1f,%.1f) r%.1f seed%llX"),
				R.Sector.X, R.Sector.Y, R.Sector.Z, R.Family, R.ObjectIndex,
				R.LocalOffset.X, R.LocalOffset.Y, R.LocalOffset.Z, R.LogicalRadius, R.Seed));
		}
	}
	Lines.Sort();
	const FString Joined = FString::Join(Lines, TEXT("\n"));
	const uint32 H = FCrc::StrCrc32(*Joined);
	return FString::Printf(TEXT("count=%d crc=%08X"), Lines.Num(), H);
}
