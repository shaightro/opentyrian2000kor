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
#ifndef WEAPDPS_H
#define WEAPDPS_H

/*
 * Damage figures shown in the shop (an OpenTyrian addition).
 *
 * They are worked out from the weapon data, assuming every shot hits one enemy:
 *  - a normal shot does its damage once;
 *  - a shot that sets off a chain reaction does the damage of the explosion it creates;
 *  - a piercing shot (damage 250 and up) does its damage every frame it overlaps an enemy, counted
 *    for a small enemy sitting in the shot's path;
 *  - an ice shot only freezes and does no damage.
 * Rates use the normal game speed and ignore the generator: a weak generator can fire less often.
 */

#include "opentyr.h"

/** Average damage per second of weapon wpnum while firing continuously. */
int weapon_dps(JE_word wpnum);

/** Average damage of one firing of weapon wpnum (for sidekicks with limited ammo). */
int weapon_damage_per_fire(JE_word wpnum);

#endif /* WEAPDPS_H */
