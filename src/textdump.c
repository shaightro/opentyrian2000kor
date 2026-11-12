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
#include "textdump.h"

#include "episodes.h"
#include "file.h"
#include "fonthand.h"
#include "hangul.h"
#include "helptext.h"
#include "opentyr.h"
#include "pofile.h"
#include "sprite.h"
#include "translate.h"

#include "SDL.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#ifdef _MSC_VER
#include <direct.h>
#define mkdir _mkdir
#endif

bool textdump_requested = false;
bool textcheck_requested = false;

extern const char *const tr_files[];
extern const size_t tr_file_count;

/* Collected text *************************************************************/

typedef struct
{
	char *key;
	char *text;      // English, UTF-8
	size_t size;     // buffer size the translation (or each line of it) must fit in; 0 = any length
	int max_lines;   // > 0: lines separated by '\n', each shown as a line on screen
	char *note;      // for translators
	bool mismatch_reported;
} TextEntry;

static TextEntry *entries = NULL;
static size_t entry_count = 0, entry_capacity = 0;

// Hash index of entries by key.
static size_t *index_table = NULL;
static size_t index_capacity = 0;

static Uint32 hash_key(const char *key)
{
	Uint32 h = 2166136261u;
	for (; *key != '\0'; ++key)
		h = (h ^ (unsigned char)*key) * 16777619u;
	return h;
}

static TextEntry *find_entry(const char *key)
{
	if (index_capacity == 0)
		return NULL;
	for (size_t i = hash_key(key) & (index_capacity - 1); index_table[i] != 0; i = (i + 1) & (index_capacity - 1))
		if (strcmp(entries[index_table[i] - 1].key, key) == 0)
			return &entries[index_table[i] - 1];
	return NULL;
}

static void index_insert(size_t entry)
{
	size_t i = hash_key(entries[entry].key) & (index_capacity - 1);
	while (index_table[i] != 0)
		i = (i + 1) & (index_capacity - 1);
	index_table[i] = entry + 1;
}

/* Report lines (warnings of the dump, results of the check). */
static char *report = NULL;
static size_t report_len = 0, report_capacity = 0;
static int report_errors = 0, report_warnings = 0, report_notes = 0;

static void report_line(const char *format, ...)
{
	char line[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(line, sizeof(line), format, args);
	va_end(args);

	const size_t len = strlen(line);
	if (report_len + len + 2 > report_capacity)
	{
		const size_t capacity = MAX(report_capacity * 2, report_len + len + 4096);
		char *grown = realloc(report, capacity);
		if (grown == NULL)
			return;
		report = grown;
		report_capacity = capacity;
	}
	memcpy(report + report_len, line, len);
	report_len += len;
	report[report_len++] = '\n';
	report[report_len] = '\0';
}

/** Converts game text (CP437 bytes, possibly UTF-8) to UTF-8. */
static char *game_text_to_utf8(const char *s)
{
	static const Uint16 cp437[128] =
	{
		0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7, 0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
		0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9, 0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192,
		0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba, 0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
		0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,
		0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f, 0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
		0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b, 0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
		0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4, 0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229,
		0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248, 0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0, 0x00a0,
	};

	char *out = malloc(strlen(s) * 3 + 1);
	if (out == NULL)
		return SDL_strdup("");

	char *o = out;
	while (*s != '\0')
	{
		Uint32 c;
		const int len = text_decode_char(s, &c);
		if (len == 1 && c >= 0x80)
			c = cp437[c - 0x80];
		s += len;

		if (c < 0x80)
			*o++ = (char)c;
		else if (c < 0x800)
			*o++ = (char)(0xc0 | (c >> 6)), *o++ = (char)(0x80 | (c & 0x3f));
		else if (c < 0x10000)
			*o++ = (char)(0xe0 | (c >> 12)), *o++ = (char)(0x80 | ((c >> 6) & 0x3f)), *o++ = (char)(0x80 | (c & 0x3f));
		else
			*o++ = (char)(0xf0 | (c >> 18)), *o++ = (char)(0x80 | ((c >> 12) & 0x3f)), *o++ = (char)(0x80 | ((c >> 6) & 0x3f)), *o++ = (char)(0x80 | (c & 0x3f));
	}
	*o = '\0';
	return out;
}

static TextEntry *add_entry(const char *key, const char *text, size_t size, const char *note)
{
	TextEntry *existing = find_entry(key);
	if (existing != NULL)
	{
		// Item data is shared by several episodes: keep the first copy.
		char *utf8 = game_text_to_utf8(text);
		if (strcmp(existing->text, utf8) != 0 && !existing->mismatch_reported)
		{
			report_line("참고: %s의 원문이 에피소드에 따라 다릅니다: \"%s\" / \"%s\" (처음 것을 씁니다)", key, existing->text, utf8);
			existing->mismatch_reported = true;
		}
		free(utf8);
		return NULL;
	}

	if (entry_count == entry_capacity)
	{
		const size_t capacity = entry_capacity == 0 ? 1024 : entry_capacity * 2;
		TextEntry *grown = realloc(entries, capacity * sizeof(*grown));
		if (grown == NULL)
			return NULL;
		entries = grown;
		entry_capacity = capacity;

		free(index_table);
		index_capacity = capacity * 2;
		index_table = calloc(index_capacity, sizeof(*index_table));
		for (size_t i = 0; i < entry_count; ++i)
			index_insert(i);
	}

	TextEntry *entry = &entries[entry_count];
	entry->key = SDL_strdup(key);
	entry->text = game_text_to_utf8(text);
	entry->size = size;
	entry->max_lines = 0;
	entry->note = SDL_strdup(note != NULL ? note : "");
	entry->mismatch_reported = false;
	index_insert(entry_count);
	++entry_count;
	return entry;
}

/* Notes for translators ******************************************************/

static const char *note_for(const char *key)
{
	static const struct { const char *prefix; const char *note; } notes[] =
	{
		{ "help.helpTxt.", "도움말 상자의 설명 (작은 글씨, 상자 안에서 자동 줄바꿈)" },
		{ "help.pName.", "행성(지역) 이름" },
		{ "help.miscText.", "여러 화면에 나오는 기타 문구 (대부분 작은 글씨 한 줄). 뒤에 숫자나 이름이 붙는 것도 있습니다" },
		{ "help.miscTextB.", "아주 짧은 기타 문구" },
		{ "help.menuText.", "메인 메뉴 항목 (보통 글씨)" },
		{ "help.outputs.", "이벤트 알림 문구" },
		{ "help.topicName.", "도움말 주제 이름" },
		{ "help.mainMenuHelp.", "게임 메뉴 화면 아래쪽 설명 (작은 글씨, 한 줄)" },
		{ "help.menuInt.6.", "키 이름 (키 설정 메뉴)" },
		{ "help.inGameText.", "게임 중 메뉴 항목" },
		{ "help.detailLevel.", "그래픽 품질 단계 이름" },
		{ "help.gameSpeedText.", "게임 속도 단계 이름" },
		{ "help.episode_name.", "에피소드 이름 (에피소드 선택 화면, 보통 글씨)" },
		{ "help.difficulty_name.", "난이도 이름 (난이도 선택 화면, 보통 글씨)" },
		{ "help.gameplay_name.", "플레이 방식 이름 (플레이어 선택 화면, 보통 글씨)" },
		{ "help.inputDevices.", "입력 장치 이름" },
		{ "help.networkText.", "네트워크 대전 문구 (이 판에서는 쓰지 않음)" },
		{ "help.difficultyNameB.", "최고 기록 화면의 난이도 이름" },
		{ "help.joyButtonNames.", "조이스틱 버튼 기능 이름" },
		{ "help.superShips.", "슈퍼 아케이드 모드의 함선 이름과 문구" },
		{ "help.specialName.", "특수 모드 이름 (아주 짧게)" },
		{ "help.destructHelp.", "숨겨진 미니게임 Destruct의 설명" },
		{ "help.weaponNames.", "숨겨진 미니게임 Destruct의 무기 이름" },
		{ "help.destructModeName.", "숨겨진 미니게임 Destruct의 모드 이름" },
		{ "help.shipInfo.", "함선 설명 (함선 정보 화면, 상자 안에서 자동 줄바꿈)" },
		{ "help.timed_battle_name.", "시간제 전투 모드의 지역 이름" },
		{ "help.licensingInfo.", "라이선스 안내 문구" },
		{ "help.defaultHighScoreNames.", "최고 기록 표의 기본 이름 (사람 이름이라 그대로 둬도 됩니다)" },
		{ "help.defaultTeamNames.", "2인용 최고 기록 표의 기본 팀 이름" },
		{ "help.orderingInfo.", "구매 안내 문구 (셰어웨어 시절의 흔적)" },
		{ "help.superTyrianText.", "슈퍼 티리안 모드 문구" },
		{ "item.weapon.", "무기 이름 (상점과 함선 정보 화면)" },
		{ "item.ship.", "함선 이름" },
		{ "item.special.", "특수 무기 이름" },
		{ "item.generator.", "발전기 이름" },
		{ "item.sidekick.", "사이드킥(보조 무기) 이름" },
		{ "item.shield.", "보호막 이름" },
		{ "credits.", "제작진 목록 (작은 글씨). 첫 글자(영어 대문자)는 줄의 색을 정하는 기호이니 그대로 두고, 이름은 그대로 두고 역할만 번역하세요" },
	};

	if (strncmp(key, "help.menuInt.", 13) == 0 && strncmp(key, "help.menuInt.6.", 15) != 0)
	{
		const char *last_dot = strrchr(key, '.');
		return strcmp(last_dot, ".0") == 0 ? "게임 메뉴 제목 (큰 글씨, 짧게)" : "게임 메뉴 항목 (보통 글씨)";
	}

	for (size_t i = 0; i < COUNTOF(notes); ++i)
		if (strncmp(key, notes[i].prefix, strlen(notes[i].prefix)) == 0)
			return notes[i].note;
	return "";
}

static void collect(const char *key, const char *text, size_t size)
{
	// Item names are padded with spaces to 30 characters.
	char trimmed[256];
	if (strncmp(key, "item.", 5) == 0)
	{
		SDL_strlcpy(trimmed, text, sizeof(trimmed));
		for (int i = (int)strlen(trimmed) - 1; i >= 0 && trimmed[i] == ' '; --i)
			trimmed[i] = '\0';
		text = trimmed;
	}

	// "NEW GAME" becomes the level name of a save, which the save file keeps
	// in 9 bytes (see JE_saveConfiguration()).
	if (strcmp(key, "help.miscText.19") == 0)
	{
		add_entry(key, text, 10, "새 게임을 시작한 뒤 저장한 칸의 레벨 이름 (불러오기 메뉴, 세이브 파일에 9바이트까지 저장)");
		return;
	}

	add_entry(key, text, size, note_for(key));
}

/* Reading the game data ******************************************************/

/** Text being assembled from several strings of the data. */
typedef struct
{
	char text[8192];
	size_t len;
	int first;     // index of its first string, -1 if empty
	int lines;
	bool has_text;
} Assembly;

static void assembly_start(Assembly *a)
{
	a->text[0] = '\0';
	a->len = 0;
	a->first = -1;
	a->lines = 0;
	a->has_text = false;
}

static void assembly_append(Assembly *a, int index, const char *s, const char *separator)
{
	if (a->first < 0)
		a->first = index;
	else
		a->len += (size_t)snprintf(a->text + a->len, sizeof(a->text) - a->len, "%s", separator);
	a->len += (size_t)snprintf(a->text + a->len, sizeof(a->text) - a->len, "%s", s);
	a->len = MIN(a->len, sizeof(a->text) - 1);
	++a->lines;
	a->has_text |= s[0] != '\0';
}

/*
 * The text of the episode scripts comes in blocks of lines ending with a '#'
 * line: after "]W" a briefing shown before a level, after "]Q" several hints,
 * one of which is shown after a level.  A block is translated as a whole, with
 * its lines separated by '\n', so that sentences can be laid out freely.
 */
static void collect_episode_script(int episode)
{
	char file[16];
	snprintf(file, sizeof(file), "levels%d.dat", episode);
	FILE *f = dir_fopen(data_dir(), file, "rb");
	if (f == NULL)
		return;

	enum { NONE, BRIEFING, HINTS } mode = NONE;
	static Assembly block;
	char s[256], key[48];
	int skip = 0;

	assembly_start(&block);

	for (int n = 0; try_read_encrypted_pascal_string(s, sizeof(s), f); ++n)
	{
		if (skip > 0)
		{
			--skip;  // item lists of "]I"
			continue;
		}

		const bool block_end = s[0] == '#' || s[0] == ']' || s[0] == '*';
		if (block_end && block.has_text)
		{
			snprintf(key, sizeof(key), "ep%d.%d", episode, block.first);
			TextEntry *entry = add_entry(key, block.text, sizeof(*levelWarningText), mode == BRIEFING
				? "레벨 시작 전 화면 가운데에 한 줄씩 빛나며 나오는 이야기/경고 (작은 글씨). 줄바꿈(\\n)으로 나뉜 각 줄이 화면의 한 줄이며, 줄 수는 바꿔도 됩니다"
				: "레벨을 마친 뒤 나오는 힌트 (작은 글씨). 줄바꿈(\\n)으로 나뉜 각 줄이 화면의 한 줄이며, 줄 수는 바꿔도 됩니다");
			if (entry != NULL)
				entry->max_lines = mode == BRIEFING ? 12 : 9;  // room left in levelWarningText
		}
		if (block_end)
			assembly_start(&block);

		if (s[0] == ']')
		{
			mode = s[1] == 'W' ? BRIEFING : s[1] == 'Q' ? HINTS : NONE;

			if (s[1] == 'I')
				skip = 9;

			if (s[1] == 'L' && strlen(s) > 13)
			{
				// level name: 9 characters from column 13
				char name[11];
				SDL_strlcpy(name, s + 13, 10);
				for (int i = (int)strlen(name) - 1; i >= 0 && name[i] == ' '; --i)
					name[i] = '\0';
				if (name[0] != '\0')
				{
					snprintf(key, sizeof(key), "level.%s", name);
					add_entry(key, name, 0, "레벨 이름 (게임 화면 오른쪽 칸과 불러오기 메뉴, 작은 글씨, 아주 짧게)");
				}
			}
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
			assembly_append(&block, n, s, "\n");
		}
		else if (s[0] != '\0')
		{
			// Leftover item lists (as after "]I") are not text.
			static const char *const item_lists[] = { " Ship", " WeapF", " WeapR", " Power", " Engine", " Opt1", " Opt2", " Armor", " Shield" };
			bool item_list = false;
			for (size_t i = 0; i < COUNTOF(item_lists); ++i)
				item_list |= strncmp(s, item_lists[i], strlen(item_lists[i])) == 0;

			if (!item_list)
				report_line("참고: levels%d.dat의 %d번 문자열은 명령이 아닌데 번역 대상에서 뺐습니다: \"%s\"", episode, n, s);
		}
	}

	fclose(f);
}

/*
 * Data cube text is stored hard-wrapped; consecutive lines (up to an empty
 * line) form a paragraph, which is translated as a whole and wrapped again by
 * the game.
 */
static void collect_cubes(int episode)
{
	char file[16];
	snprintf(file, sizeof(file), "cubetxt%d.dat", episode);
	FILE *f = dir_fopen(data_dir(), file, "rb");
	if (f == NULL)
		return;

	char s[256], key[48];
	int cube = 0, line = -1;  // line -1: title, -2: header
	static Assembly paragraph;

	assembly_start(&paragraph);

	for (bool more = true; more; )
	{
		more = try_read_encrypted_pascal_string(s, sizeof(s), f);

		if (!more || s[0] == '*' || (line >= 0 && s[0] == '\0'))
		{
			if (paragraph.has_text)
			{
				snprintf(key, sizeof(key), "cube%d.%d.%d", episode, cube, paragraph.first);
				add_entry(key, paragraph.text, 4096, "데이터 큐브 본문의 한 문단 (작은 글씨). 게임이 자동으로 줄바꿈합니다");
			}
			assembly_start(&paragraph);
		}

		if (!more)
			break;

		if (s[0] == '*')
		{
			++cube;
			line = -2;
			continue;
		}
		if (cube == 0)
			continue;

		if (line == -2)
		{
			snprintf(key, sizeof(key), "cube%d.%d.title", episode, cube);
			add_entry(key, s, TEXT_SIZE(81), "데이터 큐브 목록에 나오는 제목 (작은 글씨, 두 줄까지)");
			line = -1;
		}
		else if (line == -1)
		{
			snprintf(key, sizeof(key), "cube%d.%d.header", episode, cube);
			add_entry(key, s, TEXT_SIZE(13), "데이터 큐브를 읽을 때 위에 나오는 제목 (큰 글씨, 아주 짧게)");
			line = 0;
		}
		else
		{
			if (s[0] != '\0')
				assembly_append(&paragraph, line, s, " ");
			++line;
		}
	}

	fclose(f);
}

static void collect_credits(void)
{
	FILE *f = dir_fopen(data_dir(), "tyrian.cdt", "rb");
	if (f == NULL)
		return;

	char s[256], key[32];
	for (int n = 0; n < 126 && try_read_encrypted_pascal_string(s, sizeof(s), f); ++n)
	{
		if (s[0] == '\0' || strcmp(s, ".") == 0)  // "." lines only make space
			continue;
		snprintf(key, sizeof(key), "credits.%d", n);
		add_entry(key, s, TEXT_SIZE(66), note_for(key));
	}

	fclose(f);
}

static void collect_all(void)
{
	tr_set_collector(collect);

	JE_loadHelpText();

	JE_scanForEpisodes();
	for (int episode = 1; episode <= EPISODE_MAX; ++episode)
		if (episodeAvail[episode - 1])
			JE_initEpisode((JE_byte)episode);  // reads the item data

	tr_set_collector(NULL);

	for (int episode = 1; episode <= EPISODE_MAX; ++episode)
		collect_episode_script(episode);
	for (int episode = 1; episode <= EPISODE_MAX; ++episode)
		collect_cubes(episode);
	collect_credits();

	// Saves of finished episodes are named "Completed" internally.
	add_entry("level.Completed", "Completed", 0, "에피소드를 마친 세이브의 레벨 칸에 나오는 말 (작은 글씨, 아주 짧게)");

	for (int id = 0; id < UI_TEXT_COUNT; ++id)
	{
		const char *key, *english, *note;
		ui_text_info((UiText)id, &key, &english, &note);
		// The setup menu item is copied into the main menu text.
		add_entry(key, english, id == UI_MENU_SETUP ? sizeof(menuText[0]) : 0, note);
	}
}

static const char *file_for_key(const char *key)
{
	static char file[32];

	if (strncmp(key, "help.", 5) == 0)
		return "help.po";
	if (strncmp(key, "item.", 5) == 0)
		return "items.po";
	if (strncmp(key, "level.", 6) == 0)
		return "levels.po";
	if (strncmp(key, "ui.", 3) == 0)
		return "ui.po";
	if (strncmp(key, "credits.", 8) == 0)
		return "credits.po";
	if (strncmp(key, "ep", 2) == 0)
		return snprintf(file, sizeof(file), "episode%c.po", key[2]), file;
	if (strncmp(key, "cube", 4) == 0)
		return snprintf(file, sizeof(file), "cubes%c.po", key[4]), file;
	return "other.po";
}

/* Existing translations ******************************************************/

typedef struct
{
	char *key;
	char *msgid;
	char *msgstr;
	bool fuzzy;
	char file[32];
	int line;
	bool seen;
} Translation;

static Translation *translations = NULL;
static size_t translation_count = 0, translation_capacity = 0;
static const char *current_file = NULL;

static void add_translation(const PoEntry *entry, void *userdata)
{
	(void)userdata;

	if (entry->msgctxt == NULL)
		return;  // the header

	if (translation_count == translation_capacity)
	{
		const size_t capacity = translation_capacity == 0 ? 1024 : translation_capacity * 2;
		Translation *grown = realloc(translations, capacity * sizeof(*grown));
		if (grown == NULL)
			return;
		translations = grown;
		translation_capacity = capacity;
	}

	Translation *t = &translations[translation_count++];
	t->key = SDL_strdup(entry->msgctxt);
	t->msgid = SDL_strdup(entry->msgid);
	t->msgstr = SDL_strdup(entry->msgstr);
	t->fuzzy = entry->fuzzy;
	snprintf(t->file, sizeof(t->file), "%s", current_file);
	t->line = entry->line;
	t->seen = false;
}

static Translation *find_translation(const char *key)
{
	for (size_t i = 0; i < translation_count; ++i)
		if (strcmp(translations[i].key, key) == 0)
			return &translations[i];
	return NULL;
}

static int read_translations(const char *dir)
{
	int errors = 0;
	for (size_t i = 0; i < tr_file_count; ++i)
	{
		FILE *f = dir_fopen(dir, tr_files[i], "rb");
		if (f == NULL)
			continue;

		current_file = tr_files[i];
		int first_error_line;
		const int file_errors = po_parse_ex(f, add_translation, NULL, &first_error_line);
		fclose(f);

		if (file_errors > 0)
		{
			report_line("오류: %s: PO 형식 오류 %d개 (처음은 %d번째 줄)", tr_files[i], file_errors, first_error_line);
			++report_errors;
			errors += file_errors;
		}
	}
	return errors;
}

/* Dump ***********************************************************************/

static void write_po_header(FILE *f, const char *file)
{
	fprintf(f,
		"# OpenTyrian2000 한국어 번역: %s\n"
		"#\n"
		"# - msgctxt는 게임이 문장을 찾는 열쇠입니다. 바꾸지 마세요.\n"
		"# - msgid는 영어 원문, msgstr에 한국어 번역을 씁니다. 비워 두면 영어가 나옵니다.\n"
		"# - ~로 감싼 부분은 강조(더 밝게) 표시입니다. ~ 기호는 그대로 두세요.\n"
		"# - %%d 같은 자리 표시자는 그대로 두세요.\n"
		"# - \"최대 N바이트\"는 게임 안의 칸 크기입니다. 한글 한 글자는 3바이트입니다.\n"
		"# - 확인이 필요한 번역은 앞에 \"#, fuzzy\" 줄을 두면 게임이 쓰지 않습니다.\n"
		"msgid \"\"\n"
		"msgstr \"\"\n"
		"\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
		"\"Language: ko\\n\"\n"
		"\n",
		file);
}

static int dump(const char *dir)
{
#ifdef TARGET_WIN32
	mkdir(dir);  // may exist already
#else
	mkdir(dir, 0755);
#endif

	read_translations(dir);

	int files_written = 0, kept = 0;
	for (size_t fi = 0; fi < tr_file_count; ++fi)
	{
		const char *file = tr_files[fi];

		size_t count = 0;
		for (size_t i = 0; i < entry_count; ++i)
			if (strcmp(file_for_key(entries[i].key), file) == 0)
				++count;
		if (count == 0)
			continue;

		FILE *f = dir_fopen(dir, file, "wb");
		if (f == NULL)
		{
			report_line("오류: %s/%s를 쓸 수 없습니다", dir, file);
			++report_errors;
			continue;
		}

		write_po_header(f, file);

		for (size_t i = 0; i < entry_count; ++i)
		{
			const TextEntry *e = &entries[i];
			if (strcmp(file_for_key(e->key), file) != 0)
				continue;

			if (e->max_lines > 0)
				fprintf(f, "#. %s. 최대 %d줄, 한 줄 최대 %d바이트 (한글 약 %d자)\n", e->note, e->max_lines, (int)e->size - 1, (int)(e->size - 1) / 3);
			else if (e->size > 0)
				fprintf(f, "#. %s%s최대 %d바이트 (한글 약 %d자)\n", e->note, e->note[0] != '\0' ? ". " : "", (int)e->size - 1, (int)(e->size - 1) / 3);
			else if (e->note[0] != '\0')
				fprintf(f, "#. %s\n", e->note);

			const Translation *t = find_translation(e->key);
			if (t != NULL && t->fuzzy)
				fputs("#, fuzzy\n", f);
			if (t != NULL && t->msgstr[0] != '\0')
				++kept;

			po_write_string(f, "msgctxt", e->key);
			po_write_string(f, "msgid", e->text);
			po_write_string(f, "msgstr", t != NULL ? t->msgstr : "");
			fputc('\n', f);
		}

		fclose(f);
		++files_written;
	}

	report_line("추출: 문장 %d개, 파일 %d개, 기존 번역 %d개 유지", (int)entry_count, files_written, kept);
	return report_errors > 0 ? 1 : 0;
}

/* Check **********************************************************************/

static bool valid_utf8(const char *s)
{
	while (*s != '\0')
	{
		Uint32 c;
		const int len = text_decode_char(s, &c);
		if (len == 1 && c >= 0x80)
			return false;
		s += len;
	}
	return true;
}

static int count_char(const char *s, char c)
{
	int n = 0;
	for (; *s != '\0'; ++s)
		n += *s == c;
	return n;
}

/** The printf-style placeholders of a string, e.g. "%d%s". */
static void placeholders(const char *s, char *out, size_t size)
{
	// Only printf conversions (optionally with a width) count: a '%' in prose,
	// as in "50% of", is a percent sign (cube text is not a format string).
	size_t len = 0;
	for (; *s != '\0'; ++s)
	{
		if (*s != '%')
			continue;
		const char *c = s + 1;
		while (*c >= '0' && *c <= '9')
			++c;
		if (*c != '\0' && strchr("dsiuxc%", *c) != NULL && len + 3 < size)
		{
			out[len++] = '%';
			out[len++] = *c;
			s = c;
		}
	}
	out[len] = '\0';
}

/**
 * Font and room in pixels of text whose place on screen is known (measured
 * from the game's screens); returns -1 for other text.
 */
static int width_limit(const char *key, int *max_width)
{
	static const struct { const char *prefix; const char *suffix; int font; int width; } limits[] =
	{
		{ "ep", NULL, TINY_FONT, 300 },                         // briefings and hints, from x = 10
		{ "cube", ".header", FONT_SHAPES, 150 },                // title box of the game menu screen
		{ "help.menuInt.", ".0", FONT_SHAPES, 150 },
		{ "ui.next_level", NULL, FONT_SHAPES, 150 },
		{ "help.menuInt.", NULL, SMALL_FONT_SHAPES, 145 },      // items of the game menu, from x = 166
		{ "help.mainMenuHelp.", NULL, TINY_FONT, 300 },         // bottom line of the game menu screen
		{ "level.", NULL, TINY_FONT, 46 },                      // level name box of the game screen
		{ "help.menuText.", NULL, SMALL_FONT_SHAPES, 220 },     // title screen menu
	};

	for (size_t i = 0; i < COUNTOF(limits); ++i)
	{
		if (strncmp(key, limits[i].prefix, strlen(limits[i].prefix)) != 0)
			continue;
		if (limits[i].suffix != NULL)
		{
			const size_t key_len = strlen(key), suffix_len = strlen(limits[i].suffix);
			if (key_len < suffix_len || strcmp(key + key_len - suffix_len, limits[i].suffix) != 0)
				continue;
		}
		if (strncmp(key, "ep", 2) == 0 && (key[2] < '0' || key[2] > '9'))
			continue;

		if (max_width != NULL)
			*max_width = limits[i].width;
		return limits[i].font;
	}
	return -1;
}

static int check(const char *dir)
{
	read_translations(dir);

	int translated = 0;
	for (size_t i = 0; i < translation_count; ++i)
	{
		Translation *t = &translations[i];
		const TextEntry *e = find_entry(t->key);

		if (t->seen)
			continue;
		for (size_t j = i + 1; j < translation_count; ++j)
		{
			if (strcmp(translations[j].key, t->key) == 0)
			{
				report_line("경고: %s:%d: %s가 %s:%d에 또 있습니다 (나중 것을 씁니다)", t->file, t->line, t->key, translations[j].file, translations[j].line);
				++report_warnings;
				translations[j].seen = true;
			}
		}

		if (e == NULL)
		{
			report_line("오류: %s:%d: 게임에 없는 열쇠입니다: %s", t->file, t->line, t->key);
			++report_errors;
			continue;
		}

		if (strcmp(t->msgid, e->text) != 0)
		{
			report_line("경고: %s:%d: %s의 영어 원문이 게임과 다릅니다. --dump-text로 다시 추출해 보세요", t->file, t->line, t->key);
			++report_warnings;
		}

		if (t->msgstr[0] == '\0')
			continue;
		if (!t->fuzzy)
			++translated;

		if (!valid_utf8(t->msgstr))
		{
			report_line("오류: %s:%d: %s: UTF-8이 아닌 글자가 있습니다", t->file, t->line, t->key);
			++report_errors;
			continue;
		}

		if (e->max_lines > 0)
		{
			// Each line separately.
			int lines = 0;
			for (const char *line = t->msgstr; line != NULL; ++lines)
			{
				const char *end = strchr(line, '\n');
				const size_t len = end != NULL ? (size_t)(end - line) : strlen(line);
				if (e->size > 0 && len >= e->size)
				{
					report_line("오류: %s:%d: %s: %d번째 줄이 너무 깁니다 (%d바이트, 최대 %d바이트). 잘려서 나옵니다", t->file, t->line, t->key, lines + 1, (int)len, (int)e->size - 1);
					++report_errors;
				}
				line = end != NULL ? end + 1 : NULL;
			}
			if (lines > e->max_lines)
			{
				report_line("오류: %s:%d: %s: 줄이 너무 많습니다 (%d줄, 최대 %d줄)", t->file, t->line, t->key, lines, e->max_lines);
				++report_errors;
			}
		}
		else
		{
			const size_t len = strlen(t->msgstr);
			if (e->size > 0 && len >= e->size)
			{
				report_line("오류: %s:%d: %s: 너무 깁니다 (%d바이트, 최대 %d바이트). 잘려서 나옵니다", t->file, t->line, t->key, (int)len, (int)e->size - 1);
				++report_errors;
			}
		}

		if (count_char(e->text, '~') % 2 == 0 && count_char(t->msgstr, '~') % 2 != 0)
		{
			report_line("경고: %s:%d: %s: 강조 기호 ~의 짝이 맞지 않습니다", t->file, t->line, t->key);
			++report_warnings;
		}

		char a[32], b[32];
		placeholders(e->text, a, sizeof(a));
		placeholders(t->msgstr, b, sizeof(b));
		if (strcmp(a, b) != 0)
		{
			report_line("오류: %s:%d: %s: 자리 표시자가 원문(%s)과 다릅니다(%s)", t->file, t->line, t->key, a, b);
			++report_errors;
		}

		if (t->msgstr[0] == ' ' && e->text[0] != ' ' && e->text[0] != '\n')
		{
			report_line("경고: %s:%d: %s: 번역이 공백으로 시작합니다", t->file, t->line, t->key);
			++report_warnings;
		}

		// Credits lines start with a letter giving their color.
		if (strncmp(t->key, "credits.", 8) == 0 && t->msgstr[0] != e->text[0])
		{
			report_line("오류: %s:%d: %s: 첫 글자(색 기호 '%c')를 바꾸면 안 됩니다", t->file, t->line, t->key, e->text[0]);
			++report_errors;
		}

		// Text whose room on screen is known.
		const int limit_font = width_limit(t->key, NULL);
		if (limit_font >= 0)
		{
			int max_width;
			width_limit(t->key, &max_width);

			int line_number = 1;
			for (const char *line = t->msgstr; line != NULL; ++line_number)
			{
				const char *end = strchr(line, '\n');
				char buffer[1024];
				SDL_strlcpy(buffer, line, MIN((size_t)(end != NULL ? end - line : (ptrdiff_t)strlen(line)) + 1, sizeof(buffer)));

				const int width = JE_textWidth(buffer, (unsigned int)limit_font);
				if (width > max_width)
				{
					report_line("경고: %s:%d: %s: %s화면 칸보다 넓습니다 (%d픽셀, 칸 약 %d픽셀)", t->file, t->line, t->key,
					            e->max_lines > 0 ? (line_number == 1 ? "1번째 줄이 " : "줄 하나가 ") : "", width, max_width);
					++report_warnings;
				}
				line = end != NULL ? end + 1 : NULL;
			}
			continue;
		}

		// Other single lines (not wrapped by the game) that are much wider
		// than the English might not fit on screen.
		const bool wrapped = strncmp(t->key, "cube", 4) == 0 || strncmp(t->key, "help.helpTxt.", 13) == 0 || strncmp(t->key, "help.shipInfo.", 14) == 0;
		if (!wrapped && e->max_lines == 0)
		{
			const int english_width = JE_textWidth(e->text, TINY_FONT),
			          width = JE_textWidth(t->msgstr, TINY_FONT);
			if (width > english_width + english_width / 4 + 4)
			{
				report_line("참고: %s:%d: %s: 영어보다 꽤 넓습니다 (작은 글씨 기준 %d픽셀, 영어 %d픽셀). 화면에서 넘치지 않는지 확인하세요", t->file, t->line, t->key, width, english_width);
				++report_notes;
			}
		}
	}

	int missing = 0;
	for (size_t i = 0; i < entry_count; ++i)
		if (find_translation(entries[i].key) == NULL)
			++missing;
	if (missing > 0)
	{
		report_line("경고: 번역 파일에 없는 문장이 %d개 있습니다. --dump-text로 다시 추출하세요", missing);
		++report_warnings;
	}

	report_line("검사: 게임 문장 %d개 중 %d개 번역됨 (%d%%), 오류 %d개, 경고 %d개, 참고 %d개",
	            (int)entry_count, translated, entry_count > 0 ? (int)(translated * 100 / entry_count) : 0, report_errors, report_warnings, report_notes);

	return report_errors > 0 ? 1 : 0;
}

/******************************************************************************/

int textdump_run(void)
{
	char dir[1024];
	snprintf(dir, sizeof(dir), "%s/kor/lang", base_dir());

	if (!data_dir_found())
	{
		fprintf(stderr, "error: the Tyrian 2000 data files were not found\n");
		return 1;
	}

	collect_all();

	// The sprite fonts, for measuring text.
	if (textcheck_requested)
		JE_loadMainShapeTables("tyrian.shp");

	const int result = textdump_requested ? dump(dir) : check(dir);

	FILE *f = dir_fopen(dir, textdump_requested ? "dump-report.txt" : "check-report.txt", "wb");
	if (f != NULL)
	{
		if (report != NULL)
			fputs(report, f);
		fclose(f);
	}
	if (report != NULL)
		fputs(report, stdout);

	return result;
}
