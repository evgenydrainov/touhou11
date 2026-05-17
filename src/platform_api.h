#pragma once

#include "common.h"

#define GAME_RES_W (640)
#define GAME_RES_H (480)

struct GameAssets;
struct RenderBackend;

enum GameInputKey : u32
{
	GameInputKey_RIGHT = (1 << 0),
	GameInputKey_UP    = (1 << 1),
	GameInputKey_LEFT  = (1 << 2),
	GameInputKey_DOWN  = (1 << 3),
	GameInputKey_A     = (1 << 4),
	GameInputKey_B     = (1 << 5),
	GameInputKey_X     = (1 << 6),
	GameInputKey_Y     = (1 << 7),

	GameInputKey_COUNT = 8,
};

struct GameInputController
{
	u32 state;
	u32 statePress;
	u32 stateRelease;
};

struct GameInput
{
	GameInputController controllers[1];

	f32 delta;
	f32 time;

	i32 DEBUG_mouseDeltaX;
	i32 DEBUG_mouseDeltaY;
	bool DEBUG_mouseCaptured;

	bool DEBUG_KeyW;
	bool DEBUG_KeyA;
	bool DEBUG_KeyS;
	bool DEBUG_KeyD;

	bool DEBUG_KeyRPressed;
	bool DEBUG_KeyHPressed;
	bool DEBUG_Key1Pressed;

	bool DEBUG_skipThisFrame;

	f64 DEBUG_perfFreqF64;
};

struct GameMemory
{
	Arena permanentArena;
	Arena transientArena;

	bool isInitted;
};

inline bool
IsKeyDown(GameInput *input,
		  int controllerIndex,
		  GameInputKey key)
{
	bool result = (input->controllers[controllerIndex].state & key) != 0;
	return result;
}

inline bool
IsKeyPressed(GameInput *input,
			 int controllerIndex,
			 GameInputKey key)
{
	bool result = (input->controllers[controllerIndex].statePress & key) != 0;
	return result;
}

inline bool
IsKeyReleased(GameInput *input,
			  int controllerIndex,
			  GameInputKey key)
{
	bool result = (input->controllers[controllerIndex].stateRelease & key) != 0;
	return result;
}

enum SoundIndex : u32
{
	snd_enemy_shoot,
	snd_reimu_shoot,
	snd_enemy_hurt,
	snd_enemy_die,
	snd_pichuun,

	SoundIndex_COUNT,
};

void GameUpdateAndRender(GameMemory *memory,
						 GameAssets *assets,
						 GameInput *input,
						 RenderBackend *backend);
void *PlatformLoadAssetFile(const char *assetFilePath,
							usize *outFileSize,
							Arena *arena);
void PlatformPlayMusic(const char *filePath);
void PlatformPlaySound(SoundIndex soundIndex);
