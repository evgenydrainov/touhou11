#pragma warning(push, 0)
#pragma warning(disable: 5262)
#include "SDL.h"
#pragma warning(pop)

#include "SDL_mixer.h"

#include "platform_api.h"
#include "game_assets.h"
#include "renderer.h"
#include "math_stuff.h"
#include "render_backend_opengl.h"
#include "render_backend_software.h"
#include "render_backend_d3d9.h"
#include "debug.h"

#if ENABLE_DEBUG_PROFILER
DebugProfiler g_profiler;
#endif

static u64 g_initPerfCounter;
static u64 g_perfFreq;
static f64 g_perfFreqF64;

static Mix_Music *g_music;

static Mix_Chunk *g_soundChunks[SoundIndex_COUNT];

__declspec(noreturn) void
PlatformAssertionFailed(const char *condition,
						const char *file,
						int line)
{
	SDL_TriggerBreakpoint();
}

void
PlatformPlayMusic(const char *filePath)
{
	if (g_music)
	{
		Mix_FreeMusic(g_music);
		g_music = nullptr;
	}

	g_music = Mix_LoadMUS(filePath);

	Mix_PlayMusic(g_music, -1);
}

void
PlatformPlaySound(SoundIndex soundIndex)
{
	int nchannels = Mix_AllocateChannels(-1);
	for (int i = 0; i < nchannels; i++)
	{
		if (Mix_Playing(i))
		{
			if (Mix_GetChunk(i) == g_soundChunks[soundIndex])
			{
				Mix_HaltChannel(i);
			}
		}
	}

	Mix_PlayChannel(-1, g_soundChunks[soundIndex], 0);
}

u64
DEBUG_PlatformGetPerformanceCounter(void)
{
	u64 perfCounter = SDL_GetPerformanceCounter();
	u64 result = perfCounter - g_initPerfCounter;
	return result;
}

static f64
PlatformSDL2GetTime(void)
{
	u64 perfCounter = SDL_GetPerformanceCounter();
	f64 result = (f64)(perfCounter - g_initPerfCounter) / g_perfFreqF64;
	return result;
}

void *
PlatformLoadAssetFile(const char *assetFilePath,
					  usize *outFileSize,
					  Arena *arena)
{
	void *result = nullptr;

	char filePath[512];
	SDL_snprintf(filePath, sizeof(filePath), "assets_baked/%s", assetFilePath);

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

static SDL_LogPriority
ToSDLLogProirity(LogLevel level)
{
	SDL_LogPriority result = (SDL_LogPriority)(level + 3);
	return result;
}

#if 0
static const char *
StripDirectoryFromFilePath(const char *file)
{
	int i = 0;
	while (file[i] != '\0')
	{
		i++;
	}

	while (i >= 0 && !(file[i] == '\\' || file[i] == '/'))
	{
		i--;
	}

	i++;

	file += i;
	return file;
}
#endif

void
PlatformLogHandler(LogLevel level,
				   const char *file,
				   int line,
				   const char *format,
				   ...)
{
	SDL_LogPriority priority = ToSDLLogProirity(level);

	va_list args;
	va_start(args, format);

#if 0
	file = StripDirectoryFromFilePath(file);

	char message[512];
	SDL_vsnprintf(message, sizeof(message), format, args);

	f64 time = PlatformSDL2GetTime();
	int sec = (int)time % 60;
	int min = (int)(time/60) % 60;
	SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, priority, "[%02d:%02d] [%s:%d]: %s\n", min, sec, file, line, message);
#else
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, priority, format, args);
#endif

	va_end(args);
}

static void
LoadOpenGLFunctions(OpenGLFunctions *gl, OpenGLContextVersion gl_version)
{
	gl->Clear = (PFNGLCLEARPROC) SDL_GL_GetProcAddress("glClear");
	gl->ClearColor = (PFNGLCLEARCOLORPROC) SDL_GL_GetProcAddress("glClearColor");
	gl->Viewport = (PFNGLVIEWPORTPROC) SDL_GL_GetProcAddress("glViewport");
	gl->GenTextures = (PFNGLGENTEXTURESPROC) SDL_GL_GetProcAddress("glGenTextures");
	gl->TexImage2D = (PFNGLTEXIMAGE2DPROC) SDL_GL_GetProcAddress("glTexImage2D");
	gl->TexParameteri = (PFNGLTEXPARAMETERIPROC) SDL_GL_GetProcAddress("glTexParameteri");
	gl->BindTexture = (PFNGLBINDTEXTUREPROC) SDL_GL_GetProcAddress("glBindTexture");
	gl->Enable = (PFNGLENABLEPROC) SDL_GL_GetProcAddress("glEnable");
	gl->Disable = (PFNGLDISABLEPROC) SDL_GL_GetProcAddress("glDisable");
	gl->BlendFunc = (PFNGLBLENDFUNCPROC) SDL_GL_GetProcAddress("glBlendFunc");
	gl->DrawElements = (PFNGLDRAWELEMENTSPROC) SDL_GL_GetProcAddress("glDrawElements");
	gl->DrawArrays = (PFNGLDRAWARRAYSPROC) SDL_GL_GetProcAddress("glDrawArrays");

	if (gl_version == OpenGLContextVersion_1_1)
	{
		gl->EnableClientState = (PFNGLENABLECLIENTSTATEPROC) SDL_GL_GetProcAddress("glEnableClientState");
		gl->VertexPointer = (PFNGLVERTEXPOINTERPROC) SDL_GL_GetProcAddress("glVertexPointer");
		gl->TexCoordPointer = (PFNGLTEXCOORDPOINTERPROC) SDL_GL_GetProcAddress("glTexCoordPointer");
		gl->MatrixMode = (PFNGLMATRIXMODEPROC) SDL_GL_GetProcAddress("glMatrixMode");
		gl->LoadIdentity = (PFNGLLOADIDENTITYPROC) SDL_GL_GetProcAddress("glLoadIdentity");
		gl->Ortho = (PFNGLORTHOPROC) SDL_GL_GetProcAddress("glOrtho");
		gl->LoadMatrixf = (PFNGLLOADMATRIXFPROC) SDL_GL_GetProcAddress("glLoadMatrixf");
		gl->ColorPointer = (PFNGLCOLORPOINTERPROC) SDL_GL_GetProcAddress("glColorPointer");
	}
	else
	{
		gl->GenBuffers = (PFNGLGENBUFFERSPROC) SDL_GL_GetProcAddress("glGenBuffers");
		gl->BindBuffer = (PFNGLBINDBUFFERPROC) SDL_GL_GetProcAddress("glBindBuffer");
		gl->BufferData = (PFNGLBUFFERDATAPROC) SDL_GL_GetProcAddress("glBufferData");
		gl->BufferSubData = (PFNGLBUFFERSUBDATAPROC) SDL_GL_GetProcAddress("glBufferSubData");
		gl->VertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC) SDL_GL_GetProcAddress("glVertexAttribPointer");
		gl->EnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC) SDL_GL_GetProcAddress("glEnableVertexAttribArray");
		gl->CreateShader = (PFNGLCREATESHADERPROC) SDL_GL_GetProcAddress("glCreateShader");
		gl->ShaderSource = (PFNGLSHADERSOURCEPROC) SDL_GL_GetProcAddress("glShaderSource");
		gl->CompileShader = (PFNGLCOMPILESHADERPROC) SDL_GL_GetProcAddress("glCompileShader");
		gl->GetShaderiv = (PFNGLGETSHADERIVPROC) SDL_GL_GetProcAddress("glGetShaderiv");
		gl->GetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC) SDL_GL_GetProcAddress("glGetShaderInfoLog");
		gl->CreateProgram = (PFNGLCREATEPROGRAMPROC) SDL_GL_GetProcAddress("glCreateProgram");
		gl->AttachShader = (PFNGLATTACHSHADERPROC) SDL_GL_GetProcAddress("glAttachShader");
		gl->BindAttribLocation = (PFNGLBINDATTRIBLOCATIONPROC) SDL_GL_GetProcAddress("glBindAttribLocation");
		gl->LinkProgram = (PFNGLLINKPROGRAMPROC) SDL_GL_GetProcAddress("glLinkProgram");
		gl->GetProgramiv = (PFNGLGETPROGRAMIVPROC) SDL_GL_GetProcAddress("glGetProgramiv");
		gl->GetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC) SDL_GL_GetProcAddress("glGetProgramInfoLog");
		gl->UseProgram = (PFNGLUSEPROGRAMPROC) SDL_GL_GetProcAddress("glUseProgram");
		gl->DeleteShader = (PFNGLDELETESHADERPROC) SDL_GL_GetProcAddress("glDeleteShader");
		gl->GetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC) SDL_GL_GetProcAddress("glGetUniformLocation");
		gl->UniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC) SDL_GL_GetProcAddress("glUniformMatrix4fv");
		gl->Uniform1fv = (PFNGLUNIFORM1FVPROC) SDL_GL_GetProcAddress("glUniform1fv");
		gl->Uniform4fv = (PFNGLUNIFORM4FVPROC) SDL_GL_GetProcAddress("glUniform4fv");
		gl->GenerateMipmap = (PFNGLGENERATEMIPMAPPROC) SDL_GL_GetProcAddress("glGenerateMipmap");
		gl->GenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC) SDL_GL_GetProcAddress("glGenFramebuffers");
		gl->BindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC) SDL_GL_GetProcAddress("glBindFramebuffer");
		gl->FramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC) SDL_GL_GetProcAddress("glFramebufferTexture2D");
	}

	static_assert(sizeof(OpenGLFunctions) == 47*sizeof(void*), "");
}

enum RenderBackendType : u32
{
	RenderBackendType_OpenGL,
	RenderBackendType_Software,
	RenderBackendType_D3D9,
};

enum TimingMethod : u32
{
	TimingMethod_VSync,
	TimingMethod_SleepThenSpinlock,
};

struct SDL2PlatformState
{
	bool quit;

	SDL_Window *window;

	u32 window_create_flags;

	GameMemory gameMemory;
	GameAssets gameAssets;
	GameInput gameInput;

	f64 targetFPS;

	f64 prevTime;
	f64 lastTimePrinted;

	OpenGLFunctions gl;
	OpenGLContextVersion gl_version;

	RenderBackendType render_backend_type;
	TimingMethod timing_method;

	RenderBackendOpenGLData render_backend_opengl;
	RenderBackendSoftwareData render_backend_software;
	RenderBackendD3D9Data render_backend_d3d9;
	RenderBackend render_backend;

	SDL_Surface *software_framebuffer;
	bool software_render_to_framebuffer;

	bool DEBUG_frameAdvanceMode;
	bool DEBUG_skipThisFrame;
};

static bool
CreateOpenGLRenderBackend(SDL2PlatformState *state)
{
	bool success = false;

	SDL_GLContext gl_context = SDL_GL_CreateContext(state->window);
	if (gl_context)
	{
		PFNGLGETSTRINGPROC GetString = (PFNGLGETSTRINGPROC) SDL_GL_GetProcAddress("glGetString");
		const char *version = (const char *)GetString(GL_VERSION);
		LogInfo("GL_VERSION: %s", version);

		if (version[0] == '1')
		{
			state->gl_version = OpenGLContextVersion_1_1;
		}

		if (state->timing_method == TimingMethod_VSync)
		{
			SDL_GL_SetSwapInterval(1);
		}

		LoadOpenGLFunctions(&state->gl, state->gl_version);

		if (state->gl_version == OpenGLContextVersion_3_3_Core)
		{
			PFNGLGENVERTEXARRAYSPROC GenVertexArrays = (PFNGLGENVERTEXARRAYSPROC) SDL_GL_GetProcAddress("glGenVertexArrays");
			PFNGLBINDVERTEXARRAYPROC BindVertexArray = (PFNGLBINDVERTEXARRAYPROC) SDL_GL_GetProcAddress("glBindVertexArray");

			u32 global_vao;
			GenVertexArrays(1, &global_vao);
			BindVertexArray(global_vao);
		}

		// This seems to be enabled by default on my GPU, but only in GLES profile.
		state->gl.Disable(GL_FRAMEBUFFER_SRGB);

		state->render_backend.userdata = &state->render_backend_opengl;
		OpenGLRenderBackendInit(&state->render_backend,
								state->gl_version,
								&state->gameMemory.transientArena,
								&state->gl,
								RENDERER_MAX_BATCH_QUADS,
								2*GAME_RES_W, 2*GAME_RES_H);
		success = true;
	}

	return success;
}

static bool
CreateSoftwareRenderBackend(SDL2PlatformState *state)
{
	if (state->software_render_to_framebuffer)
	{
		state->software_framebuffer = SDL_CreateRGBSurfaceWithFormat(0, GAME_RES_W, GAME_RES_H, 32, SDL_PIXELFORMAT_XBGR8888);
	}

	state->render_backend.userdata = &state->render_backend_software;
	SoftwareRenderBackendInit(&state->render_backend);

	return true;
}

void *
PlatformSDL2GetHWNDFromWindow(SDL_Window *window);

static bool
CreateD3D9RenderBackend(SDL2PlatformState *state)
{
	bool success = false;

	void *d3d9_lib = SDL_LoadObject("d3d9.dll");
	if (d3d9_lib)
	{
		void *Direct3DCreate9 = SDL_LoadFunction(d3d9_lib, "Direct3DCreate9");
		if (Direct3DCreate9)
		{
			state->render_backend.userdata = &state->render_backend_d3d9;
			bool result = D3D9RenderBackendInit(&state->render_backend,
												PlatformSDL2GetHWNDFromWindow(state->window),
												&state->gameMemory.transientArena,
												RENDERER_MAX_BATCH_QUADS,
												Direct3DCreate9);
			if (result)
			{
				success = true;
			}
		}
	}

	return success;
}

static bool
PlatformSDL2IsExclusiveFullscreen(SDL_Window *window)
{
	bool result = false;
	u32 fullscreenFlags = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP);
	if (fullscreenFlags == SDL_WINDOW_FULLSCREEN_DESKTOP)
	{
		// borderless
	}
	else if (fullscreenFlags == SDL_WINDOW_FULLSCREEN)
	{
		// exclusive
		result = true;
	}
	else
	{
		// windowed
	}

	return result;
}

static bool
PlatformSDL2IsFullscreen(SDL2PlatformState *state)
{
	bool result = false;
	if (SDL_GetWindowFlags(state->window) & SDL_WINDOW_FULLSCREEN)
	{
		result = true;
	}

	return result;
}

#if !SDL_VERSION_ATLEAST(2, 26, 0)
static void
SDL_GetWindowSizeInPixels(SDL_Window *window, int *w, int *h)
{
	SDL_GetWindowSize(window, w, h);
}
#endif

static void
RenderBackendSetParams(SDL2PlatformState *state)
{
	int windowWidth;
	int windowHeight;
	SDL_GetWindowSizeInPixels(state->window, &windowWidth, &windowHeight);

	state->render_backend.windowWidth  = windowWidth;
	state->render_backend.windowHeight = windowHeight;

	state->render_backend.isExclusiveFullscreen = PlatformSDL2IsExclusiveFullscreen(state->window);
}

static void
PlatformSDL2SetFullscreen(SDL2PlatformState *state, bool fullscreen)
{
	if (fullscreen)
	{
#if 0
		SDL_DisplayMode mode;
		int display = SDL_GetWindowDisplayIndex(state->window);
		SDL_GetDesktopDisplayMode(display, &mode);
		SDL_SetWindowDisplayMode(state->window, &mode);

		SDL_SetWindowFullscreen(state->window, SDL_WINDOW_FULLSCREEN);
#else
		SDL_SetWindowFullscreen(state->window, SDL_WINDOW_FULLSCREEN_DESKTOP);
#endif
	}
	else
	{
		SDL_SetWindowFullscreen(state->window, 0);
	}

	RenderBackendSetParams(state);

	state->render_backend.OnFullscreenChanged(&state->render_backend);
}

static bool
CreateRenderBackend(SDL2PlatformState *state)
{
	bool result = false;

	RenderBackendSetParams(state);

	switch (state->render_backend_type)
	{
		case RenderBackendType_OpenGL:
		{
			result = CreateOpenGLRenderBackend(state);
		} break;

		case RenderBackendType_Software:
		{
			result = CreateSoftwareRenderBackend(state);
		} break;

		case RenderBackendType_D3D9:
		{
			result = CreateD3D9RenderBackend(state);
		} break;
	}

	return result;
}

static bool
RenderBackendPrepareDraw(SDL2PlatformState *state)
{
	bool success = false;

	RenderBackendSetParams(state);

	switch (state->render_backend_type)
	{
		case RenderBackendType_OpenGL:
		{
			success = OpenGLRenderBackendPrepareDraw(&state->render_backend);
		} break;

		case RenderBackendType_Software:
		{
			if (state->software_render_to_framebuffer)
			{
				state->render_backend_software.backbuffer = state->software_framebuffer->pixels;
				state->render_backend_software.backbuffer_width = state->software_framebuffer->w;
				state->render_backend_software.backbuffer_height = state->software_framebuffer->h;
				state->render_backend_software.backbuffer_pitch = state->software_framebuffer->pitch;
			}
			else
			{
				SDL_Surface *window_surface = SDL_GetWindowSurface(state->window);

				static bool printed = false;
				if (!printed)
				{
					LogInfo("SDL Window Surface Format: %s", SDL_GetPixelFormatName(window_surface->format->format));
					printed = true;
				}

				state->render_backend_software.backbuffer = window_surface->pixels;
				state->render_backend_software.backbuffer_width = window_surface->w;
				state->render_backend_software.backbuffer_height = window_surface->h;
				state->render_backend_software.backbuffer_pitch = window_surface->pitch;
			}

			success = SoftwareRenderBackendPrepareDraw(&state->render_backend);
		} break;

		case RenderBackendType_D3D9:
		{
			success = D3D9RenderBackendPrepareDraw(&state->render_backend);
		} break;
	}

	return success;
}

static void
RenderBackendPresent(SDL2PlatformState *state)
{
	switch (state->render_backend_type)
	{
		case RenderBackendType_OpenGL:
		{
			OpenGLRenderBackendPresent(&state->render_backend);

			SDL_GL_SwapWindow(state->window);
		} break;

		case RenderBackendType_Software:
		{
			if (state->software_render_to_framebuffer)
			{
				SDL_Surface *window_surface = SDL_GetWindowSurface(state->window);
				SDL_BlitScaled(state->software_framebuffer, nullptr, window_surface, nullptr);
			}

			SDL_UpdateWindowSurface(state->window);
		} break;

		case RenderBackendType_D3D9:
		{
			D3D9RenderBackendPresent(&state->render_backend);
		} break;
	}
}

static void
PlatformSDL2HandleEvent(SDL2PlatformState *state, SDL_Event *event)
{
	switch (event->type)
	{
		case SDL_QUIT:
		{
			state->quit = true;
		} break;

		case SDL_KEYDOWN:
		{
			if (!event->key.repeat)
			{
				if (event->key.keysym.scancode == SDL_SCANCODE_F11) 
				{
					bool fullscreen = PlatformSDL2IsFullscreen(state);
					PlatformSDL2SetFullscreen(state, !fullscreen);
				}
				else if (event->key.keysym.scancode == SDL_SCANCODE_RETURN)
				{
					if (event->key.keysym.mod & KMOD_ALT)
					{
						bool fullscreen = PlatformSDL2IsFullscreen(state);
						PlatformSDL2SetFullscreen(state, !fullscreen);
					}
				}
				else if (event->key.keysym.scancode == SDL_SCANCODE_F3)
				{
					SDL_SetRelativeMouseMode((SDL_bool)!SDL_GetRelativeMouseMode());
				}

				auto DEBUG_HandleKeyRange = [&](u64 scancodeFrom, u64 scancodeTo,
												u64 asciiFrom)
				{
					if (event->key.keysym.scancode >= scancodeFrom
						&& event->key.keysym.scancode <= scancodeTo)
					{
						u64 keyIndex = event->key.keysym.scancode - scancodeFrom;
						u64 key = asciiFrom + keyIndex;
						u64 bitfieldIndex = key / 64;
						u64 bitMask = 1LLU << (key % 64);

						state->gameInput.DEBUG_keyboardState[bitfieldIndex] |= bitMask;
						state->gameInput.DEBUG_keyboardStatePress[bitfieldIndex] |= bitMask;
					}
				};

				DEBUG_HandleKeyRange(SDL_SCANCODE_A, SDL_SCANCODE_Z, 'A');
				DEBUG_HandleKeyRange(SDL_SCANCODE_1, SDL_SCANCODE_9, '1');
				DEBUG_HandleKeyRange(SDL_SCANCODE_0, SDL_SCANCODE_0, '0');
			}

			if (event->key.keysym.scancode == SDL_SCANCODE_F5)
			{
				state->DEBUG_frameAdvanceMode = true;
				state->DEBUG_skipThisFrame = false;
			}
			else if (event->key.keysym.scancode == SDL_SCANCODE_F6)
			{
				state->DEBUG_frameAdvanceMode = false;
			}
		} break;

		case SDL_KEYUP:
		{
			auto DEBUG_HandleKeyRange = [&](u64 scancodeFrom, u64 scancodeTo,
											u64 asciiFrom)
			{
				if (event->key.keysym.scancode >= scancodeFrom
					&& event->key.keysym.scancode <= scancodeTo)
				{
					u64 keyIndex = event->key.keysym.scancode - scancodeFrom;
					u64 key = asciiFrom + keyIndex;
					u64 bitfieldIndex = key / 64;
					u64 bitMask = 1LLU << (key % 64);

					state->gameInput.DEBUG_keyboardState[bitfieldIndex] &= ~bitMask;
				}
			};

			DEBUG_HandleKeyRange(SDL_SCANCODE_A, SDL_SCANCODE_Z, 'A');
			DEBUG_HandleKeyRange(SDL_SCANCODE_1, SDL_SCANCODE_9, '1');
			DEBUG_HandleKeyRange(SDL_SCANCODE_0, SDL_SCANCODE_0, '0');
		} break;
	}
}

static void
PlatformSDL2HandleEvents(SDL2PlatformState *state)
{
	MemSet(&state->gameInput.DEBUG_keyboardStatePress[0], 0, sizeof(state->gameInput.DEBUG_keyboardStatePress));

	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		PlatformSDL2HandleEvent(state, &event);
	}
}

#ifndef SDL_CPUPauseInstruction

#if (defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__))
    #define SDL_CPUPauseInstruction() __asm__ __volatile__("pause\n")  /* Some assemblers can't do REP NOP, so go with PAUSE. */
#elif (defined(__arm__) && defined(__ARM_ARCH) && __ARM_ARCH >= 7) || defined(__aarch64__)
    #define SDL_CPUPauseInstruction() __asm__ __volatile__("yield" ::: "memory")
#elif (defined(__powerpc__) || defined(__powerpc64__))
    #define SDL_CPUPauseInstruction() __asm__ __volatile__("or 27,27,27");
#elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    #define SDL_CPUPauseInstruction() _mm_pause()  /* this is actually "rep nop" and not a SIMD instruction. No inline asm in MSVC x86-64! */
#elif defined(_MSC_VER) && (defined(_M_ARM) || defined(_M_ARM64))
    #define SDL_CPUPauseInstruction() __yield()
#elif defined(__WATCOMC__) && defined(__386__)
    extern __inline void SDL_CPUPauseInstruction(void);
    #pragma aux SDL_CPUPauseInstruction = ".686p" ".xmm2" "pause"
#else
    #define SDL_CPUPauseInstruction()
#endif

#endif

static void
PlatformSDL2UpdateInput(SDL2PlatformState *state)
{
	GameInputController *controller = &state->gameInput.controllers[0];

	u32 prevState = controller->state;
	controller->state = 0;

	const u8 *keyboardState = SDL_GetKeyboardState(nullptr);

	if (keyboardState[SDL_SCANCODE_RIGHT])
	{
		controller->state |= GameInputKey_RIGHT;
	}
	if (keyboardState[SDL_SCANCODE_UP])
	{
		controller->state |= GameInputKey_UP;
	}
	if (keyboardState[SDL_SCANCODE_LEFT])
	{
		controller->state |= GameInputKey_LEFT;
	}
	if (keyboardState[SDL_SCANCODE_DOWN])
	{
		controller->state |= GameInputKey_DOWN;
	}
	if (keyboardState[SDL_SCANCODE_Z])
	{
		controller->state |= GameInputKey_A;
	}
	if (keyboardState[SDL_SCANCODE_X])
	{
		controller->state |= GameInputKey_B;
	}
	if (keyboardState[SDL_SCANCODE_LSHIFT])
	{
		controller->state |= GameInputKey_X;
	}

	controller->statePress = (~prevState) & controller->state;
	controller->stateRelease = (~controller->state) & prevState;
}

static void
PlatformSDL2DoOneFrame(SDL2PlatformState *state)
{
	TIMED_FUNCTION();

	f64 frameStartTime = PlatformSDL2GetTime();
	f64 deltaSeconds = frameStartTime - state->prevTime;

	if (frameStartTime - state->lastTimePrinted > 1)
	{
		//LogInfo("numDrawCalls %d", state.render_backend.numDrawCalls);

		state->lastTimePrinted = frameStartTime;
	}

	state->DEBUG_skipThisFrame = state->DEBUG_frameAdvanceMode;

	PlatformSDL2HandleEvents(state);

	PlatformSDL2UpdateInput(state);

	{
		int mouseDeltaX;
		int mouseDeltaY;
		SDL_GetRelativeMouseState(&mouseDeltaX, &mouseDeltaY);

		if (SDL_GetRelativeMouseMode())
		{
			state->gameInput.DEBUG_mouseDeltaX = mouseDeltaX;
			state->gameInput.DEBUG_mouseDeltaY = mouseDeltaY;
			state->gameInput.DEBUG_mouseCaptured = true;
		}
		else
		{
			state->gameInput.DEBUG_mouseDeltaX = 0;
			state->gameInput.DEBUG_mouseDeltaY = 0;
			state->gameInput.DEBUG_mouseCaptured = false;
		}
	}

	if (RenderBackendPrepareDraw(state))
	{
		f32 minDelta = 1.0f/10'000.0f;
		f32 maxDelta = 1.0f/20.0f;

		f32 delta = 60.0f*Clamp((f32)deltaSeconds, minDelta, maxDelta);

		state->gameInput.time = (f32)frameStartTime;
		state->gameInput.delta = delta;
		state->gameInput.DEBUG_skipThisFrame = state->DEBUG_skipThisFrame;
		state->gameInput.DEBUG_perfFreqF64 = g_perfFreqF64;

		GameUpdateAndRender(&state->gameMemory,
							&state->gameAssets,
							&state->gameInput,
							&state->render_backend);

		RenderBackendPresent(state);
	}

	state->prevTime = frameStartTime;

	if (state->timing_method == TimingMethod_SleepThenSpinlock)
	{
		f64 frameEndTime = frameStartTime + 1.0/state->targetFPS;

		f64 time = PlatformSDL2GetTime();
		if (time < frameEndTime)
		{
			u32 milliseconds = (u32)((frameEndTime - time)*1000.0);
			if (milliseconds >= 1)
			{
				SDL_Delay(milliseconds - 1);
			}

			while (PlatformSDL2GetTime() < frameEndTime)
			{
				SDL_CPUPauseInstruction();
			}
		}
	}
}

int
PlatformSDL2Main()
{
	// SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "1");

	SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "system");

	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) != 0)
	{
		char message[512];
		SDL_snprintf(message, sizeof(message), "Couldn't initialize SDL2:\n%s", SDL_GetError());
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", message, nullptr);
		SDL_Quit();
		return 1;
	}

	g_perfFreq = SDL_GetPerformanceFrequency();
	g_perfFreqF64 = (f64)g_perfFreq;
	g_initPerfCounter = SDL_GetPerformanceCounter();

	SDL2PlatformState state = {};

	state.render_backend_type = RenderBackendType_OpenGL;
	state.timing_method = TimingMethod_SleepThenSpinlock;
	state.gl_version = OpenGLContextVersion_3_3_Core;
	state.window_create_flags = SDL_WINDOW_RESIZABLE;
	state.software_render_to_framebuffer = true;

	if (state.render_backend_type == RenderBackendType_OpenGL)
	{
		switch (state.gl_version)
		{
			case OpenGLContextVersion_3_3_Core:
			{
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
			} break;

			case OpenGLContextVersion_2_0_ES:
			{
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
			} break;

			case OpenGLContextVersion_1_1:
			{
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
			} break;
		}
	
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
		// SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 0);

		// SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		// SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

		state.window_create_flags |= SDL_WINDOW_OPENGL;
	}

	state.window = SDL_CreateWindow("game",
									SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
									2*GAME_RES_W, 2*GAME_RES_H,
									state.window_create_flags);
	if (!state.window)
	{
		char message[512];
		SDL_snprintf(message, sizeof(message), "Couldn't create window:\n%s", SDL_GetError());
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", message, nullptr);
		SDL_Quit();
		return 1;
	}

	{
		usize permanentSize = Megabytes(1);
		usize transientSize = Megabytes(50);

		usize totalSize = permanentSize + transientSize;
		void *memory = SDL_calloc(totalSize, 1);

		state.gameMemory.permanentArena.data = (u8 *)memory;
		state.gameMemory.permanentArena.capacity = permanentSize;

		state.gameMemory.transientArena.data = (u8 *)memory + permanentSize;
		state.gameMemory.transientArena.capacity = transientSize;
	}

	CreateRenderBackend(&state);

	{
		SDL_DisplayMode mode;
		int display = SDL_GetWindowDisplayIndex(state.window);
		SDL_GetDesktopDisplayMode(display, &mode);

		state.targetFPS = 2.0*mode.refresh_rate;
	}

	if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 2048) != 0)
	{
		char message[512];
		SDL_snprintf(message, sizeof(message), "Couldn't initialize SDL2_mixer:\n%s", Mix_GetError());
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", message, nullptr);
		SDL_Quit();
		return 1;
	}

	{
		f32 musicVolume  = 1.00f;
		f32 soundVolume  = 0.75f;
		f32 masterVolume = 0.20f;

		Mix_VolumeMusic((int)((musicVolume*masterVolume)*MIX_MAX_VOLUME));
		Mix_Volume(-1,  (int)((soundVolume*masterVolume)*MIX_MAX_VOLUME));
	}

	{
		g_soundChunks[snd_enemy_shoot] = Mix_LoadWAV("assets_raw/sounds/enemy_shoot.wav");
		g_soundChunks[snd_reimu_shoot] = Mix_LoadWAV("assets_raw/sounds/reimu_shoot.wav");
		g_soundChunks[snd_enemy_hurt]  = Mix_LoadWAV("assets_raw/sounds/enemy_hurt.wav");
		g_soundChunks[snd_enemy_die]   = Mix_LoadWAV("assets_raw/sounds/enemy_die.wav");
		g_soundChunks[snd_pichuun]     = Mix_LoadWAV("assets_raw/sounds/pichuun.wav");

		if (g_soundChunks[snd_enemy_shoot])
		{
			Mix_VolumeChunk(g_soundChunks[snd_enemy_shoot], (int)(0.50f*MIX_MAX_VOLUME));
		}
	}

	//SDL_SetRelativeMouseMode(SDL_TRUE);
	SDL_GetRelativeMouseState(nullptr, nullptr);

	while (!state.quit)
	{
#if ENABLE_DEBUG_PROFILER
		MemCpy(g_profiler.prevRecords, g_profiler.records, g_profiler.numRecords*sizeof(DebugTimeRecord));
		g_profiler.numPrevRecords = g_profiler.numRecords;

		MemSet(g_profiler.records, 0, g_profiler.numRecords*sizeof(DebugTimeRecord));
		g_profiler.numRecords = 0;
#endif

		PlatformSDL2DoOneFrame(&state);
	}

	Mix_Quit();

	SDL_DestroyWindow(state.window);
	SDL_Quit();

	return 0;
}

#if !WIN32_BUILD_WITHOUT_LIBC

int main(int, char**)
{
	int code = PlatformSDL2Main();
	return code;
}

#endif
