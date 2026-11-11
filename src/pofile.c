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
#include "pofile.h"

#include <stdlib.h>
#include <string.h>

typedef struct
{
	char *data;
	size_t len, capacity;
} Buffer;

static void buffer_clear(Buffer *b)
{
	b->len = 0;
	if (b->data != NULL)
		b->data[0] = '\0';
}

static void buffer_append(Buffer *b, char c)
{
	if (b->len + 1 >= b->capacity)
	{
		size_t capacity = b->capacity == 0 ? 256 : b->capacity * 2;
		char *data = realloc(b->data, capacity);
		if (data == NULL)
			return;
		b->data = data;
		b->capacity = capacity;
	}
	b->data[b->len++] = c;
	b->data[b->len] = '\0';
}

static const char *buffer_str(const Buffer *b)
{
	return b->data != NULL ? b->data : "";
}

/** Reads a line without its line break; returns false at the end of the file. */
static bool read_line(FILE *f, Buffer *line)
{
	buffer_clear(line);

	int c;
	while ((c = getc(f)) != EOF && c != '\n')
		if (c != '\r')
			buffer_append(line, (char)c);

	return c != EOF || line->len > 0;
}

/** Appends the contents of a quoted string; returns false on a syntax error. */
static bool append_quoted(const char *s, Buffer *out)
{
	while (*s == ' ' || *s == '\t')
		++s;
	if (*s++ != '"')
		return false;

	for (; *s != '"'; ++s)
	{
		if (*s == '\0')
			return false;

		if (*s == '\\')
		{
			++s;
			switch (*s)
			{
			case 'n':  buffer_append(out, '\n'); break;
			case 't':  buffer_append(out, '\t'); break;
			case 'r':  buffer_append(out, '\r'); break;
			case '"':  buffer_append(out, '"');  break;
			case '\\': buffer_append(out, '\\'); break;
			case '\0': return false;
			default:   buffer_append(out, *s);   break;
			}
		}
		else
		{
			buffer_append(out, *s);
		}
	}

	for (++s; *s == ' ' || *s == '\t'; ++s)
		;
	return *s == '\0';
}

typedef enum { FIELD_NONE, FIELD_CTXT, FIELD_ID, FIELD_STR } Field;

int po_parse_ex(FILE *f, PoCallback callback, void *userdata, int *first_error_line)
{
	Buffer line = { 0 }, ctxt = { 0 }, id = { 0 }, str = { 0 };
	bool have_ctxt = false, have_id = false, have_str = false, fuzzy = false;
	Field field = FIELD_NONE;
	int line_number = 0, entry_line = 0, errors = 0;

	*first_error_line = 0;

#define ERROR() do { if (errors++ == 0) *first_error_line = line_number; } while (0)

#define FLUSH() do { \
		if (have_id && have_str) \
		{ \
			const PoEntry entry = { have_ctxt ? buffer_str(&ctxt) : NULL, buffer_str(&id), buffer_str(&str), fuzzy, entry_line }; \
			callback(&entry, userdata); \
		} \
		else if (have_ctxt || have_id || have_str) \
		{ \
			ERROR(); \
		} \
		buffer_clear(&ctxt); buffer_clear(&id); buffer_clear(&str); \
		have_ctxt = have_id = have_str = fuzzy = false; \
		field = FIELD_NONE; \
	} while (0)

	while (read_line(f, &line))
	{
		++line_number;

		char *s = line.data != NULL ? line.data : "";
		if (line_number == 1 && (unsigned char)s[0] == 0xef && (unsigned char)s[1] == 0xbb && (unsigned char)s[2] == 0xbf)
			s += 3;  // UTF-8 byte order mark
		while (*s == ' ' || *s == '\t')
			++s;

		if (*s == '\0')
		{
			FLUSH();
		}
		else if (*s == '#')
		{
			// A comment after the strings starts the next entry.
			if (have_str)
				FLUSH();
			if (s[1] == ',' && strstr(s, "fuzzy") != NULL)
				fuzzy = true;
		}
		else if (strncmp(s, "msgctxt", 7) == 0)
		{
			if (have_ctxt || have_id || have_str)
				FLUSH();
			if (!have_id && !have_str)
				entry_line = line_number;
			have_ctxt = true;
			field = FIELD_CTXT;
			if (!append_quoted(s + 7, &ctxt))
				ERROR();
		}
		else if (strncmp(s, "msgid_plural", 12) == 0)
		{
			ERROR();  // plural forms are not used
		}
		else if (strncmp(s, "msgid", 5) == 0)
		{
			if (have_id || have_str)
				FLUSH();
			if (!have_ctxt)
				entry_line = line_number;
			have_id = true;
			field = FIELD_ID;
			if (!append_quoted(s + 5, &id))
				ERROR();
		}
		else if (strncmp(s, "msgstr", 6) == 0)
		{
			if (!have_id || have_str)
				ERROR();
			have_str = true;
			field = FIELD_STR;
			if (!append_quoted(s + 6, &str))
				ERROR();
		}
		else if (*s == '"')
		{
			Buffer *target = field == FIELD_CTXT ? &ctxt : field == FIELD_ID ? &id : field == FIELD_STR ? &str : NULL;
			if (target == NULL || !append_quoted(s, target))
				ERROR();
		}
		else
		{
			ERROR();
		}
	}
	FLUSH();

#undef FLUSH
#undef ERROR

	free(line.data);
	free(ctxt.data);
	free(id.data);
	free(str.data);

	return errors;
}

void po_write_string(FILE *f, const char *keyword, const char *text)
{
	fprintf(f, "%s \"", keyword);
	for (const char *s = text; *s != '\0'; ++s)
	{
		switch (*s)
		{
		case '"':  fputs("\\\"", f); break;
		case '\\': fputs("\\\\", f); break;
		case '\n': fputs("\\n", f);  break;
		case '\t': fputs("\\t", f);  break;
		case '\r': fputs("\\r", f);  break;
		default:   fputc(*s, f);     break;
		}
	}
	fputs("\"\n", f);
}
