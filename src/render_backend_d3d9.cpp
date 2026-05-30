#include "render_backend_d3d9.h"

#ifdef WIN32

#include "game_assets.h"
#include "minid3d9.h"

static void
D3D9RenderBackendFillPresentParams(RenderBackend *backend,
								   D3DPRESENT_PARAMETERS *pp)
{
	RenderBackendD3D9Data *data = (RenderBackendD3D9Data *)backend->userdata;

	*pp = {};
	pp->BackBufferWidth = backend->windowWidth;
	pp->BackBufferHeight = backend->windowHeight;
	pp->BackBufferCount = 1;
	pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
	pp->hDeviceWindow = (HWND)data->hwnd;
	pp->Flags = 0;
	pp->PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	if (backend->isExclusiveFullscreen)
	{
		D3DDISPLAYMODE displayMode = {};
		data->d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &displayMode);

		pp->Windowed = false;
		pp->FullScreen_RefreshRateInHz = displayMode.RefreshRate;
		pp->BackBufferFormat = displayMode.Format;
	}
	else
	{
		pp->Windowed = true;
		pp->FullScreen_RefreshRateInHz = 0;
		pp->BackBufferFormat = D3DFMT_UNKNOWN;
	}

	/*LogInfo("-------------------");
	LogInfo("D3D9 Present Params:");
	LogInfo("BackBufferWidth: %d", pp->BackBufferWidth);
	LogInfo("BackBufferHeight: %d", pp->BackBufferHeight);
	LogInfo("Windowed: %d", pp->Windowed);
	LogInfo("Refresh Rate: %d", pp->FullScreen_RefreshRateInHz);
	LogInfo("-------------------");*/
}

static bool
D3D9RenderBackendRecreateResources(RenderBackendD3D9Data *data)
{
	bool success = false;

	HRESULT hr = data->device->CreateVertexBuffer(data->max_num_vertices*sizeof(RenderVertex2D),
												  D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,
												  0,
												  D3DPOOL_DEFAULT,
												  &data->vbo,
												  nullptr);
	if (SUCCEEDED(hr))
	{
		success = true;
	}
	else
	{
		LogError("CreateVertexBuffer() failed");
	}

	return success;
}

typedef IDirect3D9 * WINAPI Direct3DCreate9Func(UINT SDKVersion);

static bool
UploadDataIBO(IDirect3DIndexBuffer9 *ibo, void *source, u32 size)
{
	bool success = false;

	void *dest;
	HRESULT hr = ibo->Lock(0, size, &dest, D3DLOCK_DISCARD);
	if (SUCCEEDED(hr))
	{
		MemCpy(dest, source, size);
		ibo->Unlock();

		success = true;
	}
	else
	{
		LogError("ibo->Lock() failed");
	}

	return success;
}

static bool
UploadDataVBO(IDirect3DVertexBuffer9 *vbo, void *source, u32 size)
{
	bool success = false;

	void *dest;
	HRESULT hr = vbo->Lock(0, size, &dest, D3DLOCK_DISCARD);
	if (SUCCEEDED(hr))
	{
		MemCpy(dest, source, size);
		vbo->Unlock();

		success = true;
	}
	else
	{
		LogError("vbo->Lock() failed");
	}

	return success;
}

bool
D3D9RenderBackendInit(RenderBackend *backend,
					  void *hwnd,
					  Arena *arena,
					  int max_num_quads,
					  void *Direct3DCreate9_)
{
	usize arena_save_pos = arena->pos;

	bool success = false;

	RenderBackendD3D9Data *data = (RenderBackendD3D9Data *)backend->userdata;

	data->hwnd = hwnd;
	backend->DrawQuads = D3D9RenderBackendDrawQuads;
	backend->UploadTextureAsset = D3D9RenderBackendUploadTextureAsset;
	backend->Clear = D3D9RenderBackendClear;
	backend->SetViewport = D3D9RenderBackendSetViewport;
	backend->OnFullscreenChanged = D3D9RenderBackendOnFullscreenChanged;

	Direct3DCreate9Func *Direct3DCreate9 = (Direct3DCreate9Func *)Direct3DCreate9_;
	data->d3d = Direct3DCreate9(D3D_SDK_VERSION);
	if (data->d3d)
	{
		D3DPRESENT_PARAMETERS pp;
		D3D9RenderBackendFillPresentParams(backend, &pp);

		HRESULT hr = data->d3d->CreateDevice(D3DADAPTER_DEFAULT,
											 D3DDEVTYPE_HAL,
											 (HWND)hwnd,
											 D3DCREATE_HARDWARE_VERTEXPROCESSING,
											 &pp,
											 &data->device);
		if (SUCCEEDED(hr))
		{
			data->savedBackBufferWidth = pp.BackBufferWidth;
			data->savedBackBufferHeight = pp.BackBufferHeight;

			int max_num_indices = 6*max_num_quads;
			int max_num_vertices = 4*max_num_quads;

			data->max_num_vertices = max_num_vertices;

			if (D3D9RenderBackendRecreateResources(data))
			{
				u16 *indices = PushArray(arena, max_num_indices, u16);

				u16 offset = 0;
				for (int i = 0; i < max_num_indices; i += 6)
				{
					indices[i + 0] = offset + 0;
					indices[i + 1] = offset + 1;
					indices[i + 2] = offset + 2;

					indices[i + 3] = offset + 2;
					indices[i + 4] = offset + 3;
					indices[i + 5] = offset + 0;

					offset += 4;
				}

				hr = data->device->CreateIndexBuffer(max_num_indices*sizeof(u16),
													 D3DUSAGE_WRITEONLY,
													 D3DFMT_INDEX16,
													 D3DPOOL_MANAGED,
													 &data->ibo,
													 nullptr);
				if (SUCCEEDED(hr))
				{
					if (UploadDataIBO(data->ibo, indices, max_num_indices*sizeof(u16)))
					{
						D3DVERTEXELEMENT9 vertexDeclDesc[] = {
							{
								0,
								(WORD)OffsetOf(RenderVertex2D, pos),
								D3DDECLTYPE_FLOAT2,
								D3DDECLMETHOD_DEFAULT,
								D3DDECLUSAGE_POSITION,
								0
							},
							{
								0,
								(WORD)OffsetOf(RenderVertex2D, texCoord),
								D3DDECLTYPE_FLOAT2,
								D3DDECLMETHOD_DEFAULT,
								D3DDECLUSAGE_TEXCOORD,
								0
							},
							{
								0,
								(WORD)OffsetOf(RenderVertex2D, color),
								D3DDECLTYPE_UBYTE4N,
								D3DDECLMETHOD_DEFAULT,
								D3DDECLUSAGE_COLOR,
								0
							},
							D3DDECL_END(),
						};

						hr = data->device->CreateVertexDeclaration(vertexDeclDesc, &data->vertexDecl);
						if (SUCCEEDED(hr))
						{
							success = true;
						}
						else
						{
							LogError("CreateVertexDeclaration() failed");
						}
					}
					else
					{
						LogError("UploadDataIBO() failed");
					}
				}
				else
				{
					LogError("CreateIndexBuffer() failed");
				}
			}
			else
			{
				LogError("D3D9RenderBackendRecreateResources() failed");
			}
		}
		else
		{
			LogError("CreateDevice() failed");
		}
	}
	else
	{
		LogError("Direct3DCreate9() failed");
	}

	arena->pos = arena_save_pos;

	return success;
}

void
D3D9RenderBackendPresent(RenderBackend *backend)
{
	RenderBackendD3D9Data *data = (RenderBackendD3D9Data *)backend->userdata;

	HRESULT hr = data->device->Present(nullptr,
									   nullptr,
									   nullptr,
									   nullptr);
	if (hr == D3DERR_DEVICELOST)
	{
		//LogInfo("Present(): D3DERR_DEVICELOST");
		data->deviceLost = true;
	}
	else if (FAILED(hr))
	{
		LogError("Present() failed. This shouldn't happen.");
		Assert(false);
	}
}

static void
D3D9RenderBackendInvalidateResources(RenderBackendD3D9Data *data)
{
	data->vbo->Release();
	data->vbo = nullptr;
}

static void
D3D9RenderBackendResetDevice(RenderBackend *backend)
{
	RenderBackendD3D9Data *data = (RenderBackendD3D9Data *)backend->userdata;

	D3D9RenderBackendInvalidateResources(data);

	D3DPRESENT_PARAMETERS pp;
	D3D9RenderBackendFillPresentParams(backend, &pp);

	HRESULT result = data->device->Reset(&pp);
	if (FAILED(result))
	{
		LogError("Reset() failed. This shouldn't happen.");
		Assert(false);
	}

	D3D9RenderBackendRecreateResources(data);

	data->savedBackBufferWidth = pp.BackBufferWidth;
	data->savedBackBufferHeight = pp.BackBufferHeight;
}

RENDER_BACKEND_ON_FULLSCREEN_CHANGED(D3D9RenderBackendOnFullscreenChanged)
{
	RenderBackendD3D9Data *data = (RenderBackendD3D9Data *)backend->userdata;

	D3D9RenderBackendResetDevice(backend);
}

bool
D3D9RenderBackendPrepareDraw(RenderBackend *backend)
{
	RenderBackendD3D9Data *data = (RenderBackendD3D9Data *)backend->userdata;

	if (data->deviceLost)
	{
		HRESULT hr = data->device->TestCooperativeLevel();
		if (hr == D3DERR_DEVICELOST)
		{
			//LogInfo("TestCooperativeLevel(): D3DERR_DEVICELOST");
		}
		else
		{
			if (hr == D3DERR_DEVICENOTRESET)
			{
				//LogInfo("TestCooperativeLevel(): D3DERR_DEVICENOTRESET");
				D3D9RenderBackendResetDevice(backend);
			}

			data->deviceLost = false;
		}
	}

	if (!data->deviceLost)
	{
		if (backend->windowWidth != data->savedBackBufferWidth
			|| backend->windowHeight != data->savedBackBufferHeight)
		{
			D3D9RenderBackendResetDevice(backend);
		}
	}

	bool success = !data->deviceLost;
	return success;
}

RENDER_BACKEND_DRAW_QUADS(D3D9RenderBackendDrawQuads)
{
	RenderBackendD3D9Data *data = (RenderBackendD3D9Data *)backend->userdata;

	IDirect3DDevice9 *device = data->device;

	IDirect3DTexture9 *textureHandle = (IDirect3DTexture9 *)texture->handle;

	if (UploadDataVBO(data->vbo, vertices, num_vertices*sizeof(RenderVertex2D)))
	{
		device->SetVertexDeclaration(data->vertexDecl);

		device->SetRenderState(D3DRS_LIGHTING, false);
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

		device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_POINT);

		device->SetTransform(D3DTS_VIEW, (D3DMATRIX *)&backend->modelView);
		device->SetTransform(D3DTS_PROJECTION, (D3DMATRIX *)&backend->projection);

		device->SetStreamSource(0, data->vbo, 0, sizeof(RenderVertex2D));
		device->SetIndices(data->ibo);

		device->SetTexture(0, textureHandle);

		HRESULT hr = device->BeginScene();
		if (SUCCEEDED(hr))
		{
			device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,
										 0,
										 0,
										 num_vertices,
										 0,
										 num_vertices/4*2);

			device->EndScene();
		}
		else
		{
			LogError("BeginScene() failed");
		}
	}
	else
	{
		LogError("UploadDataVBO() failed");
	}
}

RENDER_BACKEND_UPLOAD_TEXTURE_ASSET(D3D9RenderBackendUploadTextureAsset)
{
	RenderBackendD3D9Data *data = (RenderBackendD3D9Data *)backend->userdata;

	bool success = false;

	void *texturePixels = texture->pixels;
	int textureWidth = texture->width;
	int textureHeight = texture->height;

	IDirect3DTexture9 *textureHandle;
	HRESULT hr = data->device->CreateTexture(textureWidth, textureHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &textureHandle, nullptr);
	if (SUCCEEDED(hr))
	{
		D3DLOCKED_RECT lockedRect;
		hr = textureHandle->LockRect(0, &lockedRect, nullptr, D3DLOCK_DISCARD);
		if (SUCCEEDED(hr))
		{
			u8 *pSourceRow = (u8 *)texturePixels;
			u8 *pDestRow = (u8 *)lockedRect.pBits;
			for (int y = 0; y < textureHeight; y++)
			{
				u32 *pDestColor = (u32 *)pDestRow;
				u32 *pSourceColor = (u32 *)pSourceRow;
				for (int x = 0; x < textureWidth; x++)
				{
					u32 color = *pSourceColor++;

					u32 r = (color >>  0) & 0xff;
					u32 g = (color >>  8) & 0xff;
					u32 b = (color >> 16) & 0xff;
					u32 a = (color >> 24) & 0xff;
					*pDestColor++ = ((b << 0)
									 | (g << 8)
									 | (r << 16)
									 | (a << 24));
				}

				pDestRow += lockedRect.Pitch;
				pSourceRow += 4*textureWidth;
			}

			textureHandle->UnlockRect(0);

			texture->handle = textureHandle;
			success = true;
		}
		else
		{
			LogError("LockRect() failed");
		}
	}
	else
	{
		LogError("CreateTexture() failed");
	}

	return success;
}

RENDER_BACKEND_CLEAR(D3D9RenderBackendClear)
{
	RenderBackendD3D9Data *data = (RenderBackendD3D9Data *)backend->userdata;

	D3DCOLOR color = D3DCOLOR_RGBA((int)(255.0f*r), (int)(255.0f*g), (int)(255.0f*b), (int)(255.0f*a));
	data->device->Clear(0, nullptr, D3DCLEAR_TARGET, color, 0.0f, 0);
}

RENDER_BACKEND_SET_VIEWPORT(D3D9RenderBackendSetViewport)
{
	RenderBackendD3D9Data *data = (RenderBackendD3D9Data *)backend->userdata;

	D3DVIEWPORT9 viewport = {};
	viewport.X = x;
	viewport.Y = y;
	viewport.Width = width;
	viewport.Height = height;
	viewport.MinZ = 0.0f;
	viewport.MaxZ = 1.0f;

	HRESULT hr = data->device->SetViewport(&viewport);
	if (SUCCEEDED(hr))
	{
		// success
	}
	else
	{
		LogError("SetViewport() failed");
	}
}

#else

bool D3D9RenderBackendInit(RenderBackend *backend,
						   void *hwnd,
						   Arena *arena,
						   int max_num_quads,
						   void *Direct3DCreate9_)
{
	return false;
}
void D3D9RenderBackendPresent(RenderBackend *backend)
{
}
bool D3D9RenderBackendPrepareDraw(RenderBackend *backend)
{
	return false;
}
RENDER_BACKEND_DRAW_QUADS(D3D9RenderBackendDrawQuads)
{
}
RENDER_BACKEND_UPLOAD_TEXTURE_ASSET(D3D9RenderBackendUploadTextureAsset)
{
	return false;
}
RENDER_BACKEND_CLEAR(D3D9RenderBackendClear)
{
}
RENDER_BACKEND_SET_VIEWPORT(D3D9RenderBackendSetViewport)
{
}
RENDER_BACKEND_ON_FULLSCREEN_CHANGED(D3D9RenderBackendOnFullscreenChanged)
{
}

#endif
