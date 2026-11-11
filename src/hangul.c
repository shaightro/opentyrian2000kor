/*
 * OpenTyrian: A modern cross-platform port of Tyrian
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "hangul.h"

#include "file.h"
#include "opentyr.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

/*
 * Pixel fonts are rasterized at their design size, where their outlines fall
 * exactly on the pixel grid, and each font pixel is shown as 'scale' game
 * pixels.  'baseline' is the distance in font pixels from the top of the
 * game's character box to the font's baseline, chosen so that the top of a
 * Hangul syllable lines up with the top of the game's capital letters.
 */
typedef struct
{
	const char *file;
	int em_px;
	float scale;
	int baseline;
} FontSpec;

/*
 * Galmuri (https://github.com/quiple/galmuri, SIL Open Font License 1.1),
 * chosen to match the game's fonts: Galmuri11 Bold has the 2-pixel strokes of
 * the normal and large fonts and is shown at 1 and 1.5 game pixels per font
 * pixel; the thinner Galmuri9 matches the 1-pixel strokes of the small font
 * and is shown at 2/3 of a game pixel per font pixel, so that a syllable is as
 * tall as the small font's capital letters.
 */
static const FontSpec font_specs[HANGUL_FONT_CLASSES] =  // large, normal, small
{
	{ "Galmuri11-Bold.ttf", 12, 1.5f, 11 },
	{ "Galmuri11-Bold.ttf", 12, 1.0f, 11 },
	{ "Galmuri9.ttf", 10, 2.0f / 3.0f, 9 },
};

typedef struct
{
	char file[64];
	unsigned char *data;
	stbtt_fontinfo info;
} LoadedFont;

static LoadedFont loaded_fonts[HANGUL_FONT_CLASSES];
static int loaded_font_count = 0;

static bool initialized = false;
static LoadedFont *class_fonts[HANGUL_FONT_CLASSES];

// Glyph cache: pages of 256 code points, allocated on demand.
#define PAGE_COUNT (0x110000 >> 8)
static const HangulGlyph **glyph_pages[HANGUL_FONT_CLASSES][PAGE_COUNT];
static const HangulGlyph no_glyph = { 0 };  // marks code points the font lacks

static unsigned int font_class(unsigned int font)
{
	return font < HANGUL_FONT_CLASSES ? font : 2;
}

static LoadedFont *load_font(const char *dir, const char *file)
{
	for (int i = 0; i < loaded_font_count; ++i)
		if (strcmp(loaded_fonts[i].file, file) == 0)
			return &loaded_fonts[i];

	if (loaded_font_count == HANGUL_FONT_CLASSES)
		return NULL;

	FILE *f = dir_fopen(dir, file, "rb");
	if (f == NULL)
	{
		fprintf(stderr, "warning: failed to open font '%s'\n", file);
		return NULL;
	}

	long size = ftell_eof(f);
	unsigned char *data = malloc(size);
	if (data == NULL || fread(data, 1, size, f) != (size_t)size)
	{
		free(data);
		fclose(f);
		return NULL;
	}
	fclose(f);

	LoadedFont *font = &loaded_fonts[loaded_font_count];
	if (!stbtt_InitFont(&font->info, data, stbtt_GetFontOffsetForIndex(data, 0)))
	{
		fprintf(stderr, "warning: failed to read font '%s'\n", file);
		free(data);
		return NULL;
	}

	snprintf(font->file, sizeof(font->file), "%s", file);
	font->data = data;
	++loaded_font_count;

	return font;
}

bool hangul_init(void)
{
	char dir[1024];
	snprintf(dir, sizeof(dir), "%s/kor", base_dir());

	bool ok = true;
	for (unsigned int i = 0; i < HANGUL_FONT_CLASSES; ++i)
	{
		class_fonts[i] = load_font(dir, font_specs[i].file);
		if (class_fonts[i] == NULL)
			ok = false;
	}
	initialized = true;

	return ok;
}

void hangul_deinit(void)
{
	for (unsigned int c = 0; c < HANGUL_FONT_CLASSES; ++c)
	{
		for (int p = 0; p < PAGE_COUNT; ++p)
		{
			if (glyph_pages[c][p] == NULL)
				continue;
			for (int i = 0; i < 256; ++i)
			{
				const HangulGlyph *glyph = glyph_pages[c][p][i];
				if (glyph != NULL && glyph != &no_glyph)
				{
					free(glyph->mask);
					free(glyph->cov);
					free((HangulGlyph *)glyph);
				}
			}
			free((void *)glyph_pages[c][p]);
			glyph_pages[c][p] = NULL;
		}
	}

	for (int i = 0; i < loaded_font_count; ++i)
		free(loaded_fonts[i].data);
	loaded_font_count = 0;
}

float hangul_font_scale(unsigned int font)
{
	return font_specs[font_class(font)].scale;
}

int hangul_body_value(unsigned int font)
{
	// Measured from the game's fonts: the small font is drawn with value 7,
	// the normal and large fonts with 13.
	return font_class(font) == 2 ? 7 : 13;
}

bool hangul_has_highlight(unsigned int font)
{
	return font_class(font) != 2;
}

static HangulGlyph *rasterize(unsigned int font_cls, Uint32 codepoint)
{
	LoadedFont *font = class_fonts[font_cls];
	const FontSpec *spec = &font_specs[font_cls];

	if (font == NULL || stbtt_FindGlyphIndex(&font->info, (int)codepoint) == 0)
		return NULL;

	const float em_scale = stbtt_ScaleForMappingEmToPixels(&font->info, (float)spec->em_px);

	int w, h, xoff, yoff;
	unsigned char *bitmap = stbtt_GetCodepointBitmap(&font->info, em_scale, em_scale, (int)codepoint, &w, &h, &xoff, &yoff);

	int advance, lsb;
	stbtt_GetCodepointHMetrics(&font->info, (int)codepoint, &advance, &lsb);

	HangulGlyph *glyph = calloc(1, sizeof(*glyph));
	glyph->w = w;
	glyph->h = h;
	glyph->x = xoff;
	glyph->y = spec->baseline + yoff;
	glyph->mask = calloc(w * h > 0 ? w * h : 1, 1);
	for (int i = 0; i < w * h; ++i)
		glyph->mask[i] = bitmap[i] >= 128;
	stbtt_FreeBitmap(bitmap, NULL);

	const float s = spec->scale;
	const float eps = 0.001f;
	glyph->advance = (int)ceilf(advance * em_scale * s - eps);

	// Game cells touched by each font pixel.
	glyph->cov_x = (int)floorf(glyph->x * s + eps);
	glyph->cov_y = (int)floorf(glyph->y * s + eps);
	glyph->cov_w = MAX(1, (int)ceilf((glyph->x + w) * s - eps) - glyph->cov_x);
	glyph->cov_h = MAX(1, (int)ceilf((glyph->y + h) * s - eps) - glyph->cov_y);
	glyph->cov = calloc(glyph->cov_w * glyph->cov_h, 1);
	for (int my = 0; my < h; ++my)
	{
		for (int mx = 0; mx < w; ++mx)
		{
			if (!glyph->mask[my * w + mx])
				continue;

			const int x0 = (int)floorf((glyph->x + mx) * s + eps) - glyph->cov_x,
			          x1 = (int)ceilf((glyph->x + mx + 1) * s - eps) - glyph->cov_x,
			          y0 = (int)floorf((glyph->y + my) * s + eps) - glyph->cov_y,
			          y1 = (int)ceilf((glyph->y + my + 1) * s - eps) - glyph->cov_y;
			for (int cy = MAX(0, y0); cy < MIN(y1, glyph->cov_h); ++cy)
				for (int cx = MAX(0, x0); cx < MIN(x1, glyph->cov_w); ++cx)
					glyph->cov[cy * glyph->cov_w + cx] = 1;
		}
	}

	return glyph;
}

const HangulGlyph *hangul_glyph(unsigned int font, Uint32 codepoint)
{
	if (!initialized || codepoint >= 0x110000)
		return NULL;

	const unsigned int cls = font_class(font);
	const HangulGlyph **page = glyph_pages[cls][codepoint >> 8];
	if (page == NULL)
	{
		page = calloc(256, sizeof(*page));
		if (page == NULL)
			return NULL;
		glyph_pages[cls][codepoint >> 8] = page;
	}

	const HangulGlyph **slot = &page[codepoint & 0xff];
	if (*slot == NULL)
	{
		const HangulGlyph *glyph = rasterize(cls, codepoint);
		*slot = glyph != NULL ? glyph : &no_glyph;
	}

	return *slot != &no_glyph ? *slot : NULL;
}

int text_decode_char(const char *s, Uint32 *codepoint)
{
	const unsigned char *u = (const unsigned char *)s;

	if (u[0] == '\0')
	{
		*codepoint = 0;
		return 0;
	}

	int len;
	Uint32 c;
	if (u[0] < 0x80)
		return *codepoint = u[0], 1;
	else if ((u[0] & 0xe0) == 0xc0)
		len = 2, c = u[0] & 0x1f;
	else if ((u[0] & 0xf0) == 0xe0)
		len = 3, c = u[0] & 0x0f;
	else if ((u[0] & 0xf8) == 0xf0)
		len = 4, c = u[0] & 0x07;
	else
		return *codepoint = u[0], 1;  // legacy single-byte character

	for (int i = 1; i < len; ++i)
	{
		if ((u[i] & 0xc0) != 0x80)
			return *codepoint = u[0], 1;
		c = (c << 6) | (u[i] & 0x3f);
	}

	static const Uint32 min_value[5] = { 0, 0, 0x80, 0x800, 0x10000 };
	if (c < min_value[len] || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
		return *codepoint = u[0], 1;

	*codepoint = c;
	return len;
}
