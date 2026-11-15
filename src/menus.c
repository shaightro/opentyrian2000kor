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
#include "menus.h"

#include "config.h"
#include "episodes.h"
#include "font.h"
#include "fonthand.h"
#include "helptext.h"
#include "joystick.h"
#include "keyboard.h"
#include "mouse.h"
#include "network.h"
#include "nortsong.h"
#include "opentyr.h"
#include "palette.h"
#include "picload.h"
#include "sprite.h"
#include "textlayer.h"
#include "translate.h"
#include "vga256d.h"
#include "video.h"

char episode_name[6][TEXT_SIZE(31)];
char difficulty_name[7][TEXT_SIZE(21)];
char gameplay_name[GAMEPLAY_NAME_COUNT][TEXT_SIZE(26)];
char timed_battle_name[4][TEXT_SIZE(23)];

bool gameplaySelect(void)
{
	enum MenuItemIndex
	{
		MENU_ITEM_1_PLAYER_FULL_GAME = 0,
		MENU_ITEM_1_PLAYER_ARCADE,
		MENU_ITEM_1_PLAYER_TIMED_BATTLE,
		MENU_ITEM_2_PLAYER_ARCADE,
		MENU_ITEM_NETWORK,
	};

	if (shopSpriteSheet.data == NULL)
		JE_loadCompShapes(&shopSpriteSheet, '1');  // need mouse pointer sprites

	bool restart = true;

	const size_t menuItemsCount = COUNTOF(gameplay_name) - 1;
	size_t selectedIndex = MENU_ITEM_1_PLAYER_FULL_GAME;

	const int xCenter = 320 / 2;
	const int yMenuHeader = 20;
	const int yMenuItems = 54;
	const int dyMenuItems = 24;
	const int hMenuItem = 13;
	int wMenuItem[COUNTOF(gameplay_name) - 1] = { 0 };

	for (; ; )
	{
		if (restart)
		{
			JE_loadPic(VGAScreen2, 2, false);

			// Draw header.
			draw_font_hv_shadow(VGAScreen2, xCenter, yMenuHeader, gameplay_name[0], large_font, centered, 15, -3, false, 2);
		}

		// Restore background and header.
		surface_copy(VGAScreen, VGAScreen2);

		// Draw menu items.
		for (size_t i = 0; i < menuItemsCount; ++i)
		{
			const char *const text = gameplay_name[i + 1];

			wMenuItem[i] = JE_textWidth(text, normal_font);
			const int x = xCenter - wMenuItem[i] / 2;
			const int y = yMenuItems + dyMenuItems * i;

			const bool selected = i == selectedIndex;
			const bool disabled = i == MENU_ITEM_NETWORK;

			draw_font_hv_shadow(VGAScreen, x, y, text, normal_font, left_aligned, 15, -4 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);
		}

		if (restart)
		{
			mouseCursor = MOUSE_POINTER_NORMAL;

			fade_palette(colors, 10, 0, 255);

			restart = false;
		}

		service_SDL_events(true);

		JE_mouseStart();
		JE_showVGA();
		JE_mouseReplace();

		bool mouseMoved = false;
		do
		{
			SDL_Delay(16);

			Uint16 oldMouseX = mouse_x;
			Uint16 oldMouseY = mouse_y;

			push_joysticks_as_keyboard();
			service_SDL_events(false);

			mouseMoved = mouse_x != oldMouseX || mouse_y != oldMouseY;
		} while (!(newkey || newmouse || mouseMoved));

		// Handle interaction.

		bool action = false;
		bool cancel = false;

		if (mouseMoved || newmouse)
		{
			// Find menu item that was hovered or clicked.
			for (size_t i = 0; i < menuItemsCount; ++i)
			{
				const int xMenuItem = xCenter - wMenuItem[i] / 2;
				if (mouse_x >= xMenuItem && mouse_x < xMenuItem + wMenuItem[i])
				{
					const int yMenuItem = yMenuItems + dyMenuItems * i;
					if (mouse_y >= yMenuItem && mouse_y < yMenuItem + hMenuItem)
					{
						if (selectedIndex != i)
						{
							JE_playSampleNum(S_CURSOR);

							selectedIndex = i;
						}

						if (newmouse && lastmouse_but == SDL_BUTTON_LEFT &&
						    lastmouse_x >= xMenuItem && lastmouse_x < xMenuItem + wMenuItem[i] &&
						    lastmouse_y >= yMenuItem && lastmouse_y < yMenuItem + hMenuItem)
						{
							action = true;
						}

						break;
					}
				}
			}
		}

		if (newmouse)
		{
			if (lastmouse_but == SDL_BUTTON_RIGHT)
			{
				JE_playSampleNum(S_SPRING);

				cancel = true;
			}
		}
		else if (newkey)
		{
			switch (lastkey_scan)
			{
			case SDL_SCANCODE_UP:
			{
				JE_playSampleNum(S_CURSOR);

				selectedIndex = selectedIndex == 0
					? menuItemsCount - 1
					: selectedIndex - 1;
				break;
			}
			case SDL_SCANCODE_DOWN:
			{
				JE_playSampleNum(S_CURSOR);

				selectedIndex = selectedIndex == menuItemsCount - 1
					? 0
					: selectedIndex + 1;
				break;
			}
			case SDL_SCANCODE_SPACE:
			case SDL_SCANCODE_RETURN:
			{
				action = true;
				break;
			}
			case SDL_SCANCODE_ESCAPE:
			{
				JE_playSampleNum(S_SPRING);

				cancel = true;
				break;
			}
			default:
				break;
			}
		}

		if (action)
		{
			switch (selectedIndex)
			{
			case MENU_ITEM_1_PLAYER_FULL_GAME:
			case MENU_ITEM_1_PLAYER_ARCADE:
			case MENU_ITEM_1_PLAYER_TIMED_BATTLE:
			case MENU_ITEM_2_PLAYER_ARCADE:
			{
				JE_playSampleNum(S_SELECT);

				fade_black(10);

				onePlayerAction = selectedIndex == MENU_ITEM_1_PLAYER_ARCADE;
				timedBattleMode = selectedIndex == MENU_ITEM_1_PLAYER_TIMED_BATTLE;
				twoPlayerMode = selectedIndex == MENU_ITEM_2_PLAYER_ARCADE;
				return true;
			}
			case MENU_ITEM_NETWORK:
			{
				JE_playSampleNum(S_SPRING);
				break;
			}
			default:
				break;
			}
		}

		if (cancel)
		{
			fade_black(15);

			return false;
		}
	}
}

bool episodeSelect(void)
{
	if (shopSpriteSheet.data == NULL)
		JE_loadCompShapes(&shopSpriteSheet, '1');  // need mouse pointer sprites

	bool restart = true;

	const size_t menuItemsCount = EPISODE_AVAILABLE;
	size_t selectedIndex = 0;

	const int xCenter = 320 / 2;
	const int yMenuHeader = 20;
	const int xMenuItem = 20;
	const int yMenuItems = 50;
	const int dyMenuItems = 30;
	const int hMenuItem = 13;
	int wMenuItem[EPISODE_AVAILABLE] = { 0 };

	for (; ; )
	{
		if (restart)
		{
			JE_loadPic(VGAScreen2, 2, false);

			// Draw header.
			draw_font_hv_shadow(VGAScreen2, xCenter, yMenuHeader, episode_name[0], large_font, centered, 15, -3, false, 2);
		}

		// Restore background and header.
		surface_copy(VGAScreen, VGAScreen2);

		// Draw menu items.
		for (size_t i = 0; i < menuItemsCount; ++i)
		{
			const char *const text = episode_name[i + 1];

			wMenuItem[i] = JE_textWidth(text, normal_font);
			const int y = yMenuItems + dyMenuItems * i;

			const bool selected = i == selectedIndex;
			const bool disabled = !episodeAvail[i];

			draw_font_hv_shadow(VGAScreen, xMenuItem, y, text, normal_font, left_aligned, 15, -4 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);
		}

		if (restart)
		{
			mouseCursor = MOUSE_POINTER_NORMAL;

			fade_palette(colors, 10, 0, 255);

			restart = false;
		}

		service_SDL_events(true);

		JE_mouseStart();
		JE_showVGA();
		JE_mouseReplace();

		bool mouseMoved = false;
		do
		{
			SDL_Delay(16);

			Uint16 oldMouseX = mouse_x;
			Uint16 oldMouseY = mouse_y;

			push_joysticks_as_keyboard();
			service_SDL_events(false);

			NETWORK_KEEP_ALIVE();

			mouseMoved = mouse_x != oldMouseX || mouse_y != oldMouseY;
		} while (!(newkey || newmouse || mouseMoved));

		// Handle interaction.

		bool action = false;
		bool cancel = false;

		if (mouseMoved || newmouse)
		{
			// Find menu item that was hovered or clicked.
			for (size_t i = 0; i < menuItemsCount; ++i)
			{
				if (mouse_x >= xMenuItem && mouse_x < xMenuItem + wMenuItem[i])
				{
					const int yMenuItem = yMenuItems + dyMenuItems * i;
					if (mouse_y >= yMenuItem && mouse_y < yMenuItem + hMenuItem)
					{
						if (selectedIndex != i)
						{
							JE_playSampleNum(S_CURSOR);

							selectedIndex = i;
						}

						if (newmouse && lastmouse_but == SDL_BUTTON_LEFT &&
						    lastmouse_x >= xMenuItem && lastmouse_x < xMenuItem + wMenuItem[i] &&
						    lastmouse_y >= yMenuItem && lastmouse_y < yMenuItem + hMenuItem)
						{
							action = true;
						}

						break;
					}
				}
			}
		}

		if (newmouse)
		{
			if (lastmouse_but == SDL_BUTTON_RIGHT)
			{
				JE_playSampleNum(S_SPRING);

				cancel = true;
			}
		}
		else if (newkey)
		{
			switch (lastkey_scan)
			{
			case SDL_SCANCODE_UP:
			{
				JE_playSampleNum(S_CURSOR);

				selectedIndex = selectedIndex == 0
					? menuItemsCount - 1
					: selectedIndex - 1;
				break;
			}
			case SDL_SCANCODE_DOWN:
			{
				JE_playSampleNum(S_CURSOR);

				selectedIndex = selectedIndex == menuItemsCount - 1
					? 0
					: selectedIndex + 1;
				break;
			}
			case SDL_SCANCODE_SPACE:
			case SDL_SCANCODE_RETURN:
			{
				action = true;
				break;
			}
			case SDL_SCANCODE_ESCAPE:
			{
				JE_playSampleNum(S_SPRING);

				cancel = true;
				break;
			}
			default:
				break;
			}
		}

		if (action)
		{
			if (episodeAvail[selectedIndex])
			{
				JE_playSampleNum(S_SELECT);

				fade_black(10);

				JE_initEpisode(selectedIndex + 1);
				initial_episode_num = episodeNum;
				return true;
			}
			else
			{
				JE_playSampleNum(S_SPRING);
			}
		}

		if (cancel)
		{
			fade_black(15);

			return false;
		}
	}
}

/** The difficulty levels on the menu, from the easiest. */
static const JE_shortint difficultyMenuLevels[] =
{
	DIFFICULTY_EASY,
	DIFFICULTY_NORMAL,
	DIFFICULTY_HARD,
	DIFFICULTY_IMPOSSIBLE,
	DIFFICULTY_INSANITY,
	DIFFICULTY_SUICIDE,
	DIFFICULTY_MANIACAL,
	DIFFICULTY_ZINGLON,
	DIFFICULTY_NORTANEOUS,
};

/*
 * The original menu offers Easy, Normal, Hard, Impossible, Suicide and Lord of the Game, the last
 * three hidden behind key codes.  This menu shows every level, and the ones the original skips
 * take their names from the high score table.
 */
static const char *difficulty_menu_name(JE_shortint level)
{
	switch (level)
	{
	case DIFFICULTY_EASY:       return difficulty_name[1];
	case DIFFICULTY_NORMAL:     return difficulty_name[2];
	case DIFFICULTY_HARD:       return difficulty_name[3];
	case DIFFICULTY_IMPOSSIBLE: return difficulty_name[4];
	case DIFFICULTY_SUICIDE:    return difficulty_name[5];
	case DIFFICULTY_ZINGLON:    return difficulty_name[6];
	default:                    return difficultyNameB[level];
	}
}

/*
 * Options for the game being started, shown below the chosen difficulty (OpenTyrian addition).
 * The full game offers all of them; the arcade games leave out money and shops, which mean
 * nothing there.
 */
enum
{
	GAME_OPTION_HALF_DAMAGE,
	GAME_OPTION_START_CASH,
	GAME_OPTION_GOOD_GEAR,
	GAME_OPTION_DIFFICULTY_RISE,
};

static const int fullGameOptions[] =
{
	GAME_OPTION_HALF_DAMAGE,
	GAME_OPTION_START_CASH,
	GAME_OPTION_GOOD_GEAR,
	GAME_OPTION_DIFFICULTY_RISE,
};

static const int arcadeOptions[] =
{
	GAME_OPTION_HALF_DAMAGE,
	GAME_OPTION_DIFFICULTY_RISE,
};

/*
 * The difficulty rise option has a note under it (the original game always rises, this one does
 * not unless asked), which pushes the rows after it down.
 */
static const int gameOptionNoteGap = 10;

/** Top of an option row (count is the start row). */
static int game_option_row_y(const int *options, size_t count, int y0, int dy, size_t row)
{
	int y = y0 + dy * (int)row;
	for (size_t i = 0; i < row && i < count; ++i)
		if (options[i] == GAME_OPTION_DIFFICULTY_RISE)
			y += gameOptionNoteGap;
	return y;
}

/** Draws the options and the start row below them; row is the selected one. */
static void draw_game_options(const int *options, size_t count, int y0, int dy, size_t row)
{
	const int xLabel = 48, xValue = 214;

	for (size_t i = 0; i <= count; ++i)
	{
		const int y = game_option_row_y(options, count, y0, dy, i);
		const int brightness = -4 + (i == row ? 2 : 0);

		const char *label = ui_text(UI_OPTIONS_START);
		const char *value = NULL;
		if (i < count)
		{
			switch (options[i])
			{
			case GAME_OPTION_HALF_DAMAGE:
				label = ui_text(UI_OPTIONS_HALF_DAMAGE);
				value = ui_text(halfDamageChoice ? UI_OPTION_ON : UI_OPTION_OFF);
				break;
			case GAME_OPTION_START_CASH:
				label = ui_text(UI_OPTIONS_START_CASH);
				value = ui_text(richStartChoice ? UI_OPTIONS_CASH_RICH : UI_OPTIONS_CASH_NORMAL);
				break;
			case GAME_OPTION_GOOD_GEAR:
				label = ui_text(UI_OPTIONS_GOOD_GEAR);
				value = ui_text(goodGearChoice ? UI_OPTION_ON : UI_OPTION_OFF);
				break;
			case GAME_OPTION_DIFFICULTY_RISE:
				label = ui_text(UI_OPTIONS_DIFFICULTY_RISE);
				value = ui_text(difficultyRiseChoice ? UI_OPTION_ON : UI_OPTION_OFF);
				break;
			}
		}

		draw_font_hv_shadow(VGAScreen, xLabel, y, label, normal_font, left_aligned, 15, brightness, false, 2);
		if (value != NULL)
			draw_font_hv_shadow(VGAScreen, xValue, y, value, normal_font, left_aligned, 15, brightness, false, 2);

		if (i < count && options[i] == GAME_OPTION_DIFFICULTY_RISE)
			JE_textShade(VGAScreen, xLabel + 8, y + 14, ui_text(UI_OPTIONS_DIFFICULTY_RISE_NOTE), 15, 0, PART_SHADE);
	}

	const char *help = ui_text(UI_OPTIONS_HELP);
	JE_textShade(VGAScreen, xLabel, 191, help, 15, 4, PART_SHADE);  // clear of the copyright line of the picture
}

static void toggle_game_option(int option)
{
	JE_playSampleNum(S_CLICK);

	switch (option)
	{
	case GAME_OPTION_HALF_DAMAGE:     halfDamageChoice = !halfDamageChoice; break;
	case GAME_OPTION_START_CASH:      richStartChoice = !richStartChoice; break;
	case GAME_OPTION_GOOD_GEAR:       goodGearChoice = !goodGearChoice; break;
	case GAME_OPTION_DIFFICULTY_RISE: difficultyRiseChoice = !difficultyRiseChoice; break;
	}
}

bool difficultySelect(DifficultyOptions withOptions)
{
	if (shopSpriteSheet.data == NULL)
		JE_loadCompShapes(&shopSpriteSheet, '1');  // need mouse pointer sprites

	bool restart = true;

	const size_t menuItemsCount = COUNTOF(difficultyMenuLevels);
	size_t selectedIndex = 1;

	const int xCenter = 320 / 2;
	const int yMenuHeader = 20;
	const int yMenuItems = 48;
	const int dyMenuItems = 16;
	const int hMenuItem = 13;
	int wMenuItem[COUNTOF(difficultyMenuLevels)] = { 0 };

	// Choosing a difficulty opens the options of the game below it (OpenTyrian addition).
	const int *options = withOptions == DIFFICULTY_OPTIONS_FULL_GAME ? fullGameOptions : arcadeOptions;
	const size_t optionsCount = withOptions == DIFFICULTY_OPTIONS_FULL_GAME ? COUNTOF(fullGameOptions)
	                          : withOptions == DIFFICULTY_OPTIONS_ARCADE ? COUNTOF(arcadeOptions)
	                          : 0;
	const size_t optionsRows = optionsCount + 1;  // the options, then start
	bool optionsOpen = false;
	size_t optionsRow = optionsCount;
	const int yOptions = yMenuItems + 24;
	const int dyOptions = 16;

	for (; ; )
	{
		if (restart)
		{
			JE_loadPic(VGAScreen2, 2, false);

			// Draw header.
			draw_font_hv_shadow(VGAScreen2, xCenter, yMenuHeader, difficulty_name[0], large_font, centered, 15, -3, false, 2);
		}

		// Restore background and header.
		surface_copy(VGAScreen, VGAScreen2);

		if (optionsOpen)
		{
			// Only the chosen difficulty stays above its options.
			const char *const text = difficulty_menu_name(difficultyMenuLevels[selectedIndex]);
			draw_font_hv_shadow(VGAScreen, xCenter, yMenuItems, text, normal_font, centered, 15, -2, false, 2);

			draw_game_options(options, optionsCount, yOptions, dyOptions, optionsRow);
		}
		else
		{
			for (size_t i = 0; i < menuItemsCount; ++i)
			{
				const char *const text = difficulty_menu_name(difficultyMenuLevels[i]);

				wMenuItem[i] = JE_textWidth(text, normal_font);
				const int x = xCenter - wMenuItem[i] / 2;
				const int y = yMenuItems + dyMenuItems * i;

				draw_font_hv_shadow(VGAScreen, x, y, text, normal_font, left_aligned, 15, -4 + (i == selectedIndex ? 2 : 0), false, 2);
			}
		}

		if (restart)
		{
			mouseCursor = MOUSE_POINTER_NORMAL;

			fade_palette(colors, 10, 0, 255);

			restart = false;
		}

		service_SDL_events(true);

		JE_mouseStart();
		JE_showVGA();
		JE_mouseReplace();

		bool mouseMoved = false;
		do
		{
			SDL_Delay(16);

			Uint16 oldMouseX = mouse_x;
			Uint16 oldMouseY = mouse_y;

			push_joysticks_as_keyboard();
			service_SDL_events(false);

			NETWORK_KEEP_ALIVE();

			mouseMoved = mouse_x != oldMouseX || mouse_y != oldMouseY;
		} while (!(newkey || newmouse || mouseMoved));

		// Handle interaction.

		if (optionsOpen)
		{
			bool toggle = false, start = false, back = false;

			if (mouseMoved || newmouse)
			{
				for (size_t row = 0; row < optionsRows; ++row)
				{
					const int y = game_option_row_y(options, optionsCount, yOptions, dyOptions, row);
					if (mouse_x >= 40 && mouse_x < 300 && mouse_y >= y && mouse_y < y + hMenuItem)
					{
						if (optionsRow != row)
						{
							JE_playSampleNum(S_CURSOR);
							optionsRow = row;
						}
						if (newmouse && lastmouse_but == SDL_BUTTON_LEFT &&
						    lastmouse_x >= 40 && lastmouse_x < 300 && lastmouse_y >= y && lastmouse_y < y + hMenuItem)
						{
							if (row == optionsCount)
								start = true;
							else
								toggle = true;
						}
						break;
					}
				}
				if (newmouse && lastmouse_but == SDL_BUTTON_RIGHT)
					back = true;
			}
			else if (newkey)
			{
				switch (lastkey_scan)
				{
				case SDL_SCANCODE_UP:
					JE_playSampleNum(S_CURSOR);
					optionsRow = optionsRow == 0 ? optionsRows - 1 : optionsRow - 1;
					break;
				case SDL_SCANCODE_DOWN:
					JE_playSampleNum(S_CURSOR);
					optionsRow = optionsRow == optionsRows - 1 ? 0 : optionsRow + 1;
					break;
				case SDL_SCANCODE_LEFT:
				case SDL_SCANCODE_RIGHT:
					if (optionsRow < optionsCount)
						toggle = true;
					break;
				case SDL_SCANCODE_SPACE:
				case SDL_SCANCODE_RETURN:
					if (optionsRow < optionsCount)
						toggle = true;
					else
						start = true;
					break;
				case SDL_SCANCODE_ESCAPE:
					back = true;
					break;
				default:
					break;
				}
			}

			if (toggle)
				toggle_game_option(options[optionsRow]);

			if (back)
			{
				JE_playSampleNum(S_SPRING);
				optionsOpen = false;
			}

			if (start)
			{
				JE_playSampleNum(S_SELECT);

				difficultyLevel = difficultyMenuLevels[selectedIndex];

				fade_black(10);

				return true;
			}

			continue;
		}

		bool action = false;
		bool cancel = false;

		if (mouseMoved || newmouse)
		{
			// Find menu item that was hovered or clicked.
			for (size_t i = 0; i < menuItemsCount; ++i)
			{
				const int xMenuItem = xCenter - wMenuItem[i] / 2;
				if (mouse_x >= xMenuItem && mouse_x < xMenuItem + wMenuItem[i])
				{
					const int yMenuItem = yMenuItems + dyMenuItems * i;
					if (mouse_y >= yMenuItem && mouse_y < yMenuItem + hMenuItem)
					{
						if (selectedIndex != i)
						{
							JE_playSampleNum(S_CURSOR);

							selectedIndex = i;
						}

						if (newmouse && lastmouse_but == SDL_BUTTON_LEFT &&
						    lastmouse_x >= xMenuItem && lastmouse_x < xMenuItem + wMenuItem[i] &&
						    lastmouse_y >= yMenuItem && lastmouse_y < yMenuItem + hMenuItem)
						{
							action = true;
						}

						break;
					}
				}
			}
		}

		if (newmouse)
		{
			if (lastmouse_but == SDL_BUTTON_RIGHT)
			{
				JE_playSampleNum(S_SPRING);

				cancel = true;
			}
		}
		else if (newkey)
		{
			switch (lastkey_scan)
			{
			case SDL_SCANCODE_UP:
			{
				JE_playSampleNum(S_CURSOR);

				selectedIndex = selectedIndex == 0
					? menuItemsCount - 1
					: selectedIndex - 1;
				break;
			}
			case SDL_SCANCODE_DOWN:
			{
				JE_playSampleNum(S_CURSOR);

				selectedIndex = selectedIndex == menuItemsCount - 1
					? 0
					: selectedIndex + 1;
				break;
			}
			case SDL_SCANCODE_SPACE:
			case SDL_SCANCODE_RETURN:
			{
				action = true;
				break;
			}
			case SDL_SCANCODE_ESCAPE:
			{
				JE_playSampleNum(S_SPRING);

				cancel = true;
				break;
			}
			default:
				break;
			}
		}

		if (action && optionsCount > 0)
		{
			JE_playSampleNum(S_SELECT);

			optionsOpen = true;
			optionsRow = optionsCount;
			continue;
		}

		if (action)
		{
			JE_playSampleNum(S_SELECT);

			difficultyLevel = difficultyMenuLevels[selectedIndex];

			fade_black(10);

			return true;
		}

		if (cancel)
		{
			fade_black(15);

			return false;
		}
	}
}

bool timedBattleSelect(void)
{
	if (shopSpriteSheet.data == NULL)
		JE_loadCompShapes(&shopSpriteSheet, '1');  // need mouse pointer sprites

	bool restart = true;

	const size_t menuItemsCount = COUNTOF(timed_battle_name) - 1;
	size_t selectedIndex = 0;

	const int xCenter = 320 / 2;
	const int yMenuHeader = 20;
	const int yMenuItems = 54;
	const int dyMenuItems = 24;
	const int hMenuItem = 13;
	int wMenuItem[COUNTOF(timed_battle_name) - 1] = { 0 };

	for (; ; )
	{
		if (restart)
		{
			JE_loadPic(VGAScreen2, 2, false);

			// Draw header.
			draw_font_hv_shadow(VGAScreen2, xCenter, yMenuHeader, timed_battle_name[0], large_font, centered, 15, -3, false, 2);
		}

		// Restore background and header.
		surface_copy(VGAScreen, VGAScreen2);

		// Draw menu items.
		for (size_t i = 0; i < menuItemsCount; ++i)
		{
			const char *const text = timed_battle_name[i + 1];

			wMenuItem[i] = JE_textWidth(text, normal_font);
			const int x = xCenter - wMenuItem[i] / 2;
			const int y = yMenuItems + dyMenuItems * i;

			const bool selected = i == selectedIndex;

			draw_font_hv_shadow(VGAScreen, x, y, text, normal_font, left_aligned, 15, -4 + (selected ? 2 : 0), false, 2);
		}

		if (restart)
		{
			mouseCursor = MOUSE_POINTER_NORMAL;

			fade_palette(colors, 10, 0, 255);

			restart = false;
		}

		service_SDL_events(true);

		JE_mouseStart();
		JE_showVGA();
		JE_mouseReplace();

		bool mouseMoved = false;
		do
		{
			SDL_Delay(16);

			Uint16 oldMouseX = mouse_x;
			Uint16 oldMouseY = mouse_y;

			push_joysticks_as_keyboard();
			service_SDL_events(false);

			NETWORK_KEEP_ALIVE();

			mouseMoved = mouse_x != oldMouseX || mouse_y != oldMouseY;
		} while (!(newkey || newmouse || mouseMoved));

		// Handle interaction.

		bool action = false;
		bool cancel = false;

		if (mouseMoved || newmouse)
		{
			// Find menu item that was hovered or clicked.
			for (size_t i = 0; i < menuItemsCount; ++i)
			{
				const int xMenuItem = xCenter - wMenuItem[i] / 2;
				if (mouse_x >= xMenuItem && mouse_x < xMenuItem + wMenuItem[i])
				{
					const int yMenuItem = yMenuItems + dyMenuItems * i;
					if (mouse_y >= yMenuItem && mouse_y < yMenuItem + hMenuItem)
					{
						if (selectedIndex != i)
						{
							JE_playSampleNum(S_CURSOR);

							selectedIndex = i;
						}

						if (newmouse && lastmouse_but == SDL_BUTTON_LEFT &&
						    lastmouse_x >= xMenuItem && lastmouse_x < xMenuItem + wMenuItem[i] &&
						    lastmouse_y >= yMenuItem && lastmouse_y < yMenuItem + hMenuItem)
						{
							action = true;
						}

						break;
					}
				}
			}
		}

		if (newmouse)
		{
			if (lastmouse_but == SDL_BUTTON_RIGHT)
			{
				JE_playSampleNum(S_SPRING);

				cancel = true;
			}
		}
		else if (newkey)
		{
			switch (lastkey_scan)
			{
			case SDL_SCANCODE_UP:
			{
				JE_playSampleNum(S_CURSOR);

				selectedIndex = selectedIndex == 0
					? menuItemsCount - 1
					: selectedIndex - 1;
				break;
			}
			case SDL_SCANCODE_DOWN:
			{
				JE_playSampleNum(S_CURSOR);

				selectedIndex = selectedIndex == menuItemsCount - 1
					? 0
					: selectedIndex + 1;
				break;
			}
			case SDL_SCANCODE_SPACE:
			case SDL_SCANCODE_RETURN:
			{
				action = true;
				break;
			}
			case SDL_SCANCODE_ESCAPE:
			{
				JE_playSampleNum(S_SPRING);

				cancel = true;
				break;
			}
			default:
				break;
			}
		}

		if (action)
		{
			JE_playSampleNum(S_SELECT);

			fade_black(10);

			// I don't know how T2K determines episode from this,
			// I'm assuming it's hardcoded
			JE_initEpisode(selectedIndex == 0 ? 1 : 5);
			timeBattleSelection = selectedIndex + 1;
			return true;
		}

		if (cancel)
		{
			fade_black(15);

			return false;
		}
	}
}
