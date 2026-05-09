#include "renderer.h"
#include "math_stuff.h"
#include "debug.h"

void
RendererInit(Renderer *renderer,
			 Arena *transientArena,
			 RenderBackend *backend)
{
	renderer->vertices = PushArray(transientArena, RENDERER_MAX_BATCH_VERTICES, RenderVertex2D);
}

void
RendererFlush(Renderer *renderer,
			  RenderBackend *backend)
{
	if (renderer->num_vertices == 0)
	{
		return;
	}

	switch (renderer->primitiveType)
	{
		case RendererPrimitiveType_None:
		{
			Assert(false);
		} break;
		
		case RendererPrimitiveType_Quads:
		{
			backend->DrawQuads(backend, renderer->texture, renderer->vertices, renderer->num_vertices);
		} break;

		case RendererPrimitiveType_Circles:
		{
			backend->DrawCircles(backend, renderer->texture, renderer->vertices, renderer->num_vertices);
		} break;
	}

	renderer->num_vertices = 0;
	renderer->texture = nullptr;
	renderer->primitiveType = RendererPrimitiveType_None;
}

void
DrawQuad(Renderer *renderer,
		 RenderBackend *backend,
		 TextureAsset *texture,
		 f32 x0, f32 y0,
		 f32 x1, f32 y1,
		 f32 x2, f32 y2,
		 f32 x3, f32 y3,
		 f32 u0, f32 v0,
		 f32 u1, f32 v1,
		 vec4 color)
{
	//TIMED_FUNCTION();

	x0 += renderer->translationX;
	y0 += renderer->translationY;

	x1 += renderer->translationX;
	y1 += renderer->translationY;

	x2 += renderer->translationX;
	y2 += renderer->translationY;

	x3 += renderer->translationX;
	y3 += renderer->translationY;

	if (renderer->num_vertices + 4 > RENDERER_MAX_BATCH_VERTICES
		|| texture != renderer->texture
		|| renderer->primitiveType != RendererPrimitiveType_Quads)
	{
		RendererFlush(renderer, backend);
		renderer->texture = texture;
		renderer->primitiveType = RendererPrimitiveType_Quads;
	}

	u32 colorU32 = ColorVec4ToU32(color);

	renderer->vertices[renderer->num_vertices++] = {x0, y0, u0, v0, colorU32};
	renderer->vertices[renderer->num_vertices++] = {x1, y1, u1, v0, colorU32};
	renderer->vertices[renderer->num_vertices++] = {x2, y2, u1, v1, colorU32};
	renderer->vertices[renderer->num_vertices++] = {x3, y3, u0, v1, colorU32};
}

static void
DrawSpriteFrame(Renderer *renderer,
				RenderBackend *backend,
				int xorigin, int yorigin,
				SpriteFrame frame,
				TextureAsset *texture,
				float pos_x, float pos_y)
{
	float x0 = -(float)xorigin + frame.xoffset;
	float y0 = -(float)yorigin + frame.yoffset;

	float x1 = x0 + frame.width;
	float y1 = y0 + frame.height;

	// apply translation
	x0 += pos_x;
	y0 += pos_y;

	x1 += pos_x;
	y1 += pos_y;

	float u0 = frame.u / (float)texture->width;
	float v0 = frame.v / (float)texture->height;

	float u1 = (frame.u + frame.width) / (float)texture->width;
	float v1 = (frame.v + frame.height) / (float)texture->height;

	DrawQuad(renderer, backend,
			 texture,
			 x0, y0,
			 x1, y0,
			 x1, y1,
			 x0, y1,
			 u0, v0,
			 u1, v1,
			 {1, 1, 1, 1});
}

static void
DrawSpriteFrame(Renderer *renderer,
				RenderBackend *backend,
				int xorigin, int yorigin,
				SpriteFrame frame,
				TextureAsset *texture,
				float pos_x, float pos_y,
				float xscale, float yscale,
				float angleDegrees,
				vec4 color)
{
	float x0 = -(float)xorigin + frame.xoffset;
	float y0 = -(float)yorigin + frame.yoffset;

	float x1 = x0 + frame.width;
	float y1 = y0;

	float x2 = x0 + frame.width;
	float y2 = y0 + frame.height;

	float x3 = x0;
	float y3 = y0 + frame.height;

	// apply scale
	x0 *= xscale;
	y0 *= yscale;

	x1 *= xscale;
	y1 *= yscale;

	x2 *= xscale;
	y2 *= yscale;

	x3 *= xscale;
	y3 *= yscale;

	// apply rotation
	float angleRadians = Radians(angleDegrees);
	float C = Cos(angleRadians);
	float S = -Sin(angleRadians);

	float xx0 = x0, yy0 = y0;
	x0 = xx0*C - yy0*S;
	y0 = xx0*S + yy0*C;

	float xx1 = x1, yy1 = y1;
	x1 = xx1*C - yy1*S;
	y1 = xx1*S + yy1*C;

	float xx2 = x2, yy2 = y2;
	x2 = xx2*C - yy2*S;
	y2 = xx2*S + yy2*C;

	float xx3 = x3, yy3 = y3;
	x3 = xx3*C - yy3*S;
	y3 = xx3*S + yy3*C;

	// apply translation
	x0 += pos_x;
	y0 += pos_y;

	x1 += pos_x;
	y1 += pos_y;

	x2 += pos_x;
	y2 += pos_y;

	x3 += pos_x;
	y3 += pos_y;

	float u0 = frame.u / (float)texture->width;
	float v0 = frame.v / (float)texture->height;

	float u1 = (frame.u + frame.width) / (float)texture->width;
	float v1 = (frame.v + frame.height) / (float)texture->height;

	DrawQuad(renderer, backend,
			 texture,
			 x0, y0,
			 x1, y1,
			 x2, y2,
			 x3, y3,
			 u0, v0,
			 u1, v1,
			 color);
}

void
DrawSprite(Renderer *renderer,
		   RenderBackend *backend,
		   GameAssets *assets,
		   SpriteIndex sprite_index,
		   int frame_index,
		   float pos_x, float pos_y)
{
	SpriteInfo *info = GetSpriteInfo(sprite_index);
	SpriteFrame frame = GetSpriteFrame(info, frame_index);
	TextureAsset *texture = AssetGetTexture(assets, info->textureIndex);

	DrawSpriteFrame(renderer, backend,
					info->xorigin, info->yorigin,
					frame,
					texture,
					pos_x, pos_y);
}

void
DrawSprite(Renderer *renderer,
		   RenderBackend *backend,
		   GameAssets *assets,
		   SpriteIndex sprite_index,
		   int frame_index,
		   float pos_x, float pos_y,
		   float xscale, float yscale,
		   float angleDegrees,
		   vec4 color)
{
	SpriteInfo *info = GetSpriteInfo(sprite_index);
	SpriteFrame frame = GetSpriteFrame(info, frame_index);
	TextureAsset *texture = AssetGetTexture(assets, info->textureIndex);

	DrawSpriteFrame(renderer, backend,
					info->xorigin, info->yorigin,
					frame,
					texture,
					pos_x, pos_y,
					xscale, yscale,
					angleDegrees,
					color);
}

void
DrawText(Renderer *renderer, RenderBackend *backend, GameAssets *assets,
		 FontIndex font_index,
		 float pos_x, float pos_y,
		 const char *text,
		 HAlign halign, VAlign valign,
		 float xscale, float yscale,
		 vec4 color)
{
	FontInfo *info = GetFontInfo(font_index);
	TextureAsset *texture = AssetGetTexture(assets, info->textureIndex);

	float x = pos_x;
	float y = pos_y;

	if (halign == HAlign_Center)
	{
		vec2 textSize = GetTextSize(renderer, font_index, text, xscale, yscale, true);
		x -= 0.5f*textSize.x;
	}
	else if (halign == HAlign_Right)
	{
		vec2 textSize = GetTextSize(renderer, font_index, text, xscale, yscale, true);
		x -= textSize.x;
	}

	if (valign == VAlign_Middle)
	{
		vec2 textSize = GetTextSize(renderer, font_index, text, xscale, yscale);
		y -= 0.5f*textSize.y;
	}
	else if (valign == VAlign_Bottom)
	{
		vec2 textSize = GetTextSize(renderer, font_index, text, xscale, yscale);
		y -= textSize.y;
	}

	char ch;
	for (const char *ptr = text; (ch = *ptr); ptr++)
	{
		if (ch == '\n')
		{
			x = pos_x;
			y += info->lineHeight * yscale;

			if (halign == HAlign_Center)
			{
				vec2 textSize = GetTextSize(renderer, font_index, ptr+1, xscale, yscale, true);
				x -= 0.5f*textSize.x;
			}
			else if (halign == HAlign_Right)
			{
				vec2 textSize = GetTextSize(renderer, font_index, ptr+1, xscale, yscale, true);
				x -= textSize.x;
			}
		}
		else
		{
			int glyph_index = FontGetGlyphIndex(info, ch);
			if (glyph_index != -1)
			{
				FontGlyph *glyph = &info->glyphs[glyph_index];
			
				if (glyph->width != 0 && glyph->height != 0)
				{
					SpriteFrame frame = {};
					frame.u = glyph->u;
					frame.v = glyph->v;
					frame.width = glyph->width;
					frame.height = glyph->height;
					frame.xoffset = glyph->xoffset;
					frame.yoffset = glyph->yoffset;

					DrawSpriteFrame(renderer, backend,
									0, 0,
									frame,
									texture,
									x, y,
									xscale, yscale,
									0,
									color);
				}

				x += glyph->xadvance * xscale;
			}
		}
	}
}

vec2
GetTextSize(Renderer *renderer,
			FontIndex font_index,
			const char *text,
			f32 xscale, f32 yscale,
			bool onlyOneLine)
{
	FontInfo *info = GetFontInfo(font_index);

	f32 x = 0;
	f32 y = 0;

	f32 width = 0;
	f32 height = yscale*info->height;

	char ch;
	for (const char *ptr = text; (ch = *ptr); ptr++)
	{
		if (ch == '\n')
		{
			if (onlyOneLine)
			{
				break;
			}

			x = 0;
			y += yscale*info->lineHeight;

			height = Max(height, y + yscale*info->height);
		}
		else
		{
			int glyph_index = FontGetGlyphIndex(info, ch);
			if (glyph_index != -1)
			{
				FontGlyph *glyph = &info->glyphs[glyph_index];
			
				if (glyph->width != 0 && glyph->height != 0)
				{
					width = Max(width, x + xscale*glyph->xadvance);
				}

				x += xscale*glyph->xadvance;
			}
		}
	}

	vec2 result = {width, height};
	return result;
}

void
DrawRectangle(Renderer *renderer,
			  RenderBackend *backend,
			  GameAssets *assets,
			  float x, float y,
			  float width, float height,
			  vec4 color)
{
	TextureAsset *texture = AssetGetTexture(assets, tex_white);

	DrawQuad(renderer, backend,
			 texture,
			 x, y,
			 x+width, y,
			 x+width, y+height,
			 x, y+height,
			 0, 0, 0, 0,
			 color);
}

void
DrawTexture(Renderer *renderer, RenderBackend *backend, GameAssets *assets,
			TextureIndex textureIndex,
			f32 destX, f32 destY,
			f32 destW, f32 destH,
			int sourceX, int sourceY,
			int sourceW, int sourceH)
{
	TextureAsset *texture = AssetGetTexture(assets, textureIndex);

	f32 x0 = destX;
	f32 y0 = destY;

	f32 x1 = destX + destW;
	f32 y1 = destY;

	f32 x2 = destX + destW;
	f32 y2 = destY + destH;

	f32 x3 = destX;
	f32 y3 = destY + destH;

	f32 u0 = sourceX / (f32)texture->width;
	f32 v0 = sourceY / (f32)texture->height;

	f32 u1 = (sourceX + sourceW) / (f32)texture->width;
	f32 v1 = (sourceY + sourceH) / (f32)texture->height;

	DrawQuad(renderer, backend, texture,
			 x0, y0,
			 x1, y1,
			 x2, y2,
			 x3, y3,
			 u0, v0,
			 u1, v1,
			 {1, 1, 1, 1});
}

void
DrawCircle(Renderer *renderer, RenderBackend *backend, GameAssets *assets,
		   vec2 pos, f32 radius,
		   vec4 color)
{
	TextureAsset *texture = AssetGetTexture(assets, tex_white);

	f32 x0 = pos.x - radius;
	f32 y0 = pos.y - radius;

	f32 x1 = pos.x + radius;
	f32 y1 = pos.y - radius;

	f32 x2 = pos.x + radius;
	f32 y2 = pos.y + radius;

	f32 x3 = pos.x - radius;
	f32 y3 = pos.y + radius;

	f32 u0 = 0.0f;
	f32 v0 = 0.0f;

	f32 u1 = 1.0f;
	f32 v1 = 1.0f;

	x0 += renderer->translationX;
	y0 += renderer->translationY;

	x1 += renderer->translationX;
	y1 += renderer->translationY;

	x2 += renderer->translationX;
	y2 += renderer->translationY;

	x3 += renderer->translationX;
	y3 += renderer->translationY;

	if (renderer->num_vertices + 4 > RENDERER_MAX_BATCH_VERTICES
		|| texture != renderer->texture
		|| renderer->primitiveType != RendererPrimitiveType_Circles)
	{
		RendererFlush(renderer, backend);
		renderer->texture = texture;
		renderer->primitiveType = RendererPrimitiveType_Circles;
	}

	u32 colorU32 = ColorVec4ToU32(color);

	renderer->vertices[renderer->num_vertices++] = {x0, y0, u0, v0, colorU32};
	renderer->vertices[renderer->num_vertices++] = {x1, y1, u1, v0, colorU32};
	renderer->vertices[renderer->num_vertices++] = {x2, y2, u1, v1, colorU32};
	renderer->vertices[renderer->num_vertices++] = {x3, y3, u0, v1, colorU32};
}
