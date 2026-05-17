#pragma once

#include "common.h"
#include "math_stuff.h"

struct TextureAsset;
struct RenderBackend;

struct RenderVertex2D
{
	f32 x;
	f32 y;
	f32 u;
	f32 v;
	u32 color;
};

struct RenderVertex3D
{
	f32 x;
	f32 y;
	f32 z;
	f32 u;
	f32 v;
	u32 color;
};

enum RenderFilter : u32
{
	RenderFilter_Nearest,
	RenderFilter_Linear,
	RenderFilter_LinearMipMapLinear,
};

enum BlendFactor : u32
{
	BlendFactor_One,
	BlendFactor_SrcAlpha,
	BlendFactor_InvSrcAlpha,
};

enum ShaderIndex : u32
{
	ShaderIndex_Normal,
	ShaderIndex_Normal3D,
	ShaderIndex_Fog3D,
	ShaderIndex_Circle,

	ShaderIndex_COUNT,
};

enum ShaderUniformType : u32
{
	ShaderUniformType_F32,
	ShaderUniformType_Vec4,
};

#define RENDER_BACKEND_DRAW_QUADS(Name)            void Name(RenderBackend *backend, TextureAsset *texture, RenderVertex2D *vertices, int num_vertices)
#define RENDER_BACKEND_DRAW_TRIANGLES_3D(Name)     void Name(RenderBackend *backend, TextureAsset *texture, RenderVertex3D *vertices, int num_vertices)
#define RENDER_BACKEND_DRAW_CIRCLES(Name)          void Name(RenderBackend *backend, TextureAsset *texture, RenderVertex2D *vertices, int num_vertices)
#define RENDER_BACKEND_UPLOAD_TEXTURE_ASSET(Name)  bool Name(RenderBackend *backend, TextureAsset *texture)
#define RENDER_BACKEND_CLEAR(Name)                 void Name(RenderBackend *backend, float r, float g, float b, float a)
#define RENDER_BACKEND_SET_VIEWPORT(Name)          void Name(RenderBackend *backend, int x, int y, int width, int height)
#define RENDER_BACKEND_SET_UNIFORM(Name)           void Name(RenderBackend *backend, const char *name, ShaderUniformType type, void *value)
#define RENDER_BACKEND_ON_FULLSCREEN_CHANGED(Name) void Name(RenderBackend *backend)

typedef RENDER_BACKEND_DRAW_QUADS(RenderBackend_DrawQuads);
typedef RENDER_BACKEND_DRAW_TRIANGLES_3D(RenderBackend_DrawTriangles3D);
typedef RENDER_BACKEND_DRAW_CIRCLES(RenderBackend_DrawCircles);
typedef RENDER_BACKEND_UPLOAD_TEXTURE_ASSET(RenderBackend_UploadTextureAsset);
typedef RENDER_BACKEND_CLEAR(RenderBackend_Clear);
typedef RENDER_BACKEND_SET_VIEWPORT(RenderBackend_SetViewport);
typedef RENDER_BACKEND_SET_UNIFORM(RenderBackend_SetUniform);
typedef RENDER_BACKEND_ON_FULLSCREEN_CHANGED(RenderBackend_OnFullscreenChanged);

struct RenderBackend
{
	void *userdata;

	int windowWidth;
	int windowHeight;
	bool isExclusiveFullscreen;

	int targetWidth;
	int targetHeight;

	RenderFilter minFilter;
	RenderFilter magFilter;

	BlendFactor srcBlendFactor;
	BlendFactor destBlendFactor;
	bool enableBlending;

	ShaderIndex shaderIndex;

	mat4 modelView;
	mat4 projection;

	int numDrawCalls;
	int currNumDrawCalls;

	RenderBackend_DrawQuads           *DrawQuads;
	RenderBackend_DrawTriangles3D     *DrawTriangles3D;
	RenderBackend_DrawCircles         *DrawCircles;
	RenderBackend_UploadTextureAsset  *UploadTextureAsset;
	RenderBackend_Clear               *Clear;
	RenderBackend_SetViewport         *SetViewport;
	RenderBackend_SetUniform          *SetUniform;
	RenderBackend_OnFullscreenChanged *OnFullscreenChanged;
};
