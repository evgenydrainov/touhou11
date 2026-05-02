#pragma once

#include "common.h"
#include "game.h"
#include "platform_api.h"

/*
 *  Coroutine Implementation
 */

#define CoroutineBegin(vm)              \
	vm->callDepth++;                    \
	switch (vm->state[vm->callDepth-1]) \
	{                                   \
		case 0:

#define CoroutineEnd(vm)             \
	}                                \
	vm->state[vm->callDepth-1] = -1; \
	vm->callDepth--;                 \
	return CoroutineResult_Finished

#define CoroutineYield(vm)                                    \
	{                                                         \
		vm->state[vm->callDepth-1] = CONCATENATE(__LINE__, ); \
		vm->callDepth--;                                      \
		return CoroutineResult_Yield;                         \
		case CONCATENATE(__LINE__, ):;                        \
	}

#define CoroutineWait(vm, time)       \
	{                                 \
		vm->waitTimer += (time);      \
		while (vm->waitTimer >= 1.0f) \
		{                             \
			CoroutineYield(vm);       \
			vm->waitTimer -= 1.0f;    \
		}                             \
	}

#define CoroutineWaitUntil(vm, Cond) \
	while (!(Cond))                  \
	{                                \
		CoroutineYield(vm)           \
	}

#define CoroutineCall(vm, FuncCall)                           \
	{                                                         \
		vm->state[vm->callDepth-1] = CONCATENATE(__LINE__, ); \
		vm->state[vm->callDepth] = 0;                         \
		case CONCATENATE(__LINE__, ):                         \
		if (FuncCall == CoroutineResult_Yield)                \
		{                                                     \
			vm->callDepth--;                                  \
			return CoroutineResult_Yield;                     \
		}                                                     \
	}

/*
 *  Common Utility Functions
 */

inline void
LaunchTowardsPoint(Enemy *enemy, vec2 targetPos, f32 acceleration)
{
	Assert(acceleration > 0.0f);

	f32 distance = PointDistance(enemy->pos, targetPos);
	f32 speed = Sqrt(2.0f*distance*acceleration);
	vec2 direction = Normalize0(targetPos - enemy->pos);
	enemy->vel = speed*direction;
	enemy->acc = -acceleration*direction;
}

inline vec2
GetTargetPos(World *world,
			 Enemy *enemy)
{
	vec2 result = world->player.pos;
	return result;
}

inline f32
GetTargetDirection(World *world,
				   Enemy *enemy)
{
	vec2 targetPos = GetTargetPos(world, enemy);
	f32 result = PointDirection(enemy->pos, targetPos);
	return result;
}

enum EnemyType : u32
{
	enemy_fairy_blue,
	enemy_fairy_red,
	enemy_fairy_green,
	enemy_fairy_yellow,
};

inline SpriteIndex
GetEnemyIdleSprite(EnemyType type)
{
	switch (type)
	{
		case enemy_fairy_blue:   return spr_enemy_fairy_blue_idle;
		case enemy_fairy_red:    return spr_enemy_fairy_red_idle;
		case enemy_fairy_green:  return spr_enemy_fairy_green_idle;
		case enemy_fairy_yellow: return spr_enemy_fairy_yellow_idle;
	}

	Assert(false);
	return {};
}

inline SpriteIndex
GetEnemyMoveRightSprite(EnemyType type)
{
	switch (type)
	{
		case enemy_fairy_blue:   return spr_enemy_fairy_blue_right;
		case enemy_fairy_red:    return spr_enemy_fairy_red_right;
		case enemy_fairy_green:  return spr_enemy_fairy_green_right;
		case enemy_fairy_yellow: return spr_enemy_fairy_yellow_right;
	}

	Assert(false);
	return {};
}

inline f32
GetEnemyRadius(EnemyType type)
{
	switch (type)
	{
		case enemy_fairy_blue:   return 16;
		case enemy_fairy_red:    return 16;
		case enemy_fairy_green:  return 16;
		case enemy_fairy_yellow: return 16;
	}

	Assert(false);
	return {};
}

/*
 *  CreateEnemy variants
 */

inline Enemy *
CreateEnemyX(World *world,
			 f32 x, f32 y,
			 f32 xVel, f32 yVel,
			 SpriteIndex sprIdle, SpriteIndex sprMoveRight,
			 f32 radius,
			 ScriptFunction *scriptFunc)
{
	Enemy *enemy = AllocateEnemy(world);
	enemy->x = x;
	enemy->y = y;
	enemy->xVel = xVel;
	enemy->yVel = yVel;
	enemy->sprIdle = sprIdle;
	enemy->sprMoveRight = sprMoveRight;
	enemy->radius = radius;
	enemy->vm.func = scriptFunc;

	if (Abs(enemy->xVel) > 0.1f)
	{
		enemy->spriteIndex = enemy->sprMoveRight;
		enemy->frameIndex = 3;
	}
	else
	{
		enemy->spriteIndex = enemy->sprIdle;
	}

	if (enemy->xVel > 0.001f)
	{
		enemy->xscale = 1.0f;
	}
	if (enemy->xVel < -0.001f)
	{
		enemy->xscale = -1.0f;
	}

	return enemy;
}

inline Enemy *
CreateEnemyA(World *world,
			 EnemyType type,
			 f32 x, f32 y,
			 f32 xVel, f32 yVel,
			 ScriptFunction *scriptFunc = nullptr)
{
	Enemy *enemy = CreateEnemyX(world,
								x, y,
								xVel, yVel,
								GetEnemyIdleSprite(type), GetEnemyMoveRightSprite(type),
								GetEnemyRadius(type),
								scriptFunc);
	return enemy;
}

inline Enemy *
CreateEnemyB(World *world, EnemyType type,
			 vec2 spawnPos, vec2 targetPos,
			 f32 acceleration, ScriptFunction *scriptFunc = nullptr)
{
	Enemy *enemy = CreateEnemyX(world,
								spawnPos.x, spawnPos.y,
								0, 0,
								GetEnemyIdleSprite(type), GetEnemyMoveRightSprite(type),
								GetEnemyRadius(type),
								scriptFunc);

	LaunchTowardsPoint(enemy, targetPos, acceleration);

	return enemy;
}

inline Enemy *
CreateEnemyC(World *world, EnemyType type,
			 vec2 spawnPos, vec2 offsetPos,
			 f32 acceleration, ScriptFunction *scriptFunc = nullptr)
{
	Enemy *enemy = CreateEnemyB(world, type,
								spawnPos, spawnPos + offsetPos,
								acceleration, scriptFunc);

	return enemy;
}

/*
 *  Shoot variants
 */

inline Bullet *
ShootX(World *world,
	   f32 x, f32 y,
	   f32 xVel, f32 yVel,
	   SpriteIndex spriteIndex, i32 frameIndex)
{
	Bullet *bullet = AllocateBullet(world);
	bullet->x = x;
	bullet->y = y;
	bullet->xVel = xVel;
	bullet->yVel = yVel;
	bullet->spriteIndex = spriteIndex;
	bullet->frameIndex = frameIndex;

	// TODO: set radius based on sprite

	PlatformPlaySound(snd_enemy_shoot);

	return bullet;
}

inline Bullet *
ShootA(World *world, Enemy *enemy,
	   f32 speed, f32 direction,
	   SpriteIndex spriteIndex, i32 frameIndex)
{
	f32 x = enemy->x;
	f32 y = enemy->y;

	f32 xVel = speed *  Cos(Radians(direction));
	f32 yVel = speed * -Sin(Radians(direction));

	Bullet *bullet = ShootX(world,
							x, y,
							xVel, yVel,
							spriteIndex, frameIndex);

	return bullet;
}
