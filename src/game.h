#pragma once

#include "common.h"
#include "asset_info.h"
#include "renderer.h"
#include "math_stuff.h"
#include "random.h"

#define PLAY_AREA_X (32)
#define PLAY_AREA_Y (16)
#define PLAY_AREA_W (384)
#define PLAY_AREA_H (448)

#define PLAYER_STARTING_POS (vec2{0.0f, 384.0f})

#define MAX_NUM_BULLETS (1'000)
#define MAX_NUM_PLAYER_BULLETS (1'000)
#define MAX_NUM_ENEMIES (1'000)
#define MAX_NUM_PARTICLES (1'000)
#define MAX_NUM_PICKUPS (1'000)

struct Player;
struct Game;
struct GameInput;
struct World;

struct CharacterInfo
{
	f32 moveSpeed;
	f32 focusSpeed;
	f32 radius;
	f32 grazeRadius;
	f32 deathbombTime;
	i32 startingBombs;
	void (*ShotType)(Player *player, World *world, GameInput *input);
	void (*Bomb)(Player *player);
	SpriteIndex sprIdle;
	SpriteIndex sprMoveLeft;
	SpriteIndex sprMoveRight;
};

enum CharacterIndex : u32
{
	CharacterIndex_Reimu,
	CharacterIndex_Marisa,

	CharacterIndex_COUNT,
};

struct Player
{
	vec2 pos;
	vec2 vel;

	SpriteIndex spriteIndex;
	f32 frameIndex;

	CharacterIndex characterIndex;

	f32 hitboxAnim;

	f32 fireTimer;
	i32 fireQueue;
};

struct Bullet
{
	union
	{
		vec2 pos;
		struct
		{
			f32 x;
			f32 y;
		};
	};

	union
	{
		vec2 vel;
		struct
		{
			f32 xVel;
			f32 yVel;
		};
	};

	f32 radius;
	f32 damage;

	SpriteIndex spriteIndex;
	i32 frameIndex;

	f32 lifeSpan;
	f32 lifeTime;
};

enum CoroutineResult : u32
{
	CoroutineResult_Yield,
	CoroutineResult_Finished,
};

struct ScriptVM;

typedef CoroutineResult ScriptFunction(ScriptVM *vm, World *world, void *userdata);

struct ScriptVM
{
	ScriptFunction *func;
	i16 state[4];
	f32 waitTimer;
	i32 i, j, k, l, N, M;
	f32 f0, f1, f2, f3;
	u8 callDepth;
};

struct Boss
{
	union
	{
		vec2 pos;
		struct
		{
			f32 x;
			f32 y;
		};
	};

	ScriptVM vm;
};

struct Enemy
{
	union
	{
		vec2 pos;
		struct
		{
			f32 x;
			f32 y;
		};
	};

	union
	{
		vec2 vel;
		struct
		{
			f32 xVel;
			f32 yVel;
		};
	};

	union
	{
		vec2 acc;
		struct
		{
			f32 xAcc;
			f32 yAcc;
		};
	};

	f32 radius;
	f32 health;

	f32 lifeTime;

	ScriptVM vm;

	SpriteIndex sprIdle;
	SpriteIndex sprMoveRight;

	SpriteIndex spriteIndex;
	f32 frameIndex;
	f32 xscale;
};

enum ParticleType : u32
{
	ParticleType_reimu_card_afterimage,
	ParticleType_enemy_death,
	ParticleType_enemy_death_2,

	ParticleType_COUNT,
};

struct ParticleTypeInfo
{
	SpriteIndex spriteIndex;
	f32 lifeSpan;

	vec4 colorFrom;
	vec4 colorTo;

	f32 angleFrom;
	f32 angleTo;

	vec2 scaleFrom;
	vec2 scaleTo;
};

struct Particle
{
	vec2 pos;
	vec2 vel;
	ParticleType type;
	f32 lifeTime;
	f32 angleOffset;
};

struct StageBG
{
	vec3 camPos;
	f32 camPitch;
	f32 camYaw;
	f32 camRoll;
	f32 camMoveSpeed;

	vec2 layer1Offset;
	vec2 layer2Offset;
	vec2 layer3Offset;
	vec2 layer4Offset;
};

enum PickupType : u32
{
	PickupType_Power,
	PickupType_Point,
	PickupType_BigPower,
	PickupType_BigPoint,
	PickupType_Bomb,
	PickupType_1Up,
	PickupType_Score,
	PickupType_FullPower,

	PickupType_COUNT,
};

struct Pickup
{
	vec2 pos;
	vec2 vel;

	PickupType type;

	f32 lifeTime;
};

struct World
{
	Player player;

	Boss boss;

	i32 numBullets;
	i32 numPlayerBullets;
	i32 numEnemies;
	i32 numParticles;
	i32 numPickups;

	f32 coroutineUpdateTimer;

	ScriptVM stageVM;
	StageBG stageBG;
	f32 stageTimer;

	Xoshiro256PlusPlus stageRNG;
	Xoshiro256PlusPlus visualRNG;

	f32 stageLabelAlpha;
	f32 stageLabelTargetAlpha;

	Bullet bullets[MAX_NUM_BULLETS];
	Bullet playerBullets[MAX_NUM_PLAYER_BULLETS];
	Enemy enemies[MAX_NUM_ENEMIES];
	Particle particles[MAX_NUM_PARTICLES];
	Pickup pickups[MAX_NUM_PICKUPS];
};

struct Game
{
	Renderer renderer;

	Arena textureArena;

	bool showDebugRecords;

	World world;
};

Enemy *AllocateEnemy(World *world);
Bullet *AllocatePlayerBullet(World *world);
Bullet *AllocateBullet(World *world);
Particle *AllocateParticle(World *world);
Pickup *AllocatePickup(World *world);

CoroutineResult Stage1Script(ScriptVM *vm, World *world, void *userdata);
