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
#ifndef TRANSLATE_H
#define TRANSLATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/*
 * Translations of the game's text, read from gettext PO files in the
 * "kor/lang" folder next to the executable.  Each string has a key (the PO
 * msgctxt) derived from where the game reads it:
 *
 *   help.<array>.<i>[.<j>]   tyrian.hdt help, menu and other text
 *   item.<kind>.<i>          item names (weapon, ship, special, ...)
 *   ep<e>.<n>                string n of the episode script levels<e>.dat
 *   level.<NAME>             level names, by their English name
 *   cube<e>.<c>.<title|header|n>  data cube c of cubetxt<e>.dat
 *   credits.<n>              tyrian.cdt
 *   ui.<name>                text in the program itself (see UiText)
 *
 * A missing or empty translation leaves the English text.
 */

/** Loads the translations; returns the number of translated strings. */
int tr_init(void);
void tr_deinit(void);

/** Translation of a key, or NULL. */
const char *tr_lookup(const char *key);

/**
 * Reads an encrypted string of the game data (like
 * read_encrypted_pascal_string()) and replaces it with its translation.
 */
void tr_read_encrypted(char *dst, size_t size, FILE *f, const char *key);

/** Replaces a string with its translation, if any (cut to fit \p size). */
void tr_apply(char *dst, size_t size, const char *key);

/** Copies UTF-8 text, cutting it to fit without splitting a character. */
void tr_copy_text(char *dst, size_t size, const char *src);

/**
 * Formats translated text: replaces "%s" with \p text and "%d" with \p number.
 * Unlike printf, a mistake in a translation cannot do harm.
 */
const char *tr_format(char *buffer, size_t size, const char *format, const char *text, int number);

/** Translated name of a level for display; level names stay English internally. */
const char *tr_level_name(const char *name);

/** Name of a save slot for display: the automatic "LAST LEVEL" save is translated. */
const char *tr_save_name(const char *name);

/*
 * While a collector is set, tr_read_encrypted() and tr_apply() report each
 * string to it instead of translating; used to extract the game's text.
 */
typedef void (*TrCollector)(const char *key, const char *text, size_t size);
void tr_set_collector(TrCollector collector);
TrCollector tr_get_collector(void);

/* Text in the program itself ************************************************/

typedef enum
{
	UI_MENU_SETUP,

	UI_SETUP_TITLE,
	UI_SETUP_GRAPHICS,
	UI_SETUP_GRAPHICS_DESC,
	UI_SETUP_SOUND,
	UI_SETUP_SOUND_DESC,
	UI_SETUP_AUTO_FIRE_SPECIAL,
	UI_SETUP_AUTO_FIRE_SPECIAL_DESC,
	UI_SETUP_DROP_HINTS,
	UI_SETUP_DROP_HINTS_DESC,
	UI_SETUP_JUKEBOX,
	UI_SETUP_JUKEBOX_DESC,
	UI_SETUP_DONE,
	UI_SETUP_DONE_DESC,

	UI_GRAPHICS_TITLE,
	UI_GRAPHICS_DISPLAY,
	UI_GRAPHICS_DISPLAY_DESC,
	UI_GRAPHICS_WINDOW_SIZE,
	UI_GRAPHICS_WINDOW_SIZE_DESC,
	UI_GRAPHICS_SCALER,
	UI_GRAPHICS_SCALER_DESC,
	UI_GRAPHICS_SCALING_MODE,
	UI_GRAPHICS_SCALING_MODE_DESC,
	UI_DONE_PREVIOUS_DESC,

	UI_SOUND_TITLE,
	UI_SOUND_MUSIC_VOLUME,
	UI_SOUND_EFFECTS_VOLUME,
	UI_SOUND_VOLUME_DESC,

	UI_DISPLAY_WINDOW,
	UI_DISPLAY_N,
	UI_WINDOW_SIZE_AUTO,
	UI_SCALING_CENTER,
	UI_SCALING_INTEGER,
	UI_SCALING_FIT_8_5,
	UI_SCALING_FIT_4_3,
	UI_OPTION_ON,
	UI_OPTION_OFF,
	UI_EASY_HALF_DAMAGE,
	UI_EASY_START_CASH,
	UI_EASY_CASH_NORMAL,
	UI_EASY_CASH_RICH,
	UI_EASY_GOOD_GEAR,
	UI_EASY_START,
	UI_EASY_HELP,

	UI_JUKEBOX_QUIT,
	UI_JUKEBOX_KEYS,

	UI_WARNING,
	UI_CHEATERS,
	UI_DESTRUCT_CUSTOM,

	UI_NEXT_LEVEL,
	UI_COST,
	UI_SHOP_DPS,
	UI_SHOP_DAMAGE_PER_SHOT,
	UI_CUSTOM_SHIP,
	UI_OR_PLAY,
	UI_TYPE_AT_TITLE,
	UI_PAGE_OF,
	UI_LAST_LEVEL_SLOT,
	UI_DESTRUCT_DAMAGE,
	UI_DESTRUCT_POINTS,

	UI_TEXT_COUNT
} UiText;

/** Text of the program itself, translated if possible. */
const char *ui_text(UiText id);

/** Key, English text and translator's note of a UiText. */
void ui_text_info(UiText id, const char **key, const char **english, const char **note);

#endif /* TRANSLATE_H */
