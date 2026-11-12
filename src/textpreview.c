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
#include "textpreview.h"

#include "episodes.h"
#include "file.h"
#include "fonthand.h"
#include "game_menu.h"
#include "helptext.h"
#include "keyboard.h"
#include "lvlmast.h"
#include "mainint.h"
#include "nortsong.h"
#include "opentyr.h"
#include "palette.h"
#include "picload.h"
#include "player.h"
#include "translate.h"
#include "vga256d.h"
#include "video.h"

#include "SDL.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#ifdef _MSC_VER
#include <direct.h>
#define mkdir _mkdir
#endif

bool textpreview_requested = false;

static char out_dir[1024];
static FILE *index_file;
static int shot_count;

/** Shows the current VGAScreen and saves the frame as <name>.bmp. */
static void shot(const char *name, const char *what)
{
	char path[1200];
	snprintf(path, sizeof(path), "%s/%s.bmp", out_dir, name);
	video_capture_next_frame(path);
	JE_showVGA();
	service_SDL_events(false);

	if (index_file != NULL)
		fprintf(index_file, "%s\t%s\n", name, what);
	++shot_count;
}

/* Briefings and hints *********************************************************/

/** Adds the lines of \p text (separated by '\n') to levelWarningText. */
static void add_warning_lines(const char *text, bool translated)
{
	for (const char *line = text; line != NULL && levelWarningLines < COUNTOF(levelWarningText); )
	{
		const char *end = strchr(line, '\n');
		const size_t len = end != NULL ? (size_t)(end - line) : strlen(line);

		char buffer[256];
		SDL_strlcpy(buffer, line, MIN(len + 1, sizeof(buffer)));
		if (translated)
			tr_copy_text(levelWarningText[levelWarningLines++], sizeof(*levelWarningText), buffer);
		else
			SDL_strlcpy(levelWarningText[levelWarningLines++], buffer, sizeof(*levelWarningText));

		line = end != NULL ? end + 1 : NULL;
	}
}

/**
 * Draws a briefing ("]W" line \p w_line) or, with a NULL \p w_line, a hint
 * screen, as JE_displayText() shows it once a key has skipped the glow.
 */
static void draw_text_screen(int episode, int first, const char *english, const char *w_line)
{
	char key[32];
	snprintf(key, sizeof(key), "ep%d.%d", episode, first);
	const char *translation = tr_lookup(key);

	levelWarningLines = 0;
	if (w_line == NULL)
	{
		// after a level: the score, then the hint
		snprintf(levelWarningText[0], sizeof(*levelWarningText), "%s %lu", miscText[37], 123456UL);
		strcpy(levelWarningText[1], "");
		levelWarningLines = 2;
	}
	add_warning_lines(translation != NULL ? translation : english, translation != NULL);

	memcpy(colors, palettes[0], sizeof(colors));
	set_palette(colors, 0, 255);

	VGAScreen = VGAScreenSeg;
	JE_clr256(VGAScreen);

	levelWarningDisplay = w_line != NULL && strlen(w_line) > 2 && w_line[2] == 'y';
	warningRed = w_line != NULL && strlen(w_line) > 4 && atoi(w_line + 4) / 10 != 0;
	useLastBank = false;
	frameCountMax = 0;  // no glow animation

	int y = warningRed ? 2 : 55;
	for (int i = 0; i < levelWarningLines; ++i)
	{
		JE_outCharGlow(10, y, levelWarningText[i]);
		y += 10;
	}

	textGlowFont = TINY_FONT;
	JE_outCharGlow(JE_fontCenter(miscText[4], TINY_FONT), warningRed ? 7 * 16 + 6 : 184, miscText[4]);

	if (levelWarningDisplay)
	{
		// the flashing bars of JE_updateWarning()
		fill_rectangle_xy(VGAScreen, 0, 0, 319, 5, 14 * 16 + 7);
		fill_rectangle_xy(VGAScreen, 0, 194, 319, 199, 14 * 16 + 7);
	}

	char name[64], what[128];
	snprintf(name, sizeof(name), "ep%d-%04d-%s", episode, first, w_line != NULL ? "briefing" : "hint");
	snprintf(what, sizeof(what), "%s (%s)", key, translation != NULL ? "translated" : "ENGLISH");
	shot(name, what);
}

/** Goes through levels<e>.dat like the game does and draws every text block. */
static void preview_episode_script(int episode)
{
	char file[16];
	snprintf(file, sizeof(file), "levels%d.dat", episode);
	FILE *f = dir_fopen(data_dir(), file, "rb");
	if (f == NULL)
		return;

	enum { NONE, BRIEFING, HINTS } mode = NONE;
	char s[256], w_line[256] = "";
	static char block[4096];
	int block_first = -1, skip = 0;
	block[0] = '\0';

	for (int n = 0; ; ++n)
	{
		const bool more = try_read_encrypted_pascal_string(s, sizeof(s), f);

		if (skip > 0 && more)
		{
			--skip;  // item lists of "]I"
			continue;
		}

		const bool block_end = !more || s[0] == '#' || s[0] == ']' || s[0] == '*';
		if (block_end && block_first >= 0)
		{
			draw_text_screen(episode, block_first, block, mode == BRIEFING ? w_line : NULL);
			block_first = -1;
			block[0] = '\0';
		}
		if (!more)
			break;

		if (s[0] == ']')
		{
			mode = s[1] == 'W' ? BRIEFING : s[1] == 'Q' ? HINTS : NONE;
			if (mode == BRIEFING)
				SDL_strlcpy(w_line, s, sizeof(w_line));
			if (s[1] == 'I')
				skip = 9;
		}
		else if (s[0] == '*')
		{
			mode = NONE;
		}
		else if (s[0] == '#')
		{
			if (mode == BRIEFING)
				mode = NONE;
		}
		else if (mode != NONE)
		{
			if (block_first < 0)
				block_first = n;
			else
				SDL_strlcat(block, "\n", sizeof(block));
			SDL_strlcat(block, s, sizeof(block));
		}
	}

	fclose(f);
}

/* Credits *********************************************************************/

static void preview_credits(void)
{
	enum { lines_max = 126 };
	static char credstr[lines_max][TEXT_SIZE(66)];

	FILE *f = dir_fopen_die(data_dir(), "tyrian.cdt", "rb");
	for (int i = 0; i < lines_max; ++i)
	{
		char key[32];
		snprintf(key, sizeof(key), "credits.%d", i);
		tr_read_encrypted(credstr[i], sizeof(credstr[i]), f, key);
	}
	fclose(f);

	memcpy(colors, palettes[6 - 1], sizeof(colors));
	set_palette(colors, 0, 255);
	VGAScreen = VGAScreenSeg;

	// nine lines a screen, as they scroll by in JE_playCredits()
	for (int page = 0; page * 9 < lines_max; ++page)
	{
		JE_clr256(VGAScreen);

		int y = 20;
		for (int line = page * 9; line < page * 9 + 9 && line < lines_max; ++line, y += 20)
		{
			if (strcmp(credstr[line], ".") == 0 || strlen(credstr[line]) == 0)
				continue;

			const Uint8 color = credstr[line][0] - 65;
			const char *text = &credstr[line][1];
			const int x = 110 - JE_textWidth(text, SMALL_FONT_SHAPES) / 2;

			JE_outTextAdjust(VGAScreen, x + abs((y / 18) % 4 - 2) - 1, y - 1, text, color, -8, SMALL_FONT_SHAPES, false);
			JE_outTextAdjust(VGAScreen, x, y, text, color, -2, SMALL_FONT_SHAPES, false);
		}

		fill_rectangle_xy(VGAScreen, 0, 0, 319, 10, 0);
		fill_rectangle_xy(VGAScreen, 0, 190, 319, 199, 0);

		char name[32], what[64];
		snprintf(name, sizeof(name), "credits-%02d", page + 1);
		snprintf(what, sizeof(what), "credits.%d - credits.%d", page * 9, page * 9 + 8);
		shot(name, what);
	}
}

/* Ship specs ******************************************************************/

static void preview_ship_specs(void)
{
	JE_initEpisode(1);  // ship names

	for (int ship = 1; ship <= SHIP_NUM && ship <= HELPTEXT_SHIPINFO_COUNT; ++ship)
	{
		// the ship pictures that JE_drawShipSpecs() knows how to place
		const int graphic = ships[ship].bigshipgraphic;
		if (graphic != 28 && graphic != 32 && graphic != 33 && graphic != 45 && graphic != 46)
			continue;

		player[0].items.ship = ship;

		// as JE_doShipSpecs() in the game menu
		JE_loadPic(VGAScreen2, 1, false);
		set_palette(colors, 0, 255);
		JE_drawShipSpecs(VGAScreenSeg, VGAScreen2);
		VGAScreen = VGAScreenSeg;

		char name[32], what[64];
		snprintf(name, sizeof(name), "ship-%02d", ship);
		snprintf(what, sizeof(what), "item.ship.%d, help.shipInfo.%d", ship, ship - 1);
		shot(name, what);
	}

	JE_loadPic(VGAScreen2, 1, false);
}

/* Data cubes ******************************************************************/

static int count_cubes(int episode)
{
	char file[16];
	snprintf(file, sizeof(file), "cubetxt%d.dat", episode);
	FILE *f = dir_fopen(data_dir(), file, "rb");
	if (f == NULL)
		return 0;

	char s[256];
	int count = 0;
	while (try_read_encrypted_pascal_string(s, sizeof(s), f))
		if (s[0] == '*')
			++count;

	fclose(f);
	return MAX(0, count - 1);  // the last '*' only ends the file
}

static void preview_cubes(int episode)
{
	JE_initEpisode(episode);

	const int count = count_cubes(episode);
	for (int first = 1; first <= count; first += 4)
	{
		const int slots = MIN(4, count - first + 1);
		for (int slot = 0; slot < slots; ++slot)
			load_cube(slot, first + slot);

		char name[64], what[64];
		cube_preview_draw_list(slots);
		snprintf(name, sizeof(name), "cube%d-list-%02d", episode, first);
		snprintf(what, sizeof(what), "titles of cube%d.%d - cube%d.%d", episode, first, episode, first + slots - 1);
		shot(name, what);

		for (int slot = 0; slot < slots; ++slot)
		{
			for (int line = 0, page = 1; ; line += 9, ++page)
			{
				const int lines = cube_preview_draw_page(slot, line);
				snprintf(name, sizeof(name), "cube%d-%02d-p%d", episode, first + slot, page);
				snprintf(what, sizeof(what), "cube%d.%d, lines %d-%d of %d", episode, first + slot, line + 1, MIN(line + 9, lines), lines);
				shot(name, what);

				if (line + 9 >= lines)
					break;
			}
		}
	}
}

/* ****************************************************************************/

int textpreview_run(void)
{
	snprintf(out_dir, sizeof(out_dir), "%s/text-preview", base_dir());
#ifdef TARGET_WIN32
	mkdir(out_dir);  // may exist already
#else
	mkdir(out_dir, 0755);
#endif

	char path[1200];
	snprintf(path, sizeof(path), "%s/index.txt", out_dir);
	index_file = fopen(path, "w");
	if (index_file == NULL)
	{
		fprintf(stderr, "error: cannot write to %s\n", out_dir);
		return EXIT_FAILURE;
	}

	// 3x is enough to read the text and keeps the files small.
	if (fullscreen_display == -1)
		set_window_scale(3);

	for (int episode = 1; episode <= EPISODE_MAX; ++episode)
	{
		JE_initEpisode(episode);
		preview_episode_script(episode);
	}

	preview_credits();
	preview_ship_specs();

	for (int episode = 1; episode <= EPISODE_MAX; ++episode)
		preview_cubes(episode);

	fclose(index_file);
	printf("text preview: %d screens saved in %s\n", shot_count, out_dir);
	return EXIT_SUCCESS;
}
