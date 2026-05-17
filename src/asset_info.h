#pragma once

#include "common.h"
#include "math_stuff.h"
#include "asset_info.generated.h"

inline SpriteInfo *
GetSpriteInfo(SpriteIndex sprite_index)
{
	Assert(sprite_index < SpriteIndex_COUNT);
	return &g_spriteInfo[sprite_index];
}

inline SpriteFrame
GetSpriteFrame(SpriteInfo *info, int frame_index)
{
	Assert(frame_index >= 0);
	Assert(frame_index < info->numFrames);
	return info->frames[frame_index];
}

inline FontInfo *
GetFontInfo(FontIndex font_index)
{
	Assert(font_index < FontIndex_COUNT);
	return &g_fontInfo[font_index];
}

inline int
FontGetGlyphIndex(FontInfo *info, u32 charcode)
{
	int result = 1;
	if (charcode >= 32 && charcode <= 126)
	{
		result = charcode - 32;
	}

	return result;
}

inline f32
SpriteAnimate(SpriteIndex spriteIndex, f32 frameIndex, f32 delta)
{
	SpriteInfo *info = GetSpriteInfo(spriteIndex);

	frameIndex += info->animSpeed * delta;

	if (frameIndex >= info->numFrames)
	{
		f32 a = frameIndex - (f32)info->loopFrame;
		f32 b = (f32)(info->numFrames - info->loopFrame);
		Assert(b != 0);
		frameIndex = (f32)info->loopFrame + Fmod(a, b);
	}

	return frameIndex;
}
