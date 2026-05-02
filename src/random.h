#pragma once

#include "common.h"
#include "math_stuff.h"

struct Xoshiro256PlusPlus
{
	u64 s[4];
};

inline u64
Xoshiro256PlusPlusRotl(u64 x, int k)
{
	return (x << k) | (x >> (64 - k));
}

inline u64
RandomNextUInt64(Xoshiro256PlusPlus *rng)
{
	u64 result = Xoshiro256PlusPlusRotl(rng->s[0] + rng->s[3], 23) + rng->s[0];

	u64 t = rng->s[1] << 17;

	rng->s[2] ^= rng->s[0];
	rng->s[3] ^= rng->s[1];
	rng->s[1] ^= rng->s[2];
	rng->s[0] ^= rng->s[3];

	rng->s[2] ^= t;

	rng->s[3] = Xoshiro256PlusPlusRotl(rng->s[3], 45);

	return result;
}

inline f32
RandomRangeFloat32(Xoshiro256PlusPlus *rng, f32 min, f32 max)
{
	u64 value = RandomNextUInt64(rng);
	f32 t = (value >> 40) * 0x1.0p-24f;
	f32 result = Lerp(min, max, t);
	return result;
}

inline u32
RandomRangeUInt32(Xoshiro256PlusPlus *rng, u32 min, u32 max)
{
	u64 value = RandomNextUInt64(rng);
	u32 result = min + (u32)(value >> 32) % (max - min);
	return result;
}
