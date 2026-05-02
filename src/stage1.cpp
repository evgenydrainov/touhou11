#include "scripting.h"

#pragma warning(push)
#pragma warning(disable: 5262)
#pragma warning(disable: 4702)
#pragma warning(disable: 4102)

static ScriptVM g_stage1Tasks[2];
static CoroutineResult g_stage1TaskResult[2];

static CoroutineResult
Stage1_IntroFairy(ScriptVM *vm,
				  World *world,
				  void *userdata)
{
	Enemy* enemy = (Enemy *)userdata;

	CoroutineBegin(vm);

	CoroutineWait(vm, 40);

	ShootA(world, enemy,
		   3.5f, GetTargetDirection(world, enemy),
		   spr_bullet_outline, 6);

	CoroutineWait(vm, 2.5f*60.0f);

	enemy->yAcc = -0.15f;

	CoroutineEnd(vm);
}

static CoroutineResult
Stage1_Fairy1(ScriptVM *vm,
			  World *world,
			  void *userdata)
{
	Enemy* enemy = (Enemy *)userdata;

	CoroutineBegin(vm);

	CoroutineWait(vm, 10);

	ShootA(world, enemy,
		   4.0f, GetTargetDirection(world, enemy),
		   spr_bullet_outline, 6);

	CoroutineEnd(vm);
}

static CoroutineResult
Stage1_Fairy2(ScriptVM *vm,
			  World *world,
			  void *userdata)
{
	Enemy* enemy = (Enemy *)userdata;

	CoroutineBegin(vm);

	CoroutineWait(vm, 20);
	ShootA(world, enemy, 4.0f, GetTargetDirection(world, enemy), spr_bullet_outline, 6);
	CoroutineWait(vm, 20);
	ShootA(world, enemy, 4.0f, GetTargetDirection(world, enemy), spr_bullet_outline, 6);

	CoroutineWaitUntil(vm, enemy->lifeTime >= 2.5f*60.0f);

	enemy->yAcc = -0.15f;

	CoroutineEnd(vm);
}

static CoroutineResult
Stage1_Fairy3(ScriptVM *vm,
			  World *world,
			  void *userdata)
{
	Enemy* enemy = (Enemy *)userdata;

	CoroutineBegin(vm);

	CoroutineWait(vm, 30);

	ShootA(world, enemy,
		   3.5f, GetTargetDirection(world, enemy),
		   spr_bullet_outline, 6);

	CoroutineWaitUntil(vm, enemy->lifeTime >= 1.5f*60.0f);

	if (enemy->x >= 0.0f)
	{
		enemy->xAcc = -0.05f;
	}
	else
	{
		enemy->xAcc = 0.05f;
	}

	enemy->yVel = 1.0f;
	enemy->yAcc = -0.005f;

	CoroutineEnd(vm);
}

//
// f0: which side (1.0f or -1.0f)
//
static CoroutineResult
Stage1_AcceleratedFairiesFromTheSide(ScriptVM *vm,
									 World *world,
									 void *userdata)
{
	CoroutineBegin(vm);

	vm->N = 4;
	for (vm->i = 0; vm->i < vm->N; vm->i++)
	{
		CreateEnemyC(world,
					 enemy_fairy_blue,
					 {vm->f0*190.0f, 0.0f},
					 {vm->f0*Lerp(-40.0f, -120.0f, vm->i/(f32)(vm->N-1)), 100.0f},
					 0.05f,
					 Stage1_Fairy3);

		CoroutineWait(vm, 20);
	}

	CoroutineEnd(vm);
}

static CoroutineResult
Stage1_AcceleratedFairiesFromTheSide_2(ScriptVM *vm,
									   World *world,
									   void *userdata)
{
	CoroutineBegin(vm);

	vm->N = 20;
	for (vm->i = 0; vm->i < vm->N; vm->i++)
	{
		{
			vec2 spawnPos;
			spawnPos.x = vm->f0*RandomRangeFloat32(&world->stageRNG, 170.0f, 190.0f);
			spawnPos.y = 0.0f;

			vec2 targetPos;
			targetPos.x = vm->f0*RandomRangeFloat32(&world->stageRNG, -50.0f, -150.0f);
			targetPos.y = RandomRangeFloat32(&world->stageRNG, 50.0f, 150.0f);

			CreateEnemyC(world, enemy_fairy_blue,
						 spawnPos, targetPos,
						 0.05f, Stage1_Fairy3);
		}

		CoroutineWait(vm, 10);
	}

	CoroutineEnd(vm);
}

static void
Stage1_EnemyFormation(World *world,
					  EnemyType enemyType,
					  f32 pos)
{
	f32 yVel = 2.0f;

	CreateEnemyA(world, enemyType,
				 pos*32.0f, 0, 0, yVel);

	if (pos != 0.0f)
	{
		CreateEnemyA(world, enemyType,
					 -pos*32.0f, 0, 0, yVel);
	}
}

static CoroutineResult
Stage1_RunTwoTasks(ScriptVM *vm,
				   World *world,
				   void *userdata)
{
	CoroutineBegin(vm);

	do
	{
		g_stage1TaskResult[0] = g_stage1Tasks[0].func(&g_stage1Tasks[0], world, userdata);
		g_stage1TaskResult[1] = g_stage1Tasks[1].func(&g_stage1Tasks[1], world, userdata);

		CoroutineYield(vm);
	} while (!(g_stage1TaskResult[0] == CoroutineResult_Finished
			 && g_stage1TaskResult[1] == CoroutineResult_Finished));

	CoroutineEnd(vm);
}

CoroutineResult
Stage1Script(ScriptVM *vm,
			 World *world,
			 void *userdata)
{
	CoroutineBegin(vm);

	CoroutineWait(vm, 90);

	//goto SkipHere;

	// intro fairies
	vm->N = 6;
	for (vm->i = 0; vm->i < vm->N; vm->i++)
	{
		{
			vec2 spawnPos  = {Lerp(-180.0f, 180.0f, vm->i/(f32)(vm->N-1)), 0.0f};
			vec2 targetPos = {Lerp(-140.0f, 140.0f, vm->i/(f32)(vm->N-1)), 100.0f};
			CreateEnemyB(world, enemy_fairy_blue, spawnPos, targetPos, 0.15f, Stage1_IntroFairy);
		}

		CoroutineWait(vm, 8);
	}

	CoroutineWaitUntil(vm, world->stageTimer >= 220);

	world->stageLabelTargetAlpha = 1.0f;

	CoroutineWaitUntil(vm, world->stageTimer >= 420);

	world->stageLabelTargetAlpha = 0.0f;

	// vertical line of fairies on the left
	for (vm->i = 0; vm->i < 5; vm->i++)
	{
		CreateEnemyA(world,
					 enemy_fairy_blue,
					 -100, 0,
					 0, 2.0f,
					 Stage1_Fairy1);

		CoroutineWait(vm, 20);
	}

	CoroutineWait(vm, 60);

	// vertical line of fairies on the right
	for (vm->i = 0; vm->i < 5; vm->i++)
	{
		CreateEnemyA(world,
					 enemy_fairy_blue,
					 100, 0,
					 0, 2.0f,
					 Stage1_Fairy1);

		CoroutineWait(vm, 20);
	}

	CoroutineWait(vm, 90);

	// fairies from the sides of the screen to the center
	vm->N = 4;
	for (vm->i = 0; vm->i < vm->N; vm->i++)
	{
		CreateEnemyC(world,
					 enemy_fairy_blue,
					 {Lerp(-130.0f, 0.0f, vm->i/(f32)(vm->N-1)), 0.0f},
					 {0.0f, 100.0f},
					 0.15f,
					 Stage1_Fairy2);

		if (vm->i != vm->N-1)
		{
			CreateEnemyC(world,
						 enemy_fairy_blue,
						 {Lerp(130.0f, 0.0f, vm->i/(f32)(vm->N-1)), 0.0f},
						 {0.0f, 100.0f},
						 0.15f,
						 Stage1_Fairy2);
		}

		CoroutineWait(vm, 20);
	}

	CoroutineWait(vm, 90);


	// accelerated fairies from the left
	vm->f0 = -1.0f;
	CoroutineCall(vm, Stage1_AcceleratedFairiesFromTheSide(vm, world, userdata));
	CoroutineWait(vm, 60);

	// accelerated fairies from the right
	vm->f0 = 1.0f;
	CoroutineCall(vm, Stage1_AcceleratedFairiesFromTheSide(vm, world, userdata));
	CoroutineWait(vm, 60);

SkipHere:

	// run two tasks at once
	{
		g_stage1Tasks[0] = {};
		g_stage1Tasks[0].func = Stage1_AcceleratedFairiesFromTheSide;
		g_stage1Tasks[0].f0 = -1.0f;

		g_stage1Tasks[1] = {};
		g_stage1Tasks[1].func = Stage1_AcceleratedFairiesFromTheSide;
		g_stage1Tasks[1].f0 = 1.0f;

		CoroutineCall(vm, Stage1_RunTwoTasks(vm, world, userdata));
	}

	CoroutineWait(vm, 90);

	// enemy formation
	Stage1_EnemyFormation(world, enemy_fairy_blue, 0);
	CoroutineWait(vm, 16);
	Stage1_EnemyFormation(world, enemy_fairy_blue, 1.5);
	CoroutineWait(vm, 16);
	Stage1_EnemyFormation(world, enemy_fairy_blue, 3);
	Stage1_EnemyFormation(world, enemy_fairy_blue, 0);
	CoroutineWait(vm, 16);
	Stage1_EnemyFormation(world, enemy_fairy_blue, 1.5);
	Stage1_EnemyFormation(world, enemy_fairy_blue, 4.5);
	Stage1_EnemyFormation(world, enemy_fairy_blue, 7.5);
	CoroutineWait(vm, 16);

	CoroutineWait(vm, 90);

	vm->f0 = -1.0f;
	CoroutineCall(vm, Stage1_AcceleratedFairiesFromTheSide_2(vm, world, userdata));
	CoroutineWait(vm, 90);

	vm->f0 = 1.0f;
	CoroutineCall(vm, Stage1_AcceleratedFairiesFromTheSide_2(vm, world, userdata));
	CoroutineWait(vm, 90);


	
	CoroutineEnd(vm);
}

#pragma warning(pop)
