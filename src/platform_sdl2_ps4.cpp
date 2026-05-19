#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>

#include <SDL2/SDL.h>

#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "stb_sprintf.h"

#include "platform_api.h"
#include "game_assets.h"
#include "render_backend_software.h"
#include "debug.h"

#if ENABLE_DEBUG_PROFILER
DebugProfiler g_profiler;
#endif

static u64 g_initPerfCounter;
static u64 g_perfFreq;
static f64 g_perfFreqF64;

u64
DEBUG_PlatformGetPerformanceCounter(void)
{
	u64 perfCounter = SDL_GetPerformanceCounter();
	u64 result = perfCounter - g_initPerfCounter;
	return result;
}

__declspec(noreturn) void
PlatformAssertionFailed(const char *condition,
						const char *file,
						int line)
{
	char buf[512];
	int written = stbsp_snprintf(buf, sizeof(buf), "ASSERTION FALIED: %s:%d: %s\n", file, line, condition);
	if (written > sizeof(buf)-1)
	{
		written = sizeof(buf)-1;
	}

	write(STDERR_FILENO, buf, written); 

	char newl = '\n';
	write(STDERR_FILENO, &newl, 1); 

	for (;;) {}
}

void
PlatformPlayMusic(const char *filePath)
{
}

void
PlatformPlaySound(SoundIndex soundIndex)
{
}

void *
PlatformLoadAssetFile(const char *assetFilePath,
					  usize *outFileSize,
					  Arena *arena)
{
	void *result = nullptr;

	char filePath[512];
	SDL_snprintf(filePath, sizeof(filePath), "/app0/assets/%s", assetFilePath);

	SDL_RWops *rw = SDL_RWFromFile(filePath, "rb");
	if (rw)
	{
		usize fileSize = (usize)SDL_RWsize(rw);
		void *fileData = PushSize(arena, fileSize);
		if (SDL_RWread(rw, fileData, 1, fileSize) == fileSize)
		{
			result = fileData;
			*outFileSize = fileSize;
		}

		SDL_RWclose(rw);
	}

	return result;
}

void
PlatformLogHandler(LogLevel level,
				   const char *file,
				   int line,
				   const char *format,
				   ...)
{
	va_list args;
	va_start(args, format);

	char buf[512];
	int written = stbsp_vsnprintf(buf, sizeof(buf), format, args);
	if (written > sizeof(buf)-1)
	{
		written = sizeof(buf)-1;
	}

	write(STDERR_FILENO, buf, written); 

	char newl = '\n';
	write(STDERR_FILENO, &newl, 1); 

	va_end(args);
}

struct SDL2PlatformState
{
	SDL_Window* window;
	SDL_Joystick* controller;

	GameMemory gameMemory;
	GameAssets gameAssets;
	GameInput gameInput;

	RenderBackendSoftwareData render_backend_software;
	RenderBackend render_backend;

	SDL_Surface *software_framebuffer;
	bool software_render_to_framebuffer;
	SDL_Surface *software_window_surface;
};

static void
RenderBackendSetParams(SDL2PlatformState *state)
{
	state->render_backend.windowWidth  = 1920;
	state->render_backend.windowHeight = 1080;

	state->render_backend.isExclusiveFullscreen = true;
}

static bool
RenderBackendPrepareDraw(SDL2PlatformState *state)
{
	bool success = false;

	RenderBackendSetParams(state);

	if (state->software_render_to_framebuffer)
	{
		state->render_backend_software.backbuffer = state->software_framebuffer->pixels;
		state->render_backend_software.backbuffer_width = state->software_framebuffer->w;
		state->render_backend_software.backbuffer_height = state->software_framebuffer->h;
		state->render_backend_software.backbuffer_pitch = state->software_framebuffer->pitch;
	}
	else
	{
		if (!state->software_window_surface)
		{
			state->software_window_surface = SDL_GetWindowSurface(state->window);
		}

		state->render_backend_software.backbuffer = state->software_window_surface->pixels;
		state->render_backend_software.backbuffer_width = state->software_window_surface->w;
		state->render_backend_software.backbuffer_height = state->software_window_surface->h;
		state->render_backend_software.backbuffer_pitch = state->software_window_surface->pitch;
	}

	success = SoftwareRenderBackendPrepareDraw(&state->render_backend);

	return success;
}

static void
RenderBackendPresent(SDL2PlatformState *state)
{
	TIMED_FUNCTION();

	if (state->software_render_to_framebuffer)
	{
		if (!state->software_window_surface)
		{
			state->software_window_surface = SDL_GetWindowSurface(state->window);
		}

		{
			//TIMED_BLOCK("RenderBackendPresent:SDL_BlitScaled");
			//SDL_BlitScaled(state->software_framebuffer, nullptr, state->software_window_surface, nullptr);

			TIMED_BLOCK("RenderBackendPresent:SDL_BlitSurface");
			SDL_BlitSurface(state->software_framebuffer, nullptr, state->software_window_surface, nullptr);
		}
	}

	{
		TIMED_BLOCK("RenderBackendPresent:SDL_UpdateWindowSurface");
		SDL_UpdateWindowSurface(state->window);
	}
}

static bool
CreateSoftwareRenderBackend(SDL2PlatformState *state)
{
	if (state->software_render_to_framebuffer)
	{
		state->software_framebuffer = SDL_CreateRGBSurfaceWithFormat(0, GAME_RES_W, GAME_RES_H, 32, SDL_PIXELFORMAT_ABGR8888);
	}

	state->render_backend.userdata = &state->render_backend_software;
	SoftwareRenderBackendInit(&state->render_backend);

	return true;
}

static bool
CreateRenderBackend(SDL2PlatformState *state)
{
	bool result = false;

	RenderBackendSetParams(state);

	result = CreateSoftwareRenderBackend(state);

	return result;
}

static void
PrintDebugRecords()
{
#if ENABLE_DEBUG_PROFILER
	LogInfo("DEBUG RECORDS:");

	for (int recordIndex = 0; recordIndex < g_profiler.numPrevRecords; recordIndex++)
	{
		DebugTimeRecord *record = &g_profiler.prevRecords[recordIndex];

		char buf[128];
		if (record->hitCount == 1)
		{
			stbsp_snprintf(buf, sizeof(buf), "%s: %fms",
							record->functionName,
							1000.0*(record->cycleCount/g_perfFreqF64));
		}
		else
		{
			stbsp_snprintf(buf, sizeof(buf), "%s[%d]: %fms",
							record->functionName,
							record->hitCount,
							1000.0*(record->cycleCount/g_perfFreqF64));
		}

		LogInfo("%s", buf);
	}
#endif
}

enum
{
	PAD_BUTTON_CROSS		= 0,
	PAD_BUTTON_CIRCLE,
	PAD_BUTTON_SQUARE,
	PAD_BUTTON_TRIANGLE,
	PAD_BUTTON_L1,
	PAD_BUTTON_R1,
	PAD_BUTTON_OPTIONS		= 9,
	PAD_BUTTON_L3			= 11,
	PAD_BUTTON_R3,
	PAD_BUTTON_UP,
	PAD_BUTTON_DOWN,
	PAD_BUTTON_LEFT,
	PAD_BUTTON_RIGHT,
	PAD_BUTTON_TOUCH_PAD,
	PAD_BUTTON_L2,
	PAD_BUTTON_R2
};

static void
HandleEvent(SDL2PlatformState *state, SDL_Event *event)
{
	TIMED_FUNCTION();

	switch (event->type)
	{
		case SDL_JOYBUTTONDOWN:
		{
			if (event->jbutton.button == PAD_BUTTON_UP)
			{
				state->gameInput.controllers[0].state |= GameInputKey_UP;
			}
			else if (event->jbutton.button == PAD_BUTTON_DOWN)
			{
				state->gameInput.controllers[0].state |= GameInputKey_DOWN;
			}
			else if (event->jbutton.button == PAD_BUTTON_LEFT)
			{
				state->gameInput.controllers[0].state |= GameInputKey_LEFT;
			}
			else if (event->jbutton.button == PAD_BUTTON_RIGHT)
			{
				state->gameInput.controllers[0].state |= GameInputKey_RIGHT;
			}
			else if (event->jbutton.button == PAD_BUTTON_CROSS)
			{
				state->gameInput.controllers[0].state |= GameInputKey_A;
			}
			else if (event->jbutton.button == PAD_BUTTON_CIRCLE)
			{
				state->gameInput.controllers[0].state |= GameInputKey_B;
			}
			else if (event->jbutton.button == PAD_BUTTON_SQUARE)
			{
				state->gameInput.controllers[0].state |= GameInputKey_X;
			}
			else if (event->jbutton.button == PAD_BUTTON_TRIANGLE)
			{
				state->gameInput.controllers[0].state |= GameInputKey_Y;
			}
		} break;

		case SDL_JOYBUTTONUP:
		{
			if (event->jbutton.button == PAD_BUTTON_UP)
			{
				state->gameInput.controllers[0].state &= ~GameInputKey_UP;
			}
			else if (event->jbutton.button == PAD_BUTTON_DOWN)
			{
				state->gameInput.controllers[0].state &= ~GameInputKey_DOWN;
			}
			else if (event->jbutton.button == PAD_BUTTON_LEFT)
			{
				state->gameInput.controllers[0].state &= ~GameInputKey_LEFT;
			}
			else if (event->jbutton.button == PAD_BUTTON_RIGHT)
			{
				state->gameInput.controllers[0].state &= ~GameInputKey_RIGHT;
			}
			else if (event->jbutton.button == PAD_BUTTON_CROSS)
			{
				state->gameInput.controllers[0].state &= ~GameInputKey_A;
			}
			else if (event->jbutton.button == PAD_BUTTON_CIRCLE)
			{
				state->gameInput.controllers[0].state &= ~GameInputKey_B;
			}
			else if (event->jbutton.button == PAD_BUTTON_SQUARE)
			{
				state->gameInput.controllers[0].state &= ~GameInputKey_X;
			}
			else if (event->jbutton.button == PAD_BUTTON_TRIANGLE)
			{
				state->gameInput.controllers[0].state &= ~GameInputKey_Y;
			}
		} break;
	}
}

static void
HandleEvents(SDL2PlatformState *state)
{
	TIMED_FUNCTION();

	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		HandleEvent(state, &event);
	}
}

static void
DoOneFrame(SDL2PlatformState *state)
{
	TIMED_FUNCTION();

	HandleEvents(state);
	
	if (RenderBackendPrepareDraw(state))
	{
		f32 delta = 1.0f;

		state->gameInput.time = 0.0f;
		state->gameInput.delta = delta;

		GameUpdateAndRender(&state->gameMemory,
							&state->gameAssets,
							&state->gameInput,
							&state->render_backend);

		RenderBackendPresent(state);
	}

	{
		static int timer = 0;
		timer++;
		if (timer >= 60)
		{
			PrintDebugRecords();
			timer = 0;
		}
	}
}

int main(int argc, char* args[])
{
	setvbuf(stdout, NULL, _IONBF, 0);

	g_perfFreq = SDL_GetPerformanceFrequency();
	g_perfFreqF64 = (f64)g_perfFreq;
	g_initPerfCounter = SDL_GetPerformanceCounter();

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0)
	{
		LogError("SDL_Init failed");
		for (;;) {}
	}

	{
		int rc = sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL);
		if (rc < 0)
		{
			LogError("sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL) failed");
			for (;;) {}
		}
	}

	SDL2PlatformState state = {};
	state.software_render_to_framebuffer = false;

	state.window = SDL_CreateWindow("main", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1920, 1080, 0);
	if (!state.window)
	{
		LogError("SDL_CreateWindow failed");
		for (;;) {}
	}

	if (SDL_NumJoysticks() < 1)
	{
		LogError("SDL_NumJoysticks() < 1");
		for (;;) {}
	}

	state.controller = SDL_JoystickOpen(0);
	if (state.controller == NULL)
	{
		LogError("SDL_JoystickOpen failed");
		for (;;) {}
	}

	{
		usize permanentSize = Megabytes(1);
		usize transientSize = Megabytes(50);

		usize totalSize = permanentSize + transientSize;
		void *memory = calloc(totalSize, 1);

		state.gameMemory.permanentArena.data = (u8 *)memory;
		state.gameMemory.permanentArena.capacity = permanentSize;

		state.gameMemory.transientArena.data = (u8 *)memory + permanentSize;
		state.gameMemory.transientArena.capacity = transientSize;
	}

	CreateRenderBackend(&state);

	LogInfo("HELLO!!!");

	for (;;)
	{
#if ENABLE_DEBUG_PROFILER
		MemCpy(g_profiler.prevRecords, g_profiler.records, g_profiler.numRecords*sizeof(DebugTimeRecord));
		g_profiler.numPrevRecords = g_profiler.numRecords;

		MemSet(g_profiler.records, 0, g_profiler.numRecords*sizeof(DebugTimeRecord));
		g_profiler.numRecords = 0;
#endif

		DoOneFrame(&state);
	}
}
