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
#ifndef WINSETUP_H
#define WINSETUP_H

#include <stdbool.h>

#ifdef TARGET_WIN32

/**
 * Installs the Tyrian 2000 data files into the "data" folder next to the
 * executable: extracts a tyrian2000.zip placed next to the executable, or,
 * after asking the user, downloads it from the official freeware host first.
 *
 * Shows its own message boxes; returns false if the data is still missing.
 */
bool win_install_data_files(void);

#endif

#endif /* WINSETUP_H */
