// Copyright Epic Games, Inc. All Rights Reserved.
//
// ASpaceSectorStreamer — the endless-world substrate (strategy_hl R1 / design §4).
//
// Invariant (adversary #2/#3, Shape 3): a body's PERSISTENT LOGICAL POSITION is the source of truth.
// A body lives in logical sector S (FInt64Vector); its world position is derived as
// (S - OriginSector) * CellSize + LocalOffset. Generation reads ONLY the logical sector S — never
// GetActorLocation()/CellSize, which resets after a floating-origin recenter. On recenter we shift
// OriginSector and translate every actor + the pawn by whole sectors so world-space coordinates stay
// small (precision), while logical positions are untouched. So a sector left and returned to — near
// origin or a million km out — regenerates the exact same records.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpaceGenLibrary.h"
#include "SpaceSectorStreamer.generated.h"

class AStaticMeshActor;
class UMaterialInterface;
class UStaticMesh;

// Representation tier of a placeholder body, chosen by camera distance (design's three tiers).
UENUM()
enum class EBodyTier : uint8
{
	None,
	HorizonImpostor,   // far: a small constant-size marker, never approached
	MidProxy,          // mid: a billboard/sprite-scale proxy
	NearField          // near: the full effect you fly through
};

USTRUCT()
struct FBodyRecord
{
	GENERATED_BODY()

	UPROPERTY() FInt64Vector Sector = FInt64Vector::ZeroValue;
	UPROPERTY() int32 ObjectIndex = 0;
	UPROPERTY() uint8 Family = 0;
	UPROPERTY() FVector LocalOffset = FVector::ZeroVector; // within-sector, uu
	UPROPERTY() float LogicalRadius = 0.f;                 // uu
	UPROPERTY() uint64 Seed = 0;

	UPROPERTY() TObjectPtr<AStaticMeshActor> Actor = nullptr;
	EBodyTier Tier = EBodyTier::None;
};

USTRUCT()
struct FSectorState
{
	GENERATED_BODY()
	UPROPERTY() TArray<FBodyRecord> Bodies;
};

UCLASS()
class SPACETRAVEL_API ASpaceSectorStreamer : public AActor
{
	GENERATED_BODY()

public:
	ASpaceSectorStreamer();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	/** uu per sector edge (1 uu = 1 cm). Design scale contract cell size. */
	UPROPERTY(EditAnywhere, Category="World") double CellSize = 2000000.0; // 20 km

	/** Sectors kept loaded in each axis around the player. */
	UPROPERTY(EditAnywhere, Category="World") int32 LoadRadius = 2;

	/** Recenter when the pawn strays this many uu from world origin. */
	UPROPERTY(EditAnywhere, Category="World") double RecenterThreshold = 4000000.0; // 2 cells

	/** Deterministic world seed. */
	UPROPERTY(EditAnywhere, Category="World") int64 WorldSeedInput = 0x5EED5;

	/** Custom floating-origin rebasing on (true) vs pure LWC translated-space, no rebasing (false).
	 *  The S1 comparison toggles this to observe precision behaviour on a far flight. */
	UPROPERTY(EditAnywhere, Category="World") bool bUseFloatingOrigin = true;

	// --- telemetry the flight/benchmark reads ---
	int32 RecenterCount = 0;
	FInt64Vector OriginSector = FInt64Vector::ZeroValue;
	FInt64Vector LastPlayerLogicalSector = FInt64Vector::ZeroValue;

	/** Logical sector the pawn is currently in (OriginSector + local grid offset). */
	FInt64Vector PlayerLogicalSector() const;

	/** A stable, order-independent digest of all currently-loaded records (determinism spot-check). */
	FString RecordsDigest() const;

	uint64 WorldSeed() const { return static_cast<uint64>(WorldSeedInput); }

private:
	UPROPERTY() TMap<FInt64Vector, FSectorState> Loaded;
	UPROPERTY() TObjectPtr<UStaticMesh> SphereMesh = nullptr;
	UPROPERTY() TObjectPtr<UMaterialInterface> BodyMaterial = nullptr;

	FVector SectorToWorld(const FInt64Vector& Sector, const FVector& LocalOffset) const;
	FInt64Vector WorldToSector(const FVector& World) const;
	void EnsureSectorLoaded(const FInt64Vector& Sector);
	void UnloadSector(const FInt64Vector& Sector);
	void GenerateSector(const FInt64Vector& Sector, FSectorState& Out);
	void UpdateTiers(const FVector& CameraWorld);
	void MaybeRecenter(APawn* Pawn);
	APawn* GetTrackedPawn() const;
};

FORCEINLINE uint32 GetTypeHash(const FInt64Vector& V)
{
	return HashCombine(HashCombine(GetTypeHash(V.X), GetTypeHash(V.Y)), GetTypeHash(V.Z));
}
