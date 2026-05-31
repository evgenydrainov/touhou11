#pragma once

#include <stdint.h>
#include <stddef.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef size_t usize;
typedef ptrdiff_t isize;

typedef float f32;
typedef double f64;

static void *
MemCpy(void *dest, const void *src, usize size)
{
	u8 *dest8 = (u8 *)dest;
	const u8 *src8 = (const u8 *)src;
	for (usize i = 0; i < size; i++)
	{
		*dest8++ = *src8++;
	}
	return dest;
}

static void *
MemSet(void *dest, int value, usize size)
{
	u8 *dest8 = (u8 *)dest;
	for (usize i = 0; i < size; i++)
	{
		*dest8++ = (u8)value;
	}
	return dest;
}

static usize
StrLen(const char *string)
{
	usize length = 0;
	while (*string++)
	{
		length++;
	}
	return length;
}

static int
MemCmp(const void *a, const void *b, usize length)
{
	int result = 0;

	const u8 *a8 = (const u8 *)a;
	const u8 *b8 = (const u8 *)b;

	for (usize i = 0; i < length; i++)
	{
		if (*a8 != *b8)
		{
			result = (int)*a8 - (int)*b8;
			break;
		}

		a8++;
		b8++;
	}

	return result;
}

static int
StrCmp(const char *a, const char *b)
{
	const u8 *a8 = (const u8 *)a;
	const u8 *b8 = (const u8 *)b;

	while (*a8 != '\0' && *a8 == *b8)
	{
		a8++;
		b8++;
	}

	int result = (int)*a8 - (int)*b8;
	return result;
}

static char *
StrCpy(char *dest, const char *src)
{
	char *result = dest;
	while ((*dest++ = *src++)) {}
	return result;
}

#define STRINGIFY2(a) #a
#define STRINGIFY(a) STRINGIFY2(a)

#define CONCATENATE2(a, b) a##b
#define CONCATENATE(a, b) CONCATENATE2(a, b)

#ifdef __cplusplus

#if NDEBUG
#define Assert(condition) ((void)0)
#else
#define Assert(condition) ((condition) ? (void)0 : PlatformAssertionFailed(#condition, __FILE__, __LINE__))
#endif

#define OffsetOf(T, name) (usize)(&((T*)0)->name)

#define ArrayLength(array) (sizeof(array) / sizeof((array)[0]))

#define InvalidDefaultCase default: { Assert(false); } break

#define GetTaggedUnion(entity, EnumT, name) (Assert((entity)->type == EnumT##_##name), &(entity)->_##name)

template <typename T>
struct ExitScope
{
	T lambda;
	ExitScope(T lambda) : lambda(lambda) {}
	~ExitScope() { lambda(); }
	ExitScope(const ExitScope&);
private:
	ExitScope &operator=(const ExitScope&);
};

class ExitScopeHelp
{
public:
	template <typename T>
	ExitScope<T> operator+(T t) { return t; }
};

#define defer const auto &CONCATENATE(_defer, __LINE__) = ExitScopeHelp() + [&]()

enum LogLevel : u32
{
	LogLevel_Info,
	LogLevel_Warn,
	LogLevel_Error,
};

inline const char *
GetLogLevelTag(LogLevel level)
{
	switch (level)
	{
		case LogLevel_Info: return "INFO";
		case LogLevel_Warn: return "WARN";
		case LogLevel_Error: return "ERROR";
	}
	return "unknown";
}

#define LogInfo(format, ...) PlatformLogHandler(LogLevel_Info, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define LogWarn(format, ...) PlatformLogHandler(LogLevel_Warn, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define LogError(format, ...) PlatformLogHandler(LogLevel_Error, __FILE__, __LINE__, format, ##__VA_ARGS__)

void PlatformLogHandler(LogLevel level,
						const char *file,
						int line,
						const char *format,
						...);

__declspec(noreturn) void
PlatformAssertionFailed(const char *condition,
						const char *file,
						int line);

#define Kilobytes(n) ((n)*1024LL)
#define Megabytes(n) (Kilobytes(n)*1024LL)
#define Gigabytes(n) (Megabytes(n)*1024LL)

#define Pi32 (3.1415926535897932384626433832795f)

#define IsPowerOfTwo(x) ((x) != 0 && ((x) & ((x) - 1)) == 0)

#define DEFAULT_ALIGNMENT (sizeof(void *))

inline void
Swap(i32 *a, i32 *b)
{
	i32 temp = *a;
	*a = *b;
	*b = temp;
}

inline i32
SafeTruncateI32(u64 value)
{
	Assert(value <= INT32_MAX);
	return (i32)value;
}

inline usize
AlignForward(usize ptr, usize alignment)
{
	Assert(IsPowerOfTwo(alignment));
	return (ptr + (alignment-1)) & ~(alignment-1);
}

struct Arena
{
	u8 *data;
	usize pos;
	usize capacity;
};

inline void *
PushSize(Arena *arena,
		 usize size,
		 usize alignment = DEFAULT_ALIGNMENT)
{
	//
	// Assume that arena->data is already aligned
	//

	usize alignedPos = AlignForward(arena->pos, alignment);

	Assert(alignedPos + size <= arena->capacity);

	void *result = arena->data + alignedPos;
	arena->pos = alignedPos + size;

	return result;
}

#define PushStruct(arena, T) (T*)PushSize(arena, sizeof(T))

#define PushArray(arena, count, T) (T*)PushSize(arena, (count)*sizeof(T))

inline Arena
PushArena(Arena *arena, usize capacity)
{
	Arena result = {};
	result.data = (u8 *)PushSize(arena, capacity);
	result.capacity = capacity;

	return result;
}

#endif // __cplusplus
