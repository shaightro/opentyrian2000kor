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

/** Draws the Easy extras below the difficulty list; row is the selected one. */
static void draw_easy_extras(int y0, int dy, size_t row)
{
	const UiText labels[] = { UI_EASY_HALF_DAMAGE, UI_EASY_START_CASH, UI_EASY_GOOD_GEAR, UI_EASY_START };
	const int xLabel = 48, xValue = 214;

	for (size_t i = 0; i < COUNTOF(labels); ++i)
	{
		const int y = y0 + dy * (int)i;
		const int brightness = -4 + (i == row ? 2 : 0);

		draw_font_hv_shadow(VGAScreen, xLabel, y, ui_text(labels[i]), normal_font, left_aligned, 15, brightness, false, 2);

		const char *value = NULL;
		switch (i)
		{
		case 0: value = ui_text(easyHalfDamageChoice ? UI_OPTION_ON : UI_OPTION_OFF); break;
		case 1: value = ui_text(easyRichStartChoice ? UI_EASY_CASH_RICH : UI_EASY_CASH_NORMAL); break;
		case 2: value = ui_text(easyGoodGearChoice ? UI_OPTION_ON : UI_OPTION_OFF); break;
		}
		if (value != NULL)
			draw_font_hv_shadow(VGAScreen, xValue, y, value, normal_font, left_aligned, 15, brightness, false, 2);
	}

	const char *help = ui_text(UI_EASY_HELP);
	JE_textShade(VGAScreen, xLabel, 191, help, 15, 4, PART_SHADE);  // clear of the copyright line of the picture
}

/** Changes the Easy extra on the given row (0 to 2). */
static void toggle_easy_extra(size_t row)
{
	JE_playSampleNum(S_CLICK);

	switch (row)
	{
	case 0: easyHalfDamageChoice = !easyHalfDamageChoice; break;
	case 1: easyRichStartChoice = !easyRichStartChoice; break;
	case 2: easyGoodGearChoice = !easyGoodGearChoice; break;
	}
}

bool difficultySelect(bool easyExtras)
{
	if (shopSpriteSheet.data == NULL)
		JE_loadCompShapes(&shopSpriteSheet, '1');  // need mouse pointer sprites

	bool restart = true;

	const size_t menuItemsCount = COUNTOF(difficulty_name) - 1;
	size_t menuItemsVisibleCount = menuItemsCount - 3;
	size_t selectedIndex = 1;
	size_t lordProgress = 0;

	const int xCenter = 320 / 2;
	const int yMenuHeader = 20;
	const int yMenuItems = 54;
	const int dyMenuItems = 24;
	const int hMenuItem = 13;
	int wMenuItem[COUNTOF(difficulty_name) - 1] = { 0 };

	// Easy extras for a 1-player full game (OpenTyrian addition): choosing Easy opens a panel
	// below the list with half damage, starting cash and good gear in the first shop.
	bool extrasOpen = false;
	size_t extrasRow = 3;  // 0 to 2: the options, 3: start
	const int yExtras = yMenuItems + dyMenuItems * 3 + 2;
	const int dyExtras = 15;
	const size_t extrasRows = 4;

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

		// Draw menu items (with the Easy extras open, only Easy to Hard, Easy chosen).
		for (size_t i = 0; i < (extrasOpen ? MIN(menuItemsVisibleCount, 3) : menuItemsVisibleCount); ++i)
		{
			const char *const text = difficulty_name[i + 1];

			wMenuItem[i] = JE_textWidth(text, normal_font);
			const int x = xCenter - wMenuItem[i] / 2;
			const int y = yMenuItems + dyMenuItems * i;

			const bool selected = extrasOpen ? i == 0 : i == selectedIndex;

			draw_font_hv_shadow(VGAScreen, x, y, text, normal_font, left_aligned, 15, -4 + (selected ? 2 : 0), false, 2);
		}

		if (extrasOpen)
			draw_easy_extras(yExtras, dyExtras, extrasRow);

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

		if (menuItemsVisibleCount == 5)
		{
			const SDL_Scancode lordKeys[] = { SDL_SCANCODE_L, SDL_SCANCODE_O, SDL_SCANCODE_R, SDL_SCANCODE_D };

			for (size_t i = 0; ; ++i)
			{
				if (i == COUNTOF(lordKeys))
				{
					menuItemsVisibleCount = 6;
					break;
				}

				if (!keysactive[lordKeys[i]])
					break;
			}

			if (newkey)
			{
				// Due to key jamming, holding down 4 keys simultaneous may not be possible, so an
				// alternate input method is also provided.
				if (lordProgress < COUNTOF(lordKeys) &&
					lastkey_scan == lordKeys[lordProgress])
				{
					lordProgress += 1;

					if (lordProgress == COUNTOF(lordKeys))
						menuItemsVisibleCount = 6;
				}
				else
				{
					lordProgress = 0;
				}
			}
		}
		if (SDL_GetModState() & KMOD_SHIFT)
		{
			if (menuItemsVisibleCount == 4 && keysactive[SDL_SCANCODE_RIGHTBRACKET])
				menuItemsVisibleCount = 5;
			if (menuItemsVisibleCount == 3 && keysactive[SDL_SCANCODE_G])
				menuItemsVisibleCount = 4;
		}

		if (extrasOpen)
		{
			bool toggle = false, start = false, back = false;

			if (mouseMoved || newmouse)
			{
				for (size_t row = 0; row < extrasRows; ++row)
				{
					const int y = yExtras + dyExtras * (int)row;
					if (mouse_x >= 40 && mouse_x < 300 && mouse_y >= y && mouse_y < y + hMenuItem)
					{
						if (extrasRow != row)
						{
							JE_playSampleNum(S_CURSOR);
							extrasRow = row;
						}
						if (newmouse && lastmouse_but == SDL_BUTTON_LEFT &&
						    lastmouse_x >= 40 && lastmouse_x < 300 && lastmouse_y >= y && lastmouse_y < y + hMenuItem)
						{
							if (row == extrasRows - 1)
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
					extrasRow = extrasRow == 0 ? extrasRows - 1 : extrasRow - 1;
					break;
				case SDL_SCANCODE_DOWN:
					JE_playSampleNum(S_CURSOR);
					extrasRow = extrasRow == extrasRows - 1 ? 0 : extrasRow + 1;
					break;
				case SDL_SCANCODE_LEFT:
				case SDL_SCANCODE_RIGHT:
					if (extrasRow < extrasRows - 1)
						toggle = true;
					break;
				case SDL_SCANCODE_SPACE:
				case SDL_SCANCODE_RETURN:
					if (extrasRow < extrasRows - 1)
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
				toggle_easy_extra(extrasRow);

			if (back)
			{
				JE_playSampleNum(S_SPRING);
				extrasOpen = false;
			}

			if (start)
			{
				JE_playSampleNum(S_SELECT);

				difficultyLevel = DIFFICULTY_EASY;

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
			for (size_t i = 0; i < menuItemsVisibleCount; ++i)
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
					? menuItemsVisibleCount - 1
					: selectedIndex - 1;
				break;
			}
			case SDL_SCANCODE_DOWN:
			{
				JE_playSampleNum(S_CURSOR);

				selectedIndex = selectedIndex == menuItemsVisibleCount - 1
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

		if (action && selectedIndex == 0 && easyExtras)
		{
			JE_playSampleNum(S_SELECT);

			extrasOpen = true;
			extrasRow = extrasRows - 1;
			continue;
		}

		if (action)
		{
			JE_playSampleNum(S_SELECT);

			switch (selectedIndex)
			{
			case 0:
				difficultyLevel = DIFFICULTY_EASY;
				break;
			case 1:
				difficultyLevel = DIFFICULTY_NORMAL;
				break;
			case 2:
				difficultyLevel = DIFFICULTY_HARD;
				break;
			case 3:
				difficultyLevel = DIFFICULTY_IMPOSSIBLE;
				break;
			case 4:
				difficultyLevel = DIFFICULTY_SUICIDE;
				break;
			case 5:
				difficultyLevel = DIFFICULTY_ZINGLON;
				break;
			}

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
