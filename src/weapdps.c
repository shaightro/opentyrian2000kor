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
#include "weapdps.h"

#include "episodes.h"
#include "lvlmast.h"

#include <math.h>
#include <stdlib.h>

// Game frames per second at normal speed: a frame lasts two timer periods of 0x4300 ticks of the
// 1.19 MHz PC timer (JE_setNewGameSpeed in config.c, setDelaySpeed in nortsong.c).
#define FRAMES_PER_SECOND (14318180.0 / 12.0 / (2 * 0x4300))

// Chain reactions can set off further chain reactions; follow only a few steps.
#define MAX_CHAIN_DEPTH 3

static double fire_damage_from_start(JE_word wpnum, int depth);

static const JE_WeaponType *weapon_of(JE_word wpnum)
{
	return (wpnum > 0 && wpnum <= WEAP_NUM) ? &weapons[wpnum] : NULL;
}

/** The next pattern position after pos, as player_shot_create() (shots.c) advances shotMultiPos. */
static int next_position(const JE_WeaponType *weapon, int pos)
{
	return (pos == weapon->max || pos > 8) ? 1 : pos + 1;
}

/** Number of pattern positions a weapon cycles through. */
static int cycle_length(const JE_WeaponType *weapon)
{
	return (weapon->max >= 1 && weapon->max <= 8) ? weapon->max : 9;
}

/**
 * Frames during which a piercing shot overlaps a small enemy (the collision box in JE_main is
 * |dx| < 13, |dy| < 15) placed on the shot's path, using the movement of player_shot_move_and_draw()
 * with the player standing still.
 */
static int contact_frames(const JE_WeaponType *weapon, int k)
{
	int life = weapon->del[k] == 121 ? 255 : weapon->del[k];

	int xm = weapon->sx[k], xc = weapon->accelerationx;
	int ym, yc = -weapon->acceleration;
	if (weapon->del[k] == 99 || weapon->del[k] == 100)
		ym = MIN(MAX(-weapon->sy[k], -4), 4);  // aimed at the mouse, taken to be on the ship
	else if (weapon->sy[k] == 98)
		ym = 0, yc = -1;
	else if (weapon->sy[k] > 100)
		ym = weapon->sy[k];
	else
		ym = -weapon->sy[k];

	// the shot leaves the screen about this far from a ship near the bottom middle
	const int left = -164, right = 160, up = -175, down = 30;

	int cx = 0, cy = 0;
	int frames = 0;

	// first pass: the enemy sits where the shot first gets clear of the ship (or at the shot's
	// first position if it never moves away); second pass: count the frames inside its box
	for (int pass = 0; pass < 2; ++pass)
	{
		int x = 0, y = 0;
		int vx = xm, vy = ym;

		for (int t = 0; t < life; ++t)
		{
			vx += xc;
			x += vx;
			if (vx > 100)
				x -= (vx == 101) ? 101 : 120;

			vy += yc;
			y += vy;
			if (vy > 100)
				y -= 120;

			if (x < left || x > right || y < up || y > down)
				break;

			if (pass == 0)
			{
				if (t == 0)
					cx = x, cy = y;
				if (abs(x) >= 40 || abs(y) >= 40)
				{
					cx = x, cy = y;
					break;
				}
			}
			else if (abs(x - cx) < 13 && abs(y - cy) < 15)
			{
				++frames;
			}
		}
	}

	return frames;
}

/** Damage of the shot made at pattern position k (0-based) of a weapon. */
static double shot_damage(const JE_WeaponType *weapon, int k, int depth)
{
	if (k >= 8 || weapon->sg[k] == 0 || weapon->del[k] == 0)
		return 0;  // no shot is made

	const int attack = weapon->attack[k];
	if (attack == 99)
		return 0;  // ice: freezes only
	if (attack > 99 && attack < 250)
		return depth < MAX_CHAIN_DEPTH ? fire_damage_from_start((JE_word)(attack - 100), depth + 1) : 0;
	if (attack >= 250)
		return (double)(attack - 250) * contact_frames(weapon, k);
	return attack;
}

/** Damage of one firing that starts the pattern over, as chain reactions do. */
static double fire_damage_from_start(JE_word wpnum, int depth)
{
	const JE_WeaponType *weapon = weapon_of(wpnum);
	if (weapon == NULL)
		return 0;

	double damage = 0;
	int pos = 0;
	for (int i = 0; i < weapon->multi; ++i)
	{
		pos = next_position(weapon, pos);
		damage += shot_damage(weapon, pos - 1, depth);
	}
	return damage;
}

/** Average damage of one firing over the weapon's pattern. */
static double average_fire_damage(const JE_WeaponType *weapon)
{
	// Each firing makes multi shots at the next pattern positions, so over a whole number of
	// cycles every position is used equally often.
	const int length = cycle_length(weapon);
	double cycle = 0;
	for (int pos = 1; pos <= length; ++pos)
		cycle += shot_damage(weapon, pos - 1, 0);
	return weapon->multi * cycle / length;
}

static int clamp_figure(double value)
{
	const long rounded = lround(value);
	return (int)MIN(MAX(rounded, 0L), 9999L);
}

int weapon_dps(JE_word wpnum)
{
	const JE_WeaponType *weapon = weapon_of(wpnum);
	if (weapon == NULL)
		return 0;

	// after firing, the weapon waits shotrepeat frames (shotRepeat counts down to 0)
	return clamp_figure(average_fire_damage(weapon) * FRAMES_PER_SECOND / (weapon->shotrepeat + 1));
}

int weapon_damage_per_fire(JE_word wpnum)
{
	const JE_WeaponType *weapon = weapon_of(wpnum);
	return weapon == NULL ? 0 : clamp_figure(average_fire_damage(weapon));
}
