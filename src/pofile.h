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
#ifndef POFILE_H
#define POFILE_H

#include <stdbool.h>
#include <stdio.h>

/* Minimal reader and writer for gettext PO files (UTF-8, no plural forms). */

typedef struct
{
	const char *msgctxt;  // NULL if absent
	const char *msgid;
	const char *msgstr;
	bool fuzzy;           // "#, fuzzy": translation needs review
	int line;             // line number where the entry starts
} PoEntry;

typedef void (*PoCallback)(const PoEntry *entry, void *userdata);

/**
 * Calls \p callback for each entry of a PO file.  Returns the number of
 * syntax errors; *first_error_line receives the line of the first one.
 */
int po_parse_ex(FILE *f, PoCallback callback, void *userdata, int *first_error_line);

static inline int po_parse(FILE *f, PoCallback callback, void *userdata)
{
	int line;
	return po_parse_ex(f, callback, userdata, &line);
}

/** Writes a line such as: msgid "text with \"escapes\"" */
void po_write_string(FILE *f, const char *keyword, const char *text);

#endif /* POFILE_H */
