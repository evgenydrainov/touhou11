#pragma once

#include "common.h"
#include "game_assets.h"
#include "render_backend.h"

#define RENDERER_MAX_BATCH_QUADS (1'000)
#define RENDERER_MAX_BATCH_VERTICES (4*RENDERER_MAX_BATCH_QUADS)

struct Renderer
{
	RenderVertex2D *vertices;
	int num_vertices;

	TextureAsset *texture;

	float translationX;
	float translationY;
};

enum HAlign : u32
{
	HAlign_Left,
	HAlign_Center,
	HAlign_Right,
};

enum VAlign : u32
{
	VAlign_Top,
	VAlign_Middle,
	VAlign_Bottom,
};

void RendererInit(Renderer *renderer,
				  Arena *transientArena,
				  RenderBackend *backend);

void RendererFlush(Renderer *renderer,
				   RenderBackend *backend);

void DrawQuad(Renderer *renderer,
			  RenderBackend *backend,
			  TextureAsset *texture,
			  f32 x0, f32 y0,
			  f32 x1, f32 y1,
			  f32 x2, f32 y2,
			  f32 x3, f32 y3,
			  f32 u0, f32 v0,
			  f32 u1, f32 v1,
			  vec4 color);

void DrawSprite(Renderer *renderer,
				RenderBackend *backend,
				GameAssets *assets,
				SpriteIndex sprite_index,
				int frame_index,
				float pos_x, float pos_y);

inline void
DrawSprite(Renderer *renderer,
		   RenderBackend *backend,
		   GameAssets *assets,
		   SpriteIndex sprite_index,
		   int frame_index,
		   vec2 pos)
{
	DrawSprite(renderer, backend, assets, sprite_index, frame_index, pos.x, pos.y);
}

void DrawSprite(Renderer *renderer,
				RenderBackend *backend,
				GameAssets *assets,
				SpriteIndex sprite_index,
				int frame_index,
				float pos_x, float pos_y,
				float xscale, float yscale,
				float angleDegrees = 0.0f,
				vec4 color = c_white);

inline void
DrawSprite(Renderer *renderer,
		   RenderBackend *backend,
		   GameAssets *assets,
		   SpriteIndex sprite_index,
		   int frame_index,
		   vec2 pos,
		   vec2 scale,
		   float angleDegrees = 0.0f,
		   vec4 color = c_white)
{
	DrawSprite(renderer, backend, assets, sprite_index, frame_index, pos.x, pos.y, scale.x, scale.y, angleDegrees, color);
}

void DrawText(Renderer *renderer, RenderBackend *backend, GameAssets *assets,
			  FontIndex font_index,
			  float pos_x, float pos_y,
			  const char *text,
			  HAlign halign = HAlign_Left, VAlign valign = VAlign_Top,
			  float xscale = 1.0f, float yscale = 1.0f,
			  vec4 color = c_white);

inline void
DrawTextOutline(Renderer *renderer, RenderBackend *backend, GameAssets *assets,
				FontIndex fontIndex,
				FontIndex outlineFontIndex,
				float pos_x, float pos_y,
				const char *text,
				HAlign halign = HAlign_Left, VAlign valign = VAlign_Top,
				float xscale = 1.0f, float yscale = 1.0f,
				vec4 color = c_white,
				vec4 outlineColor = c_black)
{
	DrawText(renderer, backend, assets, outlineFontIndex, pos_x, pos_y, text, halign, valign, xscale, yscale, outlineColor);

	DrawText(renderer, backend, assets, fontIndex, pos_x, pos_y, text, halign, valign, xscale, yscale, color);
}

inline void
DrawTextShadow(Renderer *renderer, RenderBackend *backend, GameAssets *assets,
			   FontIndex fontIndex,
			   float pos_x, float pos_y,
			   const char *text,
			   HAlign halign = HAlign_Left, VAlign valign = VAlign_Top,
			   float xscale = 1.0f, float yscale = 1.0f,
			   vec4 color = c_white,
			   vec4 shadowColor = c_black,
			   vec2 shadowOffset = {1.0f, 1.0f})
{
	DrawText(renderer, backend, assets, fontIndex, pos_x+shadowOffset.x, pos_y+shadowOffset.y, text, halign, valign, xscale, yscale, shadowColor);

	DrawText(renderer, backend, assets, fontIndex, pos_x, pos_y, text, halign, valign, xscale, yscale, color);
}

vec2 GetTextSize(Renderer *renderer,
				 FontIndex font_index,
				 const char *text,
				 f32 xscale, f32 yscale,
				 bool onlyOneLine = false);

void DrawRectangle(Renderer *renderer,
				   RenderBackend *backend,
				   GameAssets *assets,
				   float x, float y,
				   float width, float height,
				   vec4 color);

void DrawTexture(Renderer *renderer, RenderBackend *backend, GameAssets *assets,
				 TextureIndex textureIndex,
				 f32 destX, f32 destY,
				 f32 destW, f32 destH,
				 int sourceX, int sourceY,
				 int sourceW, int sourceH);
