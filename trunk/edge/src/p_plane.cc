//----------------------------------------------------------------------------
//  EDGE Floor/Elevator/Teleport Action Code
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2004  The EDGE Team.
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
// -KM-  1998/09/01 Changed for DDF.
// -ACB- 1998/09/13 Moved the teleport procedure here
//

#include "i_defs.h"

#include "z_zone.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "m_random.h"
#include "p_local.h"
#include "r_state.h"
#include "s_sound.h"

#define DIRECTION_UP      1
#define DIRECTION_WAIT    0
#define DIRECTION_DOWN   -1
#define DIRECTION_STASIS -2

typedef enum
{
	RES_Ok,
	RES_Crushed,
	RES_PastDest,
	RES_Impossible
}
move_result_e;

// Linked list of moving parts.
gen_move_t *active_movparts = NULL;

static bool P_StasifySector(sector_t * sec);
static bool P_ActivateInStasis(int tag);

static elev_move_t *P_SetupElevatorAction(sector_t * sector, 
										  const elevatordef_c * type, sector_t * model);

static void MoveElevator(elev_move_t *elev);
static void MovePlane(plane_move_t *plane);
static void MoveSlider(slider_move_t *smov);

// -AJA- Perhaps using a pointer to `plane_info_t' would be better
//       than f**king about with the floorOrCeiling stuff all the
//       time.

static float HEIGHT(sector_t * sec, bool is_ceiling)
{
	if (is_ceiling)
		return sec->c_h;

	return sec->f_h;
}

static const image_t * SECPIC(sector_t * sec, bool is_ceiling,
							  const image_t *new_image)
{
	if (new_image)
	{
		if (is_ceiling) 
			sec->ceil.image = new_image;
		else
			sec->floor.image = new_image;
	}

	return is_ceiling ? sec->ceil.image : sec->floor.image;
}


//
// Get
//

//
// GetSecHeightReference
//
// Finds a sector height, using the reference provided; will select
// the approriate method of obtaining this value, if it cannot
// get it directly.
//
// -KM-  1998/09/01 Wrote Procedure.
// -ACB- 1998/09/06 Remarked and Reformatted.
// -ACB- 2001/02/04 Move to p_plane.c
//
static float GetSecHeightReference(heightref_e ref, sector_t * sec)
{
	switch (ref & REF_MASK)
	{
		case REF_Absolute:
			return 0;

		case REF_Current:
			return (ref & REF_CEILING) ? sec->c_h : sec->f_h;

		case REF_Surrounding:
			return P_FindSurroundingHeight(ref, sec);

		case REF_LowestLoTexture:
			return P_FindRaiseToTexture(sec);

		default:
			I_Error("GetSecHeightReference: undefined reference %d\n", ref);
	}

	return 0;
}

//
// GetElevatorHeightReference
//
// This is essentially the same as above, but used for elevator
// calculations. The reason behind this is that elevators have to
// work as dummy sectors.
//
// -ACB- 2001/02/04 Written
//
static float GetElevatorHeightReference(heightref_e ref, sector_t * sec)
{
	return -1;
}

//
// P_AddActivePart
//
// Adds to the tail of the list.
//
void P_AddActivePart(gen_move_t *movpart)
{
	gen_move_t *tmp;

	movpart->next = NULL;
  
	if (!active_movparts)
	{
		movpart->prev = NULL;
		active_movparts = movpart;
		return;
	}
  
	for(tmp = active_movparts; tmp->next; tmp = tmp->next)
	{ /* Do Nothing */ };

	movpart->prev = tmp;
	tmp->next = movpart;
}

//
// P_RemoveActivePart
//
static void P_RemoveActivePart(gen_move_t *movpart)
{
	elev_move_t *elev;
	plane_move_t *plane;
	slider_move_t *slider;

	switch(movpart->whatiam)
	{
		case MDT_ELEVATOR:
			elev = (elev_move_t*)movpart;
			elev->sector->ceil_move = NULL;
			elev->sector->floor_move = NULL;
			break;

		case MDT_PLANE:
			plane = (plane_move_t*)movpart;
			if (plane->is_ceiling)
				plane->sector->ceil_move = NULL;
			else
				plane->sector->floor_move = NULL;
			break;

		case MDT_SLIDER:
			slider = (slider_move_t*)movpart;
			slider->line->slider_move = NULL;
			break;

		default:
			break;
	}

	if (movpart->prev)
		movpart->prev->next = movpart->next;
	else
		active_movparts = movpart->next;

	if (movpart->next)
		movpart->next->prev = movpart->prev;
    
	Z_Free(movpart);
}


//
// P_RemoveAllActiveParts
//
void P_RemoveAllActiveParts(void)
{
	gen_move_t *movpart, *next;

	for (movpart = active_movparts; movpart; movpart = next)
	{
		next = movpart->next;
		Z_Free(movpart);            // P_RemoveActivePart() ??
	}
  
	active_movparts = NULL;
}



//
// FLOORS
//

//
// AttemptMovePlane
//
// Move a plane (floor or ceiling) and check for crushing
//
// Returns:
//    RES_Ok - the move was completely successful.
//
//    RES_Impossible - the move was not possible due to another solid
//    surface (e.g. an extrafloor) getting in the way.  The plane will
//    remain at its current height.
//
//    RES_PastDest - the destination height has been reached.  The
//    actual height in the sector may not be the target height, which
//    means some objects got in the way (whether crushed or not).
//
//    RES_Crushed - some objects got in the way.  When `crush'
//    parameter is true, those object will have been crushed (take
//    damage) and the plane height will be the new height, otherwise
//    the plane height will remain at its current height.
//
static move_result_e AttemptMovePlane(sector_t * sector, 
									  float speed, float dest, bool crush, 
									  bool is_ceiling, int direction)
{
	bool past = false;
	bool nofit;

	//
	// check whether we have gone past the destination height
	//
	if (direction == DIRECTION_UP && 
		HEIGHT(sector, is_ceiling) + speed > dest)
	{
		past = true;
		speed = dest - HEIGHT(sector, is_ceiling);
	}
	else if (direction == DIRECTION_DOWN && 
			 HEIGHT(sector, is_ceiling) - speed < dest)
	{
		past = true;
		speed = HEIGHT(sector, is_ceiling) - dest;
	}
 
	if (speed <= 0)
		return RES_PastDest;

	if (direction == DIRECTION_DOWN)
		speed = -speed;

	// check if even possible
	if (! P_CheckSolidSectorMove(sector, is_ceiling, speed))
	{
		return RES_Impossible;
	}
   
	//
	// move the actual sector, including all things in it
	//
	nofit = P_SolidSectorMove(sector, is_ceiling, speed, crush, false);
    
	if (! nofit)
		return past ? RES_PastDest : RES_Ok;

	// bugger, something got in our way !
 
	if (! crush)
	{
		// undo the change
		P_SolidSectorMove(sector, is_ceiling, -speed, false, false);
	}

	return past ? RES_PastDest : RES_Crushed;
}

//
// MovePlane
//
// Move a floor to it's destination (up or down).
//
static void MovePlane(plane_move_t *plane)
{
	move_result_e res;

	switch (plane->direction)
	{
		case DIRECTION_STASIS:
			plane->sfxstarted = false;
			break;

		case DIRECTION_DOWN:
			res = AttemptMovePlane(plane->sector, plane->speed,
								   MIN(plane->startheight, plane->destheight),
								   plane->crush && plane->is_ceiling,
								   plane->is_ceiling, plane->direction);

			if (!plane->sfxstarted)
			{
				S_StartSound((mobj_t *) &plane->sector->soundorg, 
							 plane->type->sfxdown);
				plane->sfxstarted = true;
			}

			if (res == RES_PastDest)
			{
				S_StartSound((mobj_t *) &plane->sector->soundorg, 
							 plane->type->sfxstop);
				plane->speed = plane->type->speed_up;

				if (plane->newspecial != -1)
				{
					plane->sector->props.special = (plane->newspecial <= 0) ? NULL :
						playsim::LookupSectorType(plane->newspecial);
				}

				SECPIC(plane->sector, plane->is_ceiling, plane->new_image);

				switch (plane->type->type)
				{
					case mov_Plat:
					case mov_Continuous:
						plane->direction = DIRECTION_WAIT;
						plane->waited = plane->type->wait;
						plane->speed = plane->type->speed_up;
						break;

					case mov_MoveWaitReturn:
						if (HEIGHT(plane->sector, plane->is_ceiling) == plane->startheight)
						{
							P_RemoveActivePart((gen_move_t*)plane);
						}
						else  // assume we reached the destination
						{
							plane->direction = DIRECTION_WAIT;
							plane->waited = plane->type->wait;
							plane->speed = plane->type->speed_up;
						}
						break;

					default:
					case mov_Stairs:
					case mov_Once:
						P_RemoveActivePart((gen_move_t*)plane);
						break;
				}
			}
			else if (res == RES_Crushed || res == RES_Impossible)
			{
				if (plane->crush)
				{
					plane->speed = plane->type->speed_down / 8;
				}
				else if (plane->type->type == mov_MoveWaitReturn)  // Go back up
				{
					plane->direction = 1;
					plane->sfxstarted = false;
					plane->waited = 0;
					plane->speed = plane->type->speed_up;
				}
			}

			break;

		case DIRECTION_WAIT:
			if (--plane->waited <= 0)
			{
				int dir;
				float dest;

				if (HEIGHT(plane->sector, plane->is_ceiling) == plane->destheight)
					dest = plane->startheight;
				else
					dest = plane->destheight;

				if (HEIGHT(plane->sector, plane->is_ceiling) > dest)
				{
					dir = -1;
					plane->speed = plane->type->speed_down;
				}
				else
				{
					dir = 1;
					plane->speed = plane->type->speed_up;
				}

				if (dir)
				{
					S_StartSound((mobj_t *) &plane->sector->soundorg,
								 plane->type->sfxstart);
				}

				plane->direction = dir;  // time to go back
				plane->sfxstarted = false;
			}
			break;

		case DIRECTION_UP:
			res = AttemptMovePlane(plane->sector, plane->speed,
								   MAX(plane->startheight, plane->destheight),
								   plane->crush && !plane->is_ceiling,
								   plane->is_ceiling, plane->direction);

			if (!plane->sfxstarted)
			{
				S_StartSound((mobj_t *) &plane->sector->soundorg, 
							 plane->type->sfxup);
				plane->sfxstarted = true;
			}

			if (res == RES_PastDest)
			{
				S_StartSound((mobj_t *) &plane->sector->soundorg, 
							 plane->type->sfxstop);

				if (plane->newspecial != -1)
				{
					plane->sector->props.special = (plane->newspecial <= 0) ? NULL :
						sectortypes.Lookup(plane->newspecial);
				}

				SECPIC(plane->sector, plane->is_ceiling, plane->new_image);

				switch (plane->type->type)
				{
					case mov_Plat:
					case mov_Continuous:
						plane->direction = 0;
						plane->waited = plane->type->wait;
						plane->speed = plane->type->speed_down;
						break;

					case mov_MoveWaitReturn:
						if (HEIGHT(plane->sector, plane->is_ceiling) == plane->startheight)
						{
							P_RemoveActivePart((gen_move_t*)plane);
						}
						else  // assume we reached the destination
						{
							plane->direction = 0;
							plane->speed = plane->type->speed_down;
							plane->waited = plane->type->wait;
						}
						break;

					default:
					case mov_Once:
					case mov_Stairs:
						P_RemoveActivePart((gen_move_t*)plane);
						break;
				}

			}
			else if (res == RES_Crushed || res == RES_Impossible)
			{
				if (plane->crush)
				{
					plane->speed = plane->type->speed_up / 8;
				}
				else if (plane->type->type == mov_MoveWaitReturn)  // Go back down
				{
					plane->direction = -1;
					plane->sfxstarted = false;
					plane->waited = 0;
					plane->speed = plane->type->speed_down;
				}
			}
			break;

		default:
			I_Error("MovePlane: Unknown direction %d", plane->direction);
	}
}

//
// P_RunActiveSectors
//
// Executes one tic's plane_move_t thinking.
// Active sectors can destroy themselves, but not each other.
// We do not have to bother about a removal queue, but we can not rely on
// sec still being in memory after MovePlane.
//
// -AJA- 2000/08/06 Now handles horizontal sliding doors too.
// -ACB- 2001/01/14 Now handles elevators too.
// -ACB- 2001/02/08 Now generic routine with gen_move_t;
//
void P_RunActiveSectors(void)
{
	gen_move_t *part, *part_next;

	for (part = active_movparts; part; part = part_next)
	{
		part_next = part->next;

		switch (part->whatiam)
		{
			case MDT_ELEVATOR:
				MoveElevator((elev_move_t*)part);
				break;

			case MDT_PLANE:
				MovePlane((plane_move_t*)part);
				break;

			case MDT_SLIDER:
				MoveSlider((slider_move_t*)part);
				break;

			default:
				break;
		}
	}
}

//
// P_GSS
//
static sector_t *P_GSS(sector_t * sec, float dest, bool forc)
{
	int i;
	int secnum = sec - sectors;
	sector_t *sector;

	for (i = sec->linecount-1; i; i--)
	{
		if (P_TwoSided(secnum, i))
		{
			if (P_GetSide(secnum, i, 0)->sector - sectors == secnum)
			{
				sector = P_GetSector(secnum, i, 1);

				if (SECPIC(sector, forc, NULL) != SECPIC(sec, forc, NULL)
					&& HEIGHT(sector, forc) == dest)
				{
					return sector;
				}

			}
			else
			{
				sector = P_GetSector(secnum, i, 0);
        
				if (SECPIC(sector, forc, NULL) != SECPIC(sec, forc, NULL)
					&& HEIGHT(sector, forc) == dest)
				{
					return sector;
				}
			}
		}
	}

	for (i = sec->linecount; i--;)
	{
		if (P_TwoSided(secnum, i))
		{
			if (P_GetSide(secnum, i, 0)->sector - sectors == secnum)
			{
				sector = P_GetSector(secnum, i, 1);
			}
			else
			{
				sector = P_GetSector(secnum, i, 0);
			}
			if (sector->validcount != validcount)
			{
				sector->validcount = validcount;
				sector = P_GSS(sector, dest, forc);
				if (sector)
					return sector;
			}
		}
	}

	return NULL;
}

//
// P_GetSectorSurrounding
//
static sector_t *P_GetSectorSurrounding(sector_t * sec, float dest, bool forc)
{
	validcount++;
	sec->validcount = validcount;
	return P_GSS(sec, dest, forc);
}

//
// P_SetupSectorAction
//
// Setup the Floor Action, depending on the linedeftype trigger and the
// sector info.
//
static plane_move_t *P_SetupSectorAction(sector_t * sector, 
                                         const movplanedef_c * type, 
                                         sector_t * model)
{
	plane_move_t *plane;
	float start, dest;

	// new door thinker
	plane = Z_New(plane_move_t, 1);

	if (type->is_ceiling)
		sector->ceil_move = (gen_move_t*)plane;
	else
		sector->floor_move = (gen_move_t*)plane;

	plane->whatiam = MDT_PLANE;
	plane->sector = sector;
	plane->crush = type->crush;
	plane->sfxstarted = false;
	start = HEIGHT(sector, type->is_ceiling);

	dest = GetSecHeightReference(type->destref, sector);
	dest += type->dest;

	if (type->type == mov_Plat || type->type == mov_Continuous)
	{
		start = GetSecHeightReference(type->otherref, sector);
		start += type->other;
	}

#if 0  // DEBUG
	L_WriteDebug("SEC_ACT: %d type %d %s start %1.0f dest %1.0f\n",
				 sector - sectors, type->type, type->is_ceiling ? "CEIL" : "FLOOR", 
				 start, dest);
#endif

	//--------------------------------------------------------------------------
	// Floor Speed Notes:
	//
	//   Floor speed setup; -M_PI is the default speed, This is used to simulate
	//   the use of an instant movement. i.e. a floor that raises or falls to
	//   its destination height in one tic: This is also implemented for WAD's
	//   that use odd linedef requests to achieve the instant effect.
	//
	//   If someone would want a speed close to -3.1, the probability that
	//   he would use -M_PI accidentally is really low, so we do not care about
	//   it. I can't think of any situation where there is a point in setting
	//   the speed to exactly pi.
	//
	//   Therefore a speed of -M_PI, is translated to the distance between the
	//   start and destination: instant movement; otherwise the speed is taken
	//   from the linedef type.
	//
	//--------------------------------------------------------------------------
	if (type->prewait)
	{
		plane->direction = DIRECTION_WAIT;
		plane->waited = type->prewait;
	}
	else if (type->type == mov_Continuous)
	{
		plane->direction = (P_Random() & 1) ? DIRECTION_UP : DIRECTION_DOWN;

		if (plane->direction == DIRECTION_UP)
			plane->speed = type->speed_up;
		else
			plane->speed = type->speed_down;
	}
	else if (dest > start)
	{
		plane->direction = DIRECTION_UP;

		// -ACB- 1998/09/09 See floor speed notes...
		if (type->speed_up >= 0)
			plane->speed = type->speed_up;
		else
			plane->speed = dest - start;
	}
	else if (start > dest)
	{
		plane->direction = DIRECTION_DOWN;

		// -ACB- 1998/09/09 See floor speed notes...
		if (type->speed_down >= 0)
			plane->speed = type->speed_down;
		else
			plane->speed = start - dest;
	}
	else
	{
		if (type->is_ceiling)
			sector->ceil_move = NULL;
		else
			sector->floor_move = NULL;

		Z_Free(plane);
		return NULL;
	}

	plane->destheight = dest;
	plane->startheight = start;
	plane->tag = sector->tag;
	plane->type = type;
	plane->new_image = SECPIC(sector, type->is_ceiling, NULL);
	plane->newspecial = -1;
	plane->is_ceiling = type->is_ceiling;

	// -ACB- 10/01/2001 Trigger starting sfx
	S_StopLoopingSound((mobj_t *) & sector->soundorg);
	if (type->sfxstart)
		S_StartSound((mobj_t *) & sector->soundorg, type->sfxstart);

	// change to surrounding
	if (type->tex[0] == '-')
	{
		model = P_GetSectorSurrounding(sector, plane->destheight, type->is_ceiling);
		if (model)
		{
			plane->new_image = SECPIC(model, type->is_ceiling, NULL);

			plane->newspecial = model->props.special ?
				model->props.special->ddf.number : 0;
		}
		if (plane->direction == (type->is_ceiling ? -1 : 1))
		{
			SECPIC(sector, type->is_ceiling, plane->new_image);
			if (plane->newspecial != -1)
			{
				sector->props.special = (plane->newspecial <= 0) ? NULL :
					sectortypes.Lookup(plane->newspecial);
			}
		}
	}
	else if (type->tex[0] == '+')
	{
		if (model)
		{
			if (SECPIC(model,  type->is_ceiling, NULL) == 
				SECPIC(sector, type->is_ceiling, NULL))
			{
				model = P_GetSectorSurrounding(model, plane->destheight,
											   type->is_ceiling);
			}
		}

		if (model)
		{
			plane->new_image = SECPIC(model, type->is_ceiling, NULL);
			plane->newspecial = model->props.special ?
				model->props.special->ddf.number : 0;

			if (plane->direction == (type->is_ceiling ? -1 : 1))
			{
				SECPIC(sector, type->is_ceiling, plane->new_image);

				if (plane->newspecial != -1)
				{
					sector->props.special = (plane->newspecial <= 0) ? NULL :
						sectortypes.Lookup(plane->newspecial);
				}
			}
		}
	}
	else if (type->tex[0])
	{
		plane->new_image = W_ImageFromFlat(type->tex);
	}

	P_AddActivePart((gen_move_t*)plane);
	return plane;
}

//
// EV_Teleport
//
// Teleportation is an effect which is simulated by searching for the first
// special[MOBJ_TELEPOS] in a sector with the same tag as the activation line,
// moving an object from one sector to another upon the MOBJ_TELEPOS found, and
// possibly spawning an effect object (i.e teleport flash) at either the entry &
// exit points or both.
//
// -KM- 1998/09/01 Added stuff for lines.ddf (mostly sounds)
//
// -ACB- 1998/09/11 Reformatted and cleaned up.
//
// -ACB- 1998/09/12 Teleport delay setting from linedef.
//
// -ACB- 1998/09/13 used effect objects: the objects themselves make any sound and
//                  the in effect object can be different to the out object.
//
// -ACB- 1998/09/13 Removed the missile checks: no need since this would have been
//                  Checked at the linedef stage.
//
// -KM- 1998/11/25 Changed Erik's code a bit, Teleport flash still appears.
//  if def faded_teleportation == 1, doesn't if faded_teleportation == 2
//
// -ES- 1998/11/28 Changed Kester's code a bit :-) Teleport method can now be
//  toggled in the menu. (That is the way it should be. -KM)
//
// -KM- 1999/01/31 Search only the target sector, not the entire map.
//
// -AJA- 1999/07/12: Support for TELEPORT_SPECIAL in lines.ddf.
// -AJA- 1999/07/30: Updated for extra floor support.
// -AJA- 1999/10/21: Allow line to be NULL, and added `tag' param.
//
bool EV_Teleport
(
	line_t* line,
	int tag, 
	int side,
	mobj_t* thing,
	int delay,
	int special,
	const mobjtype_c* ineffectobj,
	const mobjtype_c * outeffectobj
	)
{
	int i;
	angle_t an;
	mobj_t *currmobj;
	float oldx;
	float oldy;
	float oldz;
	float centre_x, centre_y;
	float new_x, new_y, new_z;
	mobj_t *fog;

	if (!thing)
		return false;

	for (i = 0; i < numsubsectors; i++)
	{
		if (subsectors[i].sector->tag != tag)
			continue;

		currmobj = subsectors[i].thinglist;

		while (currmobj)
		{
			// not a teleportman
			if (currmobj->info != outeffectobj)
			{
				currmobj = currmobj->snext;
				continue;
			}

			oldx = thing->x;
			oldy = thing->y;
			oldz = thing->z;

			new_x = currmobj->x;
			new_y = currmobj->y;
			new_z = currmobj->z;

			if (line && (special & TELSP_SameOffset))
			{
				centre_x = (line->v1->x + line->v2->x) / 2;
				centre_y = (line->v1->y + line->v2->y) / 2;

				new_x += thing->x - centre_x;
				new_y += thing->y - centre_y;
			}

			if (special & TELSP_SameHeight)
				new_z += (thing->z - thing->floorz);
			else if (thing->flags & MF_MISSILE)
				new_z += thing->origheight;

			if (!P_TeleportMove(thing, new_x, new_y, new_z))
				return false;

			if (thing->player)
				thing->player->viewz = thing->z + thing->player->viewheight;

			// spawn teleport fog at source and destination
			if (ineffectobj)
			{
				fog = P_MobjCreateObject(oldx, oldy, oldz, ineffectobj);

				if (fog->info->chase_state)
					P_SetMobjState(fog, fog->info->chase_state);
			}

			an = currmobj->angle;

			//
			// -ACB- 1998/09/06 Switched 40 to 20. This by my records is
			//                  the original setting.
			//
			// -ES- 1998/10/29 When fading, we don't want to see the fog.
			//
			fog = P_MobjCreateObject(currmobj->x + 20 * M_Cos(an),
									 currmobj->y + 20 * M_Sin(an),
									 currmobj->z, outeffectobj);

			if (fog->info->chase_state)
				P_SetMobjState(fog, fog->info->chase_state);

			if (thing->player && !telept_flash)
				fog->vis_target = fog->visibility = INVISIBLE;

			// don't move for a bit
			if (thing->player && !(special & TELSP_SameSpeed))
			{
				thing->reactiontime = delay;
				// -ES- 1998/10/29 Start the fading
				if (telept_effect && thing->player == displayplayer)
					R_StartFading(0, (delay * 5) / 2);
				thing->mom.x = thing->mom.y = thing->mom.z = 0;
			}

			if (special & TELSP_Rotate)
			{
				thing->angle += currmobj->angle;
			}
			else if (!(special & TELSP_SameDir))
			{
				thing->angle = currmobj->angle;
				thing->vertangle = currmobj->vertangle;
			}

			if (thing->flags & MF_MISSILE)
			{
				thing->mom.x = thing->speed * M_Cos(thing->angle);
				thing->mom.y = thing->speed * M_Sin(thing->angle);
			}

			return true;

		}  // while (currmobj)

	}  // for (subsector) loop

	return false;
}

//
// EV_BuildOneStair
//
// BUILD A STAIRCASE!
//
// -AJA- 1999/07/04: Fixed the problem on MAP20. The next stair's
// dest height should be relative to the previous stair's dest height
// (and not just the current height).
//
// -AJA- 1999/07/29: Split into two functions. The old code could do bad
// things (e.g. skip a whole staircase) when 2 or more stair sectors
// were tagged.
//
static bool EV_BuildOneStair(sector_t * sec, const movplanedef_c * type)
{
	int i;
	float next_height;
	bool more;
	bool rtn;

	plane_move_t *stairs;
	sector_t *tsec;
	float stairsize = type->dest;

	const image_t *image = sec->floor.image;

	// new floor thinker

	stairs = P_SetupSectorAction(sec, type, sec);
	rtn = stairs ? true : false;
	next_height = stairs->destheight + stairsize;

	do
	{
		more = false;

		// Find next sector to raise
		//
		// 1. Find 2-sided line with same sector side[0]
		// 2. Other side is the next sector to raise
		//
		for (i = 0; i < sec->linecount; i++)
		{
			if (!(sec->lines[i]->flags & ML_TwoSided))
				continue;

			if (sec != sec->lines[i]->frontsector)
				continue;

			tsec = sec->lines[i]->backsector;

			if (tsec->floor.image != image)
				continue;

			if (type->is_ceiling && tsec->ceil_move)
				continue;

			if (!type->is_ceiling && tsec->floor_move)
				continue;

			stairs = P_SetupSectorAction(tsec, type, tsec);

			if (stairs)
			{
				stairs->destheight = next_height;
				next_height += stairsize;
				sec = tsec;
				more = true;
			}

			break;
		}
	}
	while (more);

	return rtn;
}

//
// EV_BuildStairs
//
static bool EV_BuildStairs(sector_t * sec, const movplanedef_c * type)
{
	bool rtn = false;

	while (sec->tag_prev)
		sec = sec->tag_prev;

	for (; sec; sec = sec->tag_next)
	{
		// Already moving?  If so, keep going...
		if (sec->ceil_move && type->is_ceiling)
			continue;

		// Already moving?  If so, keep going...
		if (sec->floor_move && !type->is_ceiling)
			continue;

		if (EV_BuildOneStair(sec, type))
			rtn = true;
	}

	return rtn;
}

//
// EV_DoPlane
//
// Do Platforms/Floors/Stairs/Ceilings/Doors
//
bool EV_DoPlane(sector_t * sec, const movplanedef_c * type, sector_t * model)
{
	// Activate all <type> plats that are in_stasis
	switch (type->type)
	{
		case mov_Plat:
		case mov_Continuous:
			if (P_ActivateInStasis(sec->tag))
				return true;
			break;

		case mov_Stairs:
			return EV_BuildStairs(sec, type);

		case mov_Stop:
			return P_StasifySector(sec);

		default:
			break;
	}

	if (type->is_ceiling)
	{
		if (sec->ceil_move)
			return false;
	}
	else
	{
		if (sec->floor_move)
			return false;
	}

	// Do Floor action
	return P_SetupSectorAction(sec, type, model) ? true : false;
}

//
// EV_ManualPlane
//
bool EV_ManualPlane(line_t * line, mobj_t * thing, const movplanedef_c * type)
{
	sector_t *sec;
	plane_move_t *msec;
	int side;
	int dir = 1;
	int olddir = 1;

	side = 0;  // only front sides can be used

	// if the sector has an active thinker, use it
	sec = side ? line->frontsector : line->backsector;
	if (!sec)
		return false;

	if (type->is_ceiling)
		msec = (plane_move_t *)sec->ceil_move;
	else
		msec = (plane_move_t *)sec->floor_move;

	if (msec && thing)
	{
		switch (type->type)
		{
			case mov_MoveWaitReturn:
				olddir = msec->direction;

				// Only players close doors
				if ((msec->direction != -1) && thing->player)
					dir = msec->direction = -1;
				else
					dir = msec->direction = 1;
				break;
        
			default:
				break;
		}

		if (dir != olddir)
		{
			S_StartSound((mobj_t *) & sec->soundorg, type->sfxstart);
			msec->sfxstarted = !(thing->player);
			return true;
		}

		return false;
	}

	return EV_DoPlane(sec, type, sec);
}

//
// P_ActivateInStasis
//
static bool P_ActivateInStasis(int tag)
{
	bool rtn;
	gen_move_t *movpart;
	plane_move_t *plane;

	rtn = false;
	for (movpart = active_movparts; movpart; movpart = movpart->next)
	{
		if (movpart->whatiam == MDT_PLANE)
		{
			plane = (plane_move_t*)movpart;
			if(plane->direction == -2 && plane->tag == tag)
			{
				plane->direction = plane->olddirection;
				rtn = true;
			}
		}
	}

	return rtn;
}

//
// P_StasifySector
//
static bool P_StasifySector(sector_t * sec)
{
	bool rtn;
	gen_move_t *movpart;
	plane_move_t *plane;

	rtn = false;
	for (movpart = active_movparts; movpart; movpart = movpart->next)
	{
		if (movpart->whatiam == MDT_PLANE)
		{
			plane = (plane_move_t*)movpart;
			if(plane->direction != -2 && plane->tag == sec->tag)
			{
				plane->olddirection = plane->direction;
				plane->direction = -2;
				rtn = true;
			}
		}
	}

	return rtn;
}

// -AJA- 1999/12/07: cleaned up this donut stuff

linetype_c donut[2];
static int donut_setup = 0;

//
// EV_DoDonut
//
// Special Stuff that can not be categorized
// Mmmmmmm....  Donuts....
//
bool EV_DoDonut(sector_t * s1, sfx_t *sfx[4])
{
	sector_t *s2;
	sector_t *s3;
	bool result = false;
	int i;
	plane_move_t *sec;

	if (! donut_setup)
	{
		donut[0].Default();
		donut[0].count = 1;
		donut[0].specialtype = 0;
		donut[0].f.Default(movplanedef_c::DEFAULT_DonutFloor);
		donut[0].f.tex.Set("-");

		donut[1].Default();
		donut[1].count = 1;
		donut[1].f.Default(movplanedef_c::DEFAULT_DonutFloor);
		donut[1].f.dest = (float)INT_MIN;	// FIXME!! INT_MIN on an FP Number?

		donut_setup++;
	}
  
	// ALREADY MOVING?  IF SO, KEEP GOING...
	if (s1->floor_move)
		return false;

	s2 = P_GetNextSector(s1->lines[0], s1);

	for (i = 0; i < s2->linecount; i++)
	{
		if (!(s2->lines[i]->flags & ML_TwoSided) || (s2->lines[i]->backsector == s1))
			continue;

		s3 = s2->lines[i]->backsector;

		result = true;

		// Spawn rising slime
		donut[0].f.sfxup = sfx[0];
		donut[0].f.sfxstop = sfx[1];
    
		sec = P_SetupSectorAction(s2, &donut[0].f, s3);

		if (sec)
		{
			sec->destheight = s3->f_h;
			s2->floor.image = sec->new_image = s3->floor.image;
			s2->props.special = s3->props.special;
		}

		// Spawn lowering donut-hole
		donut[1].f.sfxup = sfx[2];
		donut[1].f.sfxstop = sfx[3];

		sec = P_SetupSectorAction(s1, &donut[1].f, s1);

		if (sec)
			sec->destheight = s3->f_h;
		break;
	}

	return result;
}

//
// SliderCanClose
//
static INLINE bool SliderCanClose(line_t *line)
{
	return ! P_ThingsOnLine(line);
}

//
// MoveSlider
//
static void MoveSlider(slider_move_t *smov)
{
	sector_t *sec = smov->line->frontsector;

	switch (smov->direction)
	{
		// WAITING
		case 0:
			if (--smov->waited <= 0)
			{
				if (SliderCanClose(smov->line))
				{
					S_StartSound((mobj_t *) & sec->soundorg, smov->info->sfx_start);
					smov->sfxstarted = false;
					smov->direction = -1;
				}
				else
				{
					// try again soon
					smov->waited = TICRATE / 3;
				}
			}
			break;

			// OPENING
		case 1:
			if (! smov->sfxstarted)
			{
				S_StartSound((mobj_t *) & sec->soundorg, smov->info->sfx_open);
				smov->sfxstarted = true;
			}

			smov->opening += smov->info->speed;

			// mark line as non-blocking (at some point)
			P_ComputeGaps(smov->line);

			if (smov->opening >= smov->target)
			{
				S_StartSound((mobj_t *) & sec->soundorg, smov->info->sfx_stop);
				smov->opening = smov->target;
				smov->direction = 0;
				smov->waited = smov->info->wait;

				if (smov->final_open)
				{
					line_t *ld = smov->line;

					// clear line special
					ld->special = NULL;

					P_RemoveActivePart((gen_move_t*)smov);

					// clear the side textures
					ld->side[0]->middle.image = NULL;
					ld->side[1]->middle.image = NULL;

					P_ComputeWallTiles(ld, 0);
					P_ComputeWallTiles(ld, 1);

					return;
				}
			}
			break;

			// CLOSING
		case -1:
			if (! smov->sfxstarted)
			{
				S_StartSound((mobj_t *) & sec->soundorg, smov->info->sfx_close);
				smov->sfxstarted = true;
			}

			smov->opening -= smov->info->speed;

			// mark line as blocking (at some point)
			P_ComputeGaps(smov->line);

			if (smov->opening <= 0.0f)
			{
				S_StartSound((mobj_t *) & sec->soundorg, smov->info->sfx_stop);
				P_RemoveActivePart((gen_move_t*)smov);
				return;
			}
			break;

		default:
			I_Error("MoveSlider: Unknown direction %d", smov->direction);
	}
}

//
// EV_DoSlider
//
// Handle thin horizontal sliding doors.
//
void EV_DoSlider(line_t * line, mobj_t * thing, const sliding_door_c * s)
{
	sector_t *sec = line->frontsector;
	slider_move_t *smov;

	if (! thing || ! sec || ! line->side[0] || ! line->side[1])
		return;

	// if the line has an active thinker, use it
	if (line->slider_move)
	{
		smov = line->slider_move;

		// only players close doors
		if (smov->direction == 0 && thing->player)
		{
			smov->waited = 0;
		}
		return;
	}

	// new sliding door thinker
	smov = Z_New(slider_move_t, 1);

	smov->whatiam = MDT_SLIDER;
	smov->info = &line->special->s;
	smov->line = line;
	smov->opening = 0.0f;
	smov->line_len = R_PointToDist(0, 0, line->dx, line->dy);
	smov->target = smov->line_len * PERCENT_2_FLOAT(smov->info->distance);

	smov->direction = 1;
	smov->sfxstarted = ! thing->player;
	smov->final_open = (line->count == 1);

	line->slider_move = smov;

	P_AddActivePart((gen_move_t*)smov);

	S_StartSound((mobj_t *) & sec->soundorg, s->sfx_start);

	// Must handle line count here, since the normal code in p_spec.c
	// will clear the line->special pointer, confusing various bits of
	// code that deal with sliding doors (--> crash).
	// 
	if (line->count > 0)
		line->count--;
}

//
// AttemptMoveElevator
//
static move_result_e AttemptMoveElevator(sector_t *sec, float speed, 
										 float dest, int direction)
{
#if 0  // -AJA- FIXME: exfloorlist[] removed
	move_result_e res;
	bool didnotfit;
	float currdest;
	float lastfh;
	float lastch;
	float diff;
	sector_t *parentsec;
	int i;

	res = RES_Ok;

	currdest = 0.0f;

	if (direction == DIRECTION_UP)
		currdest = sec->c_h + speed;
	else if (direction == DIRECTION_DOWN)
		currdest = sec->f_h - speed;

	for (i=0; i<sec->exfloornum; i++)
	{
		parentsec = sec->exfloorlist[i];

		if (direction == DIRECTION_UP)
		{
			if (currdest > dest)
			{
				lastch = sec->c_h;
				lastfh = sec->f_h;

				diff = lastch - dest;

				sec->c_h = dest;
				sec->f_h -= diff;
				didnotfit = P_ChangeSector(sec, false);
				if (didnotfit)
				{
					sec->c_h = lastch;
					sec->f_h = lastfh;
					P_ChangeSector(sec, false);
				}
				res = RES_PastDest;
			} 
			else
			{
				lastch = sec->c_h;
				lastfh = sec->f_h;

				diff = lastch - currdest;

				sec->c_h = currdest;
				sec->f_h -= diff;
				didnotfit = P_ChangeSector(sec, false);
				if (didnotfit)
				{
					sec->c_h = lastch;
					sec->f_h = lastfh;
					P_ChangeSector(sec, false);
					res = RES_PastDest;
				}
			}
		}
		else if (direction == DIRECTION_DOWN)
		{
			if (currdest < dest)
			{
				lastch = sec->c_h;
				lastfh = sec->f_h;

				diff = lastfh - dest;

				sec->c_h -= diff;
				sec->f_h = dest;
				didnotfit = P_ChangeSector(sec, false);
				if (didnotfit)
				{
					sec->c_h = lastch;
					sec->f_h = lastfh;
					P_ChangeSector(sec, false);
				}
				res = RES_PastDest;
			} 
			else
			{
				lastch = sec->c_h;
				lastfh = sec->f_h;

				diff = lastfh - currdest;

				sec->c_h -= diff;
				sec->f_h = currdest;
				didnotfit = P_ChangeSector(sec, false);
				if (didnotfit)
				{
					sec->c_h = lastch;
					sec->f_h = lastfh;
					P_ChangeSector(sec, false);
					res = RES_PastDest;
				}
			}
		}
	}
	return res;
#endif

	return RES_Ok;
}

//
// MoveElevator
//
static void MoveElevator(elev_move_t *elev)
{
	move_result_e res;
	float num;

	switch (elev->direction)
	{
		case DIRECTION_DOWN:
			res = AttemptMoveElevator(elev->sector,
									  elev->speed,
									  elev->destheight,
									  elev->direction);

			if (!elev->sfxstarted)
			{
				S_StartSound((mobj_t *) & elev->sector->soundorg, elev->type->sfxdown);
				elev->sfxstarted = true;
			}

			if (res == RES_PastDest || res == RES_Impossible)
			{
				S_StartSound((mobj_t *) & elev->sector->soundorg, elev->type->sfxstop);
				elev->speed = elev->type->speed_up;

// ---> ACB 2001/03/25 Quick hack to get continous movement
//        P_RemoveActivePart((gen_move_t*)elev);
				elev->direction = DIRECTION_UP;

				num = elev->destheight;
				elev->destheight = elev->startheight;
				elev->startheight = num;
// ---> ACB 2001/03/25 Quick hack to get continous movement
			}
			break;
      
		case DIRECTION_WAIT:
			break;
      
		case DIRECTION_UP:
			res = AttemptMoveElevator(elev->sector,
									  elev->speed,
									  elev->destheight,
									  elev->direction);

			if (!elev->sfxstarted)
			{
				S_StartSound((mobj_t *) & elev->sector->soundorg, elev->type->sfxdown);
				elev->sfxstarted = true;
			}

			if (res == RES_PastDest || res == RES_Impossible)
			{
				S_StartSound((mobj_t *) & elev->sector->soundorg, elev->type->sfxstop);
				elev->speed = elev->type->speed_down;

// ---> ACB 2001/03/25 Quick hack to get continous movement
//        P_RemoveActivePart((gen_move_t*)elev);

				elev->direction = DIRECTION_DOWN; 
				num = elev->destheight;
				elev->destheight = elev->startheight;
				elev->startheight = num;
// ---> ACB 2001/03/25 Quick hack to get continous movement

			}
			break;
      
		default:
			break;
	}

	return;
}

//
// EV_DoElevator
//
// Do Elevators
//
bool EV_DoElevator(sector_t * sec, const elevatordef_c * type, sector_t * model)
{
#if 0
	if (!sec->controller)
		return false;
#endif

	if (sec->ceil_move || sec->floor_move)
		return false;

	// Do Elevator action
	return P_SetupElevatorAction(sec, type, model) ? true : false;
}

//
// EV_ManualElevator
//
bool EV_ManualElevator(line_t * line, mobj_t * thing,  const elevatordef_c * type)
{
	return false;
}

//
// P_SetupElevatorAction
//
static elev_move_t *P_SetupElevatorAction(sector_t * sector,
										  const elevatordef_c * type, sector_t * model)
{
	elev_move_t *elev;
	float start, dest;

	// new door thinker
	elev = Z_New(elev_move_t, 1);

	sector->ceil_move = (gen_move_t*)elev;
	sector->floor_move = (gen_move_t*)elev;

	elev->whatiam = MDT_ELEVATOR;
	elev->sector = sector;
	elev->sfxstarted = false;

// -ACB- BEGINNING OF THE HACKED TO FUCK BIT (START)

	start = sector->c_h;
	dest  = 192.0f;

// -ACB- FINISH OF THE HACKED TO FUCK BIT (END)

	if (dest > start)
	{
		elev->direction = DIRECTION_UP;

		if (type->speed_up >= 0)
			elev->speed = type->speed_up;
		else
			elev->speed = dest - start;
	}
	else if (start > dest)
	{
		elev->direction = DIRECTION_DOWN;

		if (type->speed_down >= 0)
			elev->speed = type->speed_down;
		else
			elev->speed = start - dest;
	}
	else
	{
		sector->ceil_move = NULL;
		sector->floor_move = NULL;

		Z_Free(elev);
		return NULL;
	}

	elev->destheight = dest;
	elev->startheight = start;
	elev->tag = sector->tag;
	elev->type = type;

	// -ACB- 10/01/2001 Trigger starting sfx
	S_StopLoopingSound((mobj_t *) & sector->soundorg);
	if (type->sfxstart)
		S_StartSound((mobj_t *) & sector->soundorg, type->sfxstart);

	P_AddActivePart((gen_move_t*)elev);
	return elev;
}

