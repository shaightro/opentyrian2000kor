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
#ifndef HANGUL_H
#define HANGUL_H

#include "SDL.h"

#include <stdbool.h>

/*
 * Glyphs for characters that the game's own sprite fonts do not have (Korean
 * text in particular), taken from TrueType pixel fonts.  They are drawn by the
 * text layer (textlayer.h) at a finer resolution than the 320x200 screen.
 *
 * Fonts are identified by the game's font numbers: 0 = large, 1 = normal,
 * 2 = small ("tiny"); any other number is treated as small.
 */

#define HANGUL_FONT_CLASSES 3

typedef struct
{
	// Bitmap in font pixels; 'mask' holds 1 for ink.  The bitmap's top-left
	// corner is at (x, y) font pixels from the top-left corner of the box the
	// game draws the character in.
	int w, h;
	int x, y;
	Uint8 *mask;

	// Game screen cells (pixels) touched by the ink, relative to the box: a
	// cov_w x cov_h bitmap whose top-left corner is at (cov_x, cov_y).
	int cov_x, cov_y, cov_w, cov_h;
	Uint8 *cov;

	int advance;  // in game pixels
} HangulGlyph;

/** Loads the fonts from the "kor" folder next to the executable. */
bool hangul_init(void);
void hangul_deinit(void);

/** Glyph for a character, or NULL if no font has it. */
const HangulGlyph *hangul_glyph(unsigned int font, Uint32 codepoint);

/** Size of a font pixel in game pixels. */
float hangul_font_scale(unsigned int font);

/** Shading value (0..15) of the body of the game's sprite font of this size. */
int hangul_body_value(unsigned int font);

/** Whether the font draws a highlight on the top edges, like the game's normal and large fonts. */
bool hangul_has_highlight(unsigned int font);

/**
 * Decodes one character of game text: UTF-8, falling back to single bytes for
 * the legacy CP437 characters used by the original data.  Returns the number
 * of bytes consumed (0 at the end of the string).
 */
int text_decode_char(const char *s, Uint32 *codepoint);

#endif /* HANGUL_H */
