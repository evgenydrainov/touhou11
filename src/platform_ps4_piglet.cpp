#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include <orbis/Pigletv2VSH.h>
#include <orbis/libkernel.h>
#include <orbis/SystemService.h>
#include <orbis/Sysmodule.h>

#include "stb_sprintf.h"

#include "platform_api.h"
#include "game_assets.h"
#include "render_backend_opengl.h"
#include "renderer.h"
#include "debug.h"

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
{}

void
PlatformPlaySound(SoundIndex soundIndex)
{}

void *
PlatformLoadAssetFile(const char *assetFilePath,
					  usize *outFileSize,
					  Arena *arena)
{
	void *result = nullptr;

	char filePath[512];
	stbsp_snprintf(filePath, sizeof(filePath), "/app0/assets/%s", assetFilePath);

	FILE *file = fopen(filePath, "rb");
	if (file)
	{
		fseek(file, 0, SEEK_END);
		usize fileSize = ftell(file);

		void *fileData = PushSize(arena, fileSize);

		fseek(file, 0, SEEK_SET);
		if (fread(fileData, 1, fileSize, file) == fileSize)
		{
			result = fileData;
			*outFileSize = fileSize;
		}

		fclose(file);
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

	write(STDERR_FILENO, file, strlen(file));

	{
		char buf[32];
		int written = stbsp_snprintf(buf, sizeof(buf), ":%d: ", line);
		if (written > sizeof(buf)-1)
		{
			written = sizeof(buf)-1;
		}

		write(STDERR_FILENO, buf, written);
	}

	{
		char buf[512];
		int written = stbsp_vsnprintf(buf, sizeof(buf), format, args);
		if (written > sizeof(buf)-1)
		{
			written = sizeof(buf)-1;
		}

		write(STDERR_FILENO, buf, written);
	}

	char newl = '\n';
	write(STDERR_FILENO, &newl, 1);

	va_end(args);
}

struct PS4PlatformState
{
	i32 piglet;
	i32 precompiledprx;

	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;

	GameMemory gameMemory;
	GameAssets gameAssets;
	GameInput gameInput;

	OpenGLFunctions gl;
	OpenGLContextVersion gl_version;

	RenderBackendOpenGLData render_backend_opengl;
	RenderBackend render_backend;
};

static void
PlatformPS4LoadModules(PS4PlatformState *state)
{
	int ret = -1;
	int mstart_ret = 0;

	char prefix[512];
	stbsp_snprintf(prefix, sizeof(prefix), "/%s/common/lib/", sceKernelGetFsSandboxRandomWord());

	LogInfo("Loading modules...");

	LogInfo("Module prefix = %s", prefix);

	char pigletname[512];
	stbsp_snprintf(pigletname, sizeof(pigletname), "%slibScePigletv2VSH.sprx", prefix);

	char precompiledname[512];
	stbsp_snprintf(precompiledname, sizeof(precompiledname), "%slibScePrecompiledShaders.sprx", prefix);

	state->piglet = sceKernelLoadStartModule(pigletname, 0, nullptr, 0, nullptr, &mstart_ret);
	if (state->piglet < ORBIS_OK)
	{
		LogError("Failed to load the Piglet module, err=%d", state->piglet);
		for (;;) {}
	}

	LogInfo("Piglet module_start() = %d", mstart_ret);

	state->precompiledprx = sceKernelLoadStartModule(precompiledname, 0, nullptr, 0, nullptr, &mstart_ret);
	if (state->precompiledprx < ORBIS_OK)
	{
		LogError("Failed to load the Precompiled Shaders module, err=%d", state->precompiledprx);
		for (;;) {}
	}

	LogInfo("PrecompiledShaders module_start() = %d", mstart_ret);

	LogInfo("Modules loaded OK.");
}

static void
PlatformPS4CreateContext(PS4PlatformState *state, int width, int height)
{
	OrbisPglConfig pgl_config{};
	OrbisPglWindow render_window{0, static_cast<khronos_uint32_t>(width), static_cast<khronos_uint32_t>(height)};
	EGLConfig config{};
	EGLint num_configs{0};
	int ret{-1};
	int major{-1};
	int minor{-1};

	const EGLint attribs[] = {
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 0,
		EGL_STENCIL_SIZE, 0,
		EGL_SAMPLE_BUFFERS, 0,
		EGL_SAMPLES, 0,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_NONE,
	};

	const EGLint ctx_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE,
	};

	const EGLint window_attribs[] = {
		EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
		EGL_NONE,
	};

	memset(&pgl_config, 0, sizeof(pgl_config));
	pgl_config.size = sizeof(pgl_config);
	pgl_config.flags = ORBIS_PGL_FLAGS_USE_COMPOSITE_EXT | ORBIS_PGL_FLAGS_USE_FLEXIBLE_MEMORY | 0x60;
	pgl_config.processOrder = 1;
	pgl_config.systemSharedMemorySize = 250*1024*1024;
	pgl_config.videoSharedMemorySize = 512*1024*1024;
	pgl_config.maxMappedFlexibleMemory = 170*1024*1024;
	pgl_config.drawCommandBufferSize = 1*1024*1024;
	pgl_config.lcueResourceBufferSize = 1*1024*1024;
	pgl_config.dbgPosCmd_0x40 = width;
	pgl_config.dbgPosCmd_0x44 = height;
	pgl_config.dbgPosCmd_0x48 = 0;
	pgl_config.dbgPosCmd_0x4C = 0;
	pgl_config.unk_0x5C = 2;

	if (!scePigletSetConfigurationVSH(&pgl_config))
	{
		LogError("scePigletSetConfigurationVSH failed.");
		for (;;) {}
	}

	state->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (state->display == EGL_NO_DISPLAY)
	{
		LogError("eglGetDisplay failed.");;
		for (;;) {}
	}

	if (!eglInitialize(state->display, &major, &minor))
	{
		ret = eglGetError();
		LogError("eglInitialize failed: %d", ret);
		for (;;) {}
	}

	if (!eglBindAPI(EGL_OPENGL_ES_API))
	{
		ret = eglGetError();
		LogError("eglBindAPI failed: %d", ret);
		for (;;) {}
	}

	if (!eglSwapInterval(state->display, 0))
	{
		ret = eglGetError();
		LogError("eglSwapInterval failed: %d", ret);
		for (;;) {}
	}

	if (!eglChooseConfig(state->display, attribs, &config, 1, &num_configs))
	{
		ret = eglGetError();
		LogError("eglChooseConfig failed: %d", ret);
		for (;;) {}
	}

	if (num_configs < 1)
	{
		LogError("No available configuration found. num_configs=%d", num_configs);
		for (;;) {}
	}

	LogInfo("Piglet num_configs = %d", num_configs);
	LogInfo("EGL version major=%d,minor=%d", major, minor);

	state->surface = eglCreateWindowSurface(state->display, config, &render_window, window_attribs);
	if (state->surface == EGL_NO_SURFACE)
	{
		ret = eglGetError();
		LogError("eglCreateWindowSurface failed: %d", ret);
		for (;;) {}
	}

	state->context = eglCreateContext(state->display, config, EGL_NO_CONTEXT, ctx_attribs);
	if (state->context == EGL_NO_CONTEXT)
	{
		ret = eglGetError();
		LogError("eglCreateContext failed: %d", ret);
		for (;;) {}
	}

	if (!eglMakeCurrent(state->display, state->surface, state->surface, state->context))
	{
		ret = eglGetError();
		LogError("eglMakeCurrent failed: %d", ret);
		for (;;) {}
	}

	LogInfo("GL_VERSION:  %s", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
	LogInfo("GL_VENDOR:   %s", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
	LogInfo("GL_RENDERER: %s", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
	LogInfo("Piglet context created OK.");
}

static void
RenderBackendSetParams(PS4PlatformState *state)
{
	state->render_backend.windowWidth  = 1920;
	state->render_backend.windowHeight = 1080;

	state->render_backend.isExclusiveFullscreen = true;
}

static void
LoadOpenGLFunctions(OpenGLFunctions *gl, OpenGLContextVersion gl_version)
{
	gl->Clear = glClear;
	gl->ClearColor = glClearColor;
	gl->Viewport = glViewport;
	gl->GenTextures = glGenTextures;
	gl->TexImage2D = glTexImage2D;
	gl->TexParameteri = glTexParameteri;
	gl->BindTexture = glBindTexture;
	gl->Enable = glEnable;
	gl->Disable = glDisable;
	gl->BlendFunc = glBlendFunc;
	gl->DrawElements = glDrawElements;
	gl->DrawArrays = glDrawArrays;

	gl->GenBuffers = glGenBuffers;
	gl->BindBuffer = glBindBuffer;
	gl->BufferData = glBufferData;
	gl->BufferSubData = glBufferSubData;
	gl->VertexAttribPointer = glVertexAttribPointer;
	gl->EnableVertexAttribArray = glEnableVertexAttribArray;
	gl->CreateShader = glCreateShader;
	gl->ShaderSource = glShaderSource;
	gl->CompileShader = glCompileShader;
	gl->GetShaderiv = glGetShaderiv;
	gl->GetShaderInfoLog = glGetShaderInfoLog;
	gl->CreateProgram = glCreateProgram;
	gl->AttachShader = glAttachShader;
	gl->BindAttribLocation = glBindAttribLocation;
	gl->LinkProgram = glLinkProgram;
	gl->GetProgramiv = glGetProgramiv;
	gl->GetProgramInfoLog = glGetProgramInfoLog;
	gl->UseProgram = glUseProgram;
	gl->DeleteShader = glDeleteShader;
	gl->GetUniformLocation = glGetUniformLocation;
	gl->UniformMatrix4fv = glUniformMatrix4fv;
	gl->Uniform1fv = glUniform1fv;
	gl->Uniform4fv = glUniform4fv;
	gl->GenerateMipmap = glGenerateMipmap;
	gl->GenFramebuffers = glGenFramebuffers;
	gl->BindFramebuffer = glBindFramebuffer;
	gl->FramebufferTexture2D = glFramebufferTexture2D;
}

static bool
CreateOpenGLRenderBackend(PS4PlatformState *state)
{
	bool success = false;

	LoadOpenGLFunctions(&state->gl, state->gl_version);

	state->render_backend.userdata = &state->render_backend_opengl;
	OpenGLRenderBackendInit(&state->render_backend,
							state->gl_version,
							&state->gameMemory.transientArena,
							&state->gl,
							RENDERER_MAX_BATCH_QUADS,
							2*GAME_RES_W, 2*GAME_RES_H);
	success = true;

	return success;
}

static bool
CreateRenderBackend(PS4PlatformState *state)
{
	bool result = false;

	RenderBackendSetParams(state);

	result = CreateOpenGLRenderBackend(state);

	return result;
}

static bool
RenderBackendPrepareDraw(PS4PlatformState *state)
{
	bool success = false;

	RenderBackendSetParams(state);

	success = OpenGLRenderBackendPrepareDraw(&state->render_backend);

	return success;
}

static void
RenderBackendPresent(PS4PlatformState *state)
{
	OpenGLRenderBackendPresent(&state->render_backend);

	eglSwapBuffers(state->display, state->surface);
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

static void
DoOneFrame(PS4PlatformState *state)
{
	TIMED_FUNCTION();

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

static void
PlatformPS4Setup(PS4PlatformState *state)
{
	int ret = -1;

	sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE);

	ret = sceSystemServiceHideSplashScreen();
	if (ret < ORBIS_OK)
	{
		LogWarn("sceSystemServiceHideSplashScreen failed: %d", ret);
	}

	LogInfo("setup OK");
}

int main(int argc, char* args[])
{
	setvbuf(stdout, NULL, _IONBF, 0);

	PS4PlatformState state = {};
	state.gl_version = OpenGLContextVersion_2_0_ES;

	PlatformPS4LoadModules(&state);

	PlatformPS4CreateContext(&state, 1920, 1080);

	//glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

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

	PlatformPS4Setup(&state);

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
