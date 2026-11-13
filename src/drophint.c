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
#include "drophint.h"

#include "backgrnd.h"
#include "config.h"
#include "episodes.h"
#include "lvlmast.h"
#include "palette.h"
#include "tyrian2.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

enum
{
	HINT_NONE = 0,
	HINT_WAIT,   // a better drop is coming: wait
	HINT_NOW,    // it drops a major item now: shoot
	HINT_KILL,   // destroying it lets a later item appear
	HINT_SPARE,  // letting it go lets a later item appear
	HINT_EITHER, // destroying it and letting it go each lead to different later items
};

// An enemy counts as "worth waiting for" only if it is on screen at least this long before its
// drop improves; drops handed out as an enemy appears need no waiting.
#define MIN_WAIT_FRAMES 10    // about 0.3 seconds

// How far ahead the hints look: level events, and frames of simulated enemy movement.
#define LOOKAHEAD_LOC 3000
#define LOOKAHEAD_FRAMES 1050  // about 30 seconds

#define RANK_ITEM 100000L

// Trigger enemies: how many later trigger assignments the look-ahead follows both ways.
#define MAX_TRIGGER_BRANCHES 8

static Uint8 hint[100];
static Uint8 colorWait, colorNow, colorKill, colorSpare, colorShadow;
static unsigned int frameCount;

/**
 * Whether a weapon pick-up (value 30000 + front weapon number) is one worth going for: the Laser (4)
 * and the Zica Laser (5).  Super Arcade replaces weapon pick-ups with its own ship's weapons.
 */
static bool prized_weapon(int value)
{
	const int port = value - 30000;
	return superArcadeMode == SA_NONE && (port == 4 || port == 5);
}

/** How good a drop (an enemy type created when an enemy dies) is; see the pick-up code in mainint.c. */
static long drop_rank(JE_word drop)
{
	if (drop == 0 || drop > ENEMY_NUM)
		return 0;

	const int value = enemyDat[drop].value;
	if (value == 0)
		return 0;                         // nothing to pick up
	if (value == 30000)
		return 1;                         // purple power orb (arcade): minor
	if (value > 30000)                    // weapon, sidekick or special: they only come in the arcade
		return prized_weapon(value) ? RANK_ITEM : 1;  // modes, where they are common and just swap
		                                  // what is fitted; only the lasers count
	if (value > 1 && value < 10000)
		return value >= 1000 ? value : 1; // credits: 1,000 and more count
	return RANK_ITEM;                     // data cube (1), power-ups and the like (< 0), warp orbs
	                                      // and armor
}

static bool drop_is_major(JE_word drop)
{
	return drop_rank(drop) >= 1000;
}

static bool drop_is_better(JE_word from, JE_word to)
{
	return drop_is_major(to) && drop_rank(to) > drop_rank(from);
}

/** Whether a drop event (33, 45, 85) takes effect in the current game mode; see JE_eventSystem. */
static bool drop_event_applies(const struct JE_EventRecType *event)
{
	switch (event->eventtype)
	{
	case 33:
		return !((event->eventdat == 512 || event->eventdat == 513) && (twoPlayerMode || onePlayerAction || superTyrian));
	case 45:
		return !superTyrian && (twoPlayerMode || onePlayerAction);
	case 85:
		return timedBattleMode;
	default:
		return false;
	}
}

static JE_word drop_of_event(const struct JE_EventRecType *event)
{
	if (event->eventtype == 33 && superArcadeMode != SA_NONE && event->eventdat == 534)
		return 827;
	return (JE_word)event->eventdat;
}

/** One frame of enemy movement as in JE_drawEnemy, without the random homing acceleration. */
static bool move_enemy(struct JE_SingleEnemyType *e, int backMoveY)
{
	if (e->excc)
	{
		if (--e->exccw <= 0)
		{
			if (e->exc == e->exrev)
			{
				e->excc = -e->excc;
				e->exrev = -e->exrev;
				e->exccadd = -e->exccadd;
			}
			else
			{
				e->exc += e->exccadd;
				e->exccw = e->exccwmax;
				if (e->exc == e->exrev)
				{
					e->excc = -e->excc;
					e->exrev = -e->exrev;
					e->exccadd = -e->exccadd;
				}
			}
		}
	}

	if (e->eycc)
	{
		if (--e->eyccw <= 0)
		{
			if (e->eyc == e->eyrev)
			{
				e->eycc = -e->eycc;
				e->eyrev = -e->eyrev;
				e->eyccadd = -e->eyccadd;
			}
			else
			{
				e->eyc += e->eyccadd;
				e->eyccw = e->eyccwmax;
				if (e->eyc == e->eyrev)
				{
					e->eycc = -e->eycc;
					e->eyrev = -e->eyrev;
					e->eyccadd = -e->eyccadd;
				}
			}
		}
	}

	e->ey += e->fixedmovey;

	e->ex += e->exc;
	if (e->ex < -80 || e->ex > 340)
		return false;

	e->ey += e->eyc;
	if (e->ey < -112 || e->ey > 190)
		return false;

	if (e->ex <= e->xminbounce || e->ex >= e->xmaxbounce)
		e->exc = -e->exc;
	if (e->ey <= e->yminbounce || e->ey >= e->ymaxbounce)
		e->eyc = -e->eyc;

	if (e->scoreitem)
	{
		if (e->ex < -5)
			e->ex++;
		if (e->ex > 245)
			e->ex--;
	}

	e->ey += backMoveY;
	return true;
}

/**
 * Whether a movement event (19 speed, 20 acceleration, 27 reversal limits) reaches enemy slot i
 * whose link group is link; see JE_eventSystem.
 */
static bool movement_event_hits(const struct JE_EventRecType *event, int i, JE_byte link)
{
	const int select = event->eventdat3;
	const bool fromNewPL = select > 79 && select < 90;  // link group chosen earlier by event 75
	const int target = fromNewPL ? newPL[select - 80] : event->eventdat4;

	if (event->eventtype == 19)
	{
		if (fromNewPL)
			return link == target;
		switch (select)
		{
		case 0:  return link == target;
		case 2:  return i < 25;
		case 1:  return i >= 25 && i < 50;
		case 3:  return i >= 50 && i < 75;
		case 99: return true;
		default: return false;
		}
	}
	return target == 0 || link == target;  // 20 and 27
}

/** Applies a movement event to the copy of an enemy, as JE_eventSystem does. */
static void apply_movement_event(struct JE_SingleEnemyType *e, const struct JE_EventRecType *event)
{
	const int dx = event->eventdat, dy = event->eventdat2;

	switch (event->eventtype)
	{
	case 19:
		if (dx != -99)
			e->exc = dx;
		if (dy != -99)
			e->eyc = dy;
		if (event->eventdat6 != 0)
			e->fixedmovey = event->eventdat6 == -99 ? 0 : event->eventdat6;
		break;
	case 20:
		if (dx != -99)
		{
			e->excc = dx;
			e->exccw = abs(dx);
			e->exccwmax = abs(dx);
			e->exccadd = dx > 0 ? 1 : -1;
		}
		if (dy != -99)
		{
			e->eycc = dy;
			e->eyccw = abs(dy);
			e->eyccwmax = abs(dy);
			e->eyccadd = dy > 0 ? 1 : -1;
		}
		break;
	case 27:
		if (dx != -99)
			e->exrev = dx;
		if (dy != -99)
			e->eyrev = dy;
		break;
	}
}

/** Whether the enemy would be drawn (and could be shot); see JE_drawEnemy. */
static bool on_screen(const struct JE_SingleEnemyType *e)
{
	const int x = e->ex + e->mapoffset;
	return x > -29 && x < 300 && e->ey > -13 && e->ey < 190;
}

/**
 * Plays the next frames for enemy i (its movement, the background scrolling that drives level
 * time, and the level events) and tells whether it gets a better drop while it can still be shot.
 */
static bool better_drop_coming(int i)
{
	struct JE_SingleEnemyType e = enemy[i];
	const JE_word drop = e.enemydie;
	const int layer = i / 25;  // 0: sky, 1 and 3: ground, 2: top (see JE_main)

	int move = backMove, move3 = backMove3;
	int delay = map1YDelay, delayMax = map1YDelayMax;
	unsigned long loc = curLoc;
	JE_byte link = e.linknum;
	unsigned int ev = eventLoc - 1;
	bool better = false;

	for (int frame = 1; frame <= LOOKAHEAD_FRAMES; ++frame)
	{
		// background 1 and level time (JE_main)
		if (forceEvents && move == 0)
			++loc;
		if (delayMax > 1 && move < 2)
			move = (delay == 1) ? 1 : 0;
		if (--delay == 0)
		{
			delay = delayMax;
			loc += move;
		}

		if (!move_enemy(&e, layer == 0 ? 0 : layer == 2 ? move3 : move))
			return false;

		// events at the start of the next frame (JE_eventSystem); movement events keep steering
		// the enemy even after the better drop is known
		while (ev < maxEvent && eventRec[ev].eventtime <= loc)
		{
			const struct JE_EventRecType *event = &eventRec[ev++];
			switch (event->eventtype)
			{
			case 19:
			case 20:
			case 27:
				if (movement_event_hits(event, i, link))
					apply_movement_event(&e, event);
				break;
			case 2:
			case 30:
				delay = delayMax = 1;
				move = event->eventdat;
				move3 = event->eventdat3;
				break;
			case 3:
				move = 1;
				delay = delayMax = 3;
				move3 = 1;
				break;
			case 39:
				if (link == event->eventdat)
					link = (JE_byte)event->eventdat2;
				break;
			case 33:
			case 45:
			case 85:
				if (event->eventdat4 == link && drop_event_applies(event) && drop_is_better(drop, drop_of_event(event)))
					better = true;
				break;
			}
		}

		// The better drop counts once the enemy is on screen with it, and only if the player
		// had a real chance to shoot the enemy before.
		if (better && on_screen(&e))
			return enemy[i].visibleFrames + frame >= MIN_WAIT_FRAMES;
	}

	return false;
}

/*
 * Trigger enemies.  Event 60 makes the enemies of a link group set a level flag when destroyed,
 * and event 61 later skips events when a flag has a given value; skipped events can be the drop
 * or the enemy that brings a warp orb or another item.  So destroying such an enemy, or letting
 * it go, decides which items appear later in the level.  Flags can be chained: in Gyges, each
 * flagged marker only appears if the one before it was destroyed, and the third carries a warp.
 */

typedef struct
{
	Uint32 bits[(EVENT_MAXIMUM + 31) / 32];
} EventSet;

/** Link groups (enemy linknum) that have enemies; drops and trigger marks only reach those. */
typedef struct
{
	Uint32 bits[256 / 32];
} LinkSet;

static bool link_present(const LinkSet *set, int link)
{
	return link >= 0 && link < 256 && (set->bits[link / 32] & (1u << (link % 32))) != 0;
}

static void link_set(LinkSet *set, int link, bool present)
{
	if (link < 0 || link >= 256)
		return;
	if (present)
		set->bits[link / 32] |= 1u << (link % 32);
	else
		set->bits[link / 32] &= ~(1u << (link % 32));
}

/** Whether an event creates enemies of its link group eventdat4; see JE_eventSystem. */
static bool is_spawn_event(const struct JE_EventRecType *event)
{
	switch (event->eventtype)
	{
	case 6:   // ground
	case 7:   // top
	case 10:  // ground 2
	case 12:  // 4x4 ground
	case 15:  // sky
	case 17:  // ground, bottom
	case 18:  // sky, bottom
	case 23:  // sky on bottom
	case 32:  // top
	case 49:  // custom enemies
	case 50:
	case 51:
	case 52:
	case 56:  // ground 2, bottom
		return true;
	default:
		return false;
	}
}

static bool spawn_brings_item(unsigned int type)
{
	if (type == 0 || type > ENEMY_NUM)
		return false;
	return (enemyDat[type].armor == 0 && drop_is_major((JE_word)type)) ||  // a pick-up placed in the level
	       drop_is_major(enemyDat[type].eenemydie);                       // an enemy that always drops one
}

/** Whether event ev brings a major item (a drop to a link group that has enemies, or an enemy that is or carries one). */
static bool event_brings_item(const struct JE_EventRecType *event, const LinkSet *present)
{
	switch (event->eventtype)
	{
	case 33:
	case 45:
	case 85:
		return link_present(present, event->eventdat4) && drop_event_applies(event) && drop_is_major(drop_of_event(event));
	case 12:  // 4x4 enemy
		for (int k = 0; k < 4; ++k)
			if (event->eventdat >= 0 && spawn_brings_item((unsigned int)event->eventdat + k))
				return true;
		return false;
	default:
		return is_spawn_event(event) && event->eventdat > 0 && spawn_brings_item((unsigned int)event->eventdat);
	}
}

static bool flag_value(unsigned int flags, int flag)
{
	return flag >= 1 && flag <= 10 && (flags & (1u << (flag - 1))) != 0;
}

/**
 * Collects the item events met from event ev to the end of the level, given the level flags and
 * the link groups that have enemies.  At each later trigger assignment that reaches enemies, both
 * outcomes (destroyed or not) are followed, so the result is every item that can still be reached.
 * Jumps between level sections are not followed, and enemies are taken to stay until the end.
 */
static void reachable_items(unsigned int ev, unsigned int flags, LinkSet present, int branches, EventSet *items)
{
	for (; ev < maxEvent; ++ev)
	{
		const struct JE_EventRecType *event = &eventRec[ev];
		int skip = 0;

		switch (event->eventtype)
		{
		case 39:  // link group change
			if (link_present(&present, event->eventdat))
			{
				link_set(&present, event->eventdat, false);
				link_set(&present, event->eventdat2, true);
			}
			break;
		case 60:  // later trigger enemies: they may be destroyed or not
		{
			const int flag = event->eventdat;
			const bool setto = event->eventdat2 == 1;
			if (branches > 0 && flag >= 1 && flag <= 10 && flag_value(flags, flag) != setto &&
			    link_present(&present, event->eventdat4))
			{
				const unsigned int bit = 1u << (flag - 1);
				reachable_items(ev + 1, setto ? (flags | bit) : (flags & ~bit), present, branches - 1, items);
			}
			break;
		}
		case 61:  // skip events if a flag has a value
			if ((event->eventdat2 == 0 || event->eventdat2 == 1) && flag_value(flags, event->eventdat) == (event->eventdat2 == 1))
				skip = event->eventdat3;
			break;
		case 63:  // skip events if not in 2-player mode
			if (!twoPlayerMode && !onePlayerAction)
				skip = event->eventdat;
			break;
		case 66:  // skip events below a difficulty
			if (initialDifficulty <= event->eventdat)
				skip = event->eventdat2;
			break;
		case 80:  // skip events in 2-player mode
			if (twoPlayerMode)
				skip = event->eventdat;
			break;
		default:
			if (event_brings_item(event, &present))
				items->bits[ev / 32] |= 1u << (ev % 32);
			if (is_spawn_event(event))
				link_set(&present, event->eventdat4, true);
			break;
		}

		if (skip > 0)
			ev += skip;
	}
}

static unsigned int current_flags(void)
{
	unsigned int flags = 0;
	for (int i = 0; i < 10; ++i)
		if (globalFlags[i])
			flags |= 1u << i;
	return flags;
}

/**
 * Link groups with enemies now; with flag > 0, leaving out the trigger enemies of that flag, which
 * are gone either way (destroyed or flown off).
 */
static LinkSet present_links(int flag)
{
	LinkSet present;
	memset(&present, 0, sizeof(present));
	for (int i = 0; i < 100; ++i)
		if (enemyAvail[i] != 1 && !(flag > 0 && enemy[i].special && enemy[i].flagnum == flag))
			link_set(&present, enemy[i].linknum, true);
	return present;
}

/** What destroying a trigger enemy (flag := setto) changes about the items still to come. */
static Uint8 trigger_hint(int flag, bool setto)
{
	// Results only change when the level moves on, a flag changes or enemies come and go, so they
	// are kept until then.
	static unsigned int cachedLoc = 0, cachedFlags = ~0u;
	static LinkSet cachedAlive;
	static Uint8 cached[11][2];  // [flag][setto]: 0 = not worked out yet, else hint + 1

	const unsigned int flags = current_flags();
	if (flag < 1 || flag > 10 || flag_value(flags, flag) == setto)
		return HINT_NONE;  // destroying it changes nothing

	const LinkSet alive = present_links(0);
	if (cachedLoc != eventLoc || cachedFlags != flags || memcmp(&alive, &cachedAlive, sizeof(alive)) != 0)
	{
		cachedLoc = eventLoc;
		cachedFlags = flags;
		cachedAlive = alive;
		memset(cached, 0, sizeof(cached));
	}

	if (cached[flag][setto] == 0)
	{
		static EventSet kill, spare;
		const LinkSet present = present_links(flag);
		const unsigned int bit = 1u << (flag - 1);

		memset(&spare, 0, sizeof(spare));
		reachable_items(eventLoc - 1, flags, present, MAX_TRIGGER_BRANCHES, &spare);
		memset(&kill, 0, sizeof(kill));
		reachable_items(eventLoc - 1, setto ? (flags | bit) : (flags & ~bit), present, MAX_TRIGGER_BRANCHES, &kill);

		bool killOnly = false, spareOnly = false;
		for (size_t i = 0; i < COUNTOF(kill.bits); ++i)
		{
			killOnly |= (kill.bits[i] & ~spare.bits[i]) != 0;
			spareOnly |= (spare.bits[i] & ~kill.bits[i]) != 0;
		}

		const Uint8 result = killOnly && spareOnly ? HINT_EITHER : killOnly ? HINT_KILL : spareOnly ? HINT_SPARE : HINT_NONE;
		cached[flag][setto] = result + 1;
	}

	return cached[flag][setto] - 1;
}

static Uint8 nearest_color(int r, int g, int b)
{
	Uint8 best = 0;
	long bestDistance = LONG_MAX;
	for (unsigned int i = 0; i < 256; ++i)
	{
		const long dr = colors[i].r - r, dg = colors[i].g - g, db = colors[i].b - b;
		const long distance = 3 * dr * dr + 4 * dg * dg + 2 * db * db;
		if (distance < bestDistance)
		{
			bestDistance = distance;
			best = i;
		}
	}
	return best;
}

void drophint_update(void)
{
	++frameCount;
	memset(hint, HINT_NONE, sizeof(hint));

	if (!showDropHints)
		return;

	// link groups that an upcoming event gives a major drop
	bool targeted[256] = { false };
	bool anyTargeted = false;
	for (unsigned int ev = eventLoc - 1; ev < maxEvent && eventRec[ev].eventtime <= curLoc + LOOKAHEAD_LOC; ++ev)
	{
		const struct JE_EventRecType *event = &eventRec[ev];
		if (drop_event_applies(event) && drop_is_major(drop_of_event(event)))
		{
			targeted[event->eventdat4] = true;
			anyTargeted = true;
		}
	}

	bool anyHint = false;
	for (int i = 0; i < 100; ++i)
	{
		if (enemyAvail[i] != 0)  // free slot, or dying
			continue;

		const struct JE_SingleEnemyType *e = &enemy[i];
		if (anyTargeted && targeted[e->linknum] && better_drop_coming(i))
			hint[i] = HINT_WAIT;
		else if (!e->scoreitem && drop_is_major(e->enemydie))
			hint[i] = HINT_NOW;
		else if (e->special)
			hint[i] = trigger_hint(e->flagnum, e->setto);

		anyHint |= hint[i] != HINT_NONE;
	}

	if (anyHint)
	{
		// level palettes differ, so pick the closest colors each time
		colorWait = nearest_color(255, 230, 0);
		colorNow = nearest_color(40, 255, 40);
		colorKill = nearest_color(255, 30, 30);
		colorSpare = nearest_color(60, 150, 255);
		colorShadow = nearest_color(0, 0, 0);
	}
}

static void put_pixel(SDL_Surface *surface, int x, int y, Uint8 color)
{
	if (x >= 0 && y >= 0 && x < surface->w && y < surface->h)
		((Uint8 *)surface->pixels)[y * surface->pitch + x] = color;
}

/** Corner brackets around the w x h box at (x, y). */
static void draw_corners(SDL_Surface *surface, int x, int y, int w, int h, Uint8 color)
{
	const int arm = 4;
	const int x2 = x + w - 1, y2 = y + h - 1;
	for (int k = 0; k < arm; ++k)
	{
		put_pixel(surface, x + k, y, color);
		put_pixel(surface, x, y + k, color);
		put_pixel(surface, x2 - k, y, color);
		put_pixel(surface, x2, y + k, color);
		put_pixel(surface, x + k, y2, color);
		put_pixel(surface, x, y2 - k, color);
		put_pixel(surface, x2 - k, y2, color);
		put_pixel(surface, x2, y2 - k, color);
	}
}

void drophint_draw(SDL_Surface *surface, int enemyIndex, int x, int y)
{
	const Uint8 state = hint[enemyIndex];
	if (state == HINT_NONE)
		return;
	if (state == HINT_WAIT && (frameCount & 8) != 0)  // blink while waiting
		return;

	// sprite box (see JE_drawEnemy), plus a margin
	int bx = x, by = y, bw = 12, bh = 14;
	if (enemy[enemyIndex].size == 1)
	{
		bx = x - 6;
		by = y - 7;
		bw = 24;
		bh = 28;
	}
	bx -= 2;
	by -= 2;
	bw += 4;
	bh += 4;

	Uint8 color;
	switch (state)
	{
	case HINT_WAIT:   color = colorWait; break;
	case HINT_NOW:    color = colorNow; break;
	case HINT_KILL:   color = colorKill; break;
	case HINT_SPARE:  color = colorSpare; break;
	default:          color = (frameCount & 8) ? colorKill : colorSpare; break;  // either way: alternate
	}

	draw_corners(surface, bx + 1, by + 1, bw, bh, colorShadow);
	draw_corners(surface, bx, by, bw, bh, color);
}
