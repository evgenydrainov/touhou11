#pragma once

#include "common.h"
#include "render_backend.h"
#include "opengl_functions.h"

enum OpenGLContextVersion : u32
{
	OpenGLContextVersion_3_3_Core,
	OpenGLContextVersion_2_0_ES,
	OpenGLContextVersion_1_1,
};

struct RenderBackendOpenGLData
{
	u32 vertexBuffer;
	usize vertexBufferSize;

	u32 quadIndexBuffer;
	int numQuadIndices;

	u32 mainFrameBuffer;
	u32 mainFrameBufferTexture;
	int mainFrameBufferWidth;
	int mainFrameBufferHeight;

	u32 shaders[ShaderIndex_COUNT];

	OpenGLFunctions *gl;
	OpenGLContextVersion gl_version;
};

void OpenGLRenderBackendInit(RenderBackend *backend,
							 OpenGLContextVersion gl_version,
							 Arena *arena,
							 OpenGLFunctions *gl,
							 int maxNumQuads,
							 int mainFrameBufferWidth, int mainFrameBufferHeight);
bool OpenGLRenderBackendPrepareDraw(RenderBackend *backend);
void OpenGLRenderBackendPresent(RenderBackend *backend);
RENDER_BACKEND_DRAW_QUADS(OpenGLRenderBackendDrawQuads);
RENDER_BACKEND_DRAW_TRIANGLES_3D(OpenGLRenderBackendDrawTriangles3D);
RENDER_BACKEND_UPLOAD_TEXTURE_ASSET(OpenGLRenderBackendUploadTextureAsset);
RENDER_BACKEND_CLEAR(OpenGLRenderBackendClear);
RENDER_BACKEND_SET_VIEWPORT(OpenGLRenderBackendSetViewport);
RENDER_BACKEND_SET_UNIFORM(OpenGLRenderBackendSetUniform);
RENDER_BACKEND_ON_FULLSCREEN_CHANGED(OpenGLRenderBackendOnFullscreenChanged);
