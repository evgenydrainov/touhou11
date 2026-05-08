#define _CRT_SECURE_NO_WARNINGS

//
// warning C4996: 'strdup': The POSIX name for this item is deprecated. Instead,
// use the ISO C and C++ conformant name: _strdup. See online help for details.
//
#pragma warning(disable: 4996)

#include "common.h"
#include "math_stuff.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H

void
PlatformLogHandler(LogLevel level,
				   const char *file,
				   int line,
				   const char *format,
				   ...)
{
	va_list args;
	va_start(args, format);

	printf("[Asset Baker] %s: ", GetLogLevelTag(level));
	vprintf(format, args);
	printf("\n");

	va_end(args);
}

__declspec(noreturn) void
PlatformAssertionFailed(const char *condition,
						const char *file,
						int line)
{
	printf("[Asset Baker] Assertion Failed: %s\n", condition);

#ifdef _MSC_VER
	__debugbreak();
#endif

	exit(1);
}

#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#pragma warning(push, 0)
#define QOI_MALLOC(sz, userdata) malloc(sz)
#define QOI_FREE(p, userdata)    free(p)
#define QOI_ZEROARR(a) memset((a),0,sizeof(a))
#define QOI_IMPLEMENTATION
#include "qoi.h"
#pragma warning(pop)

enum RawTextureIndex : u32
{
	raw_tex_white,
	raw_tex_characters,
	raw_tex_projectiles,
	raw_tex_background,
	raw_tex_background_2,
	raw_tex_background_3,
	raw_tex_enemies,
	raw_tex_gfw_misty_lake,
	raw_tex_gfw_misty_lake2,
	raw_tex_gfw_misty_lake3,
	raw_tex_gfw_misty_lake4,

	RawTextureIndex_COUNT,
};

struct RawTextureInfo
{
	const char *filepath;
	bool includeInGame;
	const char *name;
	bool wantMipMap;
};

struct RawSpriteInfo
{
	const char *name;
	RawTextureIndex textureIndex;
	int u;
	int v;
	int width;
	int height;
	int xorigin;
	int yorigin;
	int numFrames;
	int numFramesInRow;
	float animSpeed;
	int loopFrame;

	int bakedAtlasIndex;
};

struct RawFontInfo
{
	const char *name;
	const char *filepath;
	int pixel_size;
	int outlineSize;

	int baked_atlas_index;
	int num_glyphs;
	int line_height;
	int height;
};

static RawTextureInfo g_raw_texture_info[RawTextureIndex_COUNT];

static void *
LoadFile(const char *filepath, usize *out_filesize)
{
	FILE *file = fopen(filepath, "rb");
	if (!file)
	{
		LogError("Couldn't load file %s", filepath);
		exit(1);
	}

	fseek(file, 0, SEEK_END);
	usize filesize = ftell(file);

	void *filedata = malloc(filesize);

	fseek(file, 0, SEEK_SET);
	fread(filedata, 1, filesize, file);

	*out_filesize = filesize;
	return filedata;
}

struct Image
{
	void *data;
	i32 width;
	i32 height;
};

static Image
LoadImageFromFile(const char *filepath)
{
	char full_filepath[512];
	snprintf(full_filepath, sizeof(full_filepath), "assets_raw/%s", filepath);

	usize filesize;
	void *filedata = LoadFile(full_filepath, &filesize);

	Image image = {};
	image.data = stbi_load_from_memory((u8*)filedata, SafeTruncateI32(filesize), &image.width, &image.height, nullptr, 4);

	if (!image.data)
	{
		LogError("Couldn't decode image %s", full_filepath);
		exit(1);
	}

	return image;
}

static Image g_raw_textures[RawTextureIndex_COUNT];

static Image
CopySubImage(Image source,
			 int x, int y,
			 int width, int height)
{
	Assert(x >= 0);
	Assert(y >= 0);

	Assert(x + width <= source.width);
	Assert(y + height <= source.height);

	Image image = {};
	image.data = malloc(width * height * 4);
	image.width = width;
	image.height = height;

	u8 *source_row = ((u8 *)source.data
					  + y*source.width*4 + x*4);
	u8 *dest_row = (u8 *)image.data;
	for (int yy = 0; yy < height; ++yy)
	{
		u32 *dest_color = (u32 *)dest_row;
		u32 *source_color = (u32 *)source_row;
		for (int xx = 0; xx < width; ++xx)
		{
			*dest_color = *source_color;

			dest_color++;
			source_color++;
		}

		dest_row += width * 4;
		source_row += source.width * 4;
	}

	return image;
}

static bool
ColumnHasOpaquePixel(Image image, int x)
{
	Assert(x >= 0);
	Assert(x < image.width);

	for (int y = 0; y < image.height; y++)
	{
		u32 *data = (u32 *)image.data;
		u32 pixel = data[y*image.width + x];
		u32 alpha = (pixel >> 24) & 0xff;

		if (alpha != 0)
		{
			return true;
		}
	}

	return false;
}

static bool
RowHasOpaquePixel(Image image, int y)
{
	Assert(y >= 0);
	Assert(y < image.height);

	for (int x = 0; x < image.width; x++)
	{
		u32 *data = (u32 *)image.data;
		u32 pixel = data[y*image.width + x];
		u32 alpha = (pixel >> 24) & 0xff;

		if (alpha != 0)
		{
			return true;
		}
	}

	return false;
}

static Image
TrimImage(Image image,
		  int *out_trimmed_left, int *out_trimmed_top)
{
	int x = 0;
	while (true)
	{
		if (ColumnHasOpaquePixel(image, x))
		{
			break;
		}
		x++;
	}

	int max_x = image.width-1;
	while (true)
	{
		if (ColumnHasOpaquePixel(image, max_x))
		{
			break;
		}
		max_x--;
	}

	int y = 0;
	while (true)
	{
		if (RowHasOpaquePixel(image, y))
		{
			break;
		}
		y++;
	}

	int max_y = image.height-1;
	while (true)
	{
		if (RowHasOpaquePixel(image, max_y))
		{
			break;
		}
		max_y--;
	}

	Image trimmed = CopySubImage(image, x, y, max_x-x+1, max_y-y+1);
	*out_trimmed_left = x;
	*out_trimmed_top = y;
	return trimmed;
}

static void
FreeImage(Image *image)
{
	free(image->data);
	*image = {};
}

static void
TrimImageInPlace(Image *image,
				 int *out_trimmed_left, int *out_trimmed_top)
{
	Image trimmed = TrimImage(*image, out_trimmed_left, out_trimmed_top);
	FreeImage(image);
	*image = trimmed;
}

static void
ImageBlit(Image *dest,
		  Image source,
		  int dest_x, int dest_y,
		  int source_x, int source_y,
		  int width, int height)
{
	Assert(dest_x >= 0);
	Assert(dest_y >= 0);

	Assert(source_x >= 0);
	Assert(source_y >= 0);

	Assert(dest_x + width <= dest->width);
	Assert(dest_y + height <= dest->height);

	Assert(source_x + width <= source.width);
	Assert(source_y + height <= source.height);

	u8 *source_row = ((u8 *)source.data
					  + source_y*source.width*4
					  + source_x*4);
	u8 *dest_row = ((u8 *)dest->data
					+ dest_y*dest->width*4
					+ dest_x*4);
	for (int yy = 0; yy < height; ++yy)
	{
		u32 *dest_color = (u32 *)dest_row;
		u32 *source_color = (u32 *)source_row;
		for (int xx = 0; xx < width; ++xx)
		{
			*dest_color = *source_color;

			dest_color++;
			source_color++;
		}

		dest_row += dest->width * 4;
		source_row += source.width * 4;
	}
}

static void
FillRectangle(Image *dest,
			  int dest_x, int dest_y,
			  int width, int height,
			  u32 color)
{
	Assert(dest_x >= 0);
	Assert(dest_y >= 0);

	Assert(dest_x + width <= dest->width);
	Assert(dest_y + height <= dest->height);

	u8 *dest_row = ((u8 *)dest->data
					+ dest_y*dest->width*4
					+ dest_x*4);
	for (int yy = 0; yy < height; ++yy)
	{
		u32 *dest_color = (u32 *)dest_row;
		for (int xx = 0; xx < width; ++xx)
		{
			*dest_color = color;

			dest_color++;
		}

		dest_row += dest->width * 4;
	}
}

static u32
GetPixel(Image image, int x, int y)
{
	Assert(x >= 0);
	Assert(y >= 0);

	Assert(x < image.width);
	Assert(y < image.height);

	u32 *pixels = (u32 *)image.data;
	u32 result = pixels[y*image.width + x];
	return result;
}

static Image
ExpandBorders(Image image, int border)
{
	Image expanded = {};
	expanded.width = image.width + 2*border;
	expanded.height = image.height + 2*border;
	expanded.data = calloc(expanded.width*expanded.height*4, 1);

	ImageBlit(&expanded,
			  image,
			  border, border,
			  0, 0,
			  image.width, image.height);

	for (int y = 0; y < border; y++)
	{
		// Top
		ImageBlit(&expanded,
				  image,
				  border, y,
				  0, 0,
				  image.width, 1);

		// Bottom
		ImageBlit(&expanded,
				  image,
				  border, expanded.height-y-1,
				  0, image.height-1,
				  image.width, 1);
	}

	for (int x = 0; x < border; x++)
	{
		// Left
		ImageBlit(&expanded,
				  image,
				  x, border,
				  0, 0,
				  1, image.height);

		// Right
		ImageBlit(&expanded,
				  image,
				  expanded.width-x-1, border,
				  image.width-1, 0,
				  1, image.height);
	}

	// Top-left corner
	FillRectangle(&expanded, 0, 0, border, border, GetPixel(image, 0, 0));

	// Top-right corner
	FillRectangle(&expanded, expanded.width-border, 0, border, border, GetPixel(image, image.width-1, 0));

	// Bottom-left corner
	FillRectangle(&expanded, 0, expanded.height-border, border, border, GetPixel(image, 0, image.height-1));

	// Bottom-right corner
	FillRectangle(&expanded, expanded.width-border, expanded.height-border, border, border, GetPixel(image, image.width-1, image.height-1));

	return expanded;
}

static void
ExpandBordersInPlace(Image *image, int border)
{
	Image expanded = ExpandBorders(*image, border);
	FreeImage(image);
	*image = expanded;
}

struct BakedAtlas
{
	Image image;
	int offset_x;
	int row_y;
	int next_row_y;
};

static BakedAtlas g_current_atlas;
static int g_current_atlas_index;

static void
PutIntoAtlas(Image image,
			 int *out_u, int *out_v)
{
	if (g_current_atlas.offset_x + image.width > g_current_atlas.image.width)
	{
		g_current_atlas.offset_x = 0;
		g_current_atlas.row_y = g_current_atlas.next_row_y;
	}

	if (g_current_atlas.row_y + image.height > g_current_atlas.image.height
		|| g_current_atlas.offset_x + image.width > g_current_atlas.image.width)
	{
		LogError("Not enough space in the atlas");
		exit(1);
	}

	ImageBlit(&g_current_atlas.image,
			  image,
			  g_current_atlas.offset_x, g_current_atlas.row_y,
			  0, 0,
			  image.width, image.height);
	*out_u = g_current_atlas.offset_x;
	*out_v = g_current_atlas.row_y;

	g_current_atlas.next_row_y = Max(g_current_atlas.next_row_y, g_current_atlas.row_y + image.height);

	g_current_atlas.offset_x += image.width;
}

static FILE *g_asset_info_cpp_file;
static FILE *g_asset_info_h_file;

static RawSpriteInfo *g_saved_sprite_infos;
static int g_saved_sprite_infos_count;

static void
PutSpriteIntoAtlas(RawSpriteInfo *info)
{
	LogInfo("Baking sprite %s...", info->name);

	Image texture = g_raw_textures[info->textureIndex];

	fprintf(g_asset_info_cpp_file, "static SpriteFrame g_frames_for_%s[] = {\n", info->name);

	if (info->numFrames == 0)
	{
		info->numFrames = 1;
	}

	if (info->numFramesInRow == 0)
	{
		info->numFramesInRow = info->numFrames;
	}

	for (int frame_index = 0; frame_index < info->numFrames; frame_index++)
	{
		//LogInfo("Baking frame %d...", frame_index);

		int num_frames_in_row = info->numFramesInRow;

		int sprite_frame_u = info->u + info->width*(frame_index % num_frames_in_row);
		int sprite_frame_v = info->v + info->height*(frame_index / num_frames_in_row);

		Image frame = CopySubImage(texture,
								   sprite_frame_u, sprite_frame_v,
								   info->width, info->height);

		int offset_x;
		int offset_y;
		TrimImageInPlace(&frame, &offset_x, &offset_y);

		int border = 1;
		ExpandBordersInPlace(&frame, border);

		int frame_final_u, frame_final_v;
		PutIntoAtlas(frame, &frame_final_u, &frame_final_v);

		fprintf(g_asset_info_cpp_file, "    /* [%d] = */ {\n", frame_index);
		fprintf(g_asset_info_cpp_file, "        /* .u = */ %d,\n", frame_final_u + border);
		fprintf(g_asset_info_cpp_file, "        /* .v = */ %d,\n", frame_final_v + border);
		fprintf(g_asset_info_cpp_file, "        /* .width = */ %d,\n", frame.width - 2*border);
		fprintf(g_asset_info_cpp_file, "        /* .height = */ %d,\n", frame.height - 2*border);
		fprintf(g_asset_info_cpp_file, "        /* .xoffset = */ %d,\n", offset_x);
		fprintf(g_asset_info_cpp_file, "        /* .yoffset = */ %d,\n", offset_y);
		fprintf(g_asset_info_cpp_file, "    },\n");

		FreeImage(&frame);
	}

	fprintf(g_asset_info_cpp_file, "};\n\n");

	g_saved_sprite_infos = (RawSpriteInfo *)realloc(g_saved_sprite_infos, (g_saved_sprite_infos_count + 1) * sizeof(RawSpriteInfo));
	g_saved_sprite_infos[g_saved_sprite_infos_count] = *info;
	g_saved_sprite_infos[g_saved_sprite_infos_count].bakedAtlasIndex = g_current_atlas_index;
	g_saved_sprite_infos_count++;
}

static void
PutSpriteIntoAtlas(const char *name,
				   RawTextureIndex textureIndex,
				   int u,
				   int v,
				   int width,
				   int height,
				   int xorigin = 0,
				   int yorigin = 0,
				   int numFrames = 0,
				   int numFramesInRow = 0,
				   float animSpeed = 0,
				   int loopFrame = 0)
{
	RawSpriteInfo info = {};
	info.name = name;
	info.textureIndex = textureIndex;
	info.u = u;
	info.v = v;
	info.width = width;
	info.height = height;
	info.xorigin = xorigin;
	info.yorigin = yorigin;
	info.numFrames = numFrames;
	info.numFramesInRow = numFramesInRow;
	info.animSpeed = animSpeed;
	info.loopFrame = loopFrame;

	PutSpriteIntoAtlas(&info);
}

static void
WriteImagePNG(Image image, const char *filepath)
{
	stbi_write_png(filepath, image.width, image.height, 4, image.data, image.width*4);
}

static void
WriteImageQOI(Image image, const char *filepath)
{
	qoi_desc desc = {};
	desc.width = image.width;
	desc.height = image.height;
	desc.channels = 4;
	qoi_write(filepath, image.data, &desc, nullptr);
}

static FILE *
TryOpenFileForWriting(const char *filepath)
{
	FILE *file = fopen(filepath, "wb");
	if (!file)
	{
		LogError("Couldn't open file %s for writing\n", filepath);
		exit(1);
	}

	return file;
}

static char ASSET_INFO_H_PREAMBLE[] =
R"(struct TextureInfo
{
	const char *filePath;
	bool wantMipMap;
};

struct SpriteFrame
{
	u16 u;
	u16 v;
	u16 width;
	u16 height;
	i16 xoffset;
	i16 yoffset;
};

struct SpriteInfo
{
	SpriteFrame *frames;
	TextureIndex textureIndex;
	int width;
	int height;
	int xorigin;
	int yorigin;
	int numFrames;
	float animSpeed;
	int loopFrame;
};

struct FontGlyph
{
	u16 u;
	u16 v;
	u16 width;
	u16 height;
	i16 xoffset;
	i16 yoffset;
	u16 xadvance;
};

struct FontInfo
{
	FontGlyph *glyphs;
	TextureIndex textureIndex;
	int numGlyphs;
	int lineHeight;
	int height;
};

extern TextureInfo g_textureInfo[TextureIndex_COUNT];
extern SpriteInfo g_spriteInfo[SpriteIndex_COUNT];
extern FontInfo g_fontInfo[FontIndex_COUNT];
)";

static void
BeginAtlas(int width, int height)
{
	g_current_atlas = {};

	g_current_atlas.image.width = width;
	g_current_atlas.image.height = height;
	g_current_atlas.image.data = calloc(width*height*4, 1);
}

static void
EndAtlas()
{
	char filepath[512];
	snprintf(filepath, sizeof(filepath), "assets_baked/generated_atlas_%d.qoi", g_current_atlas_index);

	WriteImageQOI(g_current_atlas.image, filepath);
	FreeImage(&g_current_atlas.image);
	g_current_atlas_index++;
}

static FT_Library g_freetype_lib;

static RawFontInfo *g_saved_font_infos;
static int g_saved_font_infos_count;

static void
PutFontIntoAtlas(RawFontInfo *info)
{
	LogInfo("Baking font %s...", info->name);

	fprintf(g_asset_info_cpp_file, "static FontGlyph g_glyphs_for_%s[] = {\n", info->name);

	FT_Face face;
	FT_New_Face(g_freetype_lib, info->filepath, 0, &face);

	FT_Set_Pixel_Sizes(face, 0, info->pixel_size);

	u32 charcode_start = 32;
	u32 charcode_end = 126;

	info->num_glyphs = charcode_end - charcode_start + 1;

	FT_Stroker stroker;
	FT_Stroker_New(g_freetype_lib, &stroker);
	FT_Stroker_Set(stroker, info->outlineSize*64, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);

	for (u32 charcode = charcode_start; charcode <= charcode_end; charcode++)
	{
		int border = 1;
		int glyph_final_u = -border;
		int glyph_final_v = -border;
		int glyph_final_w = 2*border;
		int glyph_final_h = 2*border;
		int glyph_offset_x = 0;
		int glyph_offset_y = 0;
		int glyph_xadvance = 0;

		u32 glyph_index = FT_Get_Char_Index(face, charcode);
		FT_Load_Glyph(face, glyph_index, FT_LOAD_TARGET_LIGHT);

#if 1
		FT_Render_Mode render_mode = FT_RENDER_MODE_NORMAL;
#else
		FT_Render_Mode render_mode = FT_RENDER_MODE_LCD;
#endif

		FT_Glyph glyph;
		FT_Get_Glyph(face->glyph, &glyph);

		if (info->outlineSize > 0)
		{
			FT_Glyph_Stroke(&glyph, stroker, true);
		}

		FT_Glyph_To_Bitmap(&glyph, render_mode, nullptr, true);

		FT_BitmapGlyph bitmap_glyph = (FT_BitmapGlyph)glyph;
		FT_Bitmap *bitmap = &bitmap_glyph->bitmap;

		int bytes_per_pixel = 0;
		if (bitmap->pixel_mode == FT_PIXEL_MODE_GRAY)
		{
			bytes_per_pixel = 1;
		}
		else if (bitmap->pixel_mode == FT_PIXEL_MODE_LCD)
		{
			bytes_per_pixel = 3;
		}
		else
		{
			Assert(false);
		}

		glyph_xadvance = glyph->advance.x >> 16;

		int logical_width = bitmap->width;
		int logical_height = bitmap->rows;
		if (bitmap->pixel_mode == FT_PIXEL_MODE_LCD)
		{
			logical_width /= 3;
			// height is not affected
		}

		if (logical_width > 0 && logical_height > 0)
		{
			Image image = {};
			image.width = logical_width;
			image.height = logical_height;
			image.data = calloc(image.width*image.height*4, 1);

			for (int y = 0; y < image.height; y++)
			{
				for (int x = 0; x < image.width; x++)
				{
					u32 *dest = (u32 *)((u8 *)image.data
										+ y*image.width*4
										+ x*4);
					u8 *src = ((u8 *)bitmap->buffer
							   + y*bitmap->pitch
							   + x*bytes_per_pixel);

					u32 r;
					u32 g;
					u32 b;
					u32 a;
					if (bitmap->pixel_mode == FT_PIXEL_MODE_LCD)
					{
						r = *src++;
						g = *src++;
						b = *src++;
						a = 0xff;
					}
					else
					{
						r = 0xff;
						g = 0xff;
						b = 0xff;
						a = *src++;
					}

					u32 color = ((r << 0)
								 | (g << 8)
								 | (b << 16)
								 | (a << 24));
					*dest = color;
				}
			}

			ExpandBordersInPlace(&image, 1);

			PutIntoAtlas(image, &glyph_final_u, &glyph_final_v);
			glyph_final_w = image.width;
			glyph_final_h = image.height;
			glyph_offset_x = bitmap_glyph->left;
			glyph_offset_y = (face->size->metrics.ascender - face->size->metrics.descender)/64 - bitmap_glyph->top;

			info->line_height = face->size->metrics.height/64;

			info->height = (face->size->metrics.ascender - face->size->metrics.descender)/64;

			FreeImage(&image);
		}

		fprintf(g_asset_info_cpp_file, "    /* [%u] = */ {      /* '%c' */\n", charcode - charcode_start, (char)charcode);
		fprintf(g_asset_info_cpp_file, "        /* .u = */ %d,\n", glyph_final_u + border);
		fprintf(g_asset_info_cpp_file, "        /* .v = */ %d,\n", glyph_final_v + border);
		fprintf(g_asset_info_cpp_file, "        /* .width = */ %d,\n", glyph_final_w - 2*border);
		fprintf(g_asset_info_cpp_file, "        /* .height = */ %d,\n", glyph_final_h - 2*border);
		fprintf(g_asset_info_cpp_file, "        /* .xoffset = */ %d,\n", glyph_offset_x);
		fprintf(g_asset_info_cpp_file, "        /* .yoffset = */ %d,\n", glyph_offset_y);
		fprintf(g_asset_info_cpp_file, "        /* .xadvance = */ %d,\n", glyph_xadvance);
		fprintf(g_asset_info_cpp_file, "    },\n");
	}

	fprintf(g_asset_info_cpp_file, "};\n\n");

	g_saved_font_infos = (RawFontInfo *)realloc(g_saved_font_infos, (g_saved_font_infos_count + 1) * sizeof(RawFontInfo));
	g_saved_font_infos[g_saved_font_infos_count] = *info;
	g_saved_font_infos[g_saved_font_infos_count].baked_atlas_index = g_current_atlas_index;
	g_saved_font_infos_count++;
}

static void
PutFontIntoAtlas(const char *name,
				 const char *filepath,
				 int pixelSize,
				 int outlineSize = 0)
{
	RawFontInfo info = {};
	info.name = name;
	info.filepath = filepath;
	info.pixel_size = pixelSize;
	info.outlineSize = outlineSize;

	PutFontIntoAtlas(&info);
}

static void
GenerateSineTable(void)
{
	FILE *file = fopen("src/sine_table.generated.cpp", "wb");

	fprintf(file, "#include \"common.h\"\n\n");

	fprintf(file, "f32 g_sineTable[256] = {\n");
	for (int i = 0; i < 256; i++)
	{
		f32 value = sinf(i/256.0f * (2.0f*Pi32));
		fprintf(file, "    %ff,\n", value);
	}
	fprintf(file, "};\n\n");

	fprintf(file, "f32 g_cosineTable[256] = {\n");
	for (int i = 0; i < 256; i++)
	{
		f32 value = cosf(i/256.0f * (2.0f*Pi32));
		fprintf(file, "    %ff,\n", value);
	}
	fprintf(file, "};\n\n");

	fclose(file);
}

static void
BeginAssetBaker()
{
	LogInfo("Starting...");

	{
		FT_Error error = FT_Init_FreeType(&g_freetype_lib);
		if (error != 0)
		{
			LogError("Could not initialize freetype");
			exit(1);
		}
	}

	for (int i = 0; i < ArrayLength(g_raw_textures); i++)
	{
		g_raw_textures[i] = LoadImageFromFile(g_raw_texture_info[i].filepath);
	}

	g_asset_info_cpp_file = TryOpenFileForWriting("src/asset_info.generated.cpp");
	g_asset_info_h_file = TryOpenFileForWriting("src/asset_info.generated.h");

	{
		fprintf(g_asset_info_cpp_file, "#include \"asset_info.generated.h\"\n\n");

		fprintf(g_asset_info_h_file, "#pragma once\n\n");
		fprintf(g_asset_info_h_file, "#include \"common.h\"\n\n");
	}
}

static void
EndAssetBaker()
{
	{
		fprintf(g_asset_info_cpp_file, "SpriteInfo g_spriteInfo[SpriteIndex_COUNT] = {\n");
		for (int sprite_index = 0; sprite_index < g_saved_sprite_infos_count; sprite_index++)
		{
			RawSpriteInfo *info = &g_saved_sprite_infos[sprite_index];
			fprintf(g_asset_info_cpp_file, "    /* [%s] = */ {\n", info->name);
			fprintf(g_asset_info_cpp_file, "        /* .frames = */ g_frames_for_%s,\n", info->name);
			fprintf(g_asset_info_cpp_file, "        /* .textureIndex = */ tex_generated_atlas_%d,\n", info->bakedAtlasIndex);
			fprintf(g_asset_info_cpp_file, "        /* .width = */ %d,\n", info->width);
			fprintf(g_asset_info_cpp_file, "        /* .height = */ %d,\n", info->height);
			fprintf(g_asset_info_cpp_file, "        /* .xorigin = */ %d,\n", info->xorigin);
			fprintf(g_asset_info_cpp_file, "        /* .yorigin = */ %d,\n", info->yorigin);
			fprintf(g_asset_info_cpp_file, "        /* .numFrames = */ %d,\n", info->numFrames);
			fprintf(g_asset_info_cpp_file, "        /* .animSpeed = */ %ff,\n", info->animSpeed);
			fprintf(g_asset_info_cpp_file, "        /* .loopFrame = */ %d,\n", info->loopFrame);
			fprintf(g_asset_info_cpp_file, "    },\n");
		}
		fprintf(g_asset_info_cpp_file, "};\n\n");
	}

	{
		fprintf(g_asset_info_cpp_file, "FontInfo g_fontInfo[FontIndex_COUNT] = {\n");
		for (int font_index = 0; font_index < g_saved_font_infos_count; font_index++)
		{
			RawFontInfo *info = &g_saved_font_infos[font_index];
			fprintf(g_asset_info_cpp_file, "    /* [%s] = */ {\n", info->name);
			fprintf(g_asset_info_cpp_file, "        /* .glyphs = */ g_glyphs_for_%s,\n", info->name);
			fprintf(g_asset_info_cpp_file, "        /* .textureIndex = */ tex_generated_atlas_%d,\n", info->baked_atlas_index);
			fprintf(g_asset_info_cpp_file, "        /* .numGlyphs = */ %d,\n", info->num_glyphs);
			fprintf(g_asset_info_cpp_file, "        /* .lineHeight = */ %d,\n", info->line_height);
			fprintf(g_asset_info_cpp_file, "        /* .height = */ %d,\n", info->height);
			fprintf(g_asset_info_cpp_file, "    },\n");
		}
		fprintf(g_asset_info_cpp_file, "};\n\n");
	}

	{
		fprintf(g_asset_info_cpp_file, "TextureInfo g_textureInfo[TextureIndex_COUNT] = {\n");
		for (int i = 0; i < RawTextureIndex_COUNT; i++)
		{
			if (g_raw_texture_info[i].includeInGame)
			{
				fprintf(g_asset_info_cpp_file, "    /* [%s] = */ {\n", g_raw_texture_info[i].name);
				fprintf(g_asset_info_cpp_file, "        /* .filePath = */ \"%s.qoi\",\n", g_raw_texture_info[i].name);
				fprintf(g_asset_info_cpp_file, "        /* .wantMipMap = */ %d,\n", (int)g_raw_texture_info[i].wantMipMap);
				fprintf(g_asset_info_cpp_file, "    },\n");
			}
		}
		for (int i = 0; i < g_current_atlas_index; i++)
		{
			fprintf(g_asset_info_cpp_file, "    /* [tex_generated_atlas_%d] = */ {\n", i);
			fprintf(g_asset_info_cpp_file, "        /* .filepath = */ \"generated_atlas_%d.qoi\",\n", i);
			fprintf(g_asset_info_cpp_file, "    },\n");
		}
		fprintf(g_asset_info_cpp_file, "};\n\n");
	}

	{
		fprintf(g_asset_info_h_file, "enum TextureIndex : u32\n");
		fprintf(g_asset_info_h_file, "{\n");
		for (int i = 0; i < RawTextureIndex_COUNT; i++)
		{
			if (g_raw_texture_info[i].includeInGame)
			{
				fprintf(g_asset_info_h_file, "    %s,\n", g_raw_texture_info[i].name);
			}
		}
		for (int i = 0; i < g_current_atlas_index; i++)
		{
			fprintf(g_asset_info_h_file, "    tex_generated_atlas_%d,\n", i);
		}
		fprintf(g_asset_info_h_file, "\n");
		fprintf(g_asset_info_h_file, "    TextureIndex_COUNT,\n");
		fprintf(g_asset_info_h_file, "};\n\n");
	}

	{
		fprintf(g_asset_info_h_file, "enum SpriteIndex : u32\n");
		fprintf(g_asset_info_h_file, "{\n");

		for (int sprite_index = 0; sprite_index < g_saved_sprite_infos_count; sprite_index++)
		{
			RawSpriteInfo *info = &g_saved_sprite_infos[sprite_index];
			fprintf(g_asset_info_h_file, "    %s,\n", info->name);
		}

		fprintf(g_asset_info_h_file, "\n");
		fprintf(g_asset_info_h_file, "    SpriteIndex_COUNT,\n");
		fprintf(g_asset_info_h_file, "};\n\n");
	}

	{
		fprintf(g_asset_info_h_file, "enum FontIndex : u32\n");
		fprintf(g_asset_info_h_file, "{\n");

		for (int font_index = 0; font_index < g_saved_font_infos_count; font_index++)
		{
			RawFontInfo *info = &g_saved_font_infos[font_index];
			fprintf(g_asset_info_h_file, "    %s,\n", info->name);
		}

		fprintf(g_asset_info_h_file, "\n");
		fprintf(g_asset_info_h_file, "    FontIndex_COUNT,\n");
		fprintf(g_asset_info_h_file, "};\n\n");
	}

	fprintf(g_asset_info_h_file, "%s", ASSET_INFO_H_PREAMBLE);

	fclose(g_asset_info_cpp_file);
	fclose(g_asset_info_h_file);

	for (int i = 0; i < RawTextureIndex_COUNT; i++)
	{
		if (g_raw_texture_info[i].includeInGame)
		{
			char filePath[256];
			snprintf(filePath, sizeof(filePath), "assets_baked/%s.qoi", g_raw_texture_info[i].name);
			WriteImageQOI(g_raw_textures[i], filePath);
		}
	}

	LogInfo("Finished");
}

static void
FillAssetInfo()
{
	g_raw_texture_info[raw_tex_white].filepath = "white.png";
	g_raw_texture_info[raw_tex_white].includeInGame = true;
	g_raw_texture_info[raw_tex_white].name = "tex_white";

	g_raw_texture_info[raw_tex_characters].filepath = "characters.png";
	g_raw_texture_info[raw_tex_projectiles].filepath = "projectiles.png";
	g_raw_texture_info[raw_tex_background].filepath = "background.png";
	g_raw_texture_info[raw_tex_background_2].filepath = "background_2.png";
	g_raw_texture_info[raw_tex_background_3].filepath = "background_3.png";
	g_raw_texture_info[raw_tex_enemies].filepath = "enemies.png";

	g_raw_texture_info[raw_tex_gfw_misty_lake].filepath = "gfw_misty_lake.png";
	g_raw_texture_info[raw_tex_gfw_misty_lake].includeInGame = true;
	g_raw_texture_info[raw_tex_gfw_misty_lake].name = "tex_gfw_misty_lake";
	g_raw_texture_info[raw_tex_gfw_misty_lake].wantMipMap = true;

	g_raw_texture_info[raw_tex_gfw_misty_lake2].filepath = "gfw_misty_lake2.png";
	g_raw_texture_info[raw_tex_gfw_misty_lake2].includeInGame = true;
	g_raw_texture_info[raw_tex_gfw_misty_lake2].name = "tex_gfw_misty_lake2";
	g_raw_texture_info[raw_tex_gfw_misty_lake2].wantMipMap = true;

	g_raw_texture_info[raw_tex_gfw_misty_lake3].filepath = "gfw_misty_lake3.png";
	g_raw_texture_info[raw_tex_gfw_misty_lake3].includeInGame = true;
	g_raw_texture_info[raw_tex_gfw_misty_lake3].name = "tex_gfw_misty_lake3";
	g_raw_texture_info[raw_tex_gfw_misty_lake3].wantMipMap = true;

	g_raw_texture_info[raw_tex_gfw_misty_lake4].filepath = "gfw_misty_lake4.png";
	g_raw_texture_info[raw_tex_gfw_misty_lake4].includeInGame = true;
	g_raw_texture_info[raw_tex_gfw_misty_lake4].name = "tex_gfw_misty_lake4";
	g_raw_texture_info[raw_tex_gfw_misty_lake4].wantMipMap = true;

	// New textures should also be added to CMake. Sigh...
}

int
main()
{
	clock_t startClock = clock();

	FillAssetInfo();

	BeginAssetBaker();

	/*BeginAtlas(784, 1032);
	{
		PutSpriteIntoAtlas("spr_background_left",   raw_tex_background,   0,   0,  64, 960);
		PutSpriteIntoAtlas("spr_background_right",  raw_tex_background, 832,   0, 448, 960);
		PutSpriteIntoAtlas("spr_background_top",    raw_tex_background,  64,   0, 768,  32);
		PutSpriteIntoAtlas("spr_background_bottom", raw_tex_background,  64, 928, 768,  32);
	}
	EndAtlas();*/

	/*BeginAtlas(784, 1032);
	{
		PutSpriteIntoAtlas("spr_background_2_left",   raw_tex_background_2,   0,   0,  64, 960);
		PutSpriteIntoAtlas("spr_background_2_right",  raw_tex_background_2, 832,   0, 448, 960);
		PutSpriteIntoAtlas("spr_background_2_top",    raw_tex_background_2,  64,   0, 768,  32);
		PutSpriteIntoAtlas("spr_background_2_bottom", raw_tex_background_2,  64, 928, 768,  32);
	}
	EndAtlas();*/

	BeginAtlas(784, 1032);
	{
		PutSpriteIntoAtlas("spr_background_3_left",   raw_tex_background_3,   0,   0,  64, 960);
		PutSpriteIntoAtlas("spr_background_3_right",  raw_tex_background_3, 832,   0, 448, 960);
		PutSpriteIntoAtlas("spr_background_3_top",    raw_tex_background_3,  64,   0, 768,  32);
		PutSpriteIntoAtlas("spr_background_3_bottom", raw_tex_background_3,  64, 928, 768,  32);
	}
	EndAtlas();

	BeginAtlas(1024, 1024);
	{
		PutSpriteIntoAtlas("spr_reimu_idle",            raw_tex_characters,  0,   0, 32, 48, 16, 24, 8, 8, 0.20f);
		PutSpriteIntoAtlas("spr_reimu_left",            raw_tex_characters,  0,  48, 32, 48, 16, 24, 8, 8, 0.20f, 4);
		PutSpriteIntoAtlas("spr_reimu_right",           raw_tex_characters,  0,  96, 32, 48, 16, 24, 8, 8, 0.20f, 4);
		PutSpriteIntoAtlas("spr_hitbox",                raw_tex_characters,  0, 144, 64, 64, 32, 32);
		PutSpriteIntoAtlas("spr_reimu_card",            raw_tex_characters, 64, 144, 64, 16, 56,  8);
		PutSpriteIntoAtlas("spr_reimu_card_afterimage", raw_tex_characters, 64, 160, 16, 16,  8,  8, 4, 4, 0.25f);

		PutSpriteIntoAtlas("spr_bullet_arrow",   raw_tex_projectiles, 0, 1*16, 16, 16, 8, 8, 16);
		PutSpriteIntoAtlas("spr_bullet_outline", raw_tex_projectiles, 0, 2*16, 16, 16, 8, 8, 16);
		PutSpriteIntoAtlas("spr_bullet_filled",  raw_tex_projectiles, 0, 3*16, 16, 16, 8, 8, 16);
		PutSpriteIntoAtlas("spr_bullet_rice",    raw_tex_projectiles, 0, 4*16, 16, 16, 8, 8, 16);
		PutSpriteIntoAtlas("spr_bullet_kunai",   raw_tex_projectiles, 0, 5*16, 16, 16, 8, 8, 16);
		PutSpriteIntoAtlas("spr_bullet_pellet",  raw_tex_projectiles, 0, 6*16, 16, 16, 8, 8, 16);
		PutSpriteIntoAtlas("spr_bullet_card",    raw_tex_projectiles, 0, 7*16, 16, 16, 8, 8, 16);
		PutSpriteIntoAtlas("spr_bullet_bullet",  raw_tex_projectiles, 0, 8*16, 16, 16, 8, 8, 16);

		PutSpriteIntoAtlas("spr_bullet_appear",  raw_tex_projectiles, 0, 144, 32, 32, 16, 16, 8);

		PutSpriteIntoAtlas("spr_enemy_death_particle", raw_tex_projectiles,   0, 176, 64, 64, 32, 32);

		PutSpriteIntoAtlas("spr_pickup", raw_tex_projectiles,   0, 240, 16, 16, 8, 8, 16);

		PutSpriteIntoAtlas("spr_enemy_fairy_blue_idle",    raw_tex_enemies,   0,  0, 32, 32, 16, 16, 4, 4, 0.20f);
		PutSpriteIntoAtlas("spr_enemy_fairy_blue_right",   raw_tex_enemies, 128,  0, 32, 32, 16, 16, 8, 8, 0.20f, 4);
		PutSpriteIntoAtlas("spr_enemy_fairy_red_idle",     raw_tex_enemies,   0, 32, 32, 32, 16, 16, 4, 4, 0.20f);
		PutSpriteIntoAtlas("spr_enemy_fairy_red_right",    raw_tex_enemies, 128, 32, 32, 32, 16, 16, 8, 8, 0.20f, 4);
		PutSpriteIntoAtlas("spr_enemy_fairy_green_idle",   raw_tex_enemies,   0, 64, 32, 32, 16, 16, 4, 4, 0.20f);
		PutSpriteIntoAtlas("spr_enemy_fairy_green_right",  raw_tex_enemies, 128, 64, 32, 32, 16, 16, 8, 8, 0.20f, 4);
		PutSpriteIntoAtlas("spr_enemy_fairy_yellow_idle",  raw_tex_enemies,   0, 96, 32, 32, 16, 16, 4, 4, 0.20f);
		PutSpriteIntoAtlas("spr_enemy_fairy_yellow_right", raw_tex_enemies, 128, 96, 32, 32, 16, 16, 8, 8, 0.20f, 4);
	}
	EndAtlas();

	BeginAtlas(1024, 1024);
	{
		PutFontIntoAtlas("fnt_revue",               "assets_raw/fonts/reve.ttf",                40);
		PutFontIntoAtlas("fnt_revue_outline",       "assets_raw/fonts/reve.ttf",                40, 3);
		PutFontIntoAtlas("fnt_consolas",            "assets_raw/fonts/consola.ttf",             18);
		PutFontIntoAtlas("fnt_ms_gothic",           "assets_raw/fonts/msgothic.ttc",            24);
		PutFontIntoAtlas("fnt_ms_gothic_outline",   "assets_raw/fonts/msgothic.ttc",            24, 2);
		PutFontIntoAtlas("fnt_caveat_brush",        "assets_raw/fonts/CaveatBrush-Regular.ttf", 64);
	}
	EndAtlas();

	EndAssetBaker();

	GenerateSineTable();

	clock_t endClock = clock();
	f64 secondsTook = (endClock - startClock)/(f64)CLOCKS_PER_SEC;
	LogInfo("Took %f milliseconds", 1000.0f*secondsTook);

	return 0;
}
