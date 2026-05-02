#pragma once

#include "common.h"
#include <math.h>
#include <immintrin.h>

inline f32
Floor(f32 value)
{
	// NOTE: SSE4.1
	f32 result = _mm_cvtss_f32(_mm_round_ss(_mm_setzero_ps(), _mm_set_ss(value), _MM_FROUND_TO_NEG_INF|_MM_FROUND_NO_EXC));
	return result;
}

inline f32
Ceil(f32 value)
{
	// NOTE: SSE4.1
	f32 result = _mm_cvtss_f32(_mm_round_ss(_mm_setzero_ps(), _mm_set_ss(value), _MM_FROUND_TO_POS_INF|_MM_FROUND_NO_EXC));
	return result;
}

inline f32
Round(f32 value)
{
	// NOTE: SSE4.1
	f32 result = _mm_cvtss_f32(_mm_round_ss(_mm_setzero_ps(), _mm_set_ss(value), _MM_FROUND_TO_NEAREST_INT|_MM_FROUND_NO_EXC));
	return result;
}

inline i32
TruncateFloat32ToInt32(f32 value)
{
	// NOTE: SSE2
	i32 result = _mm_cvttss_si32(_mm_set_ss(value));
	return result;
}

inline f32
Sqrt(f32 value)
{
	// NOTE: SSE2
	f32 result = _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(value)));
	return result;
}

inline i32
RoundFloat32ToInt32(f32 value)
{
	// NOTE: SSE2
	// Depends of current rounding mode
	i32 result = _mm_cvtss_si32(_mm_set_ss(value));
	return result;
}

extern f32 g_sineTable[256];
extern f32 g_cosineTable[256];

//inline f32
//Sin(f32 angleRadians)
//{
//	i32 index = (i32)(angleRadians/(2.0f*Pi32) * 256.0f);
//	index = (index%256 + 256)%256;
//	f32 result = g_sineTable[index];
//	return result;
//}
//
//inline f32
//Cos(f32 angleRadians)
//{
//	i32 index = (i32)(angleRadians/(2.0f*Pi32) * 256.0f);
//	index = (index%256 + 256)%256;
//	f32 result = g_cosineTable[index];
//	return result;
//}

extern "C" float SDL_fmodf(float, float);
extern "C" float SDL_powf(float, float);
extern "C" float SDL_atan2f(float, float);
extern "C" float SDL_tanf(float);
extern "C" float SDL_sinf(float);
extern "C" float SDL_cosf(float);

inline f32
Atan2(f32 y, f32 x)
{
	f32 result = SDL_atan2f(y, x);
	return result;
}

inline f32
Fmod(f32 a, f32 b)
{
	f32 result = SDL_fmodf(a, b);
	return result;
}

inline f32
Pow(f32 a, f32 b)
{
	f32 result = SDL_powf(a, b);
	return result;
}

inline f32
Tan(f32 value)
{
	f32 result = SDL_tanf(value);
	return result;
}

inline f32
Sin(f32 angleRadians)
{
	f32 result = SDL_sinf(angleRadians);
	return result;
}

inline f32
Cos(f32 angleRadians)
{
	f32 result = SDL_cosf(angleRadians);
	return result;
}
