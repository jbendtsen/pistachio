#include <ft2build.h>
#include FT_FREETYPE_H
#include <fontconfig/fontconfig.h>

#include "pistachio.h"

#define FLOAT_FROM_16_16(n) ((float)((n) >> 16) + (float)((n) & 0xffff) / 65536.0)

#define POOL_SIZE 1024 * 1024
#define MAX_POOLS 256

#define GAP_FACTOR  0.3
#define SLANT       0.25

static u8 *arena[MAX_POOLS] = {0};
static int pool = 0;
static int head = 0;

static u8 *allocate(int size) {
	if (size < 1 || size > POOL_SIZE)
		return NULL;

	if (head+size > POOL_SIZE) {
		pool++;
		head = 0;
	}
	if (pool >= MAX_POOLS)
		return NULL;

	if (!arena[pool])
		arena[pool] = malloc(POOL_SIZE);

	u8 *ptr = &arena[pool][head];
	head += size;
	return ptr;
}

void destroy_font_arena() {
	for (int i = 0; i < MAX_POOLS; i++) {
		if (arena[i]) {
			free(arena[i]);
			arena[i] = NULL;
		}
	}
}

char *get_font_path(char *name) {
	FcConfig *config = FcInitLoadConfigAndFonts();

	FcPattern *pattern = FcNameParse((const FcChar8*)name);
	FcConfigSubstitute(config, pattern, FcMatchPattern);
	FcDefaultSubstitute(pattern);

	FcResult result;
	FcPattern *font = FcFontMatch(config, pattern, &result);

	char *path = NULL;
	if (font)
		FcPatternGetString(font, FC_FILE, 0, (FcChar8**)&path);

	FcPatternDestroy(pattern);
	FcConfigDestroy(config);
	return path;
}

int glyph_indexof(char c) {
	return (c < MIN_CHAR || c > MAX_CHAR) ? -1 : c - MIN_CHAR;
}

FT_Library library;
FT_Face face;

bool open_font(char *font_path) {
	if (FT_Init_FreeType(&library) != 0) {
		fprintf(stderr, "Could not initialise libfreetype\n");
		return false;
	}

	if (FT_New_Face(library, font_path, 0, &face) != 0) {
		fprintf(stderr, "Error loading font \"%s\"\n", font_path);
		return false;
	}

	atexit(destroy_font_arena);
	return true;
}

bool render_font(Screen_Info *info, float font_size, bool italics, u32 foreground, u32 background, Glyph *chars) {
	ARGB fore, back;
	make_argb(foreground, &fore);
	make_argb(background, &back);

	float gap = 0;
	FT_Matrix matrix;

	if (italics) {
		gap = SLANT * GAP_FACTOR * font_size;
		matrix = (FT_Matrix) { .xx = 0x10000, .xy = (int)(SLANT * 0x10000), .yx = 0, .yy = 0x10000 };
	}

	FT_Set_Char_Size(face, 0, font_size * 64, info->dpi_w, info->dpi_h);

	for (int c = MIN_CHAR; c <= MAX_CHAR; c++) {
		if (italics)
			FT_Set_Transform(face, &matrix, NULL);

		FT_Load_Char(face, c, FT_LOAD_RENDER);
		FT_Bitmap *bmp = &face->glyph->bitmap;

		Glyph *gl = &chars[c - MIN_CHAR];
		*gl = (Glyph) {
			.img_w = bmp->width,
			.img_h = bmp->rows,
			.pitch = bmp->width * 4,
			.box_w = gap + FLOAT_FROM_16_16(face->glyph->linearHoriAdvance),
			.box_h = FLOAT_FROM_16_16(face->glyph->linearVertAdvance),
			.left  = face->glyph->bitmap_left,
			.top   = face->glyph->bitmap_top
		};

		int size = gl->pitch * gl->img_h;
		if (!size)
			continue;

		gl->data = allocate(size);
		if (!gl->data) {
			fprintf(stderr, "Failed to allocate glyph bitmap '%c' (pool:%d, head:%d)\n", c, pool, head);
			return false;
		}

		u32 *p = (u32*)gl->data;
		for (int i = 0; i < gl->img_w * gl->img_h; i++) {
			float lum = (float)bmp->buffer[i] / 255.0;
			p[i] =
				((u32)(back.a + (fore.a - back.a) * lum) << 24) |
				((u32)(back.r + (fore.r - back.r) * lum) << 16) |
				((u32)(back.g + (fore.g - back.g) * lum) << 8) |
				(u32)(back.b + (fore.b - back.b) * lum);
		}
	}

	return true;
}

void close_font() {
	FT_Done_Face(face);
	FT_Done_FreeType(library);
}
