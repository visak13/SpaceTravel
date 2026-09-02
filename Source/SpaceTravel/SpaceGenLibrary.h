// Copyright Epic Games, Inc. All Rights Reserved.
//
// Deterministic, domain-separated seeding for the endless space world.
// Determinism invariant (strategy_hl Shape 3): the generated world is a pure function of its seed
// at the level of RECORDS. Every body's RNG derives from a stable 64-bit hash of
// (world, sector, family, objectIndex, property) — never system time, a global counter, or one
// shared sequential stream (a shared stream shifts every later draw when an earlier one is added,
// adversary #9). Generation derives ONLY from the persistent int64 logical sector coordinate, so a
// far sector generates the same records it did near the origin, across runs and machines.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SpaceGenLibrary.generated.h"

// Celestial family domains — one sub-seed space each, so adding a family never perturbs another.
UENUM(BlueprintType)
enum class ESpaceFamily : uint8
{
	Galaxy   = 0,
	Nebula   = 1,
	BlackHole= 2,
	Sun      = 3,
	Count
};

UCLASS()
class SPACETRAVEL_API USpaceGenLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** splitmix64 finalizer — a strong 64-bit avalanche mix of a single word. */
	static uint64 Mix64(uint64 X);

	/** Combine two 64-bit words with good avalanche (order-sensitive), for building a hash chain. */
	static uint64 Combine64(uint64 A, uint64 B);

	/**
	 * Domain-separated seed hash. Every argument is folded in with avalanche mixing so no two
	 * (sector, family, object, property) tuples collide in practice, and adding an object index
	 * never shifts the draws of any other object.
	 */
	static uint64 HashSeed(uint64 WorldSeed, const FInt64Vector& Sector,
	                       ESpaceFamily Family, int32 ObjectIndex, uint32 Property);

	/** Uniform double in [0,1) from a 64-bit seed (top 53 bits). */
	static double UnitFromSeed(uint64 Seed);

	/** Uniform float in [Min,Max) for a named property of one object. */
	static float RandRange(uint64 WorldSeed, const FInt64Vector& Sector, ESpaceFamily Family,
	                       int32 ObjectIndex, uint32 Property, float Min, float Max);

	/** Deterministic count of bodies of a family in a sector (Poisson-ish via uniform in a range). */
	static int32 BodyCount(uint64 WorldSeed, const FInt64Vector& Sector, ESpaceFamily Family,
	                       int32 MinCount, int32 MaxCount);
};
