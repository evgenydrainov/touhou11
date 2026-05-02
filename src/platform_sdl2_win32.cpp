#pragma warning(push, 0)
#pragma warning(disable: 5262)
#include "SDL.h"
#pragma warning(pop)

#ifdef WIN32

#include "DoNotIncludeWindows.h"
#include "SDL_syswm.h"

int PlatformSDL2Main();

extern "C" __declspec(noreturn) void __stdcall
ExitProcess(unsigned int uExitCode);

#if WIN32_BUILD_WITHOUT_LIBC

//
// We link with 'SDL2main.lib', which defines 'main' and 'WinMain', but the
// linker will (hopefully) pick our 'mainCRTStartup', if we define it ourselves
//
extern "C" int
mainCRTStartup(void)
{
	int code = PlatformSDL2Main();
	ExitProcess(code);
}

#endif

//
// This function is moved here because "SDL_syswm.h" includes <windows.h>
//
void *
PlatformSDL2GetHWNDFromWindow(SDL_Window *window)
{
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	SDL_GetWindowWMInfo(window, &info);

	HWND result = info.info.win.window;
	return result;
}

#else

void *
PlatformSDL2GetHWNDFromWindow(SDL_Window *window)
{
	return nullptr;
}

#endif
