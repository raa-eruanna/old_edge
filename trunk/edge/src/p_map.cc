//----------------------------------------------------------------------------
//  EDGE Moving, Aiming, Shooting & Collision code
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2001  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------
//
// -MH- 1998/07/02 "shootupdown" --> "true3dgameplay"
//
// -AJA- 1999/07/19: Removed P_LineOpening.  Gaps are now stored 
//       in line_t, and updated whenever sector heights change.
//
// -AJA- 1999/07/21: Replaced some non-critical P_Randoms with M_Random.
//
// -AJA- 1999/07/30: Big changes for extra floor handling. Split
//       P_CheckPosition into two new routines (one handling absolute
//       positions, the other handling relative positions). Split the
//       PIT_Check* routines similiarly.
//

#include "i_defs.h"

#include "dm_defs.h"
#include "dm_state.h"
#include "m_bbox.h"
#include "m_random.h"
#include "p_local.h"
#include "r_state.h"
#include "s_sound.h"
#include "z_zone.h"

static float tmbbox[4];

typedef struct try_move_info_s
{
	// --- input --

	// thing trying to move
	mobj_t *mover;
	int flags, extflags;

	// attempted destination
	float x, y, z;

	// --- output ---

	subsector_t *sub;

	// vertical space over all contacted lines
	float floorz, ceilnz;
	float dropoff;

	// objects that end up above and below us
	mobj_t *above;
	mobj_t *below;

	// -AJA- FIXME: this is a "quick fix" (hack).  If only one line is
	// hit, and TryMove decides the move is impossible, then we know
	// this line must be the blocking line.  Real solution ?  Probably
	// to move most of the checks from TryMove into CheckRelLine.  It
	// definitely needs a lot of consideration.

	line_t *line_which;
	int line_count;
}
try_move_info_t;

static try_move_info_t tm_I;

bool mobj_hit_sky;
line_t *blockline;

// If "floatok" true, move would be ok if at float_destz.
bool floatok;
float float_destz;

// keep track of special lines as they are hit,
// but don't process them until the move is proven valid

stack_array_t spechit_a;
line_t **spechit = NULL;
int numspechit;

mobj_t *linetarget;  // who got hit (or NULL)

typedef struct shoot_trav_info_s
{
	mobj_t *source;
	float range;
	float start_z;
	angle_t angle;
	float slope;
	float topslope;
	float bottomslope;

	float damage;
	const damage_t *damtype;
	const mobjinfo_t *puff;
	float prev_z;
}
shoot_trav_info_t;

static shoot_trav_info_t shoot_I;
static shoot_trav_info_t aim_I;

// convenience function
static INLINE int PointOnLineSide(float x, float y, line_t *ld)
{
	divline_t div;

	div.x = ld->v1->x;
	div.y = ld->v1->y;
	div.dx = ld->dx;
	div.dy = ld->dy;

	return P_PointOnDivlineSide(x, y, &div);
}


//
// TELEPORT MOVE
// 

//
// PIT_StompThing
//
static bool PIT_StompThing(mobj_t * thing)
{
	float blockdist;

	if (!(thing->flags & MF_SHOOTABLE))
		return true;

	blockdist = thing->radius + tm_I.mover->radius;

	// check to see we hit it
	if (fabs(thing->x - tm_I.x) >= blockdist || fabs(thing->y - tm_I.y) >= blockdist)
		return true;  // no, we did not

	// check we aren't trying to stomp ourselves
	if (thing == tm_I.mover)
		return true;

	// -AJA- 1999/07/30: True 3d gameplay checks.
	if (level_flags.true3dgameplay)
	{
		if (tm_I.z >= thing->z + thing->height)
		{
			// went over
			tm_I.floorz = MAX(tm_I.floorz, thing->z + thing->height);
			return true;
		}

		if (tm_I.z + tm_I.mover->height <= thing->z)
		{
			// went under
			tm_I.ceilnz = MIN(tm_I.ceilnz, thing->z);
			return true;
		}
	}

	if (!tm_I.mover->player && (currentmap->force_off & MPF_Stomp))
		return false;

	P_DamageMobj(thing, tm_I.mover, tm_I.mover, 10000, NULL);
	return true;
}

//
// P_TeleportMove
//
bool P_TeleportMove(mobj_t * thing, float x, float y, float z)
{
	int xl, xh;
	int yl, yh;
	int bx, by;

	// kill anything occupying the position
	tm_I.mover = thing;
	tm_I.flags = thing->flags;
	tm_I.extflags = thing->extendedflags;

	tm_I.x = x;
	tm_I.y = y;
	tm_I.z = z;

	tmbbox[BOXTOP] = y + tm_I.mover->radius;
	tmbbox[BOXBOTTOM] = y - tm_I.mover->radius;
	tmbbox[BOXRIGHT] = x + tm_I.mover->radius;
	tmbbox[BOXLEFT] = x - tm_I.mover->radius;

	tm_I.sub = R_PointInSubsector(x, y);

	P_ComputeThingGap(thing, tm_I.sub->sector, z, &tm_I.floorz, &tm_I.ceilnz);

	// The base floor/ceiling is from the subsector that contains the point.
	// Any contacted lines the step closer together will adjust them.
	tm_I.dropoff = tm_I.floorz;
	tm_I.above = NULL;
	tm_I.below = NULL;

	validcount++;
	Z_SetArraySize(&spechit_a, numspechit = 0);

	// stomp on any things contacted
	xl = (int)(tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS) / MAPBLOCKUNITS;
	xh = (int)(tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS) / MAPBLOCKUNITS;
	yl = (int)(tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) / MAPBLOCKUNITS;
	yh = (int)(tmbbox[BOXTOP] - bmaporgy + MAXRADIUS) / MAPBLOCKUNITS;
	for (bx = xl; bx <= xh; bx++)
		for (by = yl; by <= yh; by++)
			if (!P_BlockThingsIterator(bx, by, PIT_StompThing))
				return false;

	// everything on the spot has been stomped,
	// so link the thing into its new position

	thing->floorz = tm_I.floorz;
	thing->ceilingz = tm_I.ceilnz;

	P_ChangeThingPosition(thing, x, y, z);

	return true;
}

//
// ABSOLUTE POSITION CLIPPING
//

//
// PIT_CheckAbsLine
//
static bool PIT_CheckAbsLine(line_t * ld)
{
	int i;

	if (tmbbox[BOXRIGHT] <= ld->bbox[BOXLEFT] ||
		tmbbox[BOXLEFT] >= ld->bbox[BOXRIGHT] ||
		tmbbox[BOXTOP] <= ld->bbox[BOXBOTTOM] ||
		tmbbox[BOXBOTTOM] >= ld->bbox[BOXTOP])
	{
		// no intersection with line
		return true;
	}

	if (P_BoxOnLineSide(tmbbox, ld) != -1)
		return true;

	// The spawning thing's position touches the given line.
	// If this should not be allowed, return false.

	if (!ld->backsector || ld->gap_num == 0)
		return false;  // one sided line

	if (tm_I.extflags & EF_CROSSLINES)
	{
		if ((ld->flags & ML_ShootBlock) && (tm_I.flags & MF_MISSILE))
			return false;
	}
	else
	{
		// explicitly blocking everything ?
		if (ld->flags & ML_Blocking)
			return false;

		// block monsters only ?
		if ((tm_I.extflags & EF_MONSTER) &&
			(ld->flags & ML_BlockMonsters))
		{
			return false;
		}
	}

	// does the thing fit in one of the line gaps ?
	for (i = 0; i < ld->gap_num; i++)
	{
		if (ld->gaps[i].f <= tm_I.z &&
			tm_I.z + tm_I.mover->height <= ld->gaps[i].c)
			return true;
	}

	return false;
}

//
// PIT_CheckAbsThing
//
static bool PIT_CheckAbsThing(mobj_t * thing)
{
	float blockdist;
	bool solid;

	if (thing == tm_I.mover)
		return true;

	if (!(thing->flags & (MF_SOLID | MF_SHOOTABLE)))
		return true;

	blockdist = thing->radius + tm_I.mover->radius;

	// Check that we didn't hit it
	if (fabs(thing->x - tm_I.x) >= blockdist || fabs(thing->y - tm_I.y) >= blockdist)
		return true;  // no we missed this thing

	// -KM- 1998/9/19 True 3d gameplay checks.
	if ((tm_I.flags & MF_MISSILE) || level_flags.true3dgameplay)
	{
		// overhead ?
		if (tm_I.z >= thing->z + thing->height)
			return true;

		// underneath ?
		if (tm_I.z + tm_I.mover->height <= thing->z)
			return true;
	}

	solid = (thing->flags & MF_SOLID);

	// check for missiles making contact
	// -ACB- 1998/08/04 Procedure for missile contact

	if (tm_I.mover->source && tm_I.mover->source == thing)
		return true;

	if (tm_I.flags & MF_MISSILE)
	{
		// thing isn't shootable, return depending on if the thing is solid.
		if (!(thing->flags & MF_SHOOTABLE))
			return !solid;

		// don't hurt the missile's shooter:
		if (tm_I.mover->source && tm_I.mover->source == thing)
			return true;

		P_ActMissileContact(tm_I.mover, thing);
		return (tm_I.extflags & EF_TUNNEL) ? true : false;
	}

	// -AJA- 2000/06/09: Follow MBF semantics: allow the non-solid
	// moving things to pass through solid things.
	return !solid || (thing->flags & MF_NOCLIP) || !(tm_I.flags & MF_SOLID);
}

//
// P_CheckAbsPosition
//
// Check whether the thing can be placed at the absolute position
// (x,y,z).  Makes no assumptions about the thing's current position.
// 
// This is purely informative, nothing is modified, nothing is picked
// up, no special lines are recorded, no special things are touched, and
// no information (apart from true/false) is returned.
// 
// Only used for checking if an object can be spawned at a
// particular location.
//
bool P_CheckAbsPosition(mobj_t * thing, float x, float y, float z)
{
	int xl, xh;
	int yl, yh;
	int bx, by;

	// can go anywhere
	if (tm_I.flags & MF_NOCLIP)
		return true;

	tm_I.mover = thing;
	tm_I.flags = thing->flags;
	tm_I.extflags = thing->extendedflags;

	tm_I.x = x;
	tm_I.y = y;
	tm_I.z = z;

	tmbbox[BOXTOP] = y + tm_I.mover->radius;
	tmbbox[BOXBOTTOM] = y - tm_I.mover->radius;
	tmbbox[BOXRIGHT] = x + tm_I.mover->radius;
	tmbbox[BOXLEFT] = x - tm_I.mover->radius;

	tm_I.sub = R_PointInSubsector(x, y);

	validcount++;

	// Check things first.
	// The bounding box is extended by MAXRADIUS
	// because mobj_ts are grouped into mapblocks
	// based on their origin point, and can overlap
	// into adjacent blocks by up to MAXRADIUS units.

	xl = (int)(tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS) / MAPBLOCKUNITS;
	xh = (int)(tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS) / MAPBLOCKUNITS;
	yl = (int)(tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) / MAPBLOCKUNITS;
	yh = (int)(tmbbox[BOXTOP] - bmaporgy + MAXRADIUS) / MAPBLOCKUNITS;

	for (bx = xl; bx <= xh; bx++)
		for (by = yl; by <= yh; by++)
			if (!P_BlockThingsIterator(bx, by, PIT_CheckAbsThing))
				return false;

	// check lines

	xl = (int)(tmbbox[BOXLEFT] - bmaporgx) / MAPBLOCKUNITS;
	xh = (int)(tmbbox[BOXRIGHT] - bmaporgx) / MAPBLOCKUNITS;
	yl = (int)(tmbbox[BOXBOTTOM] - bmaporgy) / MAPBLOCKUNITS;
	yh = (int)(tmbbox[BOXTOP] - bmaporgy) / MAPBLOCKUNITS;

	for (bx = xl; bx <= xh; bx++)
		for (by = yl; by <= yh; by++)
			if (!P_BlockLinesIterator(bx, by, PIT_CheckAbsLine))
				return false;

	return true;
}

//
// RELATIVE MOVEMENT CLIPPING
//

//
// PIT_CheckRelLine
//
// Adjusts tm_I.floorz & tm_I.ceilnz as lines are contacted
//
static bool PIT_CheckRelLine(line_t * ld)
{
	int i;

	if (tmbbox[BOXRIGHT] <= ld->bbox[BOXLEFT] ||
		tmbbox[BOXLEFT] >= ld->bbox[BOXRIGHT] ||
		tmbbox[BOXTOP] <= ld->bbox[BOXBOTTOM] ||
		tmbbox[BOXBOTTOM] >= ld->bbox[BOXTOP])
	{
		// no intersection with line
		return true;
	}

	if (P_BoxOnLineSide(tmbbox, ld) != -1)
		return true;

	// A line has been hit

	// The moving thing's destination position will cross the given line.
	// If this should not be allowed, return false.
	// If the line is special, keep track of it
	// to process later if the move is proven ok.
	// NOTE: specials are NOT sorted by order,
	// so two special lines that are only 8 pixels apart
	// could be crossed in either order.

	if (!ld->backsector)
	{
		blockline = ld;

		// one sided line
		return false;
	}

	if (tm_I.extflags & EF_CROSSLINES)
	{
		if ((ld->flags & ML_ShootBlock) && (tm_I.flags & MF_MISSILE))
		{
			blockline = ld;
			return false;
		}
	}
	else
	{
		// explicitly blocking everything ?
		// or just blocking monsters ?

		if ((ld->flags & ML_Blocking) ||
			((ld->flags & ML_BlockMonsters) &&
			(tm_I.extflags & EF_MONSTER)))
		{
			blockline = ld;
			return false;
		}
	}

	// handle ladders (players only !)
	if (tm_I.mover->player && ld->special && 
		ld->special->ladder.height > 0)
	{
		float z1, z2;
		float pz1, pz2;

		z1 = ld->frontsector->f_h + ld->side[0]->middle.offset.y;
		z2 = z1 + ld->special->ladder.height;

		pz1 = tm_I.mover->z;
		pz2 = tm_I.mover->z + tm_I.mover->height;

		do 
		{
			// can't reach the ladder ?
			if (pz1 > z2 || pz2 < z1)
				break;

			// FIXME: if more than one ladder, choose best one

			tm_I.mover->on_ladder = (ld - lines);
		}
		while (0);
	}

	// if contacted a special line, add it to the list
	if (ld->special)
	{
		Z_SetArraySize(&spechit_a, ++numspechit);
		spechit[numspechit - 1] = ld;
	}

	// check for hitting a sky-hack line
	{
		float f1, c1;
		float f2, c2;

		f1 = ld->frontsector->f_h;
		c1 = ld->frontsector->c_h;
		f2 = ld->backsector->f_h;
		c2 = ld->backsector->c_h;

		if (c1 != c2 && IS_SKY(ld->frontsector->ceil) &&
			IS_SKY(ld->backsector->ceil) && tm_I.z > MIN(c1, c2))
		{
			mobj_hit_sky = true;
		}

		if (f1 != f2 && IS_SKY(ld->frontsector->floor) &&
			IS_SKY(ld->backsector->floor) &&
			tm_I.z+tm_I.mover->height < MAX(f1, f2))
		{
			mobj_hit_sky = true;
		}
	}

	// CHOOSE GAP
	//
	// If this line borders a sector with multiple floors, then there will
	// be multiple gaps and we must choose one here, based on the thing's
	// current position (esp. Z).

	i = P_FindThingGap(ld->gaps, ld->gap_num, tm_I.z, tm_I.z +
		tm_I.mover->height);

	// gap has been chosen. apply it.

	if (i >= 0)
	{
		if (ld->gaps[i].f >= tm_I.floorz)
		{
			tm_I.floorz = ld->gaps[i].f;
			tm_I.below = NULL;
		}

		if (ld->gaps[i].c < tm_I.ceilnz)
			tm_I.ceilnz = ld->gaps[i].c;

		if (ld->gaps[i].f < tm_I.dropoff)
			tm_I.dropoff = ld->gaps[i].f;
	}
	else
	{
		tm_I.ceilnz = tm_I.floorz;
	}

	if (tm_I.ceilnz < tm_I.floorz + tm_I.mover->height)
		blockline = ld;

	if (! blockline)
	{
		if (tm_I.line_count == 0)
			tm_I.line_which = ld;

		tm_I.line_count++;
	}

	return true;
}

//
// PIT_CheckRelThing
//
static bool PIT_CheckRelThing(mobj_t * thing)
{
	float blockdist;
	bool solid;

	if (thing == tm_I.mover)
		return true;

	if (!(thing->flags & (MF_SOLID | MF_SPECIAL | MF_SHOOTABLE | MF_TOUCHY)))
		return true;

	blockdist = thing->radius + tm_I.mover->radius;

	// Check that we didn't hit it
	if (fabs(thing->x - tm_I.x) >= blockdist || fabs(thing->y - tm_I.y) >= blockdist)
		return true;  // no we missed this thing

	// -KM- 1998/9/19 True 3d gameplay checks.
	if (level_flags.true3dgameplay && !(thing->flags & MF_SPECIAL))
	{
		float top_z = thing->z + thing->height;

		// see if we went over
		if (tm_I.z >= top_z)
		{
			if (top_z > tm_I.floorz && !(thing->flags & MF_MISSILE))
			{
				tm_I.floorz = top_z;
				tm_I.below = thing;
			}
			return true;
		}

		// see if we went underneath
		if (tm_I.z + tm_I.mover->height <= thing->z)
		{
			if (thing->z < tm_I.ceilnz && !(thing->flags & MF_MISSILE))
			{
				tm_I.ceilnz = thing->z;
			}
			return true;
		}

		// -AJA- 1999/07/21: allow climbing on top of things.

		if (top_z > tm_I.floorz &&
			(thing->extendedflags & EF_CLIMBABLE) &&
			(tm_I.mover->player || (tm_I.extflags & EF_MONSTER)) &&
			((tm_I.flags & MF_DROPOFF) ||
			(tm_I.extflags & EF_EDGEWALKER)) &&
			(tm_I.z + tm_I.mover->info->step_size >= top_z))
		{
			tm_I.floorz = top_z;
			tm_I.below = thing;
			return true;
		}
	}

	// check for skulls slamming into things
	// -ACB- 1998/08/04 Use procedure
	// -KM- 1998/09/01 After I noticed Skulls slamming into boxes of rockets...

	solid = (thing->flags & MF_SOLID);

	if ((tm_I.flags & MF_SKULLFLY) && solid)
	{
		P_ActSlammedIntoObject(tm_I.mover, thing);

		// stop moving
		return false;
	}

	// check for missiles making contact
	// -ACB- 1998/08/04 Procedure for missile contact

	if (tm_I.flags & MF_MISSILE)
	{
		// see if it went over / under
		if (tm_I.z > thing->z + thing->height)
			return true;  // overhead

		if (tm_I.z + tm_I.mover->height < thing->z)
			return true;  // underneath

		// thing isn't shootable, return depending on if the thing is solid.
		if (!(thing->flags & MF_SHOOTABLE))
			return !solid;

		// don't hurt the missile's shooter:
		if (tm_I.mover->source && tm_I.mover->source == thing)
			return true;

		P_ActMissileContact(tm_I.mover, thing);
		return (tm_I.extflags & EF_TUNNEL) ? true : false;
	}

	// check for special pickup
	if ((tm_I.flags & MF_PICKUP) && (thing->flags & MF_SPECIAL))
	{
		// can remove thing
		P_TouchSpecialThing(thing, tm_I.mover);
	}

	// -AJA- 1999/08/21: check for touchy objects.
	if ((thing->flags & MF_TOUCHY) && (tm_I.flags & MF_SOLID) &&
		!(thing->extendedflags & EF_USABLE))
	{
		P_ActTouchyContact(thing, tm_I.mover);
		return !solid;
	}

	// -AJA- 2000/06/09: Follow MBF semantics: allow the non-solid
	// moving things to pass through solid things.
	return !solid || (thing->flags & MF_NOCLIP) || !(tm_I.flags & MF_SOLID);
}

//
// P_CheckRelPosition
//
// Checks whether the thing can be moved to the position (x,y), which is
// assumed to be relative to the thing's current position.
//
// This is purely informative, nothing is modified
// (except things picked up).
//
// Only used by P_TryMove and P_ThingHeightClip.
// 
// in:
//  a mobj_t (can be valid or invalid)
//  a position to be checked
//
// during:
//  special things are touched if MF_PICKUP
//  early out on solid lines?
//
// out:
//  tm_I.sub
//  tm_I.floorz
//  tm_I.ceilnz
//  tm_I.dropoff
//  tm_I.above
//  tm_I.below
//  speciallines[]
//  numspeciallines
//
static bool P_CheckRelPosition(mobj_t * thing, float x, float y)
{
	int xl, xh;
	int yl, yh;
	int bx, by;

	mobj_hit_sky = false;
	blockline = NULL;

	tm_I.mover = thing;
	tm_I.flags = thing->flags;
	tm_I.extflags = thing->extendedflags;

	tm_I.x = x;
	tm_I.y = y;
	tm_I.z = thing->z;

	tmbbox[BOXTOP] = y + tm_I.mover->radius;
	tmbbox[BOXBOTTOM] = y - tm_I.mover->radius;
	tmbbox[BOXRIGHT] = x + tm_I.mover->radius;
	tmbbox[BOXLEFT] = x - tm_I.mover->radius;

	tm_I.sub = R_PointInSubsector(x, y);

	// The base floor / ceiling is from the sector that contains the
	// point.  Any contacted lines the step closer together will adjust them.
	// -AJA- 1999/07/19: Extra floor support.
	P_ComputeThingGap(thing, tm_I.sub->sector, tm_I.z, &tm_I.floorz, &tm_I.ceilnz);

	tm_I.dropoff = tm_I.floorz;
	tm_I.above = NULL;
	tm_I.below = NULL;
	tm_I.line_count = 0;

	// can go anywhere
	if (tm_I.flags & MF_NOCLIP)
		return true;

	validcount++;
	Z_SetArraySize(&spechit_a, numspechit = 0);

	// -KM- 1998/11/25 Corpses aren't supposed to hang in the air...
	if (!(tm_I.flags & (MF_NOCLIP | MF_CORPSE)))
	{
		// Check things first, possibly picking things up.
		// The bounding box is extended by MAXRADIUS
		// because mobj_ts are grouped into mapblocks
		// based on their origin point, and can overlap
		// into adjacent blocks by up to MAXRADIUS units.

		xl = (int)(tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS) / MAPBLOCKUNITS;
		xh = (int)(tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS) / MAPBLOCKUNITS;
		yl = (int)(tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) / MAPBLOCKUNITS;
		yh = (int)(tmbbox[BOXTOP] - bmaporgy + MAXRADIUS) / MAPBLOCKUNITS;

		for (bx = xl; bx <= xh; bx++)
			for (by = yl; by <= yh; by++)
				if (!P_BlockThingsIterator(bx, by, PIT_CheckRelThing))
					return false;
	}

	// check lines

	xl = (int)(tmbbox[BOXLEFT] - bmaporgx) / MAPBLOCKUNITS;
	xh = (int)(tmbbox[BOXRIGHT] - bmaporgx) / MAPBLOCKUNITS;
	yl = (int)(tmbbox[BOXBOTTOM] - bmaporgy) / MAPBLOCKUNITS;
	yh = (int)(tmbbox[BOXTOP] - bmaporgy) / MAPBLOCKUNITS;;

	thing->on_ladder = -1;

	for (bx = xl; bx <= xh; bx++)
		for (by = yl; by <= yh; by++)
			if (!P_BlockLinesIterator(bx, by, PIT_CheckRelLine))
				return false;

	return true;
}

//
// P_TryMove
//
// Attempt to move to a new position,
// crossing special lines unless MF_TELEPORT is set.
//
bool P_TryMove(mobj_t * thing, float x, float y)
{
	float oldx;
	float oldy;
	int side;
	int oldside;
	line_t *ld;
	bool fell_off_thing;

	float z = thing->z;

	floatok = false;

	// solid wall or thing ?
	if (!P_CheckRelPosition(thing, x, y))
		return false;

	fell_off_thing = (thing->below_mo && !tm_I.below);

	if (!(thing->flags & MF_NOCLIP))
	{
		if (thing->height > tm_I.ceilnz - tm_I.floorz)
		{
			// doesn't fit
			if (!blockline && tm_I.line_count==1) blockline=tm_I.line_which;
			return false;
		}

		floatok = true;
		float_destz = tm_I.floorz;

		if (!(thing->flags & MF_TELEPORT) &&
			(thing->z + thing->height > tm_I.ceilnz))
		{
			// mobj must lower itself to fit.
			if (!blockline && tm_I.line_count==1) blockline=tm_I.line_which;
			return false;
		}

		if (!(thing->flags & MF_TELEPORT) &&
			(thing->z + thing->info->step_size) < tm_I.floorz)
		{
			// too big a step up.
			if (!blockline && tm_I.line_count==1) blockline=tm_I.line_which;
			return false;
		}

		if (!fell_off_thing &&
			!(thing->flags & (MF_TELEPORT | MF_DROPOFF | MF_FLOAT)) &&
			(thing->z - thing->info->step_size) > tm_I.floorz)
		{
			// too big a step down.
			return false;
		}

		if (!fell_off_thing &&
			!((thing->flags & (MF_DROPOFF | MF_FLOAT)) ||
			(thing->extendedflags & (EF_EDGEWALKER | EF_WATERWALKER))) &&
			(tm_I.floorz - tm_I.dropoff > thing->info->step_size) &&
			(thing->floorz - thing->dropoffz <= thing->info->step_size))
		{
			// don't stand over a dropoff.
			return false;
		}
	}

	// the move is ok, so link the thing into its new position

	oldx = thing->x;
	oldy = thing->y;
	thing->floorz = tm_I.floorz;
	thing->ceilingz = tm_I.ceilnz;
	thing->dropoffz = tm_I.dropoff;

	// -AJA- 1999/08/02: Improved MF_TELEPORT handling.
	if (thing->flags & (MF_TELEPORT | MF_NOCLIP))
	{
		if (z <= thing->floorz)
			z = thing->floorz;
		else if (z + thing->height > thing->ceilingz)
			z = thing->ceilingz - thing->height;
	}

	P_ChangeThingPosition(thing, x, y, z);

	P_MobjSetAboveMo(thing, tm_I.above);
	P_MobjSetBelowMo(thing, tm_I.below);

	// if any special lines were hit, do the effect
	if (!(thing->flags & (MF_TELEPORT | MF_NOCLIP)))
	{
		line_t **hits;
		int i;

		// -ES- 2000/02/05 spechit could be changed inside the loop
		hits = (line_t**)I_TmpMalloc(numspechit * sizeof(line_t *));
		Z_MoveData(hits, spechit, line_t *, numspechit);
		i = numspechit;
		Z_SetArraySize(&spechit_a, numspechit = 0);

		for (i--; i >= 0; i--)
		{
			// honour the NO_TRIGGER_LINES attack special
			if (! thing->player &&
				! (thing->extendedflags & EF_MONSTER) &&
				thing->currentattack && 
				(thing->currentattack->flags & AF_NoTriggerLines))
			{
				continue;
			}

			// see if the line was crossed
			ld = hits[i];
			side = PointOnLineSide(thing->x, thing->y, ld);
			oldside = PointOnLineSide(oldx, oldy, ld);

			if (side != oldside && ld->special)
			{
				if (thing->flags & MF_MISSILE)
					P_ShootSpecialLine(ld, oldside, thing->source);
				else
					P_CrossSpecialLine(ld, oldside, thing);
			}
		}
		I_TmpFree(hits);
	}

	return true;
}

//
// P_ThingHeightClip
//
// Takes a valid thing and adjusts the thing->floorz, thing->ceilingz,
// and possibly thing->z.
//
// This is called for all nearby things whenever a sector changes height.
//
// If the thing doesn't fit, the z will be set to the lowest value
// and false will be returned.
//
static bool P_ThingHeightClip(mobj_t * thing)
{
	bool onfloor = (fabs(thing->z - thing->floorz) < 1);

	P_CheckRelPosition(thing, thing->x, thing->y);

	thing->floorz = tm_I.floorz;
	thing->ceilingz = tm_I.ceilnz;
	thing->dropoffz = tm_I.dropoff;

	P_MobjSetAboveMo(thing, tm_I.above);
	P_MobjSetBelowMo(thing, tm_I.below);

	if (onfloor)
	{
		// walking monsters rise and fall with the floor
		thing->z = thing->floorz;
	}
	else
	{
		// don't adjust a floating monster unless forced to
		if (thing->z + thing->height > thing->ceilingz)
			thing->z = thing->ceilingz - thing->height;
	}

	if (thing->ceilingz - thing->floorz < thing->height)
		return false;

	return true;
}

//
// SLIDE MOVE
//
// Allows the player to slide along any angled walls.
//
static float bestslidefrac;
static float secondslidefrac;

static line_t *bestslideline;
static line_t *secondslideline;

static float tmxmove;
static float tmymove;

mobj_t *slidemo;

//
// P_HitSlideLine
//
// Adjusts the xmove / ymove
// so that the next move will slide along the wall.
//
static void HitSlideLine(line_t * ld)
{
	int side;

	angle_t lineangle;
	angle_t moveangle;
	angle_t deltaangle;

	float movelen;
	float newlen;

	if (ld->slopetype == ST_HORIZONTAL)
	{
		tmymove = 0;
		return;
	}

	if (ld->slopetype == ST_VERTICAL)
	{
		tmxmove = 0;
		return;
	}

	side = PointOnLineSide(slidemo->x, slidemo->y, ld);

	lineangle = R_PointToAngle(0, 0, ld->dx, ld->dy);

	if (side == 1)
		lineangle += ANG180;

	moveangle = R_PointToAngle(0, 0, tmxmove, tmymove);
	deltaangle = moveangle - lineangle;

	if (deltaangle > ANG180)
		deltaangle += ANG180;
	// I_Error ("SlideLine: ang>ANG180");

	movelen = P_ApproxDistance(tmxmove, tmymove);
	newlen = movelen * M_Cos(deltaangle);

	tmxmove = newlen * M_Cos(lineangle);
	tmymove = newlen * M_Sin(lineangle);
}

//
// PTR_SlideTraverse
//
static bool PTR_SlideTraverse(intercept_t * in)
{
	line_t *li;
	int i;

	DEV_ASSERT(in->type ==INCPT_Line, ("PTR_SlideTraverse: not a line?"));

	li = in->d.line;

	if (!(li->flags & ML_TwoSided))
	{
		// hit the back side ?
		if (PointOnLineSide(slidemo->x, slidemo->y, li))
			return true;
	}

	// -AJA- 1999/07/19: Gaps are now stored in line_t.

	for (i = 0; i < li->gap_num; i++)
	{
		// check if it can fit in the space
		if (slidemo->height > li->gaps[i].c - li->gaps[i].f)
			continue;

		// check slide mobj is not too high
		if (slidemo->z + slidemo->height > li->gaps[i].c)
			continue;

		// check slide mobj can step over
		if (slidemo->z + slidemo->info->step_size < li->gaps[i].f)
			continue;

		return true;
	}

	// the line does block movement,
	// see if it is closer than best so far
	if (in->frac < bestslidefrac)
	{
		secondslidefrac = bestslidefrac;
		secondslideline = bestslideline;
		bestslidefrac = in->frac;
		bestslideline = li;
	}

	// stop
	return false;
}

//
// P_SlideMove
//
// The momx / momy move is bad, so try to slide along a wall.
//
// Find the first line hit, move flush to it, and slide along it
//
// -ACB- 1998/07/28 This is NO LONGER a kludgy mess; removed goto rubbish.
//
void P_SlideMove(mobj_t * mo, float x, float y)
{
	float leadx;
	float leady;
	float trailx;
	float traily;
	float newx;
	float newy;
	float dx = x - mo->x;
	float dy = y - mo->y;
	int hitcount;
	bool retry;

	slidemo = mo;
	hitcount = 0;
	retry = true;

	while (retry)
	{
		if (++hitcount == 3)
		{
			if (!P_TryMove(mo, mo->x, mo->y + dy))
				P_TryMove(mo, mo->x + dx, mo->y);
			return;
		}

		// trace along the three leading corners
		if (dx > 0)
		{
			leadx = mo->x + mo->radius;
			trailx = mo->x - mo->radius;
		}
		else
		{
			leadx = mo->x - mo->radius;
			trailx = mo->x + mo->radius;
		}

		if (dy > 0)
		{
			leady = mo->y + mo->radius;
			traily = mo->y - mo->radius;
		}
		else
		{
			leady = mo->y - mo->radius;
			traily = mo->y + mo->radius;
		}

		bestslidefrac = 1.0001;

		P_PathTraverse(leadx, leady, leadx + dx, leady + dy,
			PT_ADDLINES, PTR_SlideTraverse);
		P_PathTraverse(trailx, leady, trailx + dx, leady + dy,
			PT_ADDLINES, PTR_SlideTraverse);
		P_PathTraverse(leadx, traily, leadx + dx, traily + dy,
			PT_ADDLINES, PTR_SlideTraverse);

		// move up to the wall
		if (bestslidefrac == 1.0001)
		{
			// the move must have hit the middle, so stairstep
			if (!P_TryMove(mo, mo->x, mo->y + dy))
				P_TryMove(mo, mo->x + dx, mo->y);
			return;
		}

		// fudge a bit to make sure it doesn't hit
		bestslidefrac -= 0.01;
		if (bestslidefrac > 0)
		{
			newx = dx * bestslidefrac;
			newy = dy * bestslidefrac;

			if (!P_TryMove(mo, mo->x + newx, mo->y + newy))
			{
				if (!P_TryMove(mo, mo->x, mo->y + dy))
					P_TryMove(mo, mo->x + dx, mo->y);
				return;
			}
		}

		// Now continue along the wall.
		// First calculate remainder.
		bestslidefrac = 1.0 - (bestslidefrac + 0.01);

		if (bestslidefrac > 1.0)
			bestslidefrac = 1.0f;

		if (bestslidefrac <= 0)
			return;

		tmxmove = dx * bestslidefrac;
		tmymove = dy * bestslidefrac;

		HitSlideLine(bestslideline);  // clip the moves

		dx = tmxmove;
		dy = tmymove;

		if (P_TryMove(mo, mo->x + tmxmove, mo->y + tmymove))
			retry = false;
	}
}

//
// PTR_AimTraverse
//
// Sets linetarget and slope when a target is aimed at.
//
static bool PTR_AimTraverse(intercept_t * in)
{
	line_t *li;
	mobj_t *th;
	float slope;
	float thingtopslope;
	float thingbottomslope;
	float dist;

	dist = aim_I.range * in->frac;

	if (dist < 0.01)
		return true;

	if (in->type == INCPT_Line)
	{
		li = in->d.line;

		if (!(li->flags & ML_TwoSided) || li->gap_num == 0)
			return false;  // stop

		// Crosses a two sided line.
		// A two sided line will restrict
		// the possible target ranges.
		//
		// -AJA- 1999/07/19: Gaps are now kept in line_t.

		if (li->frontsector->f_h != li->backsector->f_h)
		{
			float maxfloor = MAX(li->frontsector->f_h, li->backsector->f_h);

			slope = (maxfloor - aim_I.start_z) / dist;

			if (slope > aim_I.bottomslope)
				aim_I.bottomslope = slope;
		}

		if (li->frontsector->c_h != li->backsector->c_h)
		{
			float minceil = MIN(li->frontsector->c_h, li->backsector->c_h);

			slope = (minceil - aim_I.start_z) / dist;

			if (slope < aim_I.topslope)
				aim_I.topslope = slope;
		}

		if (aim_I.topslope <= aim_I.bottomslope)
			return false;  // stop

		// shot continues
		return true;
	}

	DEV_ASSERT2(in->type == INCPT_Thing);

	// shoot a thing
	th = in->d.thing;

	if (th == aim_I.source)
		return true;  // can't shoot self

	if (!(th->flags & MF_SHOOTABLE))
		return true;  // has to be able to be shot

	// check angles to see if the thing can be aimed at
	thingtopslope = (th->z + th->height - aim_I.start_z) / dist;

	if (thingtopslope < aim_I.bottomslope)
		return true;  // shot over the thing

	thingbottomslope = (th->z - aim_I.start_z) / dist;

	if (thingbottomslope > aim_I.topslope)
		return true;  // shot under the thing

	// this thing can be hit!
	if (thingtopslope > aim_I.topslope)
		thingtopslope = aim_I.topslope;

	if (thingbottomslope < aim_I.bottomslope)
		thingbottomslope = aim_I.bottomslope;

	aim_I.slope = (thingtopslope + thingbottomslope) / 2;
	linetarget = th;

	return false;  // don't go any farther
}

//
// ShootCheckGap
//
// Returns true if successfully passed gap.
//
static INLINE bool ShootCheckGap(float z,
									  float f_h, surface_t *floor, float c_h, surface_t *ceil)
{
	float x, y;
	float frac;

	// perfectly horizontal shots cannot hit planes
	if (shoot_I.slope == 0)
		return true;

	// check if hit the floor
	if (shoot_I.prev_z > f_h && z < f_h)
	{
		/* nothing */
	}
	// check if hit the ceiling
	else if (shoot_I.prev_z < c_h && z > c_h)
	{
		f_h = c_h;
		floor = ceil;
	}
	else
		return true;

	// don't shoot the sky!
	if (IS_SKY(floor[0]))
		return false;

	frac = (f_h - shoot_I.start_z) / (shoot_I.slope * shoot_I.range);

	x = trace.x + trace.dx * frac;
	y = trace.y + trace.dy * frac;
	z = (z < shoot_I.prev_z) ? f_h + 2 : f_h - 2;

	// Spawn bullet puff
	if (shoot_I.puff)
		P_SpawnPuff(x, y, z, shoot_I.puff);

	// don't go any farther
	return false;
}

//
// PTR_ShootTraverse
//
// -ACB- 1998/07/28 Cleaned up.
//
static bool PTR_ShootTraverse(intercept_t * in)
{
	float x, y, z;
	float frac;
	float dist;
	float thingtopslope;
	float thingbottomslope;
	int i, sidenum;

	line_t *li;
	mobj_t *th;
	side_t *side;
	bool use_puff;

	dist = shoot_I.range * in->frac;

	if (dist < 0.1)
		dist = 0.1;

	// Intercept is a line?
	if (in->type == INCPT_Line)
	{
		li = in->d.line;

		// determine coordinates of intersect
		frac = in->frac;
		x = trace.x + trace.dx * frac;
		y = trace.y + trace.dy * frac;
		z = shoot_I.start_z + frac * shoot_I.slope * shoot_I.range;

		sidenum = PointOnLineSide(trace.x, trace.y, li);
		side = li->side[sidenum];

		// Line is a special, Cause action....
		// -AJA- honour the NO_TRIGGER_LINES attack special too
		if (li->special &&
			(! shoot_I.source || ! shoot_I.source->currentattack ||
			! (shoot_I.source->currentattack->flags & AF_NoTriggerLines)))
		{
			P_ShootSpecialLine(li, sidenum, shoot_I.source);
		}

		// check if shot has hit a floor or ceiling...
		if (side)
		{
			extrafloor_t *ef;
			surface_t *floor_s = &side->sector->floor;
			float floor_h = side->sector->f_h;

			// FIXME: must go in correct order
			for (ef=side->sector->bottom_ef; ef; ef=ef->higher)
			{
				if (! ShootCheckGap(z, floor_h, floor_s, ef->bottom_h, ef->bottom))
					return false;

				floor_s = ef->top;
				floor_h = ef->top_h;
			}

			if (! ShootCheckGap(z, floor_h, floor_s, 
				side->sector->c_h, &side->sector->ceil))
			{
				return false;
			}
		}

		// shot doesn't go through a one-sided line, since one sided lines
		// do not have a sector on the other side.

		if ((li->flags & ML_TwoSided) && li->gap_num > 0 &&
			!(li->flags & ML_ShootBlock))
		{
			DEV_ASSERT2(li->backsector);

			// check all line gaps
			for (i=0; i < li->gap_num; i++)
			{
				if (li->gaps[i].f <= z && z <= li->gaps[i].c)
				{
					shoot_I.prev_z = z;
					return true;
				}
			}
		}

		// check if bullet hit a sky hack line...
		if (li->frontsector && li->backsector)
		{
			if (IS_SKY(li->frontsector->ceil) && IS_SKY(li->backsector->ceil))
			{
				float c1 = li->frontsector->c_h;
				float c2 = li->backsector->c_h;

				if (MIN(c1,c2) <= z && z <= MAX(c1,c2))
					return false;
			}

			if (IS_SKY(li->frontsector->floor) && IS_SKY(li->backsector->floor))
			{
				float f1 = li->frontsector->f_h;
				float f2 = li->backsector->f_h;

				if (MIN(f1,f2) <= z && z <= MAX(f1,f2))
					return false;
			}
		}

		// position puff off the wall

		x -= trace.dx * 6.0 / shoot_I.range;
		y -= trace.dy * 6.0 / shoot_I.range;

		// Spawn bullet puffs.
		if (shoot_I.puff)
			P_SpawnPuff(x, y, z, shoot_I.puff);

		// don't go any farther
		return false;
	}

	DEV_ASSERT2(in->type == INCPT_Thing);

	// shoot a thing
	th = in->d.thing;

	// don't shoot self
	if (th == shoot_I.source)
		return true;

	// got to able to shoot it
	if (!(th->flags & MF_SHOOTABLE) && !(th->extendedflags & EF_BLOCKSHOTS))
		return true;

	// check angles to see if the thing can be aimed at
	thingtopslope = (th->z + th->height - shoot_I.start_z) / dist;

	// shot over the thing ?
	if (thingtopslope < shoot_I.slope)
		return true;

	thingbottomslope = (th->z - shoot_I.start_z) / dist;

	// shot under the thing ?
	if (thingbottomslope > shoot_I.slope)
		return true;

	// hit thing
	// position a bit closer
	frac = in->frac - 10.0 / shoot_I.range;

	x = trace.x + trace.dx * frac;
	y = trace.y + trace.dy * frac;
	z = shoot_I.start_z + frac * shoot_I.slope * shoot_I.range;

	// Spawn bullet puffs or blood spots,
	// depending on target type.

	use_puff = !(th->flags & MF_SHOOTABLE) || (th->flags & MF_NOBLOOD);

	if (th->flags & MF_SHOOTABLE)
	{
		if (! P_ActBulletContact(shoot_I.source, th, shoot_I.damage, shoot_I.damtype))
			use_puff = true;
	}

	if (use_puff && shoot_I.puff)
		P_SpawnPuff(x, y, z, shoot_I.puff);
	else
		P_SpawnBlood(x, y, z, shoot_I.damage, shoot_I.angle, th->info->blood);

	// don't go any farther
	return false;
}

//
// P_AimLineAttack
//
float P_AimLineAttack(mobj_t * t1, angle_t angle, float distance)
{
	float x2 = t1->x + distance * M_Cos(angle);
	float y2 = t1->y + distance * M_Sin(angle);

	if (t1->info)
		aim_I.start_z = t1->z + t1->height * PERCENT_2_FLOAT(t1->info->shotheight);
	else
		aim_I.start_z = t1->z + t1->height / 2 + 8;

	if (t1->player)
	{
		aim_I.topslope = (t1->vertangle * 256 + 100.0) / 160.0f;
		aim_I.bottomslope = (t1->vertangle * 256 - 100.0) / 160.0f;
	}
	else
	{
		aim_I.topslope = 100.0 / 160.0f;
		aim_I.bottomslope = -100.0 / 160.0f;
	}

	linetarget = NULL;

	aim_I.source = t1;
	aim_I.range = distance;
	aim_I.angle = angle;
	aim_I.slope = 0;

	P_PathTraverse(t1->x, t1->y, x2, y2, PT_ADDLINES | PT_ADDTHINGS, PTR_AimTraverse);

	if (linetarget)
		return aim_I.slope;

	return 0;
}

//
// P_LineAttack
//
// Note: Damtype can be NULL.
//
void P_LineAttack(mobj_t * t1, angle_t angle, float distance, 
				  float slope, float damage, const damage_t * damtype,
				  const mobjinfo_t *puff)
{
	float x2 = t1->x + distance * M_Cos(angle);
	float y2 = t1->y + distance * M_Sin(angle);

	if (t1->info)
		shoot_I.start_z = t1->z + t1->height * PERCENT_2_FLOAT(t1->info->shotheight);
	else
		shoot_I.start_z = t1->z + t1->height / 2 + 8;

	shoot_I.source = t1;
	shoot_I.range = distance;
	shoot_I.angle = angle;
	shoot_I.slope = slope;
	shoot_I.damage  = damage;
	shoot_I.damtype = damtype;
	shoot_I.prev_z = shoot_I.start_z;
	shoot_I.puff = puff;

	P_PathTraverse(t1->x, t1->y, x2, y2, PT_ADDLINES | PT_ADDTHINGS, 
		PTR_ShootTraverse);
}

//
// P_MapTargetTheory
//
// Returns a dummy moving object for a target, used for mobjs
// that don't have a specific target; but need to launch some sort
// of projectile in the direction based upon the angle & vertical
// angle of the mobj.
//
// -ACB- 1998/09/01
//
mobj_t *P_MapTargetTheory(mobj_t * source)
{
	angle_t angle;
	float distance;
	float start_z;

	static mobj_t theorytarget;

	angle = source->angle;
	distance = MISSILERANGE;

	if (source->info)
		start_z = source->z + source->height * PERCENT_2_FLOAT(source->info->shotheight);
	else
		start_z = source->z + source->height / 2 + 8;

	Z_Clear(&theorytarget, mobj_t, 1);

	theorytarget.x = source->x + distance * M_Cos(angle);
	theorytarget.y = source->y + distance * M_Sin(angle);
	theorytarget.z = start_z   + distance * source->vertangle;

	theorytarget.extendedflags |= EF_DUMMYMOBJ;
	theorytarget.radius = theorytarget.height = 1;

	return &theorytarget;
}

//
// P_MapTargetAutoAim
//
// Returns a moving object for a target; will search for a mobj to
// lock onto, however a dummy target returned if no object cannot be
// locked onto.
//
// -ACB- 1998/09/01
// -AJA- 1999/08/08: Added `force_aim' to fix chainsaw.
//
mobj_t *P_MapTargetAutoAim(mobj_t * source, angle_t angle, float distance, bool force_aim)
{
	float x2, y2;

	// -KM- 1999/01/31 Autoaim is an option.
	if (source->player && !level_flags.autoaim && !force_aim)
	{
		return P_MapTargetTheory(source);
	}

	aim_I.source = source;

	x2 = source->x + distance * M_Cos(angle);
	y2 = source->y + distance * M_Sin(angle);

	if (source->info)
		aim_I.start_z = source->z + source->height * PERCENT_2_FLOAT(source->info->shotheight);
	else
		aim_I.start_z = source->z + source->height / 2 + 8;

	if (source->player)
	{
		aim_I.topslope = (100 + source->vertangle * 256) / 160.0f;
		aim_I.bottomslope = (-100 + source->vertangle * 256) / 160.0f;
	}
	else
	{
		aim_I.topslope = 100.0 / 160.0f;
		aim_I.bottomslope = -100.0 / 160.0f;
	}

	aim_I.range = distance;
	linetarget = NULL;

	P_PathTraverse(source->x, source->y, x2, y2, PT_ADDLINES | PT_ADDTHINGS, 
		PTR_AimTraverse);

	if (! linetarget)
	{
		return P_MapTargetTheory(source);
	}

	// -KM- 1999/01/31 Look at the thing you aimed at.  Is sometimes
	//   useful, sometimes annoying :-)
	if (source->player && level_flags.autoaim == AA_MLOOK)
	{
		source->vertangle = (linetarget->z - source->z) /
			P_ApproxDistance(source->x - linetarget->x, source->y - linetarget->y);
		if (source->vertangle > LOOKUPLIMIT)
			source->vertangle = LOOKUPLIMIT;
		if (source->vertangle < LOOKDOWNLIMIT)
			source->vertangle = LOOKDOWNLIMIT;
	}
	return linetarget;
}

//
// USE LINES
//
static mobj_t *usething;
static float use_lower, use_upper;

static bool PTR_UseTraverse(intercept_t * in)
{
	int sidenum;
	line_t *line;
	side_t *side;
	mobj_t *th;

	// intercept is a thing ?
	if (in->type == INCPT_Thing)
	{
		th = in->d.thing;

		// not a usable thing ?
		if (!(th->extendedflags & EF_USABLE) || ! th->info->touch_state)
			return true;

		if (!P_ActUseThing(usething, th, use_lower, use_upper))
			return true;

		// don't go any farther (thing was usable)
		return false;
	}

	DEV_ASSERT2(in->type == INCPT_Line);

	line = in->d.line;

	sidenum = PointOnLineSide(usething->x, usething->y, line);
	sidenum = (sidenum == 1) ? 1 : 0;

	side = line->side[sidenum];

	// update open vertical range (extrafloors are NOT checked)
	if (side)
	{
		use_lower = MAX(use_lower, side->sector->f_h);
		use_upper = MIN(use_upper, side->sector->c_h);
	}

	if (!line->special)
	{
		if (line->gap_num == 0 || use_upper <= use_lower)
		{
			// can't use through a wall
			S_StartSound(usething, usething->info->noway_sound);
			return false;
		}

		// not a special line, but keep checking
		return true;
	}

	P_UseSpecialLine(usething, line, sidenum, use_lower, use_upper);

	// can't use more than one special line in a row
	// -AJA- 1999/09/25: ...unless the line has the ML_PASSTHRU flag
	//       (Boom compatibility).

	return (line->flags & ML_PassThru) ? true : false;
}

//
// P_UseLines
//
// Looks for special lines in front of the player to activate.
//
void P_UseLines(player_t * player)
{
	int angle;
	float x1;
	float y1;
	float x2;
	float y2;

	usething = player->mo;
	use_lower = INT_MIN;
	use_upper = INT_MAX;

	angle = player->mo->angle;

	x1 = player->mo->x;
	y1 = player->mo->y;
	x2 = x1 + USERANGE * M_Cos(angle);
	y2 = y1 + USERANGE * M_Sin(angle);

	P_PathTraverse(x1, y1, x2, y2, PT_ADDLINES | PT_ADDTHINGS, PTR_UseTraverse);
}

//
// RADIUS ATTACK
//

typedef struct rds_atk_info_s
{
	float range;
	mobj_t *spot;
	mobj_t *source;
	float damage;
	const damage_t *damtype;
	bool thrust;
}
rds_atk_info_t;

static rds_atk_info_t bomb_I;

//
// PIT_RadiusAttack
//
// "bombsource" is the creature that caused the explosion at "bombspot".
//
static bool PIT_RadiusAttack(mobj_t * thing)
{
	float dx, dy;
	float dist;

	// ignore the bomb spot itself
	if (thing == bomb_I.spot)
		return true;

	if (! (thing->flags & MF_SHOOTABLE))
		return true;

	// Boss types take no damage from concussion.
	// -ACB- 1998/06/14 Changed enum reference to extended flag check.
	if (thing->info->extendedflags & EF_BOSSMAN)
		return true;

	dx = fabs(thing->x - bomb_I.spot->x);
	dy = fabs(thing->y - bomb_I.spot->y);

	// dist is the distance to the *edge* of the thing
	dist = MAX(dx, dy) - thing->radius;

	if (dist < 0)
		dist = 0;

	if (dist >= bomb_I.range)
		return true;  // out of range

	// recompute dist to be in range 0.0 (far away) to 1.0 (close)
	CHECKVAL(bomb_I.range);
	dist = (dist - bomb_I.range) / bomb_I.range;

	if (P_CheckSight(bomb_I.spot, thing))
	{
		if (bomb_I.thrust)
			P_ThrustMobj(thing, bomb_I.spot, bomb_I.damage * dist);
		else
			P_DamageMobj(thing, bomb_I.spot, bomb_I.source, 
			bomb_I.damage * dist, bomb_I.damtype);
	}
	return true;
}

//
// PIT_SphereAttack
//
// "bombsource" is the creature that caused the explosion at "bombspot".
//
// -ACB- 1998/07/15 New procedure that differs for RadiusAttack -
//                  it checks Height, therefore it is a sphere attack.
//
// -KM-  1998/11/25 Fixed.  Added z movement for rocket jumping.
//
static bool PIT_SphereAttack(mobj_t * thing)
{
	float dx, dy, dz;
	float dist;

	// ignore the bomb spot itself
	if (thing == bomb_I.spot)
		return true;

	if (!(thing->flags & MF_SHOOTABLE))
		return true;

	//
	// Boss types take no damage from concussion.
	// -ACB- 1998/06/14 Changed enum reference to extended flag check.
	//
	if (thing->info->extendedflags & EF_BOSSMAN)
		return true;

	// -KM- 1999/01/31 Use thing->height/2
	dx = fabs(thing->x - bomb_I.spot->x);
	dy = fabs(thing->y - bomb_I.spot->y);
	dz = fabs(MO_MIDZ(thing) - MO_MIDZ(bomb_I.spot));

	// dist is the distance to the *edge* of the thing
	dist = MAX(MAX(dx, dy) - thing->radius, dz - thing->height/2);

	if (dist < 0)
		dist = 0;

	if (dist >= bomb_I.range)
		return true;  // out of range

	// recompute dist to be in range 0.0 (far away) to 1.0 (close)
	CHECKVAL(bomb_I.range);
	dist = (bomb_I.range - dist) / bomb_I.range;

	if (P_CheckSight(bomb_I.spot, thing))
	{
		if (bomb_I.thrust)
			P_ThrustMobj(thing, bomb_I.spot, bomb_I.damage * dist);
		else
			P_DamageMobj(thing, bomb_I.spot, bomb_I.source, 
			bomb_I.damage * dist, bomb_I.damtype);
	}
	return true;
}

//
// P_RadiusAttack
//
// Source is the creature that caused the explosion at spot.
//
// Note: Damtype can be NULL.
//
void P_RadiusAttack(mobj_t * spot, mobj_t * source, float radius,
					float damage, const damage_t * damtype, bool thrust_only)
{
	int x, y;
	int xl, xh, yl, yh;

	yh = (int)(spot->y + radius - bmaporgy) / MAPBLOCKUNITS;
	yl = (int)(spot->y - radius - bmaporgy) / MAPBLOCKUNITS;
	xh = (int)(spot->x + radius - bmaporgx) / MAPBLOCKUNITS;
	xl = (int)(spot->x - radius - bmaporgx) / MAPBLOCKUNITS;

	bomb_I.range = radius;
	bomb_I.spot  = spot;
	bomb_I.source = source;
	bomb_I.damage = damage;
	bomb_I.damtype = damtype;
	bomb_I.thrust  = thrust_only;

	//
	// -ACB- 1998/07/15 This normally does damage to everything within
	//                  a radius regards of height, however true 3D uses
	//                  a sphere attack, which checks height.
	//
	if (level_flags.true3dgameplay)
	{
		for (y = yl; y <= yh; y++)
			for (x = xl; x <= xh; x++)
				P_BlockThingsIterator(x, y, PIT_SphereAttack);
	}
	else
	{
		for (y = yl; y <= yh; y++)
			for (x = xl; x <= xh; x++)
				P_BlockThingsIterator(x, y, PIT_RadiusAttack);
	}
}


//
//  SECTOR HEIGHT CHANGING
//

static bool crushchange;
static bool nofit;
static int crush_time;
static int crush_damage;


//
// PIT_ChangeSector
//
static bool PIT_ChangeSector(mobj_t * thing)
{
	mobj_t *mo;

	if (P_ThingHeightClip(thing))
	{
		// keep checking
		return true;
	}

	// dropped items get removed by a falling ceiling
	if (thing->flags & MF_DROPPED)
	{
		P_RemoveMobj(thing);
		return true;
	}

	// crunch bodies to giblets
	if (thing->health <= 0)
	{
		if (thing->info->gib_state && !(thing->extendedflags & EF_GIBBED))
		{
			thing->extendedflags |= EF_GIBBED;
			P_SetMobjStateDeferred(thing, thing->info->gib_state, 0);
		}

		// just been crushed, isn't solid.
		thing->flags &= ~MF_SOLID;

		thing->height = 0;
		thing->radius = 0;

		return true;
	}

	// if thing is not shootable, can't be crushed
	if (!(thing->flags & MF_SHOOTABLE) || (thing->flags & MF_NOCLIP))
		return true;

	nofit = true;

	if (crushchange && !(leveltime % crush_time))
	{
		P_DamageMobj(thing, NULL, NULL, crush_damage, NULL);

		// spray blood in a random direction
		mo = P_MobjCreateObject(thing->x, thing->y, MO_MIDZ(thing),
			thing->info->blood);

		mo->mom.x = (M_Random() - 128) / 4.0f;
		mo->mom.y = (M_Random() - 128) / 4.0f;
	}

	// keep checking (crush other things) 
	return true;
}


//
// ChangeSectorHeights
// 
// Checks all things in the given sector which is changing height.
// The original space is in f_h..c_h, and the f_dh, c_dh parameters
// give the amount the space is closing.
//
// Things will be moved vertically if they need to.  When
// "crushchange" is true, things that no longer fit will be crushed
// (and will also set the "nofit" variable).
// 
// NOTE: the heights (f_h, c_h) currently broken.
// 
static void ChangeSectorHeights(sector_t *sec, float f_h,
								float c_h, float f_dh, float c_dh)
{
	touch_node_t *tn, *next;
	mobj_t *mo;

	crush_time   = 4;
	crush_damage = 10;

	for (tn=sec->touch_things; tn; tn=next)
	{
		// allow for thing removal
		next = tn->sec_next;

		mo = tn->mo;
		DEV_ASSERT2(mo);

#if 0
		bz = mo->z;
		tz = mo->z + mo->height;

		// ignore things that are not in the space (e.g. in another
		// extrafloor).
		//
		if (tz < f_h-1 || bz > c_h+1)
			continue;
#endif

		PIT_ChangeSector(mo);
	}
}


//
// P_CheckSolidSectorMove
//
// Checks if the sector (and any attached extrafloors) can be moved.
// Only checks againgst hitting other solid floors, things are NOT
// considered here.  Returns true if OK, otherwise false.
//
bool P_CheckSolidSectorMove(sector_t *sec, bool is_ceiling,
								 float dh)
{
	extrafloor_t *ef;

	if (dh == 0)
		return true;

	//
	// first check real sector
	//

	if (is_ceiling && dh < 0 && sec->top_ef &&
		(sec->c_h - dh < sec->top_ef->top_h))
	{
		return false;
	}

	if (!is_ceiling && dh > 0 && sec->bottom_ef &&
		(sec->f_h + dh > sec->bottom_ef->bottom_h))
	{
		return false;
	}

	// don't allow a dummy sector to go FUBAR
	if (sec->control_floors)
	{
		if (is_ceiling && (sec->c_h + dh < sec->f_h))
			return false;

		if (!is_ceiling && (sec->f_h + dh > sec->c_h))
			return false;
	}

	//
	// second, check attached extrafloors
	//

	for (ef = sec->control_floors; ef; ef = ef->ctrl_next)
	{
		// liquids can go anywhere, anytime
		if (ef->ef_info->type & EXFL_Liquid)
			continue;

		// moving a thin extrafloor ?
		if (!is_ceiling && ! (ef->ef_info->type & EXFL_Thick))
		{
			float new_h = ef->top_h + dh;

			if (dh > 0 && new_h > (ef->higher ? ef->higher->bottom_h :
			ef->sector->c_h))
			{
				return false;
			}

			if (dh < 0 && new_h < (ef->lower ? ef->lower->top_h :
			ef->sector->f_h))
			{
				return false;
			}
			continue;
		}

		// moving the top of a thick extrafloor ?
		if (is_ceiling && (ef->ef_info->type & EXFL_Thick))
		{
			float new_h = ef->top_h + dh;

			if (dh < 0 && new_h < ef->bottom_h)
				return false;

			if (dh > 0 && new_h > (ef->higher ? ef->higher->bottom_h :
			ef->sector->c_h))
			{
				return false;
			}
			continue;
		}

		// moving the bottom of a thick extrafloor ?
		if (!is_ceiling && (ef->ef_info->type & EXFL_Thick))
		{
			float new_h = ef->bottom_h + dh;

			if (dh > 0 && new_h > ef->top_h)
				return false;

			if (dh < 0 && new_h < (ef->lower ? ef->lower->top_h :
			ef->sector->f_h))
			{
				return false;
			}
			continue;
		}
	}

	return true;
}

//
// P_SolidSectorMove
//
// Moves the sector and any attached extrafloors.  You MUST call
// P_CheckSolidSectorMove() first to check if move is possible.
// 
// Things are checked here, and will be moved if they overlap the
// move.  If they no longer fit and the "crush" parameter is true,
// they will take damage.  Returns true if at least one thing no
// longers fits, otherwise false.
//
bool P_SolidSectorMove(sector_t *sec, bool is_ceiling,
							float dh, bool crush, bool nocarething)
{
	extrafloor_t *ef;

	if (dh == 0)
		return false;

	crushchange = crush;
	nofit = false;

	//
	// first update real sector
	//

	if (is_ceiling)
		sec->c_h += dh;
	else
		sec->f_h += dh;

	P_RecomputeGapsAroundSector(sec);
	P_RecomputeTilesInSector(sec);
	P_FloodExtraFloors(sec);

	if (! nocarething)
	{
		if (is_ceiling)
		{
			float h = sec->top_ef ? sec->top_ef->top_h : sec->f_h;
			ChangeSectorHeights(sec, h, sec->c_h, 0, -dh);
		}
		else 
		{
			float h = sec->bottom_ef ? sec->bottom_ef->bottom_h : sec->c_h;
			ChangeSectorHeights(sec, sec->f_h, h, dh, 0);
		}
	}

	//
	// second, update attached extrafloors
	//

	for (ef = sec->control_floors; ef; ef = ef->ctrl_next)
	{
		if (ef->ef_info->type & EXFL_Thick)
		{
			ef->top_h = sec->c_h;
			ef->bottom_h = sec->f_h;
		}
		else
		{
			ef->top_h = ef->bottom_h = sec->f_h;
		}

		P_RecomputeGapsAroundSector(ef->sector);
		P_RecomputeTilesInSector(ef->sector);
		P_FloodExtraFloors(ef->sector);
	}

	if (! nocarething)
	{
		for (ef = sec->control_floors; ef; ef = ef->ctrl_next)
		{
			// liquids can go anywhere, anytime
			if (ef->ef_info->type & EXFL_Liquid)
				continue;

			// moving a thin extrafloor ?
			if (!is_ceiling && ! (ef->ef_info->type & EXFL_Thick))
			{
				if (dh > 0)
				{
					float h = ef->higher ? ef->higher->bottom_h : ef->sector->c_h;
					ChangeSectorHeights(ef->sector, ef->top_h, h, dh, 0);
				}
				else if (dh < 0)
				{
					float h = ef->lower ? ef->lower->top_h : ef->sector->f_h;
					ChangeSectorHeights(ef->sector, h, ef->top_h, 0, -dh);
				}
				continue;
			}

			// moving the top of a thick extrafloor ?
			if (is_ceiling && (ef->ef_info->type & EXFL_Thick))
			{
				float h = ef->higher ? ef->higher->bottom_h : ef->sector->c_h;
				ChangeSectorHeights(ef->sector, ef->top_h, h, dh, 0);
				continue;
			}

			// moving the bottom of a thick extrafloor ?
			if (!is_ceiling && (ef->ef_info->type & EXFL_Thick))
			{
				float h = ef->lower ? ef->lower->top_h : ef->sector->f_h;
				ChangeSectorHeights(ef->sector, h, ef->bottom_h, 0, -dh);
				continue;
			}
		}
	}

	return nofit;
}

//
// P_ChangeThingSize
//
// Readjust things in the sector when something changes size.  The
// object must be currently linked into the map.
//
void P_ChangeThingSize(mobj_t *mo)
{
	touch_node_t *tn, *next;
	sector_t *sec = mo->subsector->sector;

	for (tn=sec->touch_things; tn; tn=next)
	{
		next = tn->sec_next;

		DEV_ASSERT2(tn->mo);

		P_ThingHeightClip(tn->mo);
	}
}


//
// PIT_CorpseCheck
//
// Detect a corpse that could be raised.
//
// Based upon PIT_VileCheck: checks for any corpse within thing's radius.
//
// -ACB- 1998/08/22
//
static mobj_t *corpsehit;
static mobj_t *raiserobj;
static float raisertryx;
static float raisertryy;

static bool PIT_CorpseCheck(mobj_t * thing)
{
	float maxdist;
	float oldradius;
	float oldheight;
	bool check;

	if (!(thing->flags & MF_CORPSE))
		return true;  // not a corpse

	if (thing->tics != -1)
		return true;  // not lying still yet

	if (thing->info->raise_state == S_NULL)
		return true;  // monster doesn't have a raise state

	// -KM- 1998/12/21 Monster can't be resurrected.
	if (thing->info->extendedflags & EF_NORESURRECT)
		return true;

	// -ACB- 1998/08/06 Use raiserobj for radius info
	maxdist = thing->info->radius + raiserobj->info->radius;

	if (fabs(thing->x - raisertryx)>maxdist || fabs(thing->y - raisertryy) > maxdist)
		return true;  // not actually touching

	// -AJA- don't raise corpses blocked by extrafloors
	if (! P_CheckSightApproxVert(raiserobj, thing))
		return true;

	// -AJA- don't raise players unless on their side
	if (thing->player && (raiserobj->info->side & thing->info->side) == 0)
		return true;

	corpsehit = thing;
	corpsehit->mom.x = corpsehit->mom.y = 0;

	oldradius = corpsehit->radius;
	oldheight = corpsehit->height;

	// -ACB- 1998/08/22 Check making sure with have the correct radius & height.
	corpsehit->radius = corpsehit->info->radius;
	corpsehit->height = corpsehit->info->height;

	check = P_CheckAbsPosition(corpsehit, corpsehit->x, corpsehit->y, corpsehit->z);

	// -ACB- 1998/08/22 Restore radius & height: we are only checking.
	corpsehit->radius = oldradius;
	corpsehit->height = oldheight;

	if (!check)
		return true;  // doesn't fit here

	// got one, so stop checking
	return false;
}

//
// P_MapFindCorpse
//
// Used to detect corpses that have a raise state and therefore can be
// raised. Arch-Viles (Raisers in-general) use this procedure to pick
// their corpse. NULL is returned if no corpse is found, if one is
// found it is returned.
//
// -ACB- 1998/08/22
//
mobj_t *P_MapFindCorpse(mobj_t * thing)
{
	int xlow, xhigh, xcount, ylow, yhigh, ycount;

	if (thing->movedir != DI_NODIR)
	{
		raiserobj = thing;

		// check for corpses to raise
		raisertryx = thing->x + thing->speed * xspeed[thing->movedir];
		raisertryy = thing->y + thing->speed * yspeed[thing->movedir];

		xlow  = (int)(raisertryx - bmaporgx - MAXRADIUS * 2) / MAPBLOCKUNITS;
		xhigh = (int)(raisertryx - bmaporgx + MAXRADIUS * 2) / MAPBLOCKUNITS;
		ylow  = (int)(raisertryy - bmaporgy - MAXRADIUS * 2) / MAPBLOCKUNITS;
		yhigh = (int)(raisertryy - bmaporgy + MAXRADIUS * 2) / MAPBLOCKUNITS;

		for (xcount = xlow; xcount <= xhigh; xcount++)
			for (ycount = ylow; ycount <= yhigh; ycount++)
				if (!P_BlockThingsIterator(xcount, ycount, PIT_CorpseCheck))
					return corpsehit;  // got one - return it
	}

	return NULL;
}

//
// PIT_CheckBlockingLine
//
// Used for checking that any movement between one set of coordinates does not cross
// blocking lines. If the line is twosided and has no restrictions, the move is
// allowed; the next check is to check the respective bounding boxes, see if any
// contact is made and the check is made to see if the objects are on different
// sides of the line.
//
// -ACB- 1998/08/23
//
// -AJA- 1999/09/30: Updated for extra floors.
//
static bool crosser;

// Moving Object x,y cordinates
// for object one and object two.

static float mx1;
static float my1;
static float mx2;
static float my2;

// spawn object base
static float mb2;

// spawn object top
static float mt2;

static bool PIT_CheckBlockingLine(line_t * line)
{
	int i;

	// -KM- 1999/01/31 Changed &&s to ||s.  This condition actually does something
	//  now.
	if (tmbbox[BOXRIGHT] <= line->bbox[BOXLEFT] ||
		tmbbox[BOXLEFT] >= line->bbox[BOXRIGHT] ||
		tmbbox[BOXTOP] <= line->bbox[BOXBOTTOM] ||
		tmbbox[BOXBOTTOM] >= line->bbox[BOXTOP])
	{
		// no possible contact made between the respective bounding boxes.
		return true;
	}

	// if the result is the same, we haven't crossed the line.
	if (PointOnLineSide(mx1, my1, line) == PointOnLineSide(mx2, my2, line))
		return true;

	// -KM- 1999/01/31 Save ceilingline for bounce.
	if ((crosser && (line->flags & ML_ShootBlock)) || 
		(!crosser && (line->flags & (ML_Blocking | ML_BlockMonsters))))
	{
		blockline = line;
		return false;
	}

	if (!(line->flags & ML_TwoSided) || line->gap_num == 0)
	{
		blockline = line;
		return false;
	}

	for (i = 0; i < line->gap_num; i++)
	{
		// gap with no restriction ?
		if (line->gaps[i].f <= mb2 && mt2 <= line->gaps[i].c)
			return true;
	}

	// stop checking, objects are on different sides of a blocking line
	blockline = line;
	return false;
}

//
// P_MapCheckBlockingLine
//
// Checks for a blocking line between thing and the spawnthing coordinates
// given. Return true if there is a line; crossable indicates whether or not
// whether the ML_BLOCKING & ML_BLOCKMONSTERS should be ignored or not.
//
// -ACB- 1998/08/23
//
bool P_MapCheckBlockingLine(mobj_t * thing, mobj_t * spawnthing)
{
	int xlow, xhigh, xcount, ylow, yhigh, ycount;

	mx1 = thing->x;
	my1 = thing->y;
	mx2 = spawnthing->x;
	my2 = spawnthing->y;
	mb2 = spawnthing->z;
	mt2 = spawnthing->z + spawnthing->height;

	crosser = (spawnthing->extendedflags & EF_CROSSLINES);

	tmbbox[BOXLEFT] = mx1 < mx2 ? mx1 : mx2;
	tmbbox[BOXRIGHT] = mx1 > mx2 ? mx1 : mx2;
	tmbbox[BOXBOTTOM] = my1 < my2 ? my1 : my2;
	tmbbox[BOXTOP] = my1 > my2 ? my1 : my2;

	xlow  = (int)(tmbbox[BOXLEFT] - bmaporgx) / MAPBLOCKUNITS;
	xhigh = (int)(tmbbox[BOXRIGHT] - bmaporgx) / MAPBLOCKUNITS;
	ylow  = (int)(tmbbox[BOXBOTTOM] - bmaporgy) / MAPBLOCKUNITS;
	yhigh = (int)(tmbbox[BOXTOP] - bmaporgy) / MAPBLOCKUNITS;

	validcount++;

	mobj_hit_sky = false;
	blockline = NULL;

	for (xcount = xlow; xcount <= xhigh; xcount++)
		for (ycount = ylow; ycount <= yhigh; ycount++)
			if (!P_BlockLinesIterator(xcount, ycount, PIT_CheckBlockingLine))
				return true;

	return false;
}

//
// P_MapInit
//
bool P_MapInit(void)
{
	Z_InitStackArray(&spechit_a, (void***)&spechit, sizeof(line_t *), -8);

	return true;
}

