#pragma once

#include "common.h"

//
// Search for __rdtsc()
//
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#if ENABLE_DEBUG_PROFILER

u64
DEBUG_PlatformGetPerformanceCounter(void);

#define MAX_NUM_DEBUG_RECORDS (30)

struct DebugTimeRecord
{
	const char *fileName;
	const char *functionName;
	u64 cycleCount;
	int line;
	int hitCount;
	int callDepth;
};

struct DebugProfiler
{
	DebugTimeRecord records[MAX_NUM_DEBUG_RECORDS];
	int numRecords;

	DebugTimeRecord prevRecords[MAX_NUM_DEBUG_RECORDS];
	int numPrevRecords;

	int functionCallDepth;
};

extern DebugProfiler g_profiler;

struct TimedBlock
{
	DebugTimeRecord *record;

	TimedBlock(const char *fileName, int line, const char *functionName)
	{
		if (g_profiler.numRecords > 0
			&& g_profiler.records[g_profiler.numRecords - 1].fileName == fileName
			&& g_profiler.records[g_profiler.numRecords - 1].line == line
			&& g_profiler.records[g_profiler.numRecords - 1].functionName == functionName
			&& g_profiler.records[g_profiler.numRecords - 1].callDepth == g_profiler.functionCallDepth)
		{
			record = &g_profiler.records[g_profiler.numRecords - 1];
		}
		else
		{
			Assert(g_profiler.numRecords < MAX_NUM_DEBUG_RECORDS);
			record = &g_profiler.records[g_profiler.numRecords++];

			record->fileName = fileName;
			record->line = line;
			record->functionName = functionName;
			record->callDepth = g_profiler.functionCallDepth;
		}
		
		record->cycleCount -= DEBUG_PlatformGetPerformanceCounter(); // __rdtsc();
		record->hitCount++;

		g_profiler.functionCallDepth++;
	}

	~TimedBlock()
	{
		record->cycleCount += DEBUG_PlatformGetPerformanceCounter(); // __rdtsc();

		g_profiler.functionCallDepth--;
	}
};

#define TIMED_FUNCTION()       TimedBlock CONCATENATE(_timedBlock, __LINE__) (__FILE__, __LINE__, __FUNCTION__)
#define TIMED_BLOCK(blockName) TimedBlock CONCATENATE(_timedBlock, __LINE__) (__FILE__, __LINE__, blockName)

#else

#define TIMED_FUNCTION()
#define TIMED_BLOCK(blockName)

#endif
