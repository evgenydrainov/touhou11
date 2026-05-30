#pragma once

#include "common.h"
#include "render_backend.h"
#include "math_stuff.h"

struct RenderBackendSoftwareData
{
	void *backbuffer;
	int backbuffer_width;
	int backbuffer_height;
	int backbuffer_pitch;
};

void SoftwareRenderBackendInit(RenderBackend *backend);
bool SoftwareRenderBackendPrepareDraw(RenderBackend *backend);
RENDER_BACKEND_DRAW_QUADS(SoftwareRenderBackendDrawQuads);
RENDER_BACKEND_DRAW_TRIANGLES_3D(SoftwareRenderBackendDrawTriangles3D);
RENDER_BACKEND_DRAW_CIRCLES(SoftwareRenderBackendDrawCircles);
RENDER_BACKEND_UPLOAD_TEXTURE_ASSET(SoftwareRenderBackendUploadTextureAsset);
RENDER_BACKEND_CLEAR(SoftwareRenderBackendClear);
RENDER_BACKEND_SET_UNIFORM(SoftwareRenderBackendSetUniform);
RENDER_BACKEND_ON_FULLSCREEN_CHANGED(SoftwareRenderBackendOnFullscreenChanged);
