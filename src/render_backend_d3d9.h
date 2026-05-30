#pragma once

#include "common.h"
#include "render_backend.h"

struct IDirect3D9;
struct IDirect3DDevice9;
struct IDirect3DVertexBuffer9;
struct IDirect3DIndexBuffer9;
struct IDirect3DVertexDeclaration9;
struct IDirect3DTexture9;

struct RenderBackendD3D9Data
{
	IDirect3D9 *d3d;
	IDirect3DDevice9 *device;
	IDirect3DVertexBuffer9* vbo;
	IDirect3DIndexBuffer9* ibo;
	IDirect3DVertexDeclaration9 *vertexDecl;

	int max_num_vertices;

	void *hwnd;

	bool deviceLost;

	int savedBackBufferWidth;
	int savedBackBufferHeight;
};

bool D3D9RenderBackendInit(RenderBackend *backend,
						   void *hwnd,
						   Arena *arena,
						   int max_num_quads,
						   void *Direct3DCreate9_);
void D3D9RenderBackendPresent(RenderBackend *backend);
bool D3D9RenderBackendPrepareDraw(RenderBackend *backend);
RENDER_BACKEND_DRAW_QUADS(D3D9RenderBackendDrawQuads);
RENDER_BACKEND_UPLOAD_TEXTURE_ASSET(D3D9RenderBackendUploadTextureAsset);
RENDER_BACKEND_CLEAR(D3D9RenderBackendClear);
RENDER_BACKEND_ON_FULLSCREEN_CHANGED(D3D9RenderBackendOnFullscreenChanged);
