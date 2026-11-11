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
#include "file.h"

#include "opentyr.h"
#include "varz.h"

#include "SDL.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *custom_data_dir = NULL;

// directory containing the executable, without a trailing separator ("" if unknown)
const char *base_dir(void)
{
	static char *dir = NULL;

	if (dir == NULL)
	{
		dir = SDL_GetBasePath();
		if (dir == NULL)
		{
			dir = SDL_strdup("");
		}
		else
		{
			size_t len = strlen(dir);
			if (len > 1 && (dir[len - 1] == '/' || dir[len - 1] == '\\'))
				dir[len - 1] = '\0';
		}
	}

	return dir != NULL ? dir : "";
}

// finds the Tyrian data directory
const char *data_dir(void)
{
	static const char *dir = NULL;

	if (dir != NULL)
		return dir;

	// Look next to the executable first so that the game works no matter
	// which working directory it was started from (e.g. via a shortcut).
	static char base_data_dir[1024];
	const bool have_base = base_dir()[0] != '\0' &&
		(size_t)snprintf(base_data_dir, sizeof(base_data_dir), "%s/data", base_dir()) < sizeof(base_data_dir);

	const char *const dirs[] =
	{
		custom_data_dir,
		have_base ? base_data_dir : NULL,
		have_base ? base_dir() : NULL,
		TYRIAN_DIR,
		"data",
		".",
	};

	for (uint i = 0; i < COUNTOF(dirs); ++i)
	{
		if (dirs[i] == NULL)
			continue;

		FILE *f = dir_fopen(dirs[i], "tyrian1.lvl", "rb");
		if (f)
		{
			fclose(f);

			dir = dirs[i];
			break;
		}
	}

	// data not found; not cached so that data installed later is still found
	if (dir == NULL)
		return "";

	return dir;
}

bool data_dir_found(void)
{
	return data_dir()[0] != '\0';
}

// check if a file can be created in the directory
bool dir_is_writable(const char *dir)
{
	static const char test_file[] = ".opentyrian_write_test";

	FILE *f = dir_fopen(dir, test_file, "wb");
	if (f == NULL)
		return false;
	fclose(f);

	char *path = malloc(strlen(dir) + 1 + sizeof(test_file));
	sprintf(path, "%s/%s", dir, test_file);
	remove(path);
	free(path);

	return true;
}

static void show_error_box(const char *message)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "OpenTyrian2000", message, NULL);
}

// prepend directory and fopen
FILE *dir_fopen(const char *dir, const char *file, const char *mode)
{
	char *path = malloc(strlen(dir) + 1 + strlen(file) + 1);
	sprintf(path, "%s/%s", dir, file);

	FILE *f = fopen(path, mode);

	free(path);

	return f;
}

// warn when dir_fopen fails
FILE *dir_fopen_warn(const char *dir, const char *file, const char *mode)
{
	FILE *f = dir_fopen(dir, file, mode);

	if (f == NULL)
		fprintf(stderr, "warning: failed to open '%s': %s\n", file, strerror(errno));

	return f;
}

// die when dir_fopen fails
FILE *dir_fopen_die(const char *dir, const char *file, const char *mode)
{
	FILE *f = dir_fopen(dir, file, mode);

	if (f == NULL)
	{
		fprintf(stderr, "error: failed to open '%s': %s\n", file, strerror(errno));
		fprintf(stderr, "error: One or more of the required Tyrian " TYRIAN_VERSION " data files could not be found.\n"
		                "       Please read the README file.\n");

		// (Korean text is passed as arguments rather than in the format string
		// to keep MSVC's format checker from warning about the code page.)
		static const char message_head[] = "필요한 게임 데이터 파일을 열 수 없습니다: ";
		static const char message_tail[] = "\n\nTyrian 2000 데이터 파일이 프로그램 옆의 data 폴더에 있어야 합니다.";
		char message[512];
		snprintf(message, sizeof(message), "%s%s%s", message_head, file, message_tail);
		show_error_box(message);

		JE_tyrianHalt(1);
	}

	return f;
}

// check if file can be opened for reading
bool dir_file_exists(const char *dir, const char *file)
{
	FILE *f = dir_fopen(dir, file, "rb");
	if (f != NULL)
		fclose(f);
	return (f != NULL);
}

// returns end-of-file position
long ftell_eof(FILE *f)
{
	long pos = ftell(f);

	fseek(f, 0, SEEK_END);
	long size = ftell(f);

	fseek(f, pos, SEEK_SET);

	return size;
}

void fread_die(void *buffer, size_t size, size_t count, FILE *stream)
{
	size_t result = fread(buffer, size, count, stream);
	if (result != count)
	{
		fprintf(stderr, "error: An unexpected problem occurred while reading from a file.\n");
		SDL_Quit();
		exit(EXIT_FAILURE);
	}
}

void fwrite_die(const void *buffer, size_t size, size_t count, FILE *stream)
{
	size_t result = fwrite(buffer, size, count, stream);
	if (result != count)
	{
		fprintf(stderr, "error: An unexpected problem occurred while writing to a file.\n");
		SDL_Quit();
		exit(EXIT_FAILURE);
	}
}
