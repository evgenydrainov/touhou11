#include "game.h"
#include "platform_api.h"
#include "debug.h"
#include "stb_sprintf.h"

static void ReimuShotType(Player *player, World *world, GameInput *input);

static CharacterInfo g_characterInfo[CharacterIndex_COUNT] = {
	{
		/* .moveSpeed      = */ 4.5f,
		/* .focusSpeed     = */ 2.0f,
		/* .radius         = */ 2.0f,
		/* .grazeRadius    = */ 18.0f,
		/* .deathbombTime  = */ 15.0f,
		/* .startingBombs  = */ 3,
		/* .ShotType       = */ ReimuShotType,
		/* .Bomb           = */ 0,
		/* .sprIdle        = */ spr_reimu_idle,
		/* .sprMoveLeft    = */ spr_reimu_left,
		/* .sprMoveRight   = */ spr_reimu_right,
	},
};

static_assert(CharacterIndex_COUNT == 2);

static ParticleTypeInfo g_particleTypeInfo[ParticleType_COUNT] = {
	{
		spr_reimu_card_afterimage,
		16.0f,
		{1.0f, 1.0f, 1.0f, 1.0f},
		{1.0f, 1.0f, 1.0f, 0.0f},
		90.0f,
		90.0f + 360.0f,
		{1.0f, 1.0f},
		{1.0f, 1.0f},
	},
	{
		spr_enemy_death_particle,
		20.0f,
		{1.0f, 1.0f, 1.0f, 1.0f},
		{1.0f, 1.0f, 1.0f, 0.0f},
		0.0f,
		0.0f,
		{0.0f, 0.0f},
		{1.5f, 1.5f},
	},
	{
		spr_enemy_death_particle,
		20.0f,
		{1.0f, 1.0f, 1.0f, 1.0f},
		{1.0f, 1.0f, 1.0f, 0.0f},
		0.0f,
		0.0f,
		{0.0f, 0.50f},
		{2.0f, 0.25f},
	},
};

static_assert(ParticleType_COUNT == 3);

static CharacterInfo *
GetCharacterInfo(CharacterIndex characterIndex)
{
	Assert(characterIndex < CharacterIndex_COUNT);
	return &g_characterInfo[characterIndex];
}

static ParticleTypeInfo *
GetParticleTypeInfo(ParticleType type)
{
	Assert(type < ParticleType_COUNT);
	return &g_particleTypeInfo[type];
}

static Pickup *
CreatePickup(World *world, vec2 pos, PickupType type)
{
	Pickup *pickup = AllocatePickup(world);
	pickup->pos = pos;
	pickup->type = type;
	pickup->vel = {0.0f, -2.0f};
	pickup->radius = 6.0f;

	return pickup;
}

static void
RecreatePlayer(Player *player)
{
	CharacterInfo *characterInfo = GetCharacterInfo(player->characterIndex);

	*player = {};
	player->pos = PLAYER_STARTING_POS;
	player->spriteIndex = characterInfo->sprIdle;
	player->iframes = PLAYER_RESPAWN_IFRAMES;

	player->state = PlayerState_Appearing;
	player->timer = 0.0f;
}

static void
PlayerTryUseBomb(World *world,
				 Player *player,
				 GameInput *input)
{
	CharacterInfo *characterInfo = GetCharacterInfo(player->characterIndex);

	if (IsKeyPressed(input, 0, GameInputKey_B))
	{
		if (player->bombCoolDownTimer <= 0.0f)
		{
			if (world->stats.bombs > 0)
			{
				if (characterInfo->Bomb)
				{
					characterInfo->Bomb(player, world);
				}

				world->stats.bombs--;

				player->bombCoolDownTimer = PLAYER_BOMB_COOLDOWN_TIME;
				player->iframes = PLAYER_RESPAWN_IFRAMES;

				// deathbomb
				player->state = PlayerState_Normal;
			}
		}
	}
}

static void
UpdatePlayer(Player *player,
			 World *world,
			 GameInput *input)
{
	// :update player

	CharacterInfo *characterInfo = GetCharacterInfo(player->characterIndex);

	player->vel = {};

	switch (player->state)
	{
		case PlayerState_Normal:
		{
			vec2 moveDir = {};

			if (IsKeyDown(input, 0, GameInputKey_UP))
			{
				moveDir.y -= 1.0f;
			}
			if (IsKeyDown(input, 0, GameInputKey_DOWN))
			{
				moveDir.y += 1.0f;
			}
			if (IsKeyDown(input, 0, GameInputKey_LEFT))
			{
				moveDir.x -= 1.0f;
			}
			if (IsKeyDown(input, 0, GameInputKey_RIGHT))
			{
				moveDir.x += 1.0f;
			}

			moveDir = Normalize0(moveDir);

			{
				f32 moveSpeed = characterInfo->moveSpeed;
				f32 hitboxAnimTarget = 0.0f;

				if (IsKeyDown(input, 0, GameInputKey_X))
				{
					moveSpeed = characterInfo->focusSpeed;
					hitboxAnimTarget = 1.0f;
				}

				player->hitboxAnim = Approach(player->hitboxAnim, hitboxAnimTarget, 0.1f*input->delta);

				player->vel = moveSpeed*moveDir;
			}

			if (moveDir.x < 0.0f)
			{
				if (player->frameIndex < 4)
				{
					player->frameIndex = SpriteAnimate(player->spriteIndex, player->frameIndex, 2*input->delta);
				}
				else
				{
					player->frameIndex = SpriteAnimate(player->spriteIndex, player->frameIndex, input->delta);
				}

				if (player->spriteIndex != characterInfo->sprMoveLeft)
				{
					player->spriteIndex = characterInfo->sprMoveLeft;
					player->frameIndex = 0;
				}
			}
			else if (moveDir.x > 0.0f)
			{
				if (player->frameIndex < 4)
				{
					player->frameIndex = SpriteAnimate(player->spriteIndex, player->frameIndex, 2*input->delta);
				}
				else
				{
					player->frameIndex = SpriteAnimate(player->spriteIndex, player->frameIndex, input->delta);
				}

				if (player->spriteIndex != characterInfo->sprMoveRight)
				{
					player->spriteIndex = characterInfo->sprMoveRight;
					player->frameIndex = 0;
				}
			}
			else
			{
				if (player->spriteIndex == characterInfo->sprMoveLeft
					|| player->spriteIndex == characterInfo->sprMoveRight)
				{
					SpriteInfo *sprInfo = GetSpriteInfo(player->spriteIndex);

					player->frameIndex -= sprInfo->animSpeed * input->delta;
					player->frameIndex = Min(player->frameIndex, 3.0f);

					if (player->frameIndex < 0)
					{
						player->spriteIndex = characterInfo->sprIdle;
						player->frameIndex = 0;
					}
				}
				else
				{
					player->frameIndex = SpriteAnimate(player->spriteIndex, player->frameIndex, input->delta);
				}
			}

			if (characterInfo->ShotType)
			{
				characterInfo->ShotType(player, world, input);
			}

			player->iframes = Max(player->iframes - input->delta, 0.0f);
		} break;

		case PlayerState_Dying:
		{
			player->timer += input->delta;

			if (player->timer <= characterInfo->deathbombTime)
			{
				PlayerTryUseBomb(world, player, input);
			}
			else
			{
				if (player->timer >= PLAYER_DEATH_TIME)
				{
					if (world->stats.lives > 0)
					{
						world->stats.lives--;

						int drop = Min(world->stats.power, 16);
						world->stats.power -= drop;

						drop = Min(drop, 12);

						while (drop > 0)
						{
							PickupType type = PickupType_Power;
							if (drop >= 8)
							{
								drop -= 8;
								type = PickupType_BigPower;
							}
							else
							{
								drop--;
							}

							vec2 pos = player->pos + V2(RandomRangeFloat32(&world->stageRNG, -50.0f, 50.0f),
														RandomRangeFloat32(&world->stageRNG, -50.0f, 50.0f));
							CreatePickup(world, pos, type);
						}
					}
					else
					{
						// game over
						CreatePickup(world, player->pos, PickupType_FullPower);
					}
					
					RecreatePlayer(player);
				}
			}
		} break;

		case PlayerState_Appearing:
		{
			player->timer += input->delta;
			if (player->timer >= PLAYER_APPEAR_TIME)
			{
				player->state = PlayerState_Normal;
			}
		} break;
	}
}

Bullet *
AllocateBullet(World *world)
{
	Assert(world->numBullets < MAX_NUM_BULLETS);

	Bullet *bullet = &world->bullets[world->numBullets++];
	*bullet = {};
	bullet->lifeSpan = 30*60;

	return bullet;
}

Bullet *
AllocatePlayerBullet(World *world)
{
	Assert(world->numPlayerBullets < MAX_NUM_PLAYER_BULLETS);
	Bullet *result = &world->playerBullets[world->numPlayerBullets++];
	*result = {};
	return result;
}

Enemy *
AllocateEnemy(World *world)
{
	Assert(world->numEnemies < MAX_NUM_ENEMIES);

	Enemy *enemy = &world->enemies[world->numEnemies++];
	*enemy = {};
	enemy->xscale = 1.0f;
	enemy->health = 5.0f;

	return enemy;
}

Particle *
AllocateParticle(World *world)
{
	Assert(world->numParticles < MAX_NUM_PARTICLES);

	Particle *particle = &world->particles[world->numParticles++];
	*particle = {};

	return particle;
}

Pickup *
AllocatePickup(World *world)
{
	Assert(world->numPickups < MAX_NUM_PICKUPS);

	Pickup *pickup = &world->pickups[world->numPickups++];
	*pickup = {};

	return pickup;
}

static vec3 
GetCameraForward(StageBG *bg)
{
	vec3 camForward;
	camForward.x = Cos(bg->camYaw) * Cos(bg->camPitch);
	camForward.y = Sin(bg->camPitch);
	camForward.z = Sin(bg->camYaw) * Cos(bg->camPitch);

	return camForward;
}

static void
UpdateStage3DBackground(StageBG *bg,
						World *world,
						GameInput *input)
{
	if (input->DEBUG_mouseCaptured)
	{
		bg->camPitch -= 0.005f*input->DEBUG_mouseDeltaY;
		bg->camYaw   += 0.005f*input->DEBUG_mouseDeltaX;

		bg->camPitch = Clamp(bg->camPitch, -(Pi32/2.0f - 0.01f), Pi32/2.0f - 0.01f);
		bg->camYaw   = Wrap(bg->camYaw, 2.0f*Pi32);

		vec3 camForward = GetCameraForward(bg);

		f32 speed = 2.0f/60.0f;

		if (DEBUG_IsKeyDown(input, 'W'))
		{
			bg->camPos += (speed * input->delta) * camForward;
		}
		if (DEBUG_IsKeyDown(input, 'S'))
		{
			bg->camPos -= (speed * input->delta) * camForward;
		}
		if (DEBUG_IsKeyDown(input, 'A'))
		{
			bg->camPos.z += (speed * input->delta) * Sin(bg->camYaw - Pi32/2.0f);
			bg->camPos.x += (speed * input->delta) * Cos(bg->camYaw - Pi32/2.0f);
		}
		if (DEBUG_IsKeyDown(input, 'D'))
		{
			bg->camPos.z += (speed * input->delta) * Sin(bg->camYaw + Pi32/2.0f);
			bg->camPos.x += (speed * input->delta) * Cos(bg->camYaw + Pi32/2.0f);
		}

		bg->camRoll = 0.0f;
	}
	else
	{
		bg->camPos = {0.0f, 1.4f, 0.0f};
		bg->camPitch = Radians(-40);
		bg->camYaw = Pi32/2.0f;

		bg->camRoll = 0.05f*Sin(0.5f*world->stageTimer/60.0f);
	}

	{
		bg->camMoveSpeed = Approach(bg->camMoveSpeed, 0.0025f, (0.0025f/240.0f)*input->delta);

		bg->layer1Offset.y += bg->camMoveSpeed*input->delta;
		bg->layer2Offset.y += bg->camMoveSpeed*input->delta;
		bg->layer3Offset.y += bg->camMoveSpeed*input->delta;
		bg->layer4Offset.y += bg->camMoveSpeed*input->delta;
	}

	bg->layer1Offset.x -= 0.00010f*input->delta;
	bg->layer1Offset.y += 0.00005f*input->delta;

	bg->layer2Offset.x -= -0.00150f*input->delta;
	bg->layer2Offset.y +=  0.00010f*input->delta;

	bg->layer3Offset.x -= 0.00050886f*Sin01(0.5f*input->time)*input->delta;
	bg->layer3Offset.y += 0.00050274f*Cos01(0.5f*input->time)*input->delta;

	bg->layer4Offset.x -= 0.00040693f*Sin01(0.5f*input->time + 1.0f)*input->delta;
	bg->layer4Offset.y += 0.00040178f*Cos01(0.5f*input->time + 1.0f)*input->delta;

	bg->layer1Offset.x = Fmod(bg->layer1Offset.x, 1.0f);
	bg->layer1Offset.y = Fmod(bg->layer1Offset.y, 1.0f);

	bg->layer2Offset.x = Fmod(bg->layer2Offset.x, 1.0f);
	bg->layer2Offset.y = Fmod(bg->layer2Offset.y, 1.0f);

	bg->layer3Offset.x = Fmod(bg->layer3Offset.x, 1.0f);
	bg->layer3Offset.y = Fmod(bg->layer3Offset.y, 1.0f);

	bg->layer4Offset.x = Fmod(bg->layer4Offset.x, 1.0f);
	bg->layer4Offset.y = Fmod(bg->layer4Offset.y, 1.0f);

	world->stageLabelAlpha = Approach(world->stageLabelAlpha, world->stageLabelTargetAlpha, 0.05f*input->delta);
}

static void
WorldPhysicsUpdate(World *world,
				   GameInput *input)
{
	TIMED_FUNCTION();

	// :move player
	{
		world->player.pos += world->player.vel*input->delta;
	}

	// :keep player in bounds
	{
		int xoff = 8;
		int yoff = 12;

		vec2 minPos = {(f32)(-PLAY_AREA_W/2 + xoff), (f32)yoff};
		vec2 maxPos = {(f32)( PLAY_AREA_W/2 - xoff), (f32)(PLAY_AREA_H - yoff)};

		world->player.pos = Clamp(world->player.pos, minPos, maxPos);
	}

	// :move bullets
	for (int bulletIndex = 0;
		 bulletIndex < world->numBullets;
		 bulletIndex++)
	{
		Bullet *bullet = &world->bullets[bulletIndex];

		bullet->x += bullet->xVel*input->delta;
		bullet->y += bullet->yVel*input->delta;
	}

	// :move player bullets
	for (int playerBulletIndex = 0;
		 playerBulletIndex < world->numPlayerBullets;
		 playerBulletIndex++)
	{
		Bullet *bullet = &world->playerBullets[playerBulletIndex];

		bullet->x += bullet->xVel*input->delta;
		bullet->y += bullet->yVel*input->delta;
	}

	// :move enemies
	for (int enemyIndex = 0;
		 enemyIndex < world->numEnemies;
		 enemyIndex++)
	{
		Enemy *enemy = &world->enemies[enemyIndex];

		enemy->x += enemy->xVel*input->delta;
		enemy->y += enemy->yVel*input->delta;

		if (Signi32(enemy->xAcc) == -Signi32(enemy->xVel))
		{
			enemy->xVel = Approach(enemy->xVel, 0.0f, Abs(enemy->xAcc)*input->delta);
			if (enemy->xVel == 0.0f)
			{
				enemy->xAcc = 0.0f;
			}
		}
		else
		{
			enemy->xVel += enemy->xAcc*input->delta;
		}

		if (Signi32(enemy->yAcc) == -Signi32(enemy->yVel))
		{
			enemy->yVel = Approach(enemy->yVel, 0.0f, Abs(enemy->yAcc)*input->delta);
			if (enemy->yVel == 0.0f)
			{
				enemy->yAcc = 0.0f;
			}
		}
		else
		{
			enemy->yVel += enemy->yAcc*input->delta;
		}
	}

	// :move pickups
	for (int pickupIndex = 0;
		 pickupIndex < world->numPickups;
		 pickupIndex++)
	{
		Pickup *pickup = &world->pickups[pickupIndex];

		pickup->pos += pickup->vel*input->delta;

		f32 gravity = 0.025f;
		pickup->vel.y += gravity*input->delta;

		f32 maxYSpeed = 2.0f;
		pickup->vel.y = Min(pickup->vel.y, maxYSpeed);
	}

	// :check :collision player vs bullet
	for (int bulletIndex = 0;
		 bulletIndex < world->numBullets;
		 bulletIndex++)
	{
		Bullet *bullet = &world->bullets[bulletIndex];

		CharacterInfo *characterInfo = GetCharacterInfo(world->player.characterIndex);

		if (CirclesOverlap(world->player.pos, characterInfo->radius, bullet->pos, bullet->radius))
		{
			if (world->player.state == PlayerState_Normal)
			{
				if (world->player.iframes <= 0.0f)
				{
					world->player.state = PlayerState_Dying;
					world->player.timer = 0.0f;

					PlatformPlaySound(snd_pichuun);
				}
			}

			world->bullets[bulletIndex] = world->bullets[world->numBullets - 1];
			world->numBullets--;
			bulletIndex--;
		}
	}

	// :check :collision player bullet vs enemy
	for (int enemyIndex = 0;
		 enemyIndex < world->numEnemies;
		 enemyIndex++)
	{
		Enemy *enemy = &world->enemies[enemyIndex];

		for (int playerBulletIndex = 0;
			 playerBulletIndex < world->numPlayerBullets;
			 playerBulletIndex++)
		{
			Bullet *bullet = &world->playerBullets[playerBulletIndex];

			if (CirclesOverlap(bullet->pos, bullet->radius, enemy->pos, enemy->radius))
			{
				enemy->health -= bullet->damage;

				PlatformPlaySound(snd_enemy_hurt);

				Particle *particle = AllocateParticle(world);
				particle->pos = bullet->pos;
				particle->vel = 0.25f*bullet->vel;
				particle->type = ParticleType_reimu_card_afterimage;
				
				world->playerBullets[playerBulletIndex] = world->playerBullets[world->numPlayerBullets - 1];
				world->numPlayerBullets--;
				playerBulletIndex--;
			}
		}
	}
}

static void
RestartStage(World *world)
{
	world->stageRNG.s[0] = 0x59D7A6E1D4A27ACF;
	world->stageRNG.s[1] = 0x42851B616AB0328E;
	world->stageRNG.s[2] = 0x209C6C7F068F04F8;
	world->stageRNG.s[3] = 0x0DA559B41E6AD189;

	world->stageVM = {};
	world->stageVM.func = Stage1Script;

	world->stageTimer = 0.0f;
	world->stageLabelTargetAlpha = 0.0f;

	world->numBullets = 0;
	world->numPlayerBullets = 0;
	world->numEnemies = 0;
	world->numParticles = 0;
	world->numPickups = 0;
	world->boss = {};

	RecreatePlayer(&world->player);
}

static void
WorldUpdate(World *world,
			GameInput *input)
{
	TIMED_FUNCTION();

	UpdatePlayer(&world->player, world, input);

	// :update bullets
	for (int bulletIndex = 0; bulletIndex < world->numBullets; bulletIndex++)
	{
		Bullet *bullet = &world->bullets[bulletIndex];

		bullet->lifeTime += input->delta;

		bool dead = false;

		if (bullet->x < -PLAY_AREA_W/2 - 8
			|| bullet->x > PLAY_AREA_W/2 + 8
			|| bullet->y < -8
			|| bullet->y > PLAY_AREA_H + 8)
		{
			dead = true;
		}

		if (bullet->lifeTime > bullet->lifeSpan)
		{
			dead = true;
		}

		if (dead)
		{
			world->bullets[bulletIndex] = world->bullets[world->numBullets - 1];
			world->numBullets--;
			bulletIndex--;
		}
	}

	// :update player bullets
	for (int playerBulletIndex = 0; playerBulletIndex < world->numPlayerBullets; playerBulletIndex++)
	{
		Bullet *bullet = &world->playerBullets[playerBulletIndex];

		if (bullet->x < -PLAY_AREA_W/2 - 8
			|| bullet->x > PLAY_AREA_W/2 + 8
			|| bullet->y < -8
			|| bullet->y > PLAY_AREA_H + 8)
		{
			world->playerBullets[playerBulletIndex] = world->playerBullets[world->numPlayerBullets - 1];
			world->numPlayerBullets--;
			playerBulletIndex--;
		}
	}

	// :update enemies
	for (int enemyIndex = 0; enemyIndex < world->numEnemies; enemyIndex++)
	{
		Enemy *enemy = &world->enemies[enemyIndex];

		if (Abs(enemy->xVel) > 0.1f)
		{
			enemy->frameIndex = SpriteAnimate(enemy->spriteIndex, enemy->frameIndex, input->delta);

			if (enemy->spriteIndex != enemy->sprMoveRight)
			{
				enemy->spriteIndex = enemy->sprMoveRight;
				enemy->frameIndex = 0;
			}
		}
		else
		{
			if (enemy->spriteIndex != enemy->sprIdle)
			{
				enemy->frameIndex = Min(enemy->frameIndex, 4.0f);
				enemy->frameIndex -= GetSpriteInfo(enemy->spriteIndex)->animSpeed * input->delta;

				if (enemy->frameIndex < 1.0f)
				{
					enemy->spriteIndex = enemy->sprIdle;
					enemy->frameIndex = 0;
				}
			}
			else
			{
				enemy->frameIndex = SpriteAnimate(enemy->spriteIndex, enemy->frameIndex, input->delta);
			}
		}

		if (enemy->xVel > 0.001f)
		{
			enemy->xscale = 1.0f;
		}
		if (enemy->xVel < -0.001f)
		{
			enemy->xscale = -1.0f;
		}

		enemy->lifeTime += input->delta;

		bool dead = false;

		if (enemy->x < -PLAY_AREA_W/2 - 8
			|| enemy->x > PLAY_AREA_W/2 + 8
			|| enemy->y < -8
			|| enemy->y > PLAY_AREA_H + 8)
		{
			dead = true;
		}

		if (enemy->health <= 0.0f)
		{
			dead = true;

			PlatformPlaySound(snd_enemy_die);

			{
				CreatePickup(world, enemy->pos, PickupType_Point);
			}

			{
				Particle *particle = AllocateParticle(world);
				particle->type = ParticleType_enemy_death;
				particle->pos = enemy->pos;
			}

			{
				Particle *particle = AllocateParticle(world);
				particle->type = ParticleType_enemy_death_2;
				particle->pos = enemy->pos;
				particle->angleOffset = RandomRangeFloat32(&world->visualRNG, 0.0f, 360.0f);
			}
		}

		if (dead)
		{
			world->enemies[enemyIndex] = world->enemies[world->numEnemies - 1];
			world->numEnemies--;
			enemyIndex--;
		}
	}

	// :update particles
	for (int particleIndex = 0; particleIndex < world->numParticles; particleIndex++)
	{
		Particle *particle = &world->particles[particleIndex];
		ParticleTypeInfo *info = GetParticleTypeInfo(particle->type);

		particle->pos += particle->vel*input->delta;
		particle->lifeTime += input->delta;

		if (particle->lifeTime >= info->lifeSpan)
		{
			world->particles[particleIndex] = world->particles[world->numParticles - 1];
			world->numParticles--;
			particleIndex--;
		}
	}

	// :update pickups
	for (int pickupIndex = 0; pickupIndex < world->numPickups; pickupIndex++)
	{
		Pickup *pickup = &world->pickups[pickupIndex];

		pickup->lifeTime += input->delta;

		bool dead = false;

		if (pickup->pos.y > PLAY_AREA_H)
		{
			dead = true;
		}

		if (dead)
		{
			world->pickups[pickupIndex] = world->pickups[world->numPickups - 1];
			world->numPickups--;
			pickupIndex--;
		}
	}

	WorldPhysicsUpdate(world, input);

	// :update coroutines
	world->coroutineUpdateTimer += input->delta;
	while (world->coroutineUpdateTimer >= 1.0f)
	{
		if (world->stageVM.func)
		{
			world->stageVM.func(&world->stageVM, world, nullptr);
		}

		for (int enemyIndex = 0; enemyIndex < world->numEnemies; enemyIndex++)
		{
			Enemy *enemy = &world->enemies[enemyIndex];

			if (enemy->vm.func)
			{
				enemy->vm.func(&enemy->vm, world, enemy);
			}
		}

		world->coroutineUpdateTimer -= 1.0f;
	}

	UpdateStage3DBackground(&world->stageBG, world, input);

	world->stageTimer += input->delta;

	if (DEBUG_IsKeyPressed(input, 'R'))
	{
		RestartStage(world);
	}
}

static void
GameUpdate(Game *game,
		   GameInput *input)
{
	TIMED_FUNCTION();

	WorldUpdate(&game->world, input);
}

static void
RenderDebugRecords(Renderer *renderer,
				   RenderBackend *backend,
				   GameAssets *assets,
				   GameInput *input)
{
#if ENABLE_DEBUG_PROFILER
	{
		f32 drawY = 0.0f;

		for (int recordIndex = 0; recordIndex < g_profiler.numPrevRecords; recordIndex++)
		{
			DebugTimeRecord *record = &g_profiler.prevRecords[recordIndex];

			char buf[128];
			if (record->hitCount == 1)
			{
				stbsp_snprintf(buf, sizeof(buf), "%s: %fms",
							   record->functionName,
							   1000.0*(record->cycleCount/input->DEBUG_perfFreqF64));
			}
			else
			{
				stbsp_snprintf(buf, sizeof(buf), "%s[%d]: %fms",
							   record->functionName,
							   record->hitCount,
							   1000.0*(record->cycleCount/input->DEBUG_perfFreqF64));
			}

			f32 drawX = 20.0f*record->callDepth;

			DrawTextShadow(renderer, backend, assets,
						   fnt_consolas, drawX, drawY, buf);

			drawY += 25;
		}
	}
#else
	DrawText(renderer, backend, assets,
			 fnt_consolas, 0.0f, 0.0f, "Debug profiling is disabled");
#endif
}

static void
SetViewportKeepAspect(RenderBackend *backend,
					  i32 x, i32 y, i32 width, i32 height)
{
	Viewport viewport = GetViewportKeepAspect(backend->targetWidth, backend->targetHeight,
											  GAME_RES_W, GAME_RES_H,
											  x, y, width, height);

	backend->SetViewport(backend, viewport.x, viewport.y, viewport.width, viewport.height);
}

static void
DrawPlayer(Renderer *renderer,
		   Player *player,
		   GameAssets *assets,
		   RenderBackend *backend,
		   GameInput *input)
{
	{
		vec4 color = c_white;
		vec2 scale = {1.0f, 1.0f};

		if (player->state == PlayerState_Dying
			|| player->state == PlayerState_Appearing)
		{
			float t;
			if (player->state == PlayerState_Dying)
			{
				t = player->timer/PLAYER_DEATH_TIME;
			}
			else
			{
				t = 1.0f - player->timer/PLAYER_APPEAR_TIME;
			}

			scale.x = Lerp(1.0f, 0.25f, t);
			scale.y = Lerp(1.0f, 2.0f, t);

			color.a = Lerp(1.0f, 0.0f, t);
		}
		else
		{
			if (player->iframes > 0.0f)
			{
				color.a = 0.5f + 0.5f*Sin01(50.0f*input->time);
			}
		}

		DrawSprite(renderer, backend, assets,
				   player->spriteIndex,
				   (int)player->frameIndex,
				   player->pos, scale,
				   0.0f, color);
	}

	if (player->hitboxAnim > 0.0f)
	{
		vec4 color = c_white;
		color.a = player->hitboxAnim;

		f32 scale = EaseOutBack(player->hitboxAnim);

		DrawSprite(renderer, backend, assets,
				   spr_hitbox,
				   0,
				   player->pos,
				   {scale, scale}, 100*input->time,
				   color);

		DrawSprite(renderer, backend, assets,
				   spr_hitbox,
				   0,
				   player->pos,
				   {scale, scale}, -100*input->time,
				   color);
	}
}

static void
DrawStage3DBackground(StageBG *bg,
					  GameAssets *assets,
					  RenderBackend *backend,
					  GameInput *input,
					  GameMemory *memory)
{
	TIMED_FUNCTION();

	usize arenaSavePos = memory->transientArena.pos;

	int maxNumLayerVertices = 20*6;
	RenderVertex3D *layerVertices = PushArray(&memory->transientArena, maxNumLayerVertices, RenderVertex3D);

	{
		vec3 camForward = GetCameraForward(bg);

		vec3 eye = bg->camPos;
		vec3 center = bg->camPos + camForward;

		vec3 worldUp = {0.0f, 1.0f, 0.0f};

		vec3 camRight = Normalize(Cross(camForward, worldUp));

		float S = Sin(bg->camRoll);
		float C = Cos(bg->camRoll);

		vec3 rolledUp;
		rolledUp.x = worldUp.x*C + camRight.x*S;
		rolledUp.y = worldUp.y*C + camRight.y*S;
		rolledUp.z = worldUp.z*C + camRight.z*S;

		mat4 view = Matrix4LookAt(eye, center, rolledUp);
		mat4 projection = Matrix4Perspective(Radians(45.0f), PLAY_AREA_W/(f32)PLAY_AREA_H, 0.01f, 100.0f);

		backend->modelView = view;
		backend->projection = projection;
	}

	auto DrawLayer = [&](TextureIndex textureIndex, vec2 uvOffset, vec4 color)
	{
		u32 colorU32 = ColorVec4ToU32(color);

		int numLayerVertices = 0;

		for (f32 x = -2; x <= 1; x++)
		{
			for (f32 z = 0; z <= 4; z++)
			{
				layerVertices[numLayerVertices++] = {x+0, 0, z+0, uvOffset.x+0, uvOffset.y+0, colorU32};
				layerVertices[numLayerVertices++] = {x+1, 0, z+0, uvOffset.x+1, uvOffset.y+0, colorU32};
				layerVertices[numLayerVertices++] = {x+0, 0, z+1, uvOffset.x+0, uvOffset.y+1, colorU32};

				layerVertices[numLayerVertices++] = {x+1, 0, z+0, uvOffset.x+1, uvOffset.y+0, colorU32};
				layerVertices[numLayerVertices++] = {x+0, 0, z+1, uvOffset.x+0, uvOffset.y+1, colorU32};
				layerVertices[numLayerVertices++] = {x+1, 0, z+1, uvOffset.x+1, uvOffset.y+1, colorU32};

				Assert(numLayerVertices <= maxNumLayerVertices);
			}
		}

		backend->DrawTriangles3D(backend, AssetGetTexture(assets, textureIndex),
								 layerVertices, numLayerVertices);
	};

	backend->minFilter = RenderFilter_LinearMipMapLinear;
	backend->magFilter = RenderFilter_Linear;
	backend->shaderIndex = ShaderIndex_Fog3D;

	{
		vec4 fogColor = c_white;
		f32 fogNear = 0.0f;
		f32 fogFar = 8.0f;

		backend->SetUniform(backend, "u_fogColor", ShaderUniformType_Vec4, &fogColor);
		backend->SetUniform(backend, "u_fogNear",  ShaderUniformType_F32,  &fogNear);
		backend->SetUniform(backend, "u_fogFar",   ShaderUniformType_F32,  &fogFar);
	}

	DrawLayer(tex_gfw_misty_lake, bg->layer1Offset, c_white);

	//backend->srcBlendFactor = BlendFactor_SrcAlpha;
	//backend->destBlendFactor = BlendFactor_One;
	DrawLayer(tex_gfw_misty_lake2, bg->layer2Offset, {1.00f, 1.00f, 1.00f, 0.25f});
	//backend->srcBlendFactor = BlendFactor_SrcAlpha;
	//backend->destBlendFactor = BlendFactor_InvSrcAlpha;

	DrawLayer(tex_gfw_misty_lake3, bg->layer3Offset, c_white);

	DrawLayer(tex_gfw_misty_lake4, bg->layer4Offset + vec2{0.5f, 0.5f}, c_white);

	backend->minFilter = RenderFilter_Nearest;
	backend->magFilter = RenderFilter_Nearest;
	backend->shaderIndex = ShaderIndex_Normal;

	memory->transientArena.pos = arenaSavePos;

	backend->modelView = Matrix4Identity();
}

static i32
GetFrameIndexForBulletAppearSprite(i32 bulletFrameIndex)
{
	switch (bulletFrameIndex)
	{
		case 0: return 0;
		case 1: case 2: return 1;
		case 3: case 4: return 2;
		case 5: case 6: return 3;
		case 7: case 8: return 4;
		case 9: case 10: case 11: return 5;
		case 12: case 13: case 14: return 6;
		case 15: return 7;
	}

	Assert(false);
	return 0;
}

static void
GameRenderPlayAreaPass(Renderer *renderer,
					   World *world,
					   GameAssets *assets,
					   RenderBackend *backend,
					   GameInput *input,
					   GameMemory *memory)
{
	TIMED_FUNCTION();

	// backend->Clear(backend, 0, 0, 1, 1);

	SetViewportKeepAspect(backend,
						  PLAY_AREA_X, PLAY_AREA_Y,
						  PLAY_AREA_W, PLAY_AREA_H);

	DrawStage3DBackground(&world->stageBG, assets, backend, input, memory);

	{
		mat4 projection = Matrix4Ortho(0.0f, PLAY_AREA_W, PLAY_AREA_H, 0.0f);
		backend->projection = projection;
	}

	renderer->translationX = PLAY_AREA_W/2;
	renderer->translationY = 0;

	// :draw player bullets
	{
		//RendererFlush(&game->renderer, backend);
		//backend->srcBlendFactor = BlendFactor_One;
		//backend->destBlendFactor = BlendFactor_InvSrcAlpha;

		for (int playerBulletIndex = 0; playerBulletIndex < world->numPlayerBullets; playerBulletIndex++)
		{
			Bullet *bullet = &world->playerBullets[playerBulletIndex];

			f32 angle = Degrees(Atan2(-bullet->yVel, bullet->xVel));

			vec4 color = {1.0f, 1.0f, 1.0f, 0.50f};

			DrawSprite(renderer, backend, assets,
					   bullet->spriteIndex, bullet->frameIndex,
					   bullet->x, bullet->y,
					   1.0f, 1.0f,
					   angle,
					   color);
		}

		//RendererFlush(&game->renderer, backend);
		//backend->srcBlendFactor = BlendFactor_SrcAlpha;
		//backend->destBlendFactor = BlendFactor_InvSrcAlpha;
	}

	// :draw player
	DrawPlayer(renderer, &world->player, assets, backend, input);

	// :draw enemies
	for (int enemyIndex = 0; enemyIndex < world->numEnemies; enemyIndex++)
	{
		Enemy *enemy = &world->enemies[enemyIndex];

		DrawSprite(renderer, backend, assets,
				   enemy->spriteIndex, (i32)enemy->frameIndex,
				   enemy->x, enemy->y,
				   enemy->xscale, 1.0f);
	}

	// :draw bullets
	for (int bulletIndex = 0; bulletIndex < world->numBullets; bulletIndex++)
	{
		Bullet *bullet = &world->bullets[bulletIndex];

		SpriteIndex spriteIndex = bullet->spriteIndex;
		i32 frameIndex = bullet->frameIndex;
		f32 xscale = 1.0f;
		f32 yscale = 1.0f;
		f32 angle = 0;
		vec4 color = c_white;

		if (!(bullet->spriteIndex == spr_bullet_outline
			|| bullet->spriteIndex == spr_bullet_filled))
		{
			angle = Degrees(Atan2(-bullet->yVel, bullet->xVel));
		}

		if (bullet->lifeTime < 8.0f)
		{
			spriteIndex = spr_bullet_appear;
			frameIndex = GetFrameIndexForBulletAppearSprite(bullet->frameIndex);

			xscale = Lerp(3.0f, 1.0f, bullet->lifeTime/8.0f);
			yscale = Lerp(3.0f, 1.0f, bullet->lifeTime/8.0f);

			color.a = bullet->lifeTime/8.0f;

			angle = 0;
		}

		DrawSprite(renderer, backend, assets,
				   spriteIndex, frameIndex,
				   bullet->x, bullet->y,
				   xscale, yscale,
				   angle, color);
	}

	// :draw pickups
	for (int pickupIndex = 0; pickupIndex < world->numPickups; pickupIndex++)
	{
		Pickup *pickup = &world->pickups[pickupIndex];

		f32 angle = 0.0f;
		if (pickup->lifeTime < 24.0f)
		{
			angle = 45.0f*pickup->lifeTime;
		}

		DrawSprite(renderer, backend, assets,
				   spr_pickup, pickup->type,
				   pickup->pos, {1.0f, 1.0f}, angle);
	}

	// :draw particles
	for (int particleIndex = 0; particleIndex < world->numParticles; particleIndex++)
	{
		Particle *particle = &world->particles[particleIndex];
		ParticleTypeInfo *info = GetParticleTypeInfo(particle->type);

		f32 frameIndex = GetSpriteInfo(info->spriteIndex)->animSpeed * particle->lifeTime;
		frameIndex = SpriteAnimate(info->spriteIndex, frameIndex, 0);

		f32 t = particle->lifeTime/info->lifeSpan;

		vec4 color = Lerp(info->colorFrom, info->colorTo, t);
		f32 angle = Lerp(info->angleFrom, info->angleTo, t);
		vec2 scale = Lerp(info->scaleFrom, info->scaleTo, t);

		angle += particle->angleOffset;

		DrawSprite(renderer, backend, assets,
				   info->spriteIndex, (i32)frameIndex,
				   particle->pos.x, particle->pos.y,
				   scale.x, scale.y,
				   angle,
				   color);
	}

	if (world->DEBUG_showHitboxes)
	{
		DrawRectangle(renderer, backend, assets,
					  -PLAY_AREA_W/2.0f, 0,
					  PLAY_AREA_W, PLAY_AREA_H,
					  {0.0f, 0.0f, 0.0f, 0.5f});

		{
			CharacterInfo *characterInfo = GetCharacterInfo(world->player.characterIndex);

			DrawCircle(renderer, backend, assets,
					   world->player.pos, characterInfo->grazeRadius,
					   {1.0f, 1.0f, 1.0f, 0.25f});

			DrawCircle(renderer, backend, assets,
					   world->player.pos, characterInfo->radius,
					   c_white);
		}

		for (int bulletIndex = 0;
			 bulletIndex < world->numBullets;
			 bulletIndex++)
		{
			Bullet *bullet = &world->bullets[bulletIndex];

			DrawCircle(renderer, backend, assets,
					   bullet->pos, bullet->radius,
					   c_white);
		}

		for (int enemyIndex = 0;
			 enemyIndex < world->numEnemies;
			 enemyIndex++)
		{
			Enemy *enemy = &world->enemies[enemyIndex];

			DrawCircle(renderer, backend, assets,
					   enemy->pos, enemy->radius,
					   {1.0f, 1.0f, 1.0f, 0.25f});
		}

		for (int pickupIndex = 0;
			 pickupIndex < world->numPickups;
			 pickupIndex++)
		{
			Pickup *pickup = &world->pickups[pickupIndex];

			DrawCircle(renderer, backend, assets,
					   pickup->pos, pickup->radius,
					   {1.0f, 1.0f, 1.0f, 0.5f});
		}

		for (int playerBulletIndex = 0;
			 playerBulletIndex < world->numPlayerBullets;
			 playerBulletIndex++)
		{
			Bullet *bullet = &world->playerBullets[playerBulletIndex];

			DrawCircle(renderer, backend, assets,
					   bullet->pos, bullet->radius,
					   {1.0f, 1.0f, 1.0f, 0.25f});
		}
	}

	RendererFlush(renderer, backend);

	renderer->translationX = 0;
	renderer->translationY = 0;
}

static void
GameRenderHUDPass(Game *game,
				  GameAssets *assets,
				  RenderBackend *backend,
				  GameInput *input)
{
	TIMED_FUNCTION();

	// :draw hud

	SetViewportKeepAspect(backend,
						  0, 0,
						  GAME_RES_W, GAME_RES_H);

	{
		mat4 projection = Matrix4Ortho(0.0f, 2*GAME_RES_W, 2*GAME_RES_H, 0.0f);
		backend->projection = projection;
	}

	// draw background
#if 1
	{
		DrawSprite(&game->renderer, backend, assets,
				   spr_background_3_left, 0,
				   0, 0);
		DrawSprite(&game->renderer, backend, assets,
				   spr_background_3_right, 0,
				   2*416, 0);
		DrawSprite(&game->renderer, backend, assets,
				   spr_background_3_top, 0,
				   2*32, 0);
		DrawSprite(&game->renderer, backend, assets,
				   spr_background_3_bottom, 0,
				   2*32, 2*464);
	}
#endif

	// draw difficulty label
	{
		f32 x = 2*(PLAY_AREA_X+PLAY_AREA_W);
		f32 y = 2*PLAY_AREA_Y;

		DrawTextOutline(&game->renderer, backend, assets,
						fnt_revue,
						fnt_revue_outline,
						x+2*(224/2), y,
						"NORMAL",
						HAlign_Center, VAlign_Top);
	}

	// draw fps counter
	{
		char buf[32];
		stbsp_snprintf(buf, sizeof(buf), "%.1ffps", 60.0f/input->delta);
		DrawTextOutline(&game->renderer, backend, assets,
						fnt_ms_gothic,
						fnt_ms_gothic_outline,
						2*(GAME_RES_W-60), 2*(GAME_RES_H-20),
						buf);
	}

	if (game->world.stageLabelAlpha > 0.0f)
	{
		f32 x = 2*PLAY_AREA_X;
		f32 y = 2*PLAY_AREA_Y;

		vec4 textColor = {1.0f, 1.0f, 1.0f, game->world.stageLabelAlpha};
		vec4 shadowColor = {0.0f, 0.0f, 0.0f, 0.5f*game->world.stageLabelAlpha};

		DrawTextShadow(&game->renderer, backend, assets,
					   fnt_caveat_brush,
					   x + 2*(PLAY_AREA_W/2), y + 2*(PLAY_AREA_H*0.40f),
					   "Stage 1\nMisty Lake",
					   HAlign_Center, VAlign_Middle,
					   1, 1,
					   textColor,
					   shadowColor,
					   {5.0f, 5.0f});
	}

	RendererFlush(&game->renderer, backend);
}

static void
GameRenderDebugPass(Game *game,
					GameAssets *assets,
					RenderBackend *backend,
					GameInput *input)
{
	TIMED_FUNCTION();

	backend->SetViewport(backend,
						 0, 0,
						 backend->targetWidth, backend->targetHeight);

	{
		mat4 projection = Matrix4Ortho(0.0f, (f32)backend->targetWidth, (f32)backend->targetHeight, 0.0f);
		backend->projection = projection;
	}

#if 0
	{
		//
		// manually draw black borders
		//

		Viewport viewport = GetViewportKeepAspect(backend->targetWidth, backend->targetHeight,
												  GAME_RES_W, GAME_RES_H,
												  0, 0, GAME_RES_W, GAME_RES_H);

		if (viewport.x > 0)
		{
			DrawRectangle(&game->renderer, backend, assets,
						  0, 0, (f32)viewport.x, (f32)backend->targetHeight,
						  c_black);

			DrawRectangle(&game->renderer, backend, assets,
						  (f32)(viewport.x+viewport.width), 0, (f32)(backend->targetWidth-viewport.x-viewport.width), (f32)backend->targetHeight,
						  c_black);
		}

		if (viewport.y > 0)
		{
			DrawRectangle(&game->renderer, backend, assets,
						  0, 0, (f32)backend->targetWidth, (f32)viewport.y,
						  c_black);

			DrawRectangle(&game->renderer, backend, assets,
						  0, (f32)(viewport.y+viewport.height), (f32)backend->targetWidth, (f32)(backend->targetHeight-viewport.y-viewport.height),
						  c_black);
		}
	}
#endif

	if (game->showDebugRecords)
	{
		RenderDebugRecords(&game->renderer, backend, assets, input);
	}

	RendererFlush(&game->renderer, backend);
}

static void
GameRender(Game *game,
		   GameAssets *assets,
		   RenderBackend *backend,
		   GameInput *input,
		   GameMemory *memory)
{
	TIMED_FUNCTION();

	// TODO: disable this
	backend->Clear(backend, 0, 0, 0, 1);

	GameRenderPlayAreaPass(&game->renderer, &game->world, assets, backend, input, memory);
	GameRenderHUDPass(game, assets, backend, input);
	GameRenderDebugPass(game, assets, backend, input);
}

static void
GameInit(Game *game,
		 GameMemory *memory,
		 GameAssets *assets,
		 RenderBackend *backend)
{
	backend->modelView = Matrix4Identity();
	backend->projection = Matrix4Identity();

	backend->srcBlendFactor = BlendFactor_SrcAlpha;
	backend->destBlendFactor = BlendFactor_InvSrcAlpha;

	backend->enableBlending = true;

	game->world.visualRNG.s[0] = 0x6A02F2E06C548411;
	game->world.visualRNG.s[1] = 0x6475F1C985E11BBE;
	game->world.visualRNG.s[2] = 0x249B8F08423E76BD;
	game->world.visualRNG.s[3] = 0x4EBE120309A6FB30;

	RestartStage(&game->world);

	//
	// NOTE: 1024x1024 rgba texture is 4mb
	//
	game->textureArena = PushArena(&memory->transientArena, Megabytes(32));

	for (int i = 0; i < ArrayLength(assets->textures); i++)
	{
		LoadTextureAsset(&assets->textures[i],
						 backend,
						 g_textureInfo[i].filePath,
						 &game->textureArena,
						 &memory->transientArena,
						 g_textureInfo[i].wantMipMap);
	}
	
	RendererInit(&game->renderer,
				 &memory->transientArena,
				 backend);

	PlatformPlayMusic("assets_raw/music/dbu_lunate_elf.mp3");

	game->showDebugRecords = false;
}

static void
ReimuShotType(Player *player, World *world, GameInput *input)
{
	player->fireTimer += input->delta;

	float fireTime = 4.0f;

	while (player->fireTimer >= fireTime)
	{
		if (player->fireQueue == 0)
		{
			if (IsKeyDown(input, 0, GameInputKey_A))
			{
				player->fireQueue += 4;
			}
		}

		if (player->fireQueue > 0)
		{
			{
				Bullet *bullet = AllocatePlayerBullet(world);
				bullet->pos = player->pos + vec2{-10.0f, -20.0f};
				bullet->vel.y = -16.0f;
				bullet->spriteIndex = spr_reimu_card;
				bullet->radius = 8;
				bullet->damage = 1.66f;
			}

			{
				Bullet *bullet = AllocatePlayerBullet(world);
				bullet->pos = player->pos + vec2{9.0f, -20.0f};
				bullet->vel.y = -16.0f;
				bullet->spriteIndex = spr_reimu_card;
				bullet->radius = 8;
				bullet->damage = 1.66f;
			}

			PlatformPlaySound(snd_reimu_shoot);

			player->fireQueue--;
		}

		player->fireTimer -= fireTime;
	}
}

void
GameUpdateAndRender(GameMemory *memory,
					GameAssets *assets,
					GameInput *input,
					RenderBackend *backend)
{
	TIMED_FUNCTION();

	Game *game = (Game *)memory->permanentArena.data;

	if (!memory->isInitted)
	{
		Assert(memory->permanentArena.pos == 0);

		Game *game2 = PushStruct(&memory->permanentArena, Game);
		Assert(game == game2);

		GameInit(game, memory, assets, backend);

		memory->isInitted = true;
	}

	if (!input->DEBUG_skipThisFrame)
	{
		GameUpdate(game, input);
	}

	game->showDebugRecords ^= DEBUG_IsKeyPressed(input, '1');
	game->world.DEBUG_showHitboxes ^= DEBUG_IsKeyPressed(input, 'H');

	GameRender(game, assets, backend, input, memory);
}
