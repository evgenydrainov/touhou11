#include "game_assets.h"
#include "platform_api.h"
#include "render_backend.h"

#pragma warning(push, 0)
#define QOI_NO_STDIO
#define QOI_MALLOC(size, userdata) PushSize((Arena*)userdata, size)
#define QOI_FREE(ptr, userdata) ((void)ptr)
#define QOI_ZEROARR(array) MemSet(array, 0, sizeof(array))
#define QOI_IMPLEMENTATION
#include "qoi.h"
#pragma warning(pop)

bool
LoadTextureAsset(TextureAsset *texture,
				 RenderBackend *backend,
				 const char *filePath,
				 Arena *textureArena,
				 Arena *transientArena,
				 bool wantMipMap)
{
	bool result = false;

	usize arenaSavePos = transientArena->pos;

	usize fileSize;
	void *fileData = PlatformLoadAssetFile(filePath, &fileSize, transientArena);
	if (fileData)
	{
		qoi_desc desc = {};
		void *pixels = qoi_decode(fileData, SafeTruncateI32(fileSize), &desc, 4, textureArena);
		if (pixels)
		{
			texture->pixels = pixels;
			texture->width = desc.width;
			texture->height = desc.height;

			texture->wantMipMap = wantMipMap;

			if (backend->UploadTextureAsset(backend, texture))
			{
				result = true;
			}
			else
			{
				LogError("Couldn't upload texture %s", filePath);
			}
		}
		else
		{
			LogError("Couldn't decode texture %s", filePath);
		}
	}
	else
	{
		LogError("Couldn't load file %s", filePath);
	}

	transientArena->pos = arenaSavePos;

	return result;
}
