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
#include "textlayer.h"

#include "hangul.h"
#include "opentyr.h"
#include "palette.h"
#include "video.h"

#include <math.h>
#include <stdlib.h>

#define CELL_COUNT (vga_width * vga_height)
#define MAX_SURFACES 8
#define MAX_OPS 8192

typedef struct
{
	Uint32 serial;  // when it was drawn; see reset_serial
	Uint32 codepoint;
	Sint16 x, y;
	Uint8 font;
	Uint8 mode;
	Uint8 hue;
	Sint8 value;
} TextOp;

typedef struct
{
	SDL_Surface *surface;

	// Per game pixel ("cell"):
	Uint8 active[CELL_COUNT];          // holds a text proxy
	Uint8 expected[CELL_COUNT];        // value of the proxy we left there
	Uint8 bg[CELL_COUNT];              // value before any text was drawn there
	Uint32 reset_serial[CELL_COUNT];   // serial of the latest time the cell lost its previous text
	int active_count;

	// A glyph is shown only while all of its cells are active and none was
	// reset after the glyph was drawn.
	TextOp *ops;
	int op_count, op_capacity;

	// Area that the game covers only for a while (the mouse pointer, whose
	// pixels it restores afterwards).  Changed cells in it hide their text
	// in the shown frame instead of losing it.
	SDL_Rect overlay;
	Uint8 hidden[CELL_COUNT];          // active cell covered in the shown frame
	SDL_Rect hidden_area;              // where hidden may be set
} SurfaceText;

static SurfaceText *states[MAX_SURFACES];
static Uint32 serial_counter = 0;

static SDL_Surface *scratch_surface = NULL;

/* Glyph textures *************************************************************/

typedef struct
{
	SDL_Texture *texture;
	int upscale;
} GlyphTexture;

#define PAGE_COUNT (0x110000 >> 8)
static GlyphTexture *texture_pages[HANGUL_FONT_CLASSES][PAGE_COUNT];

static unsigned int font_class(unsigned int font)
{
	return font < HANGUL_FONT_CLASSES ? font : 2;
}

static int layer_count(unsigned int font)
{
	return hangul_has_highlight(font) ? 2 : 1;
}

/**
 * Texture of a glyph, enlarged by an integer factor; each layer (body, top
 * highlight) is a white mask placed side by side, tinted when drawn.
 */
static SDL_Texture *glyph_texture(SDL_Renderer *renderer, unsigned int font, Uint32 codepoint, const HangulGlyph *glyph, int upscale)
{
	const unsigned int cls = font_class(font);

	GlyphTexture *page = texture_pages[cls][codepoint >> 8];
	if (page == NULL)
	{
		page = calloc(256, sizeof(*page));
		if (page == NULL)
			return NULL;
		texture_pages[cls][codepoint >> 8] = page;
	}

	GlyphTexture *entry = &page[codepoint & 0xff];
	if (entry->texture != NULL && entry->upscale == upscale)
		return entry->texture;

	if (entry->texture != NULL)
		SDL_DestroyTexture(entry->texture);
	entry->texture = NULL;

	const int layers = layer_count(font);
	const int w = glyph->w * upscale,
	          h = glyph->h * upscale,
	          tw = w * layers;

	Uint32 *pixels = calloc((size_t)tw * h, sizeof(*pixels));
	if (pixels == NULL)
		return NULL;

	for (int my = 0; my < glyph->h; ++my)
	{
		for (int mx = 0; mx < glyph->w; ++mx)
		{
			if (!glyph->mask[my * glyph->w + mx])
				continue;

			const bool top_edge = my == 0 || !glyph->mask[(my - 1) * glyph->w + mx];

			for (int layer = 0; layer < layers; ++layer)
			{
				if (layer == 1 && !top_edge)
					continue;

				for (int y = 0; y < upscale; ++y)
					for (int x = 0; x < upscale; ++x)
						pixels[(my * upscale + y) * tw + layer * w + mx * upscale + x] = 0xffffffff;
			}
		}
	}

	SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, tw, h);
	if (texture != NULL)
	{
		SDL_UpdateTexture(texture, NULL, pixels, tw * (int)sizeof(*pixels));
		SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
	}
	free(pixels);

	entry->texture = texture;
	entry->upscale = upscale;

	return texture;
}

void textlayer_release_textures(void)
{
	for (unsigned int c = 0; c < HANGUL_FONT_CLASSES; ++c)
	{
		for (int p = 0; p < PAGE_COUNT; ++p)
		{
			GlyphTexture *page = texture_pages[c][p];
			if (page == NULL)
				continue;
			for (int i = 0; i < 256; ++i)
				if (page[i].texture != NULL)
					SDL_DestroyTexture(page[i].texture);
			free(page);
			texture_pages[c][p] = NULL;
		}
	}
}

/* Surface records ************************************************************/

static SurfaceText *get_state(SDL_Surface *surface, bool create)
{
	SurfaceText **free_slot = NULL;

	for (int i = 0; i < MAX_SURFACES; ++i)
	{
		if (states[i] != NULL && states[i]->surface == surface)
			return states[i];
		if (states[i] == NULL && free_slot == NULL)
			free_slot = &states[i];
	}

	if (!create || free_slot == NULL ||
	    surface->w != vga_width || surface->h != vga_height || surface->format->BitsPerPixel != 8)
		return NULL;

	SurfaceText *state = calloc(1, sizeof(*state));
	if (state == NULL)
		return NULL;
	state->surface = surface;
	*free_slot = state;

	return state;
}

static void clear_state(SurfaceText *state)
{
	if (state->active_count > 0)
		memset(state->active, 0, sizeof(state->active));
	state->active_count = 0;
	state->op_count = 0;
	state->overlay = (SDL_Rect){ 0, 0, 0, 0 };
}

void textlayer_clear(SDL_Surface *surface)
{
	SurfaceText *state = get_state(surface, false);
	if (state != NULL)
		clear_state(state);
}

void textlayer_clear_rect(SDL_Surface *surface, int x, int y, int w, int h)
{
	SurfaceText *state = get_state(surface, false);
	if (state == NULL || state->active_count == 0)
		return;

	const SDL_Rect area = { x, y, w, h }, bounds = { 0, 0, vga_width, vga_height };
	SDL_Rect r;
	if (!SDL_IntersectRect(&area, &bounds, &r))
		return;

	const Uint32 serial = ++serial_counter;
	for (int cy = r.y; cy < r.y + r.h; ++cy)
	{
		for (int cx = r.x; cx < r.x + r.w; ++cx)
		{
			const int i = cy * vga_width + cx;
			if (state->active[i])
			{
				state->active[i] = 0;
				state->reset_serial[i] = serial;
				--state->active_count;
			}
		}
	}
}

void textlayer_set_overlay(SDL_Surface *surface, int x, int y, int w, int h)
{
	SurfaceText *state = get_state(surface, false);
	if (state == NULL)
		return;

	const SDL_Rect area = { x, y, w, h }, bounds = { 0, 0, vga_width, vga_height };
	if (!SDL_IntersectRect(&area, &bounds, &state->overlay))
		state->overlay = (SDL_Rect){ 0, 0, 0, 0 };
}

void textlayer_copy(SDL_Surface *dst, SDL_Surface *src)
{
	if (dst == src)
		return;

	SurfaceText *s = get_state(src, false);
	if (s == NULL || (s->active_count == 0 && s->op_count == 0))
	{
		textlayer_clear(dst);
		return;
	}

	SurfaceText *d = get_state(dst, true);
	if (d == NULL)
		return;

	memcpy(d->active, s->active, sizeof(d->active));
	memcpy(d->expected, s->expected, sizeof(d->expected));
	memcpy(d->bg, s->bg, sizeof(d->bg));
	memcpy(d->reset_serial, s->reset_serial, sizeof(d->reset_serial));
	d->active_count = s->active_count;

	if (d->op_capacity < s->op_count)
	{
		TextOp *ops = realloc(d->ops, s->op_count * sizeof(*ops));
		if (ops == NULL)
		{
			d->op_count = 0;
			return;
		}
		d->ops = ops;
		d->op_capacity = s->op_count;
	}
	memcpy(d->ops, s->ops, s->op_count * sizeof(*d->ops));
	d->op_count = s->op_count;
}

static bool op_is_shadow(Uint8 mode)
{
	return mode == TEXT_MODE_DARK || mode == TEXT_MODE_BLACK;
}

/**
 * Whether a cell of a glyph still shows the glyph: true for cells the glyph
 * does not touch, and for touched cells that are active and were not reset
 * after the glyph was drawn.  With \p shown, cells covered for the moment
 * (see SurfaceText.hidden) count as not showing it.
 */
static bool cell_shows_glyph(const SurfaceText *state, const TextOp *op, const HangulGlyph *glyph, int cx, int cy, bool shown)
{
	if (!glyph->cov[cy * glyph->cov_w + cx])
		return true;

	const int x = op->x + glyph->cov_x + cx,
	          y = op->y + glyph->cov_y + cy;
	if (x < 0 || x >= vga_width || y < 0 || y >= vga_height)
		return false;

	const int i = y * vga_width + x;
	return state->active[i] && state->reset_serial[i] <= op->serial && !(shown && state->hidden[i]);
}

/** Counts the cells of a glyph still showing it; *all is set if none was drawn over. */
static int visible_cells(const SurfaceText *state, const TextOp *op, bool shown, bool *all)
{
	const HangulGlyph *glyph = hangul_glyph(op->font, op->codepoint);
	*all = false;
	if (glyph == NULL)
		return 0;

	int visible = 0, touched = 0;
	for (int cy = 0; cy < glyph->cov_h; ++cy)
	{
		for (int cx = 0; cx < glyph->cov_w; ++cx)
		{
			if (!glyph->cov[cy * glyph->cov_w + cx])
				continue;
			++touched;
			visible += cell_shows_glyph(state, op, glyph, cx, cy, shown);
		}
	}

	*all = visible == touched;
	return visible;
}

static bool op_is_valid(const SurfaceText *state, const TextOp *op)
{
	// Partly overwritten glyphs are kept and shown clipped to their intact
	// cells (text scrolling behind a mask, a sprite over part of the text).
	bool all;
	return visible_cells(state, op, false, &all) > 0;
}

/** Drops records of text the game has drawn over since. */
static void validate(SurfaceText *state)
{
	const SDL_Surface *surface = state->surface;
	const Uint32 serial = ++serial_counter;

	// Cells covered in the previous frame are judged again.
	const SDL_Rect *h = &state->hidden_area;
	for (int y = h->y; y < h->y + h->h; ++y)
		memset(&state->hidden[y * vga_width + h->x], 0, (size_t)h->w);
	state->hidden_area = state->overlay;

	const SDL_Rect *o = &state->overlay;

	int active_count = 0;
	for (int y = 0; y < vga_height; ++y)
	{
		const Uint8 *row = (const Uint8 *)surface->pixels + y * surface->pitch;
		for (int x = 0; x < vga_width; ++x)
		{
			const int i = y * vga_width + x;
			if (!state->active[i])
				continue;

			if (row[x] == state->expected[i])
			{
				++active_count;
			}
			else if (x >= o->x && x < o->x + o->w && y >= o->y && y < o->y + o->h)
			{
				// Covered for now; the game restores the cell later.
				state->hidden[i] = 1;
				++active_count;
			}
			else
			{
				state->active[i] = 0;
				state->reset_serial[i] = serial;
			}
		}
	}
	state->active_count = active_count;

	int kept = 0;
	for (int i = 0; i < state->op_count; ++i)
		if (op_is_valid(state, &state->ops[i]))
			state->ops[kept++] = state->ops[i];
	state->op_count = kept;
}

static void push_op(SurfaceText *state, const TextOp *op)
{
	if (state->op_count >= MAX_OPS)
		return;

	if (state->op_count == state->op_capacity)
	{
		const int capacity = MAX(64, state->op_capacity * 2);
		TextOp *ops = realloc(state->ops, capacity * sizeof(*ops));
		if (ops == NULL)
			return;
		state->ops = ops;
		state->op_capacity = capacity;
	}

	state->ops[state->op_count++] = *op;
}

void textlayer_copy_rect(SDL_Surface *dst, int dx, int dy, SDL_Surface *src, int sx, int sy, int w, int h)
{
	if (dst == src)
		return;

	// Keep the rectangle inside both surfaces.
	if (dx < 0) { sx -= dx; w += dx; dx = 0; }
	if (dy < 0) { sy -= dy; h += dy; dy = 0; }
	if (sx < 0) { dx -= sx; w += sx; sx = 0; }
	if (sy < 0) { dy -= sy; h += sy; sy = 0; }
	w = MIN(w, MIN(vga_width - dx, vga_width - sx));
	h = MIN(h, MIN(vga_height - dy, vga_height - sy));
	if (w <= 0 || h <= 0)
		return;

	SurfaceText *s = src != NULL ? get_state(src, false) : NULL;
	if (s != NULL && s->active_count > 0)
		validate(s);  // the copy carries only text that is still there
	if (s != NULL && s->active_count == 0)
		s = NULL;

	SurfaceText *d = get_state(dst, s != NULL);
	if (d == NULL)
		return;

	const Uint32 serial = ++serial_counter;
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			const int di = (dy + y) * vga_width + dx + x;
			const int si = (sy + y) * vga_width + sx + x;

			d->active_count -= d->active[di];
			if (s != NULL && s->active[si])
			{
				d->active[di] = 1;
				d->expected[di] = s->expected[si];
				d->bg[di] = s->bg[si];
				d->reset_serial[di] = s->reset_serial[si];
				++d->active_count;
			}
			else
			{
				d->active[di] = 0;
				d->reset_serial[di] = serial;
			}
		}
	}

	// Glyphs placed in the rectangle are replaced by those of the source.
	int kept = 0;
	for (int i = 0; i < d->op_count; ++i)
	{
		const TextOp *op = &d->ops[i];
		if (!(op->x >= dx && op->x < dx + w && op->y >= dy && op->y < dy + h))
			d->ops[kept++] = *op;
	}
	d->op_count = kept;

	if (s != NULL)
	{
		for (int i = 0; i < s->op_count; ++i)
		{
			TextOp op = s->ops[i];
			if (op.x >= sx && op.x < sx + w && op.y >= sy && op.y < sy + h)
			{
				op.x = (Sint16)(op.x - sx + dx);
				op.y = (Sint16)(op.y - sy + dy);
				push_op(d, &op);
			}
		}
	}
}

/* Drawing ********************************************************************/

static Uint8 hv_index(TextMode mode, Uint8 hue, Sint8 value, int layer_value)
{
	// Same arithmetic as blit_sprite_hv() and blit_sprite_hv_unsafe().
	Uint8 v = (Uint8)(layer_value + value);
	if (mode == TEXT_MODE_HV_UNSAFE)
		return (Uint8)((hue << 4) | v);

	if (v > 0xf)
		v = (v >= 0x1f) ? 0x0 : 0xf;
	return (Uint8)((hue << 4) | v);
}

static Uint8 proxy_value(TextMode mode, Uint8 below, Uint8 hue, Sint8 value, int body)
{
	switch (mode)
	{
	case TEXT_MODE_HV:
	case TEXT_MODE_HV_UNSAFE:
		return hv_index(mode, hue, value, body);
	case TEXT_MODE_BLEND:
	{
		const Uint8 v = hv_index(TEXT_MODE_HV, hue, value, body) & 0x0f;
		return (Uint8)((hue << 4) | (((below & 0x0f) + v) / 2));
	}
	case TEXT_MODE_DARK:
		return (Uint8)((below & 0xf0) | ((below & 0x0f) / 2));
	case TEXT_MODE_BLACK:
	default:
		return 0;
	}
}

int textlayer_glyph_advance(unsigned int font, Uint32 codepoint)
{
	const HangulGlyph *glyph = hangul_glyph(font, codepoint);
	return glyph != NULL ? glyph->advance : -1;
}

int textlayer_draw_glyph(SDL_Surface *surface, int x, int y, unsigned int font, Uint32 codepoint, TextMode mode, Uint8 hue, Sint8 value)
{
	const HangulGlyph *glyph = hangul_glyph(font, codepoint);
	if (glyph == NULL)
		return -1;

	SurfaceText *state = get_state(surface, true);
	const Uint32 serial = ++serial_counter;
	const int body = hangul_body_value(font);

	if (state != NULL)
	{
		// A character drawn again in the same place (blinking menu items,
		// glowing text, ...) replaces the earlier one.
		int kept = 0;
		for (int i = 0; i < state->op_count; ++i)
		{
			const TextOp *op = &state->ops[i];
			if (!(op->x == x && op->y == y && op->font == font && op_is_shadow(op->mode) == op_is_shadow(mode)))
				state->ops[kept++] = *op;
		}
		state->op_count = kept;

		if (state->op_count == MAX_OPS)
			validate(state);
	}

	for (int cy = 0; cy < glyph->cov_h; ++cy)
	{
		const int py = y + glyph->cov_y + cy;
		if (py < 0 || py >= surface->h)
			continue;

		for (int cx = 0; cx < glyph->cov_w; ++cx)
		{
			const int px = x + glyph->cov_x + cx;
			if (px < 0 || px >= surface->w || !glyph->cov[cy * glyph->cov_w + cx])
				continue;

			Uint8 *pixel = (Uint8 *)surface->pixels + py * surface->pitch + px;
			Uint8 proxy = proxy_value(mode, *pixel, hue, value, body);

			if (state != NULL)
			{
				const int i = py * vga_width + px;

				if (!state->active[i] || *pixel != state->expected[i])
				{
					// Text on top of fresh background.
					if (!state->active[i])
						++state->active_count;
					state->active[i] = 1;
					state->bg[i] = *pixel;
					state->reset_serial[i] = serial;
				}

				// Visible text must change the cell, or erasing it with the
				// background color would go unnoticed.  (Proxies are never
				// shown, so the slightly different value does not matter.)
				if (proxy == state->bg[i] && !op_is_shadow(mode))
					proxy ^= 1;

				state->expected[i] = proxy;
			}

			*pixel = proxy;
		}
	}

	if (state != NULL)
	{
		push_op(state, &(TextOp){
			.serial = serial,
			.codepoint = codepoint,
			.x = (Sint16)x,
			.y = (Sint16)y,
			.font = (Uint8)font,
			.mode = (Uint8)mode,
			.hue = hue,
			.value = value,
		});
	}

	return glyph->advance;
}

/* Display ********************************************************************/

SDL_Surface *textlayer_prepare_frame(SDL_Surface *src)
{
	SurfaceText *state = get_state(src, false);
	if (state == NULL || state->active_count == 0)
		return src;

	validate(state);
	if (state->active_count == 0)
		return src;

	if (scratch_surface == NULL)
	{
		scratch_surface = SDL_CreateRGBSurface(0, vga_width, vga_height, 8, 0, 0, 0, 0);
		if (scratch_surface == NULL)
			return src;
	}

	for (int y = 0; y < vga_height; ++y)
	{
		const Uint8 *src_row = (const Uint8 *)src->pixels + y * src->pitch;
		Uint8 *dst_row = (Uint8 *)scratch_surface->pixels + y * scratch_surface->pitch;
		memcpy(dst_row, src_row, vga_width);

		for (int x = 0; x < vga_width; ++x)
		{
			const int i = y * vga_width + x;
			if (state->active[i] && !state->hidden[i])
				dst_row[x] = state->bg[i];
		}
	}

	return scratch_surface;
}

static void set_color(SDL_Texture *texture, Uint8 index, Uint8 alpha)
{
	Uint8 r, g, b;
	SDL_GetRGB(rgb_palette[index], main_window_tex_format, &r, &g, &b);
	SDL_SetTextureColorMod(texture, r, g, b);
	SDL_SetTextureAlphaMod(texture, alpha);
}

/** Draws the layers (body, highlight) of a glyph in the colors of its record. */
static void draw_glyph_layers(SDL_Renderer *renderer, SDL_Texture *texture, const TextOp *op, const HangulGlyph *glyph, int upscale, const SDL_FRect *dst)
{
	SDL_Rect layer_src = { 0, 0, glyph->w * upscale, glyph->h * upscale };

	const TextMode mode = (TextMode)op->mode;
	const int body = hangul_body_value(op->font);

	switch (mode)
	{
	case TEXT_MODE_DARK:
		set_color(texture, 0, 128);  // halves the brightness, like halving the value
		SDL_RenderCopyF(renderer, texture, &layer_src, dst);
		break;

	case TEXT_MODE_BLACK:
		set_color(texture, 0, 255);
		SDL_RenderCopyF(renderer, texture, &layer_src, dst);
		break;

	case TEXT_MODE_HV:
	case TEXT_MODE_HV_UNSAFE:
	case TEXT_MODE_BLEND:
	{
		const Uint8 alpha = mode == TEXT_MODE_BLEND ? 128 : 255;
		const TextMode index_mode = mode == TEXT_MODE_BLEND ? TEXT_MODE_HV : mode;

		set_color(texture, hv_index(index_mode, op->hue, op->value, body), alpha);
		SDL_RenderCopyF(renderer, texture, &layer_src, dst);

		if (layer_count(op->font) > 1)
		{
			layer_src.x = layer_src.w;
			set_color(texture, hv_index(index_mode, op->hue, op->value, body + 1), alpha);
			SDL_RenderCopyF(renderer, texture, &layer_src, dst);
		}
		break;
	}
	}
}

void textlayer_render(SDL_Renderer *renderer, SDL_Surface *src, const SDL_Rect *dst_rect)
{
	SurfaceText *state = get_state(src, false);
	if (state == NULL || state->op_count == 0)
		return;

	const float sx = (float)dst_rect->w / vga_width,
	            sy = (float)dst_rect->h / vga_height;

	SDL_RenderSetClipRect(renderer, dst_rect);

	for (int i = 0; i < state->op_count; ++i)
	{
		const TextOp *op = &state->ops[i];
		const HangulGlyph *glyph = hangul_glyph(op->font, op->codepoint);
		if (glyph == NULL || glyph->w == 0 || glyph->h == 0)
			continue;

		bool all_visible;
		if (visible_cells(state, op, true, &all_visible) == 0)
			continue;

		// Size of a font pixel on screen.  Glyphs are pre-enlarged by an
		// integer factor so that they can be drawn crisply (nearest neighbor)
		// when that is exact, or reduced smoothly otherwise.
		const float f = hangul_font_scale(op->font),
		            gx = f * sx,
		            gy = f * sy;
		const int upscale = MAX(1, (int)ceilf(MAX(gx, gy) - 0.01f));

		SDL_Texture *texture = glyph_texture(renderer, op->font, op->codepoint, glyph, upscale);
		if (texture == NULL)
			continue;

		const bool exact = fabsf(gx - upscale) < 0.01f && fabsf(gy - upscale) < 0.01f;
#if SDL_VERSION_ATLEAST(2, 0, 12)
		SDL_SetTextureScaleMode(texture, exact ? SDL_ScaleModeNearest : SDL_ScaleModeLinear);
#endif

		const SDL_FRect dst = {
			dst_rect->x + op->x * sx + glyph->x * gx,
			dst_rect->y + op->y * sy + glyph->y * gy,
			glyph->w * gx,
			glyph->h * gy,
		};

		if (all_visible)
		{
			draw_glyph_layers(renderer, texture, op, glyph, upscale, &dst);
			continue;
		}

		// Partly drawn over: draw the glyph once per run of intact cells of
		// each row, clipped to the run.
		for (int cy = 0; cy < glyph->cov_h; ++cy)
		{
			for (int cx = 0; cx < glyph->cov_w; )
			{
				if (!cell_shows_glyph(state, op, glyph, cx, cy, true))
				{
					++cx;
					continue;
				}

				int run_end = cx + 1;
				while (run_end < glyph->cov_w && cell_shows_glyph(state, op, glyph, run_end, cy, true))
					++run_end;

				const int x0 = op->x + glyph->cov_x + cx,
				          x1 = op->x + glyph->cov_x + run_end,
				          y0 = op->y + glyph->cov_y + cy;
				SDL_Rect clip;
				clip.x = dst_rect->x + (int)lroundf(x0 * sx);
				clip.y = dst_rect->y + (int)lroundf(y0 * sy);
				clip.w = dst_rect->x + (int)lroundf(x1 * sx) - clip.x;
				clip.h = dst_rect->y + (int)lroundf((y0 + 1) * sy) - clip.y;

				if (SDL_IntersectRect(&clip, dst_rect, &clip))
				{
					SDL_RenderSetClipRect(renderer, &clip);
					draw_glyph_layers(renderer, texture, op, glyph, upscale, &dst);
				}

				cx = run_end;
			}
		}
		SDL_RenderSetClipRect(renderer, dst_rect);
	}

	SDL_RenderSetClipRect(renderer, NULL);
}
