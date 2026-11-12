/*
 * OpenTyrian: A modern cross-platform port of Tyrian
 * Copyright (C) 2007-2009  The OpenTyrian Development Team
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
#include "fonthand.h"

#include "hangul.h"
#include "network.h"
#include "nortsong.h"
#include "nortvars.h"
#include "opentyr.h"
#include "params.h"
#include "sprite.h"
#include "textlayer.h"
#include "vga256d.h"
#include "video.h"

/** Whether a font sprite has no visible pixels (the normal font has such placeholders, e.g. for parentheses). */
static bool sprite_is_blank(unsigned int font, int sprite_id)
{
	const Sprite *s = sprite(font, sprite_id);
	for (unsigned int i = 0; i < s->size; ++i)
	{
		if (s->data[i] == 255)
			++i;  // run of transparent pixels; next byte is its length
		else if (s->data[i] < 253)
			return false;
	}
	return true;
}

bool JE_nextTextChar(const char **s, unsigned int font, Uint32 *codepoint)
{
	const char *c = *s;
	const int len = text_decode_char(c, codepoint);
	*s += MAX(1, len);

	if (len > 1)
		return true;  // multi-byte (UTF-8): not in the sprite fonts

	// ASCII symbols missing from this sprite font (e.g. parentheses in the
	// normal font) are taken from the Korean fonts.
	const unsigned char ch = (unsigned char)*c;
	if (ch > ' ' && ch < 0x7f && ch != '~')
	{
		const int sprite_id = font_ascii[ch];
		if (sprite_id == -1 || !sprite_exists(font, sprite_id) || sprite_is_blank(font, sprite_id))
			return textlayer_glyph_advance(font, ch) >= 0;
	}
	return false;
}

bool JE_isWideChar(const char *s)
{
	// '~' only toggles highlighting and opening marks belong to the word that
	// follows, so look at the character after them (as the text loops do for
	// after_wide, see JE_keepsSpacing()).
	while (*s != '\0' && strchr("~([\"'", *s) != NULL)
		++s;
	return (unsigned char)*s >= 0x80;
}

bool JE_keepsSpacing(char c)
{
	// After a Korean word, closing punctuation (as in "...다. 3개") does not end
	// the narrow spacing of Korean text.
	return c != '\0' && strchr(" ~.,!?:;)]\"'", c) != NULL;
}

int JE_spaceWidth(unsigned int font, bool after_wide, bool before_wide)
{
	// A Korean syllable of the small font is about as wide as a normal
	// space, so spaces between Korean words are narrower.
	const bool small = font != FONT_SHAPES && font != SMALL_FONT_SHAPES;
	return small && (after_wide || before_wide) ? 3 : 6;
}

/**
 * Like JE_nextTextChar() for loops over s[i]: if the character at s[i] is to
 * be drawn by the text layer, advances i to its last byte and returns true.
 */
static bool next_layer_char(const char *s, int *i, unsigned int font, Uint32 *codepoint)
{
	const char *p = &s[*i];
	if (!JE_nextTextChar(&p, font, codepoint))
		return false;

	*i = (int)(p - s) - 1;
	return true;
}

const int font_ascii[256] =
{
	 -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
	 -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
	 -1,  26,  33,  60,  61,  62,  -1,  32,  64,  65,  63,  84,  29,  83,  28,  80, //  !"#$%&'()*+,-./
	 79,  70,  71,  72,  73,  74,  75,  76,  77,  78,  31,  30,  -1,  85,  -1,  27, // 0123456789:;<=>?
	 -1,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14, // @ABCDEFGHIJKLMNO
	 15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  68,  82,  69,  -1,  -1, // PQRSTUVWXYZ[\]^_
	 -1,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48, // `abcdefghijklmno
	 49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  66,  81,  67,  -1,  -1, // pqrstuvwxyz{|}~⌂

	 86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  96,  97,  98,  99, 100, 101, // ÇüéâäàåçêëèïîìÄÅ
	102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, // ÉæÆôöòûùÿÖÜ¢£¥₧ƒ
	118, 119, 120, 121, 122, 123, 124, 125, 126,  -1,  -1,  -1,  -1,  -1,  -1,  -1, // áíóúñÑªº¿
	 -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
	 -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
	 -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
	 -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
	 -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
};

/* shape constants included in newshape.h */

JE_byte textGlowFont, textGlowBrightness = 6;

JE_boolean levelWarningDisplay;
JE_byte levelWarningLines;
char levelWarningText[12][TEXT_SIZE(61)]; /* [1..10] of string [60] */
JE_boolean warningRed;

JE_byte warningSoundDelay;
JE_word armorShipDelay;
JE_byte warningCol;
JE_shortint warningColChange;

void JE_dString(SDL_Surface * screen, int x, int y, const char *s, unsigned int font)
{
	const int defaultBrightness = -3;

	int bright = 0;

	bool after_wide = false;

	for (int i = 0; s[i] != '\0'; ++i)
	{
		Uint32 codepoint;
		if (next_layer_char(s, &i, font, &codepoint))
		{
			textlayer_draw_glyph(screen, x + 2, y + 2, font, codepoint, TEXT_MODE_DARK, 0, 0);
			const int advance = textlayer_draw_glyph(screen, x, y, font, codepoint, TEXT_MODE_HV_UNSAFE, 0xf, defaultBrightness + bright);
			if (advance > 0)
				x += advance;
			after_wide = codepoint >= 0x80;
			continue;
		}
		if (!JE_keepsSpacing(s[i]))
			after_wide = false;

		int sprite_id = font_ascii[(unsigned char)s[i]];

		switch (s[i])
		{
		case ' ':
			x += JE_spaceWidth(font, after_wide, JE_isWideChar(&s[i + 1]));
			break;

		case '~':
			bright = (bright == 0) ? 2 : 0;
			break;

		default:
			if (sprite_id != -1)
			{
				blit_sprite_dark(screen, x + 2, y + 2, font, sprite_id, false);
				blit_sprite_hv_unsafe(screen, x, y, font, sprite_id, 0xf, defaultBrightness + bright);

				x += sprite(font, sprite_id)->width + 1;
			}
			break;
		}
	}
}

int JE_fontCenter(const char *s, unsigned int font)
{
	return 160 - (JE_textWidth(s, font) / 2);
}

int JE_textWidth(const char *s, unsigned int font)
{
	int x = 0;

	bool after_wide = false;

	for (int i = 0; s[i] != '\0'; ++i)
	{
		Uint32 codepoint;
		if (next_layer_char(s, &i, font, &codepoint))
		{
			const int advance = textlayer_glyph_advance(font, codepoint);
			if (advance > 0)
				x += advance;
			after_wide = codepoint >= 0x80;
			continue;
		}
		if (!JE_keepsSpacing(s[i]))
			after_wide = false;

		int sprite_id = font_ascii[(unsigned char)s[i]];

		if (s[i] == ' ')
			x += JE_spaceWidth(font, after_wide, JE_isWideChar(&s[i + 1]));
		else if (sprite_id != -1)
			x += sprite(font, sprite_id)->width + 1;
	}

	return x;
}

void JE_textShade(SDL_Surface * screen, int x, int y, const char *s, unsigned int colorbank, int brightness, unsigned int shadetype)
{
	switch (shadetype)
	{
		case PART_SHADE:
			JE_outText(screen, x+1, y+1, s, 0, -1);
			JE_outText(screen, x, y, s, colorbank, brightness);
			break;
		case FULL_SHADE:
			JE_outText(screen, x-1, y, s, 0, -1);
			JE_outText(screen, x+1, y, s, 0, -1);
			JE_outText(screen, x, y-1, s, 0, -1);
			JE_outText(screen, x, y+1, s, 0, -1);
			JE_outText(screen, x, y, s, colorbank, brightness);
			break;
		case DARKEN:
			JE_outTextAndDarken(screen, x+1, y+1, s, colorbank, brightness, TINY_FONT);
			break;
		case TRICK:
			JE_outTextModify(screen, x, y, s, colorbank, brightness, TINY_FONT);
			break;
	}
}

void JE_outText(SDL_Surface * screen, int x, int y, const char *s, unsigned int colorbank, int brightness)
{
	int bright = 0;

	bool after_wide = false;

	for (int i = 0; s[i] != '\0'; ++i)
	{
		Uint32 codepoint;
		if (next_layer_char(s, &i, TINY_FONT, &codepoint))
		{
			const int advance = brightness >= 0
				? textlayer_draw_glyph(screen, x, y, TINY_FONT, codepoint, TEXT_MODE_HV_UNSAFE, colorbank, brightness + bright)
				: textlayer_draw_glyph(screen, x, y, TINY_FONT, codepoint, TEXT_MODE_BLACK, 0, 0);
			if (advance > 0)
				x += advance;
			after_wide = codepoint >= 0x80;
			continue;
		}
		if (!JE_keepsSpacing(s[i]))
			after_wide = false;

		int sprite_id = font_ascii[(unsigned char)s[i]];

		switch (s[i])
		{
		case ' ':
			x += JE_spaceWidth(TINY_FONT, after_wide, JE_isWideChar(&s[i + 1]));
			break;

		case '~':
			bright = (bright == 0) ? 4 : 0;
			break;

		default:
			if (sprite_id != -1 && sprite_exists(TINY_FONT, sprite_id))
			{
				if (brightness >= 0)
					blit_sprite_hv_unsafe(screen, x, y, TINY_FONT, sprite_id, colorbank, brightness + bright);
				else
					blit_sprite_dark(screen, x, y, TINY_FONT, sprite_id, true);

				x += sprite(TINY_FONT, sprite_id)->width + 1;
			}
			break;
		}
	}
}

void JE_outTextModify(SDL_Surface * screen, int x, int y, const char *s, unsigned int filter, unsigned int brightness, unsigned int font)
{
	bool after_wide = false;

	for (int i = 0; s[i] != '\0'; ++i)
	{
		Uint32 codepoint;
		if (next_layer_char(s, &i, font, &codepoint))
		{
			const int advance = textlayer_draw_glyph(screen, x, y, font, codepoint, TEXT_MODE_BLEND, filter, brightness);
			if (advance > 0)
				x += advance;
			after_wide = codepoint >= 0x80;
			continue;
		}
		if (!JE_keepsSpacing(s[i]))
			after_wide = false;

		int sprite_id = font_ascii[(unsigned char)s[i]];

		if (s[i] == ' ')
		{
			x += JE_spaceWidth(font, after_wide, JE_isWideChar(&s[i + 1]));
		}
		else if (sprite_id != -1)
		{
			blit_sprite_hv_blend(screen, x, y, font, sprite_id, filter, brightness);

			x += sprite(font, sprite_id)->width + 1;
		}
	}
}

void JE_outTextAdjust(SDL_Surface * screen, int x, int y, const char *s, unsigned int filter, int brightness, unsigned int font, JE_boolean shadow)
{
	int bright = 0;

	bool after_wide = false;

	for (int i = 0; s[i] != '\0'; ++i)
	{
		Uint32 codepoint;
		if (next_layer_char(s, &i, font, &codepoint))
		{
			if (shadow)
				textlayer_draw_glyph(screen, x + 2, y + 2, font, codepoint, TEXT_MODE_DARK, 0, 0);
			const int advance = textlayer_draw_glyph(screen, x, y, font, codepoint, TEXT_MODE_HV, filter, brightness + bright);
			if (advance > 0)
				x += advance;
			after_wide = codepoint >= 0x80;
			continue;
		}
		if (!JE_keepsSpacing(s[i]))
			after_wide = false;

		int sprite_id = font_ascii[(unsigned char)s[i]];

		switch (s[i])
		{
		case ' ':
			x += JE_spaceWidth(font, after_wide, JE_isWideChar(&s[i + 1]));
			break;

		case '~':
			bright = (bright == 0) ? 4 : 0;
			break;

		default:
			if (sprite_id != -1 && sprite_exists(TINY_FONT, sprite_id))
			{
				if (shadow)
					blit_sprite_dark(screen, x + 2, y + 2, font, sprite_id, false);
				blit_sprite_hv(screen, x, y, font, sprite_id, filter, brightness + bright);

				x += sprite(font, sprite_id)->width + 1;
			}
			break;
		}
	}
}

void JE_outTextAndDarken(SDL_Surface * screen, int x, int y, const char *s, unsigned int colorbank, unsigned int brightness, unsigned int font)
{
	int bright = 0;

	bool after_wide = false;

	for (int i = 0; s[i] != '\0'; ++i)
	{
		Uint32 codepoint;
		if (next_layer_char(s, &i, font, &codepoint))
		{
			textlayer_draw_glyph(screen, x + 1, y + 1, font, codepoint, TEXT_MODE_DARK, 0, 0);
			const int advance = textlayer_draw_glyph(screen, x, y, font, codepoint, TEXT_MODE_HV_UNSAFE, colorbank, brightness + bright);
			if (advance > 0)
				x += advance;
			after_wide = codepoint >= 0x80;
			continue;
		}
		if (!JE_keepsSpacing(s[i]))
			after_wide = false;

		int sprite_id = font_ascii[(unsigned char)s[i]];

		switch (s[i])
		{
		case ' ':
			x += JE_spaceWidth(font, after_wide, JE_isWideChar(&s[i + 1]));
			break;

		case '~':
			bright = (bright == 0) ? 4 : 0;
			break;

		default:
			if (sprite_id != -1 && sprite_exists(TINY_FONT, sprite_id))
			{
				blit_sprite_dark(screen, x + 1, y + 1, font, sprite_id, false);
				blit_sprite_hv_unsafe(screen, x, y, font, sprite_id, colorbank, brightness + bright);

				x += sprite(font, sprite_id)->width + 1;
			}
			break;
		}
	}
}

void JE_updateWarning(SDL_Surface * screen)
{
	if (getDelayTicks2() == 0)
	{ /*Update Color Bars*/

		warningCol += warningColChange;
		if (warningCol > 14 * 16 + 10 || warningCol < 14 * 16 + 4)
		{
			warningColChange = -warningColChange;
		}
		fill_rectangle_xy(screen, 0, 0, 319, 5, warningCol);
		fill_rectangle_xy(screen, 0, 194, 319, 199, warningCol);
		JE_showVGA();

		setDelay2(6);

		if (warningSoundDelay > 0)
		{
			warningSoundDelay--;
		}
		else
		{
			warningSoundDelay = 14;
			JE_playSampleNum(S_WARNING);
		}
	}
}

void JE_outTextGlow(SDL_Surface * screen, int x, int y, const char *s)
{
	JE_integer z;
	JE_byte c = 15;

	if (warningRed)
	{
		c = 7;
	}

	JE_outTextAdjust(screen, x - 1, y,     s, 0, -12, textGlowFont, false);
	JE_outTextAdjust(screen, x,     y - 1, s, 0, -12, textGlowFont, false);
	JE_outTextAdjust(screen, x + 1, y,     s, 0, -12, textGlowFont, false);
	JE_outTextAdjust(screen, x,     y + 1, s, 0, -12, textGlowFont, false);
	if (frameCountMax > 0)
		for (z = 1; z <= 12; z++)
		{
			setDelay(frameCountMax);
			JE_outTextAdjust(screen, x, y, s, c, z - 10, textGlowFont, false);
			if (JE_anyButton())
			{
				frameCountMax = 0;
			}

			NETWORK_KEEP_ALIVE();

			JE_showVGA();

			wait_delay();
		}
	for (z = (frameCountMax == 0) ? 6 : 12; z >= textGlowBrightness; z--)
	{
		setDelay(frameCountMax);
		JE_outTextAdjust(screen, x, y, s, c, z - 10, textGlowFont, false);
		if (JE_anyButton())
		{
			frameCountMax = 0;
		}

		NETWORK_KEEP_ALIVE();

		JE_showVGA();

		wait_delay();
	}
	textGlowBrightness = 6;
}
