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
#include "helptext.h"

#include "config.h"
#include "episodes.h"
#include "file.h"
#include "fonthand.h"
#include "hangul.h"
#include "menus.h"
#include "opentyr.h"
#include "sprite.h"
#include "translate.h"
#include "video.h"

#include <assert.h>
#include <string.h>

const JE_byte menuHelp[MENU_MAX][11] = /* [1..maxmenu, 1..11] */
{
	{  1, 34,  2,  3,  4,  5,                  0, 0, 0, 0, 0 },
	{  6,  7,  8,  9, 10, 11, 11, 12,                0, 0, 0 },
	{ 13, 14, 15, 15, 16, 17, 35, 12,                0, 0, 0 },
	{                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{  4, 30, 30,  3,  5,                   0, 0, 0, 0, 0, 0 },
	{  4, 37, 12,                     0, 0, 0, 0, 0, 0, 0, 0 },
	{ 16, 17, 15, 15, 12,                   0, 0, 0, 0, 0, 0 },
	{ 31, 31, 31, 31, 32, 12,                  0, 0, 0, 0, 0 },
	{  4, 34,  3,  5,                    0, 0, 0, 0, 0, 0, 0 },
	{ 35, 35, 35, 36, 12,                   0, 0, 0, 0, 0, 0 }
};

JE_byte verticalHeight = 7;
JE_byte helpBoxColor = 12;
JE_byte helpBoxBrightness = 1;
JE_byte helpBoxShadeType = FULL_SHADE;

char helpTxt[39][TEXT_SIZE(231)];                                                   /* [1..39] of string [230] */
char pName[21][TEXT_SIZE(16)];                                                      /* [1..21] of string [15] */
char miscText[HELPTEXT_MISCTEXT_COUNT][TEXT_SIZE(42)];                              /* [1..68] of string [41] */
char miscTextB[HELPTEXT_MISCTEXTB_COUNT][HELPTEXT_MISCTEXTB_SIZE];       /* [1..5] of string [10] */
char keyName[8][18];                                                     /* [1..8] of string [17] */
char menuText[7][HELPTEXT_MENUTEXT_SIZE];                                /* [1..7] of string [20] */
char outputs[9][TEXT_SIZE(31)];                                                     /* [1..9] of string [30] */
char topicName[6][TEXT_SIZE(21)];                                                   /* [1..6] of string [20] */
char mainMenuHelp[HELPTEXT_MAINMENUHELP_COUNT][TEXT_SIZE(66)];                      /* [1..34] of string [65] */
char inGameText[6][TEXT_SIZE(21)];                                                  /* [1..6] of string [20] */
char detailLevel[6][TEXT_SIZE(13)];                                                 /* [1..6] of string [12] */
char gameSpeedText[5][TEXT_SIZE(13)];                                               /* [1..5] of string [12] */
char inputDevices[3][TEXT_SIZE(13)];                                                /* [1..3] of string [12] */
char networkText[HELPTEXT_NETWORKTEXT_COUNT][HELPTEXT_NETWORKTEXT_SIZE]; /* [1..4] of string [20] */
char difficultyNameB[11][TEXT_SIZE(21)];                                            /* [0..9] of string [20] */
char joyButtonNames[5][TEXT_SIZE(21)];                                              /* [1..5] of string [20] */
char superShips[HELPTEXT_SUPERSHIPS_COUNT][TEXT_SIZE(26)];                          /* [0..10] of string [25] */
char specialName[HELPTEXT_SPECIALNAME_COUNT][TEXT_SIZE(10)];                        /* [1..9] of string [9] */
char destructHelp[25][TEXT_SIZE(22)];                                               /* [1..25] of string [21] */
char weaponNames[17][TEXT_SIZE(17)];                                                /* [1..17] of string [16] */
char destructModeName[DESTRUCT_MODES][TEXT_SIZE(13)];                               /* [1..destructmodes] of string [12] */
char shipInfo[HELPTEXT_SHIPINFO_COUNT][2][TEXT_SIZE(256)];                          /* [1..13, 1..2] of string */
char licensingInfo[3][TEXT_SIZE(46)];                                               /* [1..3] of string */
char orderingInfo[6][TEXT_SIZE(32)];                                                /* [1..6] of string */
char superTyrianText[6][TEXT_SIZE(64)];                                             /* [1..6] of string */
char menuInt[MENU_MAX+1][11][TEXT_SIZE(18)];                                        /* [0..14, 1..11] of string [17] */

static void decrypt_string(char *s, size_t len)
{
	static const unsigned char crypt_key[] = { 204, 129, 63, 255, 71, 19, 25, 62, 1, 99 };

	if (len == 0)
		return;

	for (size_t i = len - 1; ; --i)
	{
		s[i] ^= crypt_key[i % sizeof(crypt_key)];
		if (i == 0)
			break;
		s[i] ^= s[i - 1];
	}
}

void read_encrypted_pascal_string(char *s, size_t size, FILE *f)
{
	Uint8 len;
	char buffer[255];

	fread_u8_die(&len, 1, f);
	fread_die(buffer, 1, len, f);

	if (size == 0)
		return;

	decrypt_string(buffer, len);

	assert(len < size);

	len = MIN(len, size - 1);
	memcpy(s, buffer, len);
	s[len] = '\0';
}

bool try_read_encrypted_pascal_string(char *s, size_t size, FILE *f)
{
	int len = getc(f);
	if (len == EOF)
		return false;

	char buffer[255];
	if (fread(buffer, 1, (size_t)len, f) != (size_t)len)
		return false;

	decrypt_string(buffer, (size_t)len);

	const size_t n = MIN((size_t)len, size - 1);
	memcpy(s, buffer, n);
	s[n] = '\0';
	return true;
}

void skip_pascal_string(FILE *f)
{
	Uint8 len;
	char buffer[255];

	fread_u8_die(&len, 1, f);
	fread_die(buffer, 1, len, f);
}

/** Reads string i (and j) of an array of tyrian.hdt text, translated; see translate.h. */
static void read_help_string(char *s, size_t size, FILE *f, const char *array, int i, int j)
{
	char key[48];
	if (j < 0)
		snprintf(key, sizeof(key), "help.%s.%d", array, i);
	else
		snprintf(key, sizeof(key), "help.%s.%d.%d", array, i, j);

	tr_read_encrypted(s, size, f, key);
}

static int help_text_width(const char *start, const char *end)
{
	char buffer[TEXT_SIZE(256)];
	SDL_strlcpy(buffer, start, MIN((size_t)(end - start) + 1, sizeof(buffer)));
	return JE_textWidth(buffer, TINY_FONT);
}

/**
 * Word-wraps translated text by its width in pixels.  (The original wraps by
 * the number of bytes, which does not work for multi-byte characters.)
 */
static void help_box_wrap_by_width(SDL_Surface *screen, int x, int y, const char *message, unsigned int boxwidth)
{
	// boxwidth is in characters of the small font, about 5 pixels each.
	const int max_width = (int)boxwidth * 5;

	bool highlighted = false;
	const char *p = message;
	for (;;)
	{
		while (*p == ' ')
			++p;
		if (*p == '\0')
			break;

		// Take words while they fit.
		const char *line_end = p;
		for (const char *word = p; *word != '\0'; )
		{
			const char *word_end = word;
			while (*word_end != '\0' && *word_end != ' ')
				++word_end;

			if (help_text_width(p, word_end) <= max_width)
			{
				line_end = word_end;
			}
			else if (line_end == p)
			{
				// A single word too long for the line: break it between characters.
				for (const char *c = p; c < word_end; )
				{
					Uint32 codepoint;
					const char *next = c + MAX(1, text_decode_char(c, &codepoint));
					if (line_end != p && help_text_width(p, next) > max_width)
						break;
					line_end = next;
					c = next;
				}
				break;
			}
			else
			{
				break;
			}

			word = word_end;
			while (*word == ' ')
				++word;
		}

		// Every line is drawn starting unhighlighted, so a ~highlight~ that
		// goes on to the next line is opened again there.
		char line[TEXT_SIZE(256) + 1];
		size_t len = 0;
		if (highlighted)
			line[len++] = '~';
		SDL_strlcpy(line + len, p, MIN((size_t)(line_end - p) + 1, sizeof(line) - len));
		JE_textShade(screen, x, y, line, helpBoxColor, helpBoxBrightness, helpBoxShadeType);
		y += verticalHeight;

		for (const char *c = p; c < line_end; ++c)
			if (*c == '~')
				highlighted = !highlighted;

		p = line_end;
	}
}

void JE_helpBox(SDL_Surface *screen,  int x, int y, const char *message, unsigned int boxwidth)
{
	JE_byte startpos, endpos, pos;
	JE_boolean endstring;

	char substring[256];

	if (strlen(message) == 0)
	{
		return;
	}

	for (const char *c = message; *c != '\0'; ++c)
	{
		if ((unsigned char)*c >= 0x80)
		{
			help_box_wrap_by_width(screen, x, y, message, boxwidth);

			helpBoxColor = 12;
			helpBoxShadeType = FULL_SHADE;
			return;
		}
	}

	pos = 1;
	endpos = 0;
	endstring = false;

	do
	{
		startpos = endpos + 1;

		do
		{
			endpos = pos;
			do
			{
				pos++;
				if (pos == strlen(message))
				{
					endstring = true;
					if ((unsigned)(pos - startpos) < boxwidth)
					{
						endpos = pos + 1;
					}
				}

			} while (!(message[pos-1] == ' ' || endstring));

		} while (!((unsigned)(pos - startpos) > boxwidth || endstring));

		SDL_strlcpy(substring, message + startpos - 1, MIN((size_t)(endpos - startpos + 1), sizeof(substring)));
		JE_textShade(screen, x, y, substring, helpBoxColor, helpBoxBrightness, helpBoxShadeType);

		y += verticalHeight;

	} while (!endstring);

	if (endpos != pos + 1)
	{
		JE_textShade(screen, x, y, message + endpos, helpBoxColor, helpBoxBrightness, helpBoxShadeType);
	}

	helpBoxColor = 12;
	helpBoxShadeType = FULL_SHADE;
}

void JE_HBox(SDL_Surface *screen, int x, int y, unsigned int  messagenum, unsigned int boxwidth)
{
	JE_helpBox(screen, x, y, helpTxt[messagenum-1], boxwidth);
}

void JE_loadHelpText(void)
{
	const unsigned int menuInt_entries[MENU_MAX + 1] = { -1, 7, 9, 9, -1, -1, 11, -1, -1, -1, 6, 4, 7, 7, 5, 6 };
	const unsigned int setup_entries[10] = {10, 5, 4, 4, 5, 7, 7, 21, 3, 3};
	
	FILE *f = dir_fopen_die(data_dir(), "tyrian.hdt", "rb");
	fread_s32_die(&episode1DataLoc, 1, f);

	/*Online Help*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(helpTxt); ++i)
		read_help_string(helpTxt[i], sizeof(helpTxt[i]), f, "helpTxt", (int)i, -1);
	skip_pascal_string(f);

	/*Planet names*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(pName); ++i)
		read_help_string(pName[i], sizeof(pName[i]), f, "pName", (int)i, -1);
	skip_pascal_string(f);

	/*Miscellaneous text*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(miscText); ++i)
		read_help_string(miscText[i], sizeof(miscText[i]), f, "miscText", (int)i, -1);
	skip_pascal_string(f);

	/*Little Miscellaneous text*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(miscTextB); ++i)
		read_help_string(miscTextB[i], sizeof(miscTextB[i]), f, "miscTextB", (int)i, -1);
	skip_pascal_string(f);

	/*Key names*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < menuInt_entries[6]; ++i)
		read_help_string(menuInt[6][i], sizeof(menuInt[6][i]), f, "menuInt", 6, (int)i);
	skip_pascal_string(f);

	/*Main Menu*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(menuText); ++i)
		read_help_string(menuText[i], sizeof(menuText[i]), f, "menuText", (int)i, -1);
	skip_pascal_string(f);

	// OpenTyrian2000 Override
	strcpy(menuText[6], menuText[5]);
	strcpy(menuText[5], menuText[4]);
	snprintf(menuText[4], sizeof(menuText[4]), "%s", ui_text(UI_MENU_SETUP));

	/*Event text*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(outputs); ++i)
		read_help_string(outputs[i], sizeof(outputs[i]), f, "outputs", (int)i, -1);
	skip_pascal_string(f);

	/*Help topics*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(topicName); ++i)
		read_help_string(topicName[i], sizeof(topicName[i]), f, "topicName", (int)i, -1);
	skip_pascal_string(f);

	/*Main Menu Help*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(mainMenuHelp); ++i)
		read_help_string(mainMenuHelp[i], sizeof(mainMenuHelp[i]), f, "mainMenuHelp", (int)i, -1);
	skip_pascal_string(f);

	/*Menu 1 - Main*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < menuInt_entries[1]; ++i)
		read_help_string(menuInt[1][i], sizeof(menuInt[1][i]), f, "menuInt", 1, (int)i);
	skip_pascal_string(f);

	/*Menu 2 - Items*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < menuInt_entries[2]; ++i)
		read_help_string(menuInt[2][i], sizeof(menuInt[2][i]), f, "menuInt", 2, (int)i);
	skip_pascal_string(f);

	/*Menu 3 - Options*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < menuInt_entries[3]; ++i)
		read_help_string(menuInt[3][i], sizeof(menuInt[3][i]), f, "menuInt", 3, (int)i);
	skip_pascal_string(f);

	/*InGame Menu*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(inGameText); ++i)
		read_help_string(inGameText[i], sizeof(inGameText[i]), f, "inGameText", (int)i, -1);
	skip_pascal_string(f);

	/*Detail Level*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(detailLevel); ++i)
		read_help_string(detailLevel[i], sizeof(detailLevel[i]), f, "detailLevel", (int)i, -1);
	skip_pascal_string(f);

	/*Game speed text*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(gameSpeedText); ++i)
		read_help_string(gameSpeedText[i], sizeof(gameSpeedText[i]), f, "gameSpeedText", (int)i, -1);
	skip_pascal_string(f);

	// episode names
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(episode_name); ++i)
		read_help_string(episode_name[i], sizeof(episode_name[i]), f, "episode_name", (int)i, -1);
	skip_pascal_string(f);

	// difficulty names
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(difficulty_name); ++i)
		read_help_string(difficulty_name[i], sizeof(difficulty_name[i]), f, "difficulty_name", (int)i, -1);
	skip_pascal_string(f);

	// gameplay mode names
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(gameplay_name); ++i)
		read_help_string(gameplay_name[i], sizeof(gameplay_name[i]), f, "gameplay_name", (int)i, -1);
	skip_pascal_string(f);

	/*Menu 10 - 2Player Main*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < menuInt_entries[10]; ++i)
		read_help_string(menuInt[10][i], sizeof(menuInt[10][i]), f, "menuInt", 10, (int)i);
	skip_pascal_string(f);

	/*Input Devices*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(inputDevices); ++i)
		read_help_string(inputDevices[i], sizeof(inputDevices[i]), f, "inputDevices", (int)i, -1);
	skip_pascal_string(f);

	/*Network text*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(networkText); ++i)
		read_help_string(networkText[i], sizeof(networkText[i]), f, "networkText", (int)i, -1);
	skip_pascal_string(f);

	/*Menu 11 - 2Player Network*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < menuInt_entries[11]; ++i)
		read_help_string(menuInt[11][i], sizeof(menuInt[11][i]), f, "menuInt", 11, (int)i);
	skip_pascal_string(f);

	/*HighScore Difficulty Names*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(difficultyNameB); ++i)
		read_help_string(difficultyNameB[i], sizeof(difficultyNameB[i]), f, "difficultyNameB", (int)i, -1);
	skip_pascal_string(f);

	/*Menu 12 - Network Options*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < menuInt_entries[12]; ++i)
		read_help_string(menuInt[12][i], sizeof(menuInt[12][i]), f, "menuInt", 12, (int)i);
	skip_pascal_string(f);

	/*Menu 13 - Joystick*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < menuInt_entries[13]; ++i)
		read_help_string(menuInt[13][i], sizeof(menuInt[13][i]), f, "menuInt", 13, (int)i);
	skip_pascal_string(f);

	/*Joystick Button Assignments*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(joyButtonNames); ++i)
		read_help_string(joyButtonNames[i], sizeof(joyButtonNames[i]), f, "joyButtonNames", (int)i, -1);
	skip_pascal_string(f);

	/*SuperShips - For Super Arcade Mode*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(superShips); ++i)
		read_help_string(superShips[i], sizeof(superShips[i]), f, "superShips", (int)i, -1);
	skip_pascal_string(f);

	/*SuperShips - For Super Arcade Mode*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(specialName); ++i)
		read_encrypted_pascal_string(specialName[i], sizeof(specialName[i]), f);  // secret codes typed at the title screen: never translated
	skip_pascal_string(f);

	/*Secret DESTRUCT game*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(destructHelp); ++i)
		read_help_string(destructHelp[i], sizeof(destructHelp[i]), f, "destructHelp", (int)i, -1);
	skip_pascal_string(f);

	/*Secret DESTRUCT weapons*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(weaponNames); ++i)
		read_help_string(weaponNames[i], sizeof(weaponNames[i]), f, "weaponNames", (int)i, -1);
	skip_pascal_string(f);

	/*Secret DESTRUCT modes*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(destructModeName); ++i)
		read_help_string(destructModeName[i], sizeof(destructModeName[i]), f, "destructModeName", (int)i, -1);
	skip_pascal_string(f);

	/*NEW: Ship Info*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(shipInfo); ++i)
	{
		read_help_string(shipInfo[i][0], sizeof(shipInfo[i][0]), f, "shipInfo", (int)i, 0);
		read_help_string(shipInfo[i][1], sizeof(shipInfo[i][1]), f, "shipInfo", (int)i, 1);
	}
	skip_pascal_string(f);

	/*Menu 14 - Super Tyrian*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < menuInt_entries[14]; ++i)
		read_help_string(menuInt[14][i], sizeof(menuInt[14][i]), f, "menuInt", 14, (int)i);
	skip_pascal_string(f);

	/*Timed Battle Planets*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(timed_battle_name); ++i)
		read_help_string(timed_battle_name[i], sizeof(timed_battle_name[i]), f, "timed_battle_name", (int)i, -1);
	skip_pascal_string(f);

	/*Setup entries, skipped since we don't use them*/
	for (unsigned int entry = 0; entry < COUNTOF(setup_entries); ++entry)
	{
		skip_pascal_string(f);
		for (unsigned int i = 0; i < setup_entries[entry]; ++i)
			skip_pascal_string(f);
		skip_pascal_string(f);
	}

	/*Menu 15 - Mouse Settings*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < menuInt_entries[15]; ++i)
		read_help_string(menuInt[15][i], sizeof(menuInt[15][i]), f, "menuInt", 15, (int)i);
	skip_pascal_string(f);

	/*Tyrian Licensing Info*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(licensingInfo); ++i)
		read_help_string(licensingInfo[i], sizeof(licensingInfo[i]), f, "licensingInfo", (int)i, -1);
	skip_pascal_string(f);

	/*Default High Score Names*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(defaultHighScoreNames); ++i)
		read_help_string(defaultHighScoreNames[i], sizeof(defaultHighScoreNames[i]), f, "defaultHighScoreNames", (int)i, -1);
	skip_pascal_string(f);

	/*Default Team Names*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(defaultTeamNames); ++i)
		read_help_string(defaultTeamNames[i], sizeof(defaultTeamNames[i]), f, "defaultTeamNames", (int)i, -1);
	skip_pascal_string(f);

	/*Ordering Info?*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(orderingInfo); ++i)
		read_help_string(orderingInfo[i], sizeof(orderingInfo[i]), f, "orderingInfo", (int)i, -1);
	skip_pascal_string(f);

	/*Super Tyrian text*/
	skip_pascal_string(f);
	for (unsigned int i = 0; i < COUNTOF(superTyrianText); ++i)
		read_help_string(superTyrianText[i], sizeof(superTyrianText[i]), f, "superTyrianText", (int)i, -1);

	fclose(f);
}
