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
#include "translate.h"

#include "file.h"
#include "helptext.h"
#include "opentyr.h"
#include "pofile.h"

#include "SDL.h"

#include <stdlib.h>
#include <string.h>

/* Lookup table ***************************************************************/

typedef struct
{
	char *key;
	char *text;
} Entry;

static Entry *table = NULL;
static size_t table_capacity = 0;
static size_t table_count = 0;

static TrCollector collector = NULL;

static Uint32 hash_key(const char *key)
{
	Uint32 h = 2166136261u;  // FNV-1a
	for (; *key != '\0'; ++key)
		h = (h ^ (unsigned char)*key) * 16777619u;
	return h;
}

static bool table_grow(void)
{
	const size_t capacity = table_capacity == 0 ? 4096 : table_capacity * 2;
	Entry *entries = calloc(capacity, sizeof(*entries));
	if (entries == NULL)
		return false;

	for (size_t i = 0; i < table_capacity; ++i)
	{
		if (table[i].key == NULL)
			continue;
		size_t j = hash_key(table[i].key) & (capacity - 1);
		while (entries[j].key != NULL)
			j = (j + 1) & (capacity - 1);
		entries[j] = table[i];
	}

	free(table);
	table = entries;
	table_capacity = capacity;
	return true;
}

static void table_put(const char *key, const char *text)
{
	if ((table_count + 1) * 2 > table_capacity && !table_grow())
		return;

	size_t i = hash_key(key) & (table_capacity - 1);
	while (table[i].key != NULL)
	{
		if (strcmp(table[i].key, key) == 0)
		{
			free(table[i].text);
			table[i].text = SDL_strdup(text);
			return;
		}
		i = (i + 1) & (table_capacity - 1);
	}

	table[i].key = SDL_strdup(key);
	table[i].text = SDL_strdup(text);
	++table_count;
}

const char *tr_lookup(const char *key)
{
	if (table_count == 0)
		return NULL;

	size_t i = hash_key(key) & (table_capacity - 1);
	while (table[i].key != NULL)
	{
		if (strcmp(table[i].key, key) == 0)
			return table[i].text;
		i = (i + 1) & (table_capacity - 1);
	}
	return NULL;
}

/* Loading ********************************************************************/

// The translation files, one per source of text.
const char *const tr_files[] =
{
	"help.po", "items.po", "levels.po", "ui.po", "credits.po",
	"episode1.po", "episode2.po", "episode3.po", "episode4.po", "episode5.po",
	"cubes1.po", "cubes2.po", "cubes3.po", "cubes4.po", "cubes5.po",
};
const size_t tr_file_count = COUNTOF(tr_files);

static void add_translation(const PoEntry *entry, void *userdata)
{
	int *count = userdata;

	// Untranslated entries, the header and entries marked as needing review
	// are ignored.
	if (entry->msgctxt == NULL || entry->msgstr == NULL || entry->msgstr[0] == '\0' || entry->fuzzy)
		return;

	table_put(entry->msgctxt, entry->msgstr);
	++*count;
}

int tr_init(void)
{
	char dir[1024];
	snprintf(dir, sizeof(dir), "%s/kor/lang", base_dir());

	int count = 0;
	for (size_t i = 0; i < tr_file_count; ++i)
	{
		FILE *f = dir_fopen(dir, tr_files[i], "rb");
		if (f == NULL)
			continue;
		po_parse(f, add_translation, &count);
		fclose(f);
	}

	return count;
}

void tr_deinit(void)
{
	for (size_t i = 0; i < table_capacity; ++i)
	{
		free(table[i].key);
		free(table[i].text);
	}
	free(table);
	table = NULL;
	table_capacity = table_count = 0;
}

/* Applying *******************************************************************/

void tr_set_collector(TrCollector new_collector)
{
	collector = new_collector;
}

TrCollector tr_get_collector(void)
{
	return collector;
}

void tr_copy_text(char *dst, size_t size, const char *src)
{
	if (size == 0)
		return;

	size_t len = strlen(src);
	if (len >= size)
	{
		len = size - 1;
		while (len > 0 && ((unsigned char)src[len] & 0xc0) == 0x80)
			--len;
	}

	memmove(dst, src, len);
	dst[len] = '\0';
}

/** Removes an incomplete UTF-8 character from the end of a string. */
static void trim_partial_utf8(char *s, size_t len)
{
	if (len == 0)
		return;

	size_t start = len - 1;
	while (start > 0 && ((unsigned char)s[start] & 0xc0) == 0x80)
		--start;

	const unsigned char lead = (unsigned char)s[start];
	const size_t expected = lead < 0x80 ? 1 : (lead & 0xe0) == 0xc0 ? 2 : (lead & 0xf0) == 0xe0 ? 3 : (lead & 0xf8) == 0xf0 ? 4 : 1;
	if (len - start < expected)
		s[start] = '\0';
}

const char *tr_format(char *buffer, size_t size, const char *format, const char *text, int number)
{
	if (size == 0)
		return buffer;

	char number_text[16];
	snprintf(number_text, sizeof(number_text), "%d", number);

	size_t len = 0;
	bool full = false;
	for (const char *s = format; *s != '\0' && !full; ++s)
	{
		char single[2] = { *s, '\0' };
		const char *insert = single;
		if (s[0] == '%' && s[1] == 'd')
			insert = number_text, ++s;
		else if (s[0] == '%' && s[1] == 's')
			insert = text != NULL ? text : "", ++s;

		size_t n = strlen(insert);
		if (len + n >= size)
		{
			n = size - 1 - len;
			full = true;
		}
		memcpy(buffer + len, insert, n);
		len += n;
	}
	buffer[len] = '\0';

	if (full)
		trim_partial_utf8(buffer, len);
	return buffer;
}

void tr_apply(char *dst, size_t size, const char *key)
{
	if (collector != NULL)
	{
		collector(key, dst, size);
		return;
	}

	const char *text = tr_lookup(key);
	if (text != NULL)
		tr_copy_text(dst, size, text);
}

void tr_read_encrypted(char *dst, size_t size, FILE *f, const char *key)
{
	read_encrypted_pascal_string(dst, size, f);
	tr_apply(dst, size, key);
}

const char *tr_level_name(const char *name)
{
	// Level names come from fixed-width fields of the episode files and are
	// padded with spaces; the keys are trimmed (see textdump.c).
	char key[32];
	snprintf(key, sizeof(key), "level.%s", name);
	for (size_t len = strlen(key); len > 6 && key[len - 1] == ' '; --len)
		key[len - 1] = '\0';

	const char *text = tr_lookup(key);
	return text != NULL ? text : name;
}

/* Text in the program itself *************************************************/

typedef struct
{
	const char *key;
	const char *english;
	const char *note;
} UiTextInfo;

static const UiTextInfo ui_texts[UI_TEXT_COUNT] =
{
	[UI_MENU_SETUP] = { "ui.menu.setup", "Setup", "메인 메뉴 항목 (보통 글씨). OpenTyrian이 추가한 설정 메뉴" },

	[UI_SETUP_TITLE] = { "ui.setup.title", "Setup", "설정 메뉴 제목 (큰 글씨)" },
	[UI_SETUP_GRAPHICS] = { "ui.setup.graphics", "Graphics...", "설정 메뉴 항목 (보통 글씨, 약 130픽셀)" },
	[UI_SETUP_GRAPHICS_DESC] = { "ui.setup.graphics_desc", "Change the graphics settings.", "화면 아래 설명 (작은 글씨, 한 줄)" },
	[UI_SETUP_SOUND] = { "ui.setup.sound", "Sound...", "설정 메뉴 항목" },
	[UI_SETUP_SOUND_DESC] = { "ui.setup.sound_desc", "Change the sound settings.", "화면 아래 설명" },
	[UI_SETUP_JUKEBOX] = { "ui.setup.jukebox", "Jukebox", "설정 메뉴 항목 (게임 음악 듣기)" },
	[UI_SETUP_JUKEBOX_DESC] = { "ui.setup.jukebox_desc", "Listen to the music of Tyrian.", "화면 아래 설명" },
	[UI_SETUP_DONE] = { "ui.setup.done", "Done", "메뉴를 닫는 항목" },
	[UI_SETUP_DONE_DESC] = { "ui.setup.done_desc", "Return to the main menu.", "화면 아래 설명" },

	[UI_GRAPHICS_TITLE] = { "ui.graphics.title", "Graphics", "그래픽 설정 메뉴 제목 (큰 글씨)" },
	[UI_GRAPHICS_DISPLAY] = { "ui.graphics.display", "Display:", "그래픽 설정 항목 이름 (보통 글씨, 약 130픽셀)" },
	[UI_GRAPHICS_DISPLAY_DESC] = { "ui.graphics.display_desc", "Change the display mode.", "화면 아래 설명" },
	[UI_GRAPHICS_WINDOW_SIZE] = { "ui.graphics.window_size", "Window Size:", "그래픽 설정 항목 이름" },
	[UI_GRAPHICS_WINDOW_SIZE_DESC] = { "ui.graphics.window_size_desc", "Change the window size (in Window mode).", "화면 아래 설명" },
	[UI_GRAPHICS_SCALER] = { "ui.graphics.scaler", "Scaler:", "그래픽 설정 항목 이름 (픽셀 확대 방식)" },
	[UI_GRAPHICS_SCALER_DESC] = { "ui.graphics.scaler_desc", "Change the pixel art scaling algorithm.", "화면 아래 설명" },
	[UI_GRAPHICS_SCALING_MODE] = { "ui.graphics.scaling_mode", "Scaling Mode:", "그래픽 설정 항목 이름 (화면 맞춤 방식)" },
	[UI_GRAPHICS_SCALING_MODE_DESC] = { "ui.graphics.scaling_mode_desc", "Change the scaling mode.", "화면 아래 설명" },
	[UI_DONE_PREVIOUS_DESC] = { "ui.done_previous_desc", "Return to the previous menu.", "화면 아래 설명" },

	[UI_SOUND_TITLE] = { "ui.sound.title", "Sound", "소리 설정 메뉴 제목 (큰 글씨)" },
	[UI_SOUND_MUSIC_VOLUME] = { "ui.sound.music_volume", "Music Volume", "소리 설정 항목 이름" },
	[UI_SOUND_EFFECTS_VOLUME] = { "ui.sound.effects_volume", "Sound Volume", "소리 설정 항목 이름 (효과음)" },
	[UI_SOUND_VOLUME_DESC] = { "ui.sound.volume_desc", "Change volume with the left/right arrow keys.", "화면 아래 설명" },

	[UI_DISPLAY_WINDOW] = { "ui.display.window", "Window", "화면 모드 값: 창 모드 (보통 글씨, 약 95픽셀)" },
	[UI_DISPLAY_N] = { "ui.display.n", "Display %d", "화면 모드 값: n번 모니터 전체 화면. %d(모니터 번호)는 그대로 두세요" },
	[UI_WINDOW_SIZE_AUTO] = { "ui.window_size.auto", "Auto (%dx)", "창 크기 값: 자동. %d(배수)는 그대로 두세요" },
	[UI_SCALING_CENTER] = { "ui.scaling.center", "Center", "화면 맞춤 방식 값: 확대하지 않고 가운데" },
	[UI_SCALING_INTEGER] = { "ui.scaling.integer", "Integer", "화면 맞춤 방식 값: 정수배로만 확대" },
	[UI_SCALING_FIT_8_5] = { "ui.scaling.fit_8_5", "Fit 8:5", "화면 맞춤 방식 값: 8:5 비율로 꽉 차게" },
	[UI_SCALING_FIT_4_3] = { "ui.scaling.fit_4_3", "Fit 4:3", "화면 맞춤 방식 값: 4:3 비율(옛 CRT)로 꽉 차게" },

	[UI_JUKEBOX_QUIT] = { "ui.jukebox.quit", "Press ESC to quit the jukebox.", "음악 감상 화면 아래 안내 (작은 글씨, 한 줄)" },
	[UI_JUKEBOX_KEYS] = { "ui.jukebox.keys", "Arrow keys change the song being played.", "음악 감상 화면 아래 안내" },

	[UI_WARNING] = { "ui.warning", "WARNING", "게임 중 경고 표시 (작은 글씨, 짧게)" },
	[UI_CHEATERS] = { "ui.cheaters", "Cheaters always prosper.", "치트를 쓴 사람에게 보이는 농담 문구" },
	[UI_DESTRUCT_CUSTOM] = { "ui.destruct.custom", "Custom", "숨겨진 미니게임 Destruct의 모드 이름" },

	[UI_NEXT_LEVEL] = { "ui.next_level", "Next Level", "다음 레벨 선택 메뉴의 제목 (큰 글씨, 짧게)" },
	[UI_COST] = { "ui.cost", "Cost: %d", "상점의 아이템 가격 (작은 글씨). %d(가격)는 그대로 두세요" },
	[UI_CUSTOM_SHIP] = { "ui.custom_ship", "Custom Ship %d", "상점의 사용자 정의 함선 이름. %d(번호)는 그대로 두세요" },
	[UI_OR_PLAY] = { "ui.or_play", "Or play... %s", "슈퍼 아케이드 모드를 마친 뒤 나오는 안내. %s는 미니게임 이름입니다" },
	[UI_TYPE_AT_TITLE] = { "ui.type_at_title", "Type %s at Title", "슈퍼 아케이드 모드를 마친 뒤 나오는 비밀 코드 안내. %s는 타이틀 화면에서 입력할 코드입니다 (영어 그대로)" },
	[UI_PAGE_OF] = { "ui.page_of", "%s of %d", "도움말 화면 오른쪽 아래의 쪽 번호. %s는 \"페이지 3\" 같은 현재 쪽, %d는 전체 쪽 수입니다 (작은 글씨)" },
};

const char *ui_text(UiText id)
{
	if (id < 0 || id >= UI_TEXT_COUNT)
		return "";

	const char *text = tr_lookup(ui_texts[id].key);
	return text != NULL ? text : ui_texts[id].english;
}

void ui_text_info(UiText id, const char **key, const char **english, const char **note)
{
	*key = ui_texts[id].key;
	*english = ui_texts[id].english;
	*note = ui_texts[id].note;
}
