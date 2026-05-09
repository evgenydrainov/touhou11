#pragma once

#include "common.h"
#include "intrinsics.h"

union vec2
{
	struct
	{
		f32 x, y;
	};
	struct
	{
		f32 r, g;
	};
	f32 e[2];

	vec2 &operator+=(vec2 other)
	{
		this->x += other.x;
		this->y += other.y;
		return *this;
	}

	vec2 &operator-=(vec2 other)
	{
		this->x -= other.x;
		this->y -= other.y;
		return *this;
	}
};

union vec3
{
	struct
	{
		f32 x, y, z;
	};
	struct
	{
		f32 r, g, b;
	};
	f32 e[3];

	vec3 &operator+=(vec3 other)
	{
		this->x += other.x;
		this->y += other.y;
		this->z += other.z;
		return *this;
	}

	vec3 &operator-=(vec3 other)
	{
		this->x -= other.x;
		this->y -= other.y;
		this->z -= other.z;
		return *this;
	}
};

union vec4
{
	struct
	{
		f32 x, y, z, w;
	};
	struct
	{
		f32 r, g, b, a;
	};
	f32 e[4];

	vec4 &operator+=(vec4 other)
	{
		this->x += other.x;
		this->y += other.y;
		this->z += other.z;
		this->w += other.w;
		return *this;
	}

	vec4 &operator-=(vec4 other)
	{
		this->x -= other.x;
		this->y -= other.y;
		this->z -= other.z;
		this->w -= other.w;
		return *this;
	}
};

inline vec2
V2(f32 x, f32 y)
{
	vec2 result;
	result.x = x;
	result.y = y;

	return result;
}

inline vec3
V3(f32 x, f32 y, f32 z)
{
	vec3 result;
	result.x = x;
	result.y = y;
	result.z = z;

	return result;
}

inline vec4
V4(f32 x, f32 y, f32 z, f32 w)
{
	vec4 result;
	result.x = x;
	result.y = y;
	result.z = z;
	result.w = w;

	return result;
}

template <typename T>
inline T
Abs(T value)
{
	T result = (value >= 0) ? value : -value;
	return result;
}

template <typename T>
inline T
Min(T a, T b)
{
	T result = (a < b) ? a : b;
	return result;
}

template <>
inline vec2
Min<vec2>(vec2 a, vec2 b)
{
	vec2 result = {Min(a.x, b.x), Min(a.y, b.y)};
	return result;
}

template <>
inline vec3
Min<vec3>(vec3 a, vec3 b)
{
	vec3 result = {Min(a.x, b.x), Min(a.y, b.y), Min(a.z, b.z)};
	return result;
}

template <>
inline vec4
Min<vec4>(vec4 a, vec4 b)
{
	vec4 result = {Min(a.x, b.x), Min(a.y, b.y), Min(a.z, b.z), Min(a.w, b.w)};
	return result;
}

template <typename T>
inline T
Min(T a, T b, T c)
{
	T result = Min(Min(a, b), c);
	return result;
}

template <typename T>
inline T
Min(T a, T b, T c, T d)
{
	T result = Min(Min(Min(a, b), c), d);
	return result;
}

template <typename T>
inline T
Max(T a, T b)
{
	T result = (a > b) ? a : b;
	return result;
}

template <>
inline vec2
Max<vec2>(vec2 a, vec2 b)
{
	vec2 result = {Max(a.x, b.x), Max(a.y, b.y)};
	return result;
}

template <>
inline vec3
Max<vec3>(vec3 a, vec3 b)
{
	vec3 result = {Max(a.x, b.x), Max(a.y, b.y), Max(a.z, b.z)};
	return result;
}

template <>
inline vec4
Max<vec4>(vec4 a, vec4 b)
{
	vec4 result = {Max(a.x, b.x), Max(a.y, b.y), Max(a.z, b.z), Max(a.w, b.w)};
	return result;
}

template <typename T>
inline T
Max(T a, T b, T c)
{
	T result = Max(Max(a, b), c);
	return result;
}

template <typename T>
inline T
Max(T a, T b, T c, T d)
{
	T result = Max(Max(Max(a, b), c), d);
	return result;
}

template <typename T>
inline T
Clamp(T value, T min, T max)
{
	T result = Max(Min(value, max), min);
	return result;
}

template <typename T>
inline T
Lerp(T a, T b, f32 t)
{
	T result = a*(1.0f - t) + b*t;
	return result;
}

template <typename T>
inline T
LerpDelta(T a, T b, f32 t, f32 delta)
{
	T result = Lerp(a, b, 1.0f - Pow(1.0f - t, delta));
	return result;
}

inline f32
Wrap(f32 a, f32 b)
{
	f32 result = Fmod(a, b);
	if (result < 0.0f)
	{
		result += b;
	}

	return result;
}

inline f32
Approach(f32 source, f32 dest, f32 delta)
{
	f32 result = source + Clamp(dest - source, -delta, delta);
	return result;
}

inline f32
Radians(f32 degrees)
{
	f32 radians = degrees * (Pi32 / 180.0f);
	return radians;
}

inline f32
Degrees(f32 radians)
{
	f32 degrees = radians * (180.0f / Pi32);
	return degrees;
}

inline i32
Signi32(f32 value)
{
	if (value > 0)
	{
		return 1;
	}
	if (value == 0)
	{
		return 0;
	}
	return -1;
}

inline f32
Signf32(f32 value)
{
	if (value > 0)
	{
		return 1;
	}
	if (value == 0)
	{
		return 0;
	}
	return -1;
}

inline f32
EaseOutBack(f32 x)
{
	f32 c1 = 1.70158f;
	f32 c3 = c1 + 1.0f;
	f32 result = 1.0f + c3*(x - 1.0f)*(x - 1.0f)*(x - 1.0f) + c1*(x - 1.0f)*(x - 1.0f);
	return result;
}

inline f32
Sin01(f32 value)
{
	f32 result = 0.5f*Sin(value) + 0.5f;
	return result;
}

inline f32
Cos01(f32 value)
{
	f32 result = 0.5f*Cos(value) + 0.5f;
	return result;
}

inline vec2
operator+(vec2 a, vec2 b)
{
	vec2 result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;

	return result;
}

inline vec2
operator-(vec2 a, vec2 b)
{
	vec2 result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;

	return result;
}

inline vec2
operator*(f32 scalar, vec2 vector)
{
	vec2 result;
	result.x = scalar*vector.x;
	result.y = scalar*vector.y;

	return result;
}

inline vec2
operator*(vec2 vector, f32 scalar)
{
	vec2 result = scalar*vector;
	return result;
}

inline vec3
operator+(vec3 a, vec3 b)
{
	vec3 result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	result.z = a.z + b.z;

	return result;
}

inline vec3
operator-(vec3 a, vec3 b)
{
	vec3 result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	result.z = a.z - b.z;

	return result;
}

inline vec3
operator*(f32 scalar, vec3 vector)
{
	vec3 result;
	result.x = scalar*vector.x;
	result.y = scalar*vector.y;
	result.z = scalar*vector.z;

	return result;
}

inline vec3
operator*(vec3 vector, f32 scalar)
{
	vec3 result = scalar*vector;
	return result;
}



inline vec4
operator+(vec4 a, vec4 b)
{
	vec4 result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	result.z = a.z + b.z;
	result.w = a.w + b.w;

	return result;
}

inline vec4
operator-(vec4 a, vec4 b)
{
	vec4 result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	result.z = a.z - b.z;
	result.w = a.w - b.w;

	return result;
}

inline vec4
operator*(f32 scalar, vec4 vector)
{
	vec4 result;
	result.x = scalar*vector.x;
	result.y = scalar*vector.y;
	result.z = scalar*vector.z;
	result.w = scalar*vector.w;

	return result;
}

inline vec4
operator*(vec4 vector, f32 scalar)
{
	vec4 result = scalar*vector;
	return result;
}

union mat4
{
	f32 m[4][4];
	f32 e[16];
};

inline mat4
operator*(const mat4& a, const mat4& b)
{
	mat4 result;

	for (int c = 0; c < 4; c++)
	{
		for (int r = 0; r < 4; r++)
		{
			result.m[c][r] = (a.m[0][r] * b.m[c][0]
							  + a.m[1][r] * b.m[c][1]
							  + a.m[2][r] * b.m[c][2]
							  + a.m[3][r] * b.m[c][3]);
		}
	}

	return result;
}

#define c_white (vec4{1.0f, 1.0f, 1.0f, 1.0f})
#define c_black (vec4{0.0f, 0.0f, 0.0f, 1.0f})

inline f32
Length(f32 x, f32 y)
{
	f32 result = Sqrt(x*x + y*y);
	return result;
}

inline f32
Length(vec2 vector)
{
	f32 result = Sqrt(vector.x*vector.x + vector.y*vector.y);
	return result;
}

inline f32
Length(vec3 vector)
{
	f32 result = Sqrt(vector.x*vector.x + vector.y*vector.y + vector.z*vector.z);
	return result;
}

inline f32
LengthSquared(vec2 vector)
{
	f32 result = vector.x*vector.x + vector.y*vector.y;
	return result;
}

inline vec3
Normalize(vec3 vector)
{
	f32 length = Length(vector);

	vec3 result;
	result.x = vector.x/length;
	result.y = vector.y/length;
	result.z = vector.z/length;

	return result;
}

inline void
Normalize0(f32 *x, f32 *y)
{
	if ((*x) != 0.0f || (*y) != 0.0f)
	{
		f32 length = Length(*x, *y);
		*x /= length;
		*y /= length;
	}
}

inline vec2
Normalize0(vec2 vector)
{
	vec2 result = {};
	if (vector.x != 0.0f || vector.y != 0.0f)
	{
		f32 length = Length(vector);
		result.x = vector.x/length;
		result.y = vector.y/length;
	}

	return result;
}

inline vec3
Cross(vec3 a, vec3 b)
{
	vec3 result;
	result.x = a.y*b.z - b.y*a.z;
	result.y = a.z*b.x - b.z*a.x;
	result.z = a.x*b.y - b.x*a.y;

	return result;
}

inline f32
Dot(vec3 a, vec3 b)
{
	f32 result = a.x*b.x + a.y*b.y + a.z*b.z;
	return result;
}

inline mat4
Matrix4Identity(void)
{
	mat4 result = {};
	result.m[0][0] = 1.0f;
	result.m[1][1] = 1.0f;
	result.m[2][2] = 1.0f;
	result.m[3][3] = 1.0f;

	return result;
}

inline mat4
Matrix4Ortho(f32 left, f32 right, f32 bottom, f32 top)
{
	mat4 result = {};
	result.m[0][0] = 2.0f / (right - left);
	result.m[1][1] = 2.0f / (top - bottom);
	result.m[2][2] = -1.0f;
	result.m[3][0] = -(right + left) / (right - left);
	result.m[3][1] = -(top + bottom) / (top - bottom);
	result.m[3][3] = 1.0f;

	return result;
}

inline mat4
Matrix4Perspective(f32 fovY, f32 aspect, f32 zNear, f32 zFar)
{
	f32 tanHalfFovy = Tan(fovY / 2.0f);

	mat4 result = {};
	result.m[0][0] = 1.0f / (aspect * tanHalfFovy);
	result.m[1][1] = 1.0f / (tanHalfFovy);
	result.m[2][2] = -(zFar + zNear) / (zFar - zNear);
	result.m[2][3] = -1.0f;
	result.m[3][2] = -(2.0f * zFar * zNear) / (zFar - zNear);

	return result;
}

inline mat4
Matrix4LookAt(vec3 eye, vec3 center, vec3 up)
{
	vec3 f = Normalize(center - eye);
	vec3 s = Normalize(Cross(f, up));
	vec3 u = Cross(s, f);

	mat4 result = {};
	result.m[0][0] = s.x;
	result.m[1][0] = s.y;
	result.m[2][0] = s.z;

	result.m[0][1] = u.x;
	result.m[1][1] = u.y;
	result.m[2][1] = u.z;

	result.m[0][2] = -f.x;
	result.m[1][2] = -f.y;
	result.m[2][2] = -f.z;

	result.m[3][0] = -Dot(s, eye);
	result.m[3][1] = -Dot(u, eye);
	result.m[3][2] = Dot(f, eye);

	result.m[3][3] = 1.0f;

	return result;
}

inline u32
ColorVec4ToU32(vec4 color)
{
	u32 r = (u32)(255.0f * color.r);
	u32 g = (u32)(255.0f * color.g);
	u32 b = (u32)(255.0f * color.b);
	u32 a = (u32)(255.0f * color.a);

	u32 result = ((r << 0)
				  | (g << 8)
				  | (b << 16)
				  | (a << 24));
	return result;
}

inline f32
PointDistance(f32 x1, f32 y1, f32 x2, f32 y2)
{
	f32 result = Length(x2 - x1, y2 - y1);
	return result;
}

inline f32
PointDistance(vec2 a, vec2 b)
{
	f32 result = PointDistance(a.x, a.y, b.x, b.y);
	return result;
}

inline f32
PointDirection(f32 x1, f32 y1, f32 x2, f32 y2)
{
	f32 result = Degrees(Atan2(-(y2-y1), x2-x1));
	return result;
}

inline f32
PointDirection(vec2 a, vec2 b)
{
	f32 result = PointDirection(a.x, a.y, b.x, b.y);
	return result;
}

inline bool
CirclesOverlap(vec2 pos1, f32 radius1,
			   vec2 pos2, f32 radius2)
{
	f32 distanceSq = LengthSquared(pos2 - pos1);
	bool result = (distanceSq < (radius1+radius2)*(radius1+radius2));
	return result;
}

struct Viewport
{
	i32 x, y, width, height;
};

inline Viewport
GetViewportKeepAspect(i32 targetWidth, i32 targetHeight,
					  i32 gameWidth, i32 gameHeight,
					  i32 x, i32 y, i32 width, i32 height)
{
	f32 xscale = targetWidth/(f32)gameWidth;
	f32 yscale = targetHeight/(f32)gameHeight;
	f32 scale = Min(xscale, yscale);

	i32 gameScaledW = (i32)(gameWidth*scale);
	i32 gameScaledH = (i32)(gameHeight*scale);

	i32 gameScaledX = (targetWidth - gameScaledW)/2;
	i32 gameScaledY = (targetHeight - gameScaledH)/2;

	Viewport result = {};

	result.x = gameScaledX + (i32)(x*scale);
	result.y = gameScaledY + (i32)(y*scale);

	result.width = (i32)(width*scale);
	result.height = (i32)(height*scale);

	return result;
}
