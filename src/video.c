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
#include "video.h"

#include "keyboard.h"
#include "opentyr.h"
#include "palette.h"
#include "textlayer.h"
#include "video_scale.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const scaling_mode_names[ScalingMode_MAX] = {
	"Center",
	"Integer",
	"Fit 8:5",
	"Fit 4:3",
};

int fullscreen_display;
ScalingMode scaling_mode = SCALE_INTEGER;
int window_scale = WINDOW_SCALE_AUTO;
static SDL_Rect last_output_rect = { 0, 0, vga_width, vga_height };

SDL_Surface *VGAScreen, *VGAScreenSeg;
SDL_Surface *VGAScreen2;
SDL_Surface *game_screen;

SDL_Window *main_window = NULL;
static SDL_Renderer *main_window_renderer = NULL;
SDL_PixelFormat *main_window_tex_format = NULL;
static SDL_Texture *main_window_texture = NULL;

// Render target used to scale the output by non-integer factors without
// uneven pixels (see sharp_upscale()).
static SDL_Texture *upscale_texture = NULL;

static ScalerFunction scaler_function;

static void init_renderer(void);
static void deinit_renderer(void);
static void init_texture(void);
static void deinit_texture(void);

static int window_get_display_index(void);
static void window_center_in_display(int display_index);
static void calc_dst_render_rect(SDL_Surface *src_surface, SDL_Rect *dst_rect);
static void scale_and_flip(SDL_Surface *);
static void update_mouse_confinement(void);

void init_video(void)
{
	if (SDL_WasInit(SDL_INIT_VIDEO))
		return;

#if SDL_VERSION_ATLEAST(2, 24, 0)
	// Draw at the monitor's real resolution instead of letting Windows stretch
	// (and blur) the window when display scaling is above 100%.  Window sizes
	// and mouse coordinates are then in physical pixels.
	SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
#endif

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) == -1)
	{
		fprintf(stderr, "error: failed to initialize SDL video: %s\n", SDL_GetError());
		exit(1);
	}

	// Create the software surfaces that the game renders to. These are all 320x200x8 regardless
	// of the window size or monitor resolution.
	VGAScreen = VGAScreenSeg = SDL_CreateRGBSurface(0, vga_width, vga_height, 8, 0, 0, 0, 0);
	VGAScreen2 = SDL_CreateRGBSurface(0, vga_width, vga_height, 8, 0, 0, 0, 0);
	game_screen = SDL_CreateRGBSurface(0, vga_width, vga_height, 8, 0, 0, 0, 0);

	// The game code writes to surface->pixels directly without locking, so make sure that we
	// indeed created software surfaces that support this.
	assert(!SDL_MUSTLOCK(VGAScreen));
	assert(!SDL_MUSTLOCK(VGAScreen2));
	assert(!SDL_MUSTLOCK(game_screen));

	JE_clr256(VGAScreen);

	// Create the window with a temporary initial size, hidden until we set up the
	// scaler and find the true window size
	main_window = SDL_CreateWindow(opentyrian_str,
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		vga_width, vga_height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);

	if (main_window == NULL)
	{
		fprintf(stderr, "error: failed to create window: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	SDL_SetWindowMinimumSize(main_window, vga_width, vga_height);

	reinit_fullscreen(fullscreen_display);
	init_renderer();
	init_texture();
	init_scaler(scaler);

	SDL_ShowWindow(main_window);

	SDL_SetRenderDrawColor(main_window_renderer, 0, 0, 0, 255);
	SDL_RenderClear(main_window_renderer);
	SDL_RenderPresent(main_window_renderer);
}

void deinit_video(void)
{
	deinit_texture();
	deinit_renderer();

	SDL_DestroyWindow(main_window);

	SDL_FreeSurface(VGAScreenSeg);
	SDL_FreeSurface(VGAScreen2);
	SDL_FreeSurface(game_screen);

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void init_renderer(void)
{
	main_window_renderer = SDL_CreateRenderer(main_window, -1, 0);

	if (main_window_renderer == NULL)
	{
		fprintf(stderr, "error: failed to create renderer: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}
}

static void deinit_renderer(void)
{
	if (main_window_renderer != NULL)
	{
		textlayer_release_textures();
		SDL_DestroyRenderer(main_window_renderer);
		main_window_renderer = NULL;
	}
}

static void init_texture(void)
{
	assert(main_window_renderer != NULL);

	int bpp = 32; // TODOSDL2
	Uint32 format = bpp == 32 ? SDL_PIXELFORMAT_RGB888 : SDL_PIXELFORMAT_RGB565;
	int scaler_w = scalers[scaler].width;
	int scaler_h = scalers[scaler].height;

	main_window_tex_format = SDL_AllocFormat(format);

	main_window_texture = SDL_CreateTexture(main_window_renderer, format, SDL_TEXTUREACCESS_STREAMING, scaler_w, scaler_h);

	if (main_window_texture == NULL)
	{
		fprintf(stderr, "error: failed to create scaler texture %dx%dx%s: %s\n", scaler_w, scaler_h, SDL_GetPixelFormatName(format), SDL_GetError());
		exit(EXIT_FAILURE);
	}

#if SDL_VERSION_ATLEAST(2, 0, 12)
	SDL_SetTextureScaleMode(main_window_texture, SDL_ScaleModeNearest);
#endif
}

static void deinit_texture(void)
{
	if (upscale_texture != NULL)
	{
		SDL_DestroyTexture(upscale_texture);
		upscale_texture = NULL;
	}

	if (main_window_texture != NULL)
	{
		SDL_DestroyTexture(main_window_texture);
		main_window_texture = NULL;
	}

	if (main_window_tex_format != NULL)
	{
		SDL_FreeFormat(main_window_tex_format);
		main_window_tex_format = NULL;
	}
}

static int window_get_display_index(void)
{
	return SDL_GetWindowDisplayIndex(main_window);
}

static void window_center_in_display(int display_index)
{
	int win_w, win_h;
	SDL_GetWindowSize(main_window, &win_w, &win_h);

	SDL_Rect bounds;
	SDL_GetDisplayBounds(display_index, &bounds);

	SDL_SetWindowPosition(main_window, bounds.x + (bounds.w - win_w) / 2, bounds.y + (bounds.h - win_h) / 2);
}

/** Size of the window's drawable area at an integer scale of the game screen. */
static void window_content_size(int scale, int *w, int *h)
{
	*w = vga_width * scale;
	// In 4:3 mode the 320x200 screen is shown with the tall pixels of a CRT.
	*h = (scaling_mode == SCALE_ASPECT_4_3 ? vga_width * 3 / 4 : vga_height) * scale;
}

/**
 * Largest integer scale whose window (including its frame) fits in
 * \p percent percent of the usable area of the window's display.
 */
static int fit_window_scale(int percent)
{
	int display = SDL_GetWindowDisplayIndex(main_window);
	if (display < 0)
		display = 0;

	SDL_Rect usable;
	if (SDL_GetDisplayUsableBounds(display, &usable) != 0)
		return 2;

	int top, left, bottom, right;
	if (SDL_GetWindowBordersSize(main_window, &top, &left, &bottom, &right) != 0)
	{
		// Typical Windows frame, adjusted for display scaling.
		float dpi = 96.0f;
		if (SDL_GetDisplayDPI(display, NULL, &dpi, NULL) != 0 || dpi <= 0.0f)
			dpi = 96.0f;
		const float s = dpi / 96.0f;
		top = (int)(32 * s);
		left = right = bottom = (int)(8 * s);
	}

	int unit_w, unit_h;
	window_content_size(1, &unit_w, &unit_h);

	const int scale_w = (usable.w * percent / 100 - left - right) / unit_w;
	const int scale_h = (usable.h * percent / 100 - top - bottom) / unit_h;

	return MAX(1, MIN(MIN(scale_w, scale_h), WINDOW_SCALE_MAX));
}

int get_max_window_scale(void)
{
	return fit_window_scale(100);
}

int get_auto_window_scale(void)
{
	// Big, but leaving some of the desktop visible around the window.
	return fit_window_scale(90);
}

int get_effective_window_scale(void)
{
	if (window_scale == WINDOW_SCALE_AUTO)
		return get_auto_window_scale();

	return MIN(window_scale, get_max_window_scale());
}

void reset_window_size(void)
{
	if (fullscreen_display != -1)
		return;  // applied when leaving fullscreen

	int w, h;
	window_content_size(get_effective_window_scale(), &w, &h);

	SDL_SetWindowSize(main_window, w, h);
	window_center_in_display(window_get_display_index());
}

void set_window_scale(int new_scale)
{
	window_scale = MAX(WINDOW_SCALE_AUTO, MIN(new_scale, WINDOW_SCALE_MAX));

	reset_window_size();
}

void reinit_fullscreen(int new_display)
{
	fullscreen_display = new_display;

	if (fullscreen_display >= SDL_GetNumVideoDisplays())
	{
		fullscreen_display = 0;
	}

	SDL_SetWindowFullscreen(main_window, SDL_FALSE);

	int w, h;
	window_content_size(get_effective_window_scale(), &w, &h);
	SDL_SetWindowSize(main_window, w, h);

	if (fullscreen_display == -1)
	{
		window_center_in_display(window_get_display_index());
	}
	else
	{
		window_center_in_display(fullscreen_display);

		if (SDL_SetWindowFullscreen(main_window, SDL_WINDOW_FULLSCREEN_DESKTOP) != 0)
		{
			reinit_fullscreen(-1);
			return;
		}
	}
}

void video_on_win_resize(void)
{
	int w, h;

	// Enforce a minimum size on the window.  (The picture is fitted into
	// whatever size the user drags the window to; see calc_dst_render_rect().)

	SDL_GetWindowSize(main_window, &w, &h);

	if (w < vga_width || h < vga_height)
	{
		w = w < vga_width ? vga_width : w;
		h = h < vga_height ? vga_height : h;

		SDL_SetWindowSize(main_window, w, h);
	}
}

void toggle_fullscreen(void)
{
	if (fullscreen_display != -1)
		reinit_fullscreen(-1);
	else
		reinit_fullscreen(SDL_GetWindowDisplayIndex(main_window));
}

bool init_scaler(unsigned int new_scaler)
{
	int bpp = main_window_tex_format->BitsPerPixel; // TODOSDL2

	scaler = new_scaler;

	// The scaler only picks the pixel art algorithm (and the size of the
	// texture it renders to); the window size is chosen by window_scale.
	deinit_texture();
	init_texture();

	switch (bpp)
	{
	case 32:
		scaler_function = scalers[scaler].scaler32;
		break;
	case 16:
		scaler_function = scalers[scaler].scaler16;
		break;
	default:
		scaler_function = NULL;
		break;
	}

	if (scaler_function == NULL)
	{
		assert(false);
		return false;
	}

	return true;
}

bool set_scaling_mode_by_name(const char *name)
{
	for (int i = 0; i < ScalingMode_MAX; ++i)
	{
		 if (strcmp(name, scaling_mode_names[i]) == 0)
		 {
			 scaling_mode = i;
			 return true;
		 }
	}
	return false;
}

void JE_clr256(SDL_Surface *screen)
{
	SDL_FillRect(screen, NULL, 0);
	textlayer_clear(screen);
}

void JE_showVGA(void) 
{ 
	scale_and_flip(VGAScreen); 
}

static void calc_dst_render_rect(SDL_Surface *const src_surface, SDL_Rect *const dst_rect)
{
	// Decides how the logical output texture (after software scaling applied) will fit
	// in the window.

	int win_w, win_h;
	SDL_GetWindowSize(main_window, &win_w, &win_h);

	int maxh_width, maxw_height;

	switch (scaling_mode)
	{
	case SCALE_CENTER:
		SDL_QueryTexture(main_window_texture, NULL, NULL, &dst_rect->w, &dst_rect->h);
		break;
	case SCALE_INTEGER:
		dst_rect->w = src_surface->w;
		dst_rect->h = src_surface->h;
		while (dst_rect->w + src_surface->w <= win_w && dst_rect->h + src_surface->h <= win_h)
		{
			dst_rect->w += src_surface->w;
			dst_rect->h += src_surface->h;
		}
		break;
	case SCALE_ASPECT_8_5:
		maxh_width = win_h * (8.f / 5.f);
		maxw_height = win_w * (5.f / 8.f);

		if (maxh_width > win_w)
		{
			dst_rect->w = win_w;
			dst_rect->h = maxw_height;
		}
		else
		{
			dst_rect->w = maxh_width;
			dst_rect->h = win_h;
		}
		break;
	case SCALE_ASPECT_4_3:
		maxh_width = win_h * (4.f / 3.f);
		maxw_height = win_w * (3.f / 4.f);

		if (maxh_width > win_w)
		{
			dst_rect->w = win_w;
			dst_rect->h = maxw_height;
		}
		else
		{
			dst_rect->w = maxh_width;
			dst_rect->h = win_h;
		}
		break;
	case ScalingMode_MAX:
		assert(false);
		break;
	}

	dst_rect->x = (win_w - dst_rect->w) / 2;
	dst_rect->y = (win_h - dst_rect->h) / 2;
}

/**
 * Prepares the scaled frame for drawing at a size that is not an integer
 * multiple of it ("sharp bilinear"): the frame is first enlarged by the
 * smallest integer factor that covers the output size, keeping pixels square
 * and crisp, and then smoothly reduced to the exact output size.  This avoids
 * both the uneven pixel sizes of plain nearest-neighbor scaling and the blur
 * of plain bilinear scaling.
 *
 * Returns the texture to draw, which is the unmodified frame if render
 * targets are not available.
 */
static SDL_Texture *sharp_upscale(int tex_w, int tex_h, int dst_w, int dst_h)
{
#if SDL_VERSION_ATLEAST(2, 0, 12)
	if (!SDL_RenderTargetSupported(main_window_renderer))
		return main_window_texture;

	const int factor = MAX(1, MAX((dst_w + tex_w - 1) / tex_w, (dst_h + tex_h - 1) / tex_h));
	const int w = tex_w * factor,
	          h = tex_h * factor;

	int upscale_w = 0, upscale_h = 0;
	if (upscale_texture != NULL)
		SDL_QueryTexture(upscale_texture, NULL, NULL, &upscale_w, &upscale_h);

	if (upscale_texture == NULL || upscale_w != w || upscale_h != h)
	{
		if (upscale_texture != NULL)
			SDL_DestroyTexture(upscale_texture);

		upscale_texture = SDL_CreateTexture(main_window_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w, h);
		if (upscale_texture == NULL)
			return main_window_texture;

		SDL_SetTextureScaleMode(upscale_texture, SDL_ScaleModeLinear);
	}

	if (SDL_SetRenderTarget(main_window_renderer, upscale_texture) != 0)
		return main_window_texture;
	SDL_RenderCopy(main_window_renderer, main_window_texture, NULL, NULL);
	SDL_SetRenderTarget(main_window_renderer, NULL);

	return upscale_texture;
#else
	(void)tex_w, (void)tex_h, (void)dst_w, (void)dst_h;
	return main_window_texture;
#endif
}

/* Screenshots for the text preview (textpreview.c). */
static char capture_path[512];

void video_capture_next_frame(const char *bmp_path)
{
	SDL_strlcpy(capture_path, bmp_path, sizeof(capture_path));
}

static void capture_frame(const SDL_Rect *rect)
{
	SDL_Surface *shot = SDL_CreateRGBSurfaceWithFormat(0, rect->w, rect->h, 32, SDL_PIXELFORMAT_ARGB8888);
	if (shot != NULL)
	{
		if (SDL_RenderReadPixels(main_window_renderer, rect, SDL_PIXELFORMAT_ARGB8888, shot->pixels, shot->pitch) == 0)
			SDL_SaveBMP(shot, capture_path);
		SDL_FreeSurface(shot);
	}
	capture_path[0] = '\0';
}

static void scale_and_flip(SDL_Surface *src_surface)
{
	assert(src_surface->format->BitsPerPixel == 8);

	// Do software scaling (of the frame with high-resolution text taken out)
	assert(scaler_function != NULL);
	scaler_function(textlayer_prepare_frame(src_surface), main_window_texture);

	SDL_Rect dst_rect;
	calc_dst_render_rect(src_surface, &dst_rect);

	int tex_w, tex_h;
	SDL_QueryTexture(main_window_texture, NULL, NULL, &tex_w, &tex_h);

	SDL_Texture *texture = main_window_texture;
	if (dst_rect.w % tex_w != 0 || dst_rect.h % tex_h != 0)
		texture = sharp_upscale(tex_w, tex_h, dst_rect.w, dst_rect.h);

	// Clear the window and blit the output texture to it
	SDL_SetRenderDrawColor(main_window_renderer, 0, 0, 0, 255);
	SDL_RenderClear(main_window_renderer);
	SDL_RenderCopy(main_window_renderer, texture, NULL, &dst_rect);
	textlayer_render(main_window_renderer, src_surface, &dst_rect);
	if (capture_path[0] != '\0')
		capture_frame(&dst_rect);
	SDL_RenderPresent(main_window_renderer);

	// Save output rect to be used by mouse functions
	last_output_rect = dst_rect;

	update_mouse_confinement();
}

/**
 * In fullscreen, keeps the mouse pointer inside the picture so that it can
 * neither wander into the letterbox bars nor onto another monitor.  (During
 * gameplay the pointer is captured by relative mouse mode in any case.)
 */
static void update_mouse_confinement(void)
{
#if SDL_VERSION_ATLEAST(2, 0, 18)
	static bool confined = false;
	static SDL_Rect confined_rect;

	if (fullscreen_display != -1)
	{
		if (!confined || !SDL_RectEquals(&confined_rect, &last_output_rect))
		{
			SDL_SetWindowMouseRect(main_window, &last_output_rect);
			confined_rect = last_output_rect;
			confined = true;
		}
	}
	else if (confined)
	{
		SDL_SetWindowMouseRect(main_window, NULL);
		confined = false;
	}
#endif
}

/** Maps a specified point in game screen coordinates to window coordinates. */
void mapScreenPointToWindow(Sint32 *const inout_x, Sint32 *const inout_y)
{
	*inout_x = (2 * *inout_x + 1) * last_output_rect.w / (2 * VGAScreen->w) + last_output_rect.x;
	*inout_y = (2 * *inout_y + 1) * last_output_rect.h / (2 * VGAScreen->h) + last_output_rect.y;
}

/** Maps a specified point in window coordinates to game screen coordinates. */
void mapWindowPointToScreen(Sint32 *const inout_x, Sint32 *const inout_y)
{
	*inout_x = (2 * (*inout_x - last_output_rect.x) + 1) * VGAScreen->w / (2 * last_output_rect.w);
	*inout_y = (2 * (*inout_y - last_output_rect.y) + 1) * VGAScreen->h / (2 * last_output_rect.h);
}

/** Scales a distance in window coordinates to game screen coordinates. */
void scaleWindowDistanceToScreen(Sint32 *const inout_x, Sint32 *const inout_y)
{
	*inout_x = (2 * *inout_x + 1) * VGAScreen->w / (2 * last_output_rect.w);
	*inout_y = (2 * *inout_y + 1) * VGAScreen->h / (2 * last_output_rect.h);
}
