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
#ifndef TEXTPREVIEW_H
#define TEXTPREVIEW_H

#include <stdbool.h>

/*
 * Translation preview, run instead of the game (--preview-text):
 *
 * draws the text screens that are hard to reach by playing -- every briefing
 * and secret hint, the credits, the ship specs, the data cube lists and every page of every
 * data cube -- with the game's own drawing code, and saves each frame as a
 * BMP file in the "text-preview" folder (with index.txt listing them).
 */

extern bool textpreview_requested;

/** Draws and saves the screens; returns the process exit code. */
int textpreview_run(void);

#endif /* TEXTPREVIEW_H */
