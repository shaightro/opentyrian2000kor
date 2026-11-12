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
#ifndef TEXTLAYER_H
#define TEXTLAYER_H

#include "SDL.h"

#include <stdbool.h>
#include <string.h>

/*
 * High-resolution text layer.
 *
 * The game draws everything into 320x200 8-bit surfaces, which is too coarse
 * for Korean.  Characters that the game's sprite fonts lack are therefore
 * drawn twice:
 *
 *  - into the surface, as a coarse stand-in ("proxy") made of the cells the
 *    glyph touches, and
 *  - as a record of the glyph, its position and its color, kept per surface.
 *
 * When a surface is shown, every cell that still holds the value we left in
 * it is restored to what was below the text, and the recorded glyphs whose
 * cells are all intact are drawn on top at the output resolution.  Whatever
 * the game does to the surface afterwards (erasing, drawing sprites over the
 * text, ...) changes those cells, so text disappears exactly when the game
 * would have overwritten it, without having to track every drawing routine.
 * Copies of whole surfaces must go through surface_copy() so that the records
 * travel with the pixels.
 */

typedef enum
{
	TEXT_MODE_HV,         // like blit_sprite_hv(): palette hue and value, value clamped
	TEXT_MODE_HV_UNSAFE,  // like blit_sprite_hv_unsafe(): value not clamped
	TEXT_MODE_BLEND,      // like blit_sprite_hv_blend(): half transparent
	TEXT_MODE_DARK,       // like blit_sprite_dark(black = false): darkens what is below
	TEXT_MODE_BLACK,      // like blit_sprite_dark(black = true): solid color 0
} TextMode;

/**
 * Draws a character that the sprite fonts lack.  Returns its advance in game
 * pixels, or -1 if no font has the character (nothing is drawn then).
 */
int textlayer_draw_glyph(SDL_Surface *surface, int x, int y, unsigned int font, Uint32 codepoint, TextMode mode, Uint8 hue, Sint8 value);

/** Advance of a character in game pixels, or -1 if no font has it. */
int textlayer_glyph_advance(unsigned int font, Uint32 codepoint);

/** Copies the text records of a whole surface (after copying its pixels). */
void textlayer_copy(SDL_Surface *dst, SDL_Surface *src);

/**
 * Copies the text records of a rectangle (after copying its pixels unchanged);
 * the text of \p dst in the rectangle is replaced.  With a NULL \p src, the
 * rectangle of \p dst just loses its text records (for copies that change the
 * pixels).
 */
void textlayer_copy_rect(SDL_Surface *dst, int dx, int dy, SDL_Surface *src, int sx, int sy, int w, int h);

/** Forgets all text records of a surface. */
void textlayer_clear(SDL_Surface *surface);

/**
 * Forgets the text in a rectangle that the game has filled.  (Text drawn
 * there afterwards in the color of the old text would otherwise look like the
 * old text was still there.)
 */
void textlayer_clear_rect(SDL_Surface *surface, int x, int y, int w, int h);

/**
 * Marks a rectangle of \p surface that the game covers only for a while: the
 * mouse pointer, which it draws into the surface and later erases by putting
 * back the pixels from below it.  Text under the rectangle is hidden while
 * covered instead of being forgotten.  An empty rectangle removes the mark.
 */
void textlayer_set_overlay(SDL_Surface *surface, int x, int y, int w, int h);

/**
 * Returns the surface to scale for display: \p src itself, or a copy with the
 * text proxies replaced by what was below them.
 */
SDL_Surface *textlayer_prepare_frame(SDL_Surface *src);

/** Draws the text of \p src (validated by textlayer_prepare_frame()) into \p dst_rect. */
void textlayer_render(SDL_Renderer *renderer, SDL_Surface *src, const SDL_Rect *dst_rect);

/** Must be called before the renderer is destroyed. */
void textlayer_release_textures(void);

/** Copies a whole surface, together with its text records. */
static inline void surface_copy(SDL_Surface *dst, SDL_Surface *src)
{
	memcpy(dst->pixels, src->pixels, (size_t)dst->pitch * dst->h);
	textlayer_copy(dst, src);
}

#endif /* TEXTLAYER_H */
