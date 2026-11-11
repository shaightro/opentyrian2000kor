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
#ifndef TEXTDUMP_H
#define TEXTDUMP_H

#include <stdbool.h>

/*
 * Translation tools, run instead of the game:
 *
 *  --dump-text   writes all of the game's text to the PO files in kor/lang
 *                (keeping translations already there)
 *  --check-text  checks the translations in kor/lang and writes
 *                kor/lang/check-report.txt
 */

extern bool textdump_requested;
extern bool textcheck_requested;

/** Runs the requested tool; returns the process exit code. */
int textdump_run(void);

#endif /* TEXTDUMP_H */
