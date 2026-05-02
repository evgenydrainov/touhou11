#pragma once

#include "common.h"
#include "asset_info.h"

struct RenderBackend;

struct TextureAsset
{
	void *handle;
	void *pixels;
	i32 width;
	i32 height;
	bool wantMipMap;
};

struct GameAssets
{
	TextureAsset textures[TextureIndex_COUNT];
};

bool
LoadTextureAsset(TextureAsset *texture,
				 RenderBackend *backend,
				 const char *filePath,
				 Arena *textureArena,
				 Arena *transientArena,
				 bool wantMipMap);

inline TextureAsset *
AssetGetTexture(GameAssets *assets,
				TextureIndex textureIndex)
{
	Assert(textureIndex < ArrayLength(assets->textures));
	return &assets->textures[textureIndex];
}
