// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpaceGenLibrary.h"

uint64 USpaceGenLibrary::Mix64(uint64 X)
{
	// splitmix64 finalizer.
	X += 0x9E3779B97F4A7C15ULL;
	X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ULL;
	X = (X ^ (X >> 27)) * 0x94D049BB133111EBULL;
	return X ^ (X >> 31);
}

uint64 USpaceGenLibrary::Combine64(uint64 A, uint64 B)
{
	// Order-sensitive avalanche combine (boost::hash_combine-style, 64-bit).
	A ^= Mix64(B) + 0x9E3779B97F4A7C15ULL + (A << 6) + (A >> 2);
	return Mix64(A);
}

uint64 USpaceGenLibrary::HashSeed(uint64 WorldSeed, const FInt64Vector& Sector,
                                  ESpaceFamily Family, int32 ObjectIndex, uint32 Property)
{
	uint64 H = Mix64(WorldSeed ^ 0xD1B54A32D192ED03ULL);
	H = Combine64(H, static_cast<uint64>(Sector.X));
	H = Combine64(H, static_cast<uint64>(Sector.Y));
	H = Combine64(H, static_cast<uint64>(Sector.Z));
	H = Combine64(H, static_cast<uint64>(Family));
	H = Combine64(H, static_cast<uint64>(static_cast<uint32>(ObjectIndex)));
	H = Combine64(H, static_cast<uint64>(Property));
	return H;
}

double USpaceGenLibrary::UnitFromSeed(uint64 Seed)
{
	// Top 53 bits -> uniform double in [0,1).
	return static_cast<double>(Seed >> 11) * (1.0 / 9007199254740992.0);
}

float USpaceGenLibrary::RandRange(uint64 WorldSeed, const FInt64Vector& Sector, ESpaceFamily Family,
                                  int32 ObjectIndex, uint32 Property, float Min, float Max)
{
	const double U = UnitFromSeed(HashSeed(WorldSeed, Sector, Family, ObjectIndex, Property));
	return Min + static_cast<float>(U) * (Max - Min);
}

int32 USpaceGenLibrary::BodyCount(uint64 WorldSeed, const FInt64Vector& Sector, ESpaceFamily Family,
                                  int32 MinCount, int32 MaxCount)
{
	if (MaxCount <= MinCount)
	{
		return FMath::Max(0, MinCount);
	}
	// Property 0 reserved for the per-family body count of a sector.
	const double U = UnitFromSeed(HashSeed(WorldSeed, Sector, Family, /*ObjectIndex*/ -1, /*Property*/ 0));
	return MinCount + static_cast<int32>(U * static_cast<double>(MaxCount - MinCount + 1));
}
