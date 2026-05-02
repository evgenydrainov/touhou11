#pragma once

#include "common.h"
#include "render_backend.h"

struct RenderBackendSoftwareData
{
	void *backbuffer;
	int backbuffer_width;
	int backbuffer_height;
	int backbuffer_pitch;

	f32 scale_x;
	f32 scale_y;
};

void SoftwareRenderBackendInit(RenderBackend *backend);
RENDER_BACKEND_DRAW_QUADS(SoftwareRenderBackendDrawQuads);
RENDER_BACKEND_UPLOAD_TEXTURE_ASSET(SoftwareRenderBackendUploadTextureAsset);
RENDER_BACKEND_CLEAR(SoftwareRenderBackendClear);
RENDER_BACKEND_SET_VIEWPORT(SoftwareRenderBackendSetViewport);
RENDER_BACKEND_ON_FULLSCREEN_CHANGED(SoftwareRenderBackendOnFullscreenChanged);
