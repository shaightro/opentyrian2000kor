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
#ifndef DROPHINT_H
#define DROPHINT_H

/*
 * Drop timing hints (an OpenTyrian option, on by default).
 *
 * Level events hand items to the enemies that are alive at that moment, so some enemies drop
 * something better if the player waits before shooting them.  When the option is on, an enemy
 * on screen whose drop will become a major item (power-up, data cube, 1,000+ credits, warp orb...;
 * of the weapons and sidekicks of the arcade modes, only the Laser and the Zica Laser) gets a
 * blinking yellow frame, and an enemy that drops a major item when destroyed now (whether it
 * always carried it or got it while the player waited) gets a solid green one.
 * Enemies whose destruction sets a level flag that decides a later item get a red frame (destroy
 * it), a blue one (let it go), or both in turn (each choice leads to a different item).
 * The hints only read the game state; they never change it or use the random number generator.
 */

#include "opentyr.h"
#include "varz.h"

#include "SDL.h"

/** Works out the hints for this frame; call after the frame's events, before enemies are drawn. */
void drophint_update(void);

/** Draws the hint of enemy enemyIndex, whose sprite was just drawn at (x, y). */
void drophint_draw(SDL_Surface *surface, int enemyIndex, int x, int y);

#endif /* DROPHINT_H */
