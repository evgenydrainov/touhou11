#include "render_backend_software.h"
#include "game_assets.h"
#include "math_stuff.h"

//
// https://haqr.eu/tinyrenderer
//

void SoftwareRenderBackendInit(RenderBackend *backend)
{
	backend->DrawQuads = SoftwareRenderBackendDrawQuads;
	backend->UploadTextureAsset = SoftwareRenderBackendUploadTextureAsset;
	backend->Clear = SoftwareRenderBackendClear;
	backend->SetViewport = SoftwareRenderBackendSetViewport;
	backend->OnFullscreenChanged = SoftwareRenderBackendOnFullscreenChanged;
}

static void SoftwareRenderBackendDrawLine(RenderBackendSoftwareData *backend,
										  f32 float_x1, f32 float_y1,
										  f32 float_x2, f32 float_y2)
{
	float_x1 *= backend->scale_x;
	float_y1 *= backend->scale_y;

	float_x2 *= backend->scale_x;
	float_y2 *= backend->scale_y;

	i32 x1 = (i32)float_x1;
	i32 y1 = (i32)float_y1;

	i32 x2 = (i32)float_x2;
	i32 y2 = (i32)float_y2;

	bool is_steep = Abs(x1 - x2) < Abs(y1 - y2);
	if (is_steep)
	{
		Swap(&x1, &y1);
		Swap(&x2, &y2);
	}

	if (x1 > x2)
	{
		Swap(&x1, &x2);
		Swap(&y1, &y2);
	}

	for (i32 x = x1; x <= x2; x++)
	{
		f32 t = (x - x1) / (f32)(x2 - x1);
		i32 y = (i32)(y1 + (y2 - y1)*t);

		i32 draw_x;
		i32 draw_y;
		if (is_steep)
		{
			draw_x = y;
			draw_y = x;
		}
		else
		{
			draw_x = x;
			draw_y = y;
		}

		if (draw_x >= 0 && draw_x < backend->backbuffer_width
			&& draw_y >= 0 && draw_y < backend->backbuffer_height)
		{
			u32 *pixel = (u32*)((u8*)backend->backbuffer
								+ draw_y*backend->backbuffer_pitch
								+ draw_x*4);
			*pixel = 0;
		}
	}
}

static f64 SignedTriangleArea(f64 x1, f64 y1, f64 x2, f64 y2, f64 x3, f64 y3)
{
	f64 result = 0.5*((y2-y1)*(x2+x1) + (y3-y2)*(x3+x2) + (y1-y3)*(x1+x3));
	return result;
}

//struct SoftwareRenderBackendTexture
//{
//	int width;
//	int height;
//	u32 pixels[];
//};

static u32 SampleTexture(u32 *texture_pixels,
						 int texture_width, int texture_height,
						 f32 u, f32 v)
{
	u = Clamp(u, 0.0f, 1.0f);
	v = Clamp(v, 0.0f, 1.0f);

	i32 x = (i32)(u * (texture_width-1));
	i32 y = (i32)(v * (texture_height-1));

	u32 result = texture_pixels[y*texture_width + x];
	return result;
}

static void SoftwareRenderBackendDrawTriangle(RenderBackendSoftwareData *backend,
											  TextureAsset *texture,
											  RenderVertex2D vertex1,
											  RenderVertex2D vertex2,
											  RenderVertex2D vertex3)
{
	vertex1.x *= backend->scale_x;
	vertex1.y *= backend->scale_y;

	vertex2.x *= backend->scale_x;
	vertex2.y *= backend->scale_y;

	vertex3.x *= backend->scale_x;
	vertex3.y *= backend->scale_y;

	i32 x1 = RoundFloat32ToInt32(vertex1.x);
	i32 y1 = RoundFloat32ToInt32(vertex1.y);

	i32 x2 = RoundFloat32ToInt32(vertex2.x);
	i32 y2 = RoundFloat32ToInt32(vertex2.y);

	i32 x3 = RoundFloat32ToInt32(vertex3.x);
	i32 y3 = RoundFloat32ToInt32(vertex3.y);

	i32 bbminx = Min(x1, x2, x3);
	i32 bbminy = Min(y1, y2, y3);
	i32 bbmaxx = Max(x1, x2, x3);
	i32 bbmaxy = Max(y1, y2, y3);
	f64 total_area = SignedTriangleArea(x1, y1, x2, y2, x3, y3);

	bbminx = Max(bbminx, 0);
	bbminy = Max(bbminy, 0);

	bbmaxx = Min(bbmaxx, backend->backbuffer_width-1);
	bbmaxy = Min(bbmaxy, backend->backbuffer_height-1);

	u32 *texturePixels = (u32 *)texture->pixels;
	int textureWidth = texture->width;
	int textureHeight = texture->height;

	u8 *pDestRow = ((u8 *)backend->backbuffer
					+ bbminy*backend->backbuffer_pitch
					+ bbminx*4);

	for (i32 y = bbminy; y <= bbmaxy; y++)
	{
		u32 *pDestColor = (u32 *)pDestRow;

		for (i32 x = bbminx; x <= bbmaxx; x++)
		{
			f64 pixel_center_x = (f64)x + 0.5;
			f64 pixel_center_y = (f64)y + 0.5;

			f64 alpha = SignedTriangleArea(pixel_center_x, pixel_center_y, x2, y2, x3, y3) / total_area;
			f64 beta  = SignedTriangleArea(pixel_center_x, pixel_center_y, x3, y3, x1, y1) / total_area;
			f64 gamma = 1.0 - alpha - beta;

			if (alpha >= 0 && beta >= 0 && gamma >= 0)
			{
				f32 u = (f32)(alpha*vertex1.u + beta*vertex2.u + gamma*vertex3.u);
				f32 v = (f32)(alpha*vertex1.v + beta*vertex2.v + gamma*vertex3.v);

				u32 srcColor = SampleTexture(texturePixels, textureWidth, textureHeight, u, v);
				f32 srcR = ((srcColor >> 16) & 0xff) / 255.0f;
				f32 srcG = ((srcColor >>  8) & 0xff) / 255.0f;
				f32 srcB = ((srcColor >>  0) & 0xff) / 255.0f;
				f32 srcA = ((srcColor >> 24) & 0xff) / 255.0f;

				u32 destColor = *pDestColor;
				f32 destR = ((destColor >> 16) & 0xff) / 255.0f;
				f32 destG = ((destColor >>  8) & 0xff) / 255.0f;
				f32 destB = ((destColor >>  0) & 0xff) / 255.0f;
				f32 destA = ((destColor >> 24) & 0xff) / 255.0f;

				f32 outA = srcA + destA*(1.0f - srcA);
				f32 outR = (srcR*srcA + destR*destA*(1.0f - srcA))/outA;
				f32 outG = (srcG*srcA + destG*destA*(1.0f - srcA))/outA;
				f32 outB = (srcB*srcA + destB*destA*(1.0f - srcA))/outA;

				*pDestColor = ColorVec4ToU32({outB, outG, outR, outA});
			}

			pDestColor++;
		}

		pDestRow += backend->backbuffer_pitch;
	}
}

RENDER_BACKEND_DRAW_QUADS(SoftwareRenderBackendDrawQuads)
{
	RenderBackendSoftwareData *data = (RenderBackendSoftwareData *)backend->userdata;

	Assert(num_vertices % 4 == 0);

	for (int vertex_index = 0; vertex_index < num_vertices; vertex_index += 4)
	{
#if 1
		SoftwareRenderBackendDrawTriangle(data,
										  texture,
										  vertices[vertex_index+0],
										  vertices[vertex_index+1],
										  vertices[vertex_index+2]);

		SoftwareRenderBackendDrawTriangle(data,
										  texture,
										  vertices[vertex_index+2],
										  vertices[vertex_index+3],
										  vertices[vertex_index+0]);
#else
		SoftwareRenderBackendDrawLine(data,
									  vertices[vertex_index].x, vertices[vertex_index].y,
									  vertices[vertex_index+1].x, vertices[vertex_index+1].y);

		SoftwareRenderBackendDrawLine(data,
									  vertices[vertex_index+1].x, vertices[vertex_index+1].y,
									  vertices[vertex_index+2].x, vertices[vertex_index+2].y);

		SoftwareRenderBackendDrawLine(data,
									  vertices[vertex_index+2].x, vertices[vertex_index+2].y,
									  vertices[vertex_index+3].x, vertices[vertex_index+3].y);

		SoftwareRenderBackendDrawLine(data,
									  vertices[vertex_index+3].x, vertices[vertex_index+3].y,
									  vertices[vertex_index].x, vertices[vertex_index].y);
#endif
	}

	// LogInfo("DrawQuads: %d quads", num_vertices/4);
}

RENDER_BACKEND_UPLOAD_TEXTURE_ASSET(SoftwareRenderBackendUploadTextureAsset)
{
	RenderBackendSoftwareData *data = (RenderBackendSoftwareData *)backend->userdata;

	/*SoftwareRenderBackendTexture *texture = (SoftwareRenderBackendTexture *)SDL_malloc(sizeof(SoftwareRenderBackendTexture) + width*height*4);
	texture->width = width;
	texture->height = height;
	MemCpy(texture->pixels, pixel_data, width*height*4);*/

	//
	// nothing to do
	//

	return true;
}

static u32 PackColorU32(float r, float g, float b, float a)
{
	u32 result = (((u32)(255.0f*r) << 24)
				  | ((u32)(255.0f*g) << 16)
				  | ((u32)(255.0f*b) << 8)
				  | ((u32)(255.0f*a) << 0));
	return result;
}

RENDER_BACKEND_CLEAR(SoftwareRenderBackendClear)
{
	RenderBackendSoftwareData *data = (RenderBackendSoftwareData *)backend->userdata;

	u32 color = PackColorU32(r, g, b, a);

	u8 *pDestRow = (u8 *)data->backbuffer;
	for (int y = 0; y < data->backbuffer_height; y++)
	{
		u32 *pDestColor = (u32 *)pDestRow;
		for (int x = 0; x < data->backbuffer_width; x++)
		{
			*pDestColor++ = color;
		}

		pDestRow += data->backbuffer_pitch;
	}
}

RENDER_BACKEND_SET_VIEWPORT(SoftwareRenderBackendSetViewport)
{
}

RENDER_BACKEND_ON_FULLSCREEN_CHANGED(SoftwareRenderBackendOnFullscreenChanged)
{
}
