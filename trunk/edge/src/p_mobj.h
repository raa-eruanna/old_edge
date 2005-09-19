//----------------------------------------------------------------------------
//  EDGE Moving Object Header
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2005  The EDGE Team.
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
// IMPORTANT NOTE: Altering anything within the mobj_t will most likely
//                 require changes to p_saveg.c and the save-game object
//                 (savegmobj_t); if you experience any problems with
//                 savegames, check here!
//

#ifndef __P_MOBJ__
#define __P_MOBJ__

#include "m_math.h"

// forward decl.
class atkdef_c;
class mobjtype_c;

struct mobj_s;
struct player_s;
struct rad_script_s;
struct region_properties_s;
struct state_s;
struct subsector_s;
struct touch_node_s;

//
// NOTES: mobj_t
//
// mobj_ts are used to tell the refresh where to draw an image,
// tell the world simulation when objects are contacted,
// and tell the sound driver how to position a sound.
//
// The refresh uses the next and prev links to follow
// lists of things in sectors as they are being drawn.
// The sprite, frame, and angle elements determine which patch_t
// is used to draw the sprite if it is visible.
// The sprite and frame values are allmost always set
// from state_t structures.
//
// The statescr.exe utility generates the states.h and states.c
// files that contain the sprite/frame numbers from the
// statescr.txt source file.
//
// The xyz origin point represents a point at the bottom middle
// of the sprite (between the feet of a biped).
// This is the default origin position for patch_ts grabbed
// with lumpy.exe.
// A walking creature will have its z equal to the floor
// it is standing on.
//
// The sound code uses the x,y, and subsector fields
// to do stereo positioning of any sound effited by the mobj_t.
//
// The play simulation uses the blocklinks, x,y,z, radius, height
// to determine when mobj_ts are touching each other,
// touching lines in the map, or hit by trace lines (gunshots,
// lines of sight, etc).
// The mobj_t->flags element has various bit flags
// used by the simulation.
//
// Every mobj_t is linked into a single sector
// based on its origin coordinates.
// The subsector_t is found with R_PointInSubsector(x,y),
// and the sector_t can be found with subsector->sector.
// The sector links are only used by the rendering code,
// the play simulation does not care about them at all.
//
// Any mobj_t that needs to be acted upon by something else
// in the play world (block movement, be shot, etc) will also
// need to be linked into the blockmap.
// If the thing has the MF_NOBLOCK flag set, it will not use
// the block links. It can still interact with other things,
// but only as the instigator (missiles will run into other
// things, but nothing can run into a missile).
// Each block in the grid is 128*128 units, and knows about
// every line_t that it contains a piece of, and every
// interactable mobj_t that has its origin contained.  
//
// A valid mobj_t is a mobj_t that has the proper subsector_t
// filled in for its xy coordinates and is linked into the
// sector from which the subsector was made, or has the
// MF_NOSECTOR flag set (the subsector_t needs to be valid
// even if MF_NOSECTOR is set), and is linked into a blockmap
// block or has the MF_NOBLOCKMAP flag set.
// Links should only be modified by the P_[Un]SetThingPosition()
// functions.
// Do not change the MF_NO? flags while a thing is valid.
//
// Any questions?
//

//
// Misc. mobj flags
//
typedef enum
{
	// Call P_TouchSpecialThing when touched.
	MF_SPECIAL = 1,

	// Blocks.
	MF_SOLID = 2,

	// Can be hit.
	MF_SHOOTABLE = 4,

	// Don't use the sector links (invisible but touchable).
	MF_NOSECTOR = 8,

	// Don't use the blocklinks (inert but displayable)
	MF_NOBLOCKMAP = 16,

	// Not to be activated by sound, deaf monster.
	MF_AMBUSH = 32,

	// Will try to attack right back.
	MF_JUSTHIT = 64,

	// Will take at least one step before attacking.
	MF_JUSTATTACKED = 128,

	// On level spawning (initial position),
	// hang from ceiling instead of stand on floor.
	MF_SPAWNCEILING = 256,

	// Don't apply gravity (every tic), that is, object will float,
	// keeping current height or changing it actively.
	MF_NOGRAVITY = 512,

	// Movement flags. This allows jumps from high places.
	MF_DROPOFF = 0x400,

	// For players, will pick up items.
	MF_PICKUP = 0x800,

	// Object is not checked when moving, no clipping is used.
	MF_NOCLIP = 0x1000,

	// Player: keep info about sliding along walls.
	MF_SLIDE = 0x2000,

	// Allow moves to any height, no gravity.
	// For active floaters, e.g. cacodemons, pain elementals.
	MF_FLOAT = 0x4000,

	// Instantly cross lines, whatever the height differences may be
	// (e.g. go from the bottom of a cliff to the top).
	// Note: nothing to do with teleporters.
	MF_TELEPORT = 0x8000,

	// Don't hit same species, explode on block.
	// Player missiles as well as fireballs of various kinds.
	MF_MISSILE = 0x10000,

	// Dropped by a demon, not level spawned.
	// E.g. ammo clips dropped by dying former humans.
	MF_DROPPED = 0x20000,

	// Use fuzzy draw (shadow demons or spectres),
	// temporary player invisibility powerup.
	MF_FUZZY = 0x40000,

	// Flag: don't bleed when shot (use puff),
	// barrels and shootable furniture shall not bleed.
	MF_NOBLOOD = 0x80000,

	// Don't stop moving halfway off a step,
	// that is, have dead bodies slide down all the way.
	MF_CORPSE = 0x100000,

	// Floating to a height for a move, ???
	// don't auto float to target's height.
	MF_INFLOAT = 0x200000,

	// On kill, count this enemy object
	// towards intermission kill total.
	// Happy gathering.
	MF_COUNTKILL = 0x400000,

	// On picking up, count this item object
	// towards intermission item total.
	MF_COUNTITEM = 0x800000,

	// Special handling: skull in flight.
	// Neither a cacodemon nor a missile.
	MF_SKULLFLY = 0x1000000,

	// Don't spawn this object
	// in death match mode (e.g. key cards).
	MF_NOTDMATCH = 0x2000000,

	// Monster grows (in)visible at certain times.
	MF_STEALTH = 0x4000000,

	// Used so bots know they have picked up their target item.
	MF_JUSTPICKEDUP = 0x8000000,

	// Object reacts to being touched (often violently :->)
	MF_TOUCHY = 0x10000000
}
mobjflag_t;

typedef enum
{
	// -AJA- 2004/07/22: ignore certain types of damage
	// (previously was the EF_BOSSMAN flag).
	EF_EXPLODEIMMUNE = 1,

	// Used when varying visibility levels
	EF_LESSVIS = 2,

	// This thing does not respawn
	EF_NORESPAWN = 4,

	// double the chance of object using range attack
	EF_NOGRAVKILL = 8,

	// This thing is not loyal to its own type, fights its own
	EF_DISLOYALTYPE = 16,

	// This thing can be hurt by another thing with same attack
	EF_OWNATTACKHURTS = 32,

	// Used for tracing (homing) projectiles, its the first time
	// this projectile has been checked for tracing if set.
	EF_FIRSTCHECK = 64,

	EF_UNUSED_128 = 128,   // was: NOTRACE

	// double the chance of object using range attack
	EF_TRIGGERHAPPY = 256,

	// not targeted by other monsters for damaging them
	EF_NEVERTARGET = 512,

	// Normally most monsters will follow a target which caused them
	// damage for a length of time, even if another object inflicted
	// pain upon them; with this enabled, they will not hold the grudge
	// and switch targets to the other object that has caused them the
	// more recent pain.
	EF_NOGRUDGE = 1024,

	EF_UNUSED_2048 = 2048,   // was: DUMMYMOBJ

	// Archvile cannot resurrect this monster
	EF_NORESURRECT = 4096,

	// Object bounces
	EF_BOUNCE = 8192,

	// Thing walks along the edge near large dropoffs. 
	EF_EDGEWALKER = 0x4000,

	// Monster falls with gravity when walks over cliff. 
	EF_GRAVFALL = 0x8000,

	// Thing can be climbed on-top-of or over. 
	EF_CLIMBABLE = 0x10000,

	// Thing won't penetrate WATER extra floors. 
	EF_WATERWALKER = 0x20000,

	// Thing is a monster. 
	EF_MONSTER = 0x40000,

	// Thing can cross blocking lines.
	EF_CROSSLINES = 0x80000,

	// Thing is never affected by friction
	EF_NOFRICTION = 0x100000,

	// Thing is optional, won't exist when -noextra is used.
	EF_EXTRA = 0x200000,

	// Just bounced, won't enter bounce states until BOUNCE_REARM.
	EF_JUSTBOUNCED = 0x400000,

	// Thing can be "used" (like linedefs) with the spacebar.  Thing
	// will then enter its TOUCH_STATES (when they exist).
	EF_USABLE = 0x800000,

	// Thing will block bullets and missiles.  -AJA- 2000/09/29
	EF_BLOCKSHOTS = 0x1000000,

	// Player is currently crouching.  -AJA- 2000/10/19
	EF_CROUCHING = 0x2000000,

	// Missile can tunnel through enemies.  -AJA- 2000/10/23
	EF_TUNNEL = 0x4000000,

	EF_UNUSED_8000000 = 0x8000000,   // was: DLIGHT

	// Thing has been gibbed.
	EF_GIBBED = 0x10000000,

	// -AJA- 2004/07/22: play the monster sounds at full volume
	// (separated out from the BOSSMAN flag).
	EF_ALWAYSLOUD = 0x20000000
}
mobjextendedflag_t;

typedef enum
{
	// -AJA- 2004/08/25: always pick up this item
	HF_FORCEPICKUP = 1,

	// -AJA- 2004/09/02: immune from friendly fire
	HF_SIDEIMMUNE = 2,

	// -AJA- 2005/05/15: friendly fire passes through you
	HF_SIDEGHOST = 4,

	// -AJA- 2004/09/02: don't retaliate if hurt by friendly fire
	HF_ULTRALOYAL = 8,

	// -AJA- 2005/05/14: don't update the Z buffer (particles).
	HF_NOZBUFFER = 16,

	// -AJA- 2005/05/15: the sprite hovers up and down
	HF_HOVER = 32,
}
mobjhyperflag_t;

// Directions
typedef enum
{
	DI_EAST,
	DI_NORTHEAST,
	DI_NORTH,
	DI_NORTHWEST,
	DI_WEST,
	DI_SOUTHWEST,
	DI_SOUTH,
	DI_SOUTHEAST,
	DI_NODIR,
	NUMDIRS,

	DI_SLOWTURN,
	DI_FASTTURN,
	DI_WALKING,
	DI_EVASIVE
}
dirtype_e;

typedef struct
{
	// location on the map.  `z' can take the special values ONFLOORZ
	// and ONCEILINGZ.
	float x, y, z;

	// direction thing faces
	angle_t angle;
	angle_t vertangle;

	// type of thing
	const mobjtype_c *info;

	// certain flags (mainly MF_AMBUSH).
	int flags;
}
spawnpoint_t;

// --> Spawnpoint array class
class spawnpointarray_c : public epi::array_c
{
public:
	spawnpointarray_c() : epi::array_c(sizeof(spawnpoint_t)) {}
	~spawnpointarray_c() { Clear(); }

private:
	void CleanupObject(void *obj) { /* ... */ }

public:
	int Insert(spawnpoint_t* sp) { return InsertObject((void*)sp); }
	int GetSize() { return array_entries; } 
	
	spawnpoint_t* operator[](int idx) { return (spawnpoint_t*)FetchObject(idx); } 

	spawnpoint_t* FindPlayer(int pnum, int skip = 0);
};

// Map Object definition.
typedef struct mobj_s mobj_t;

struct mobj_s
{
	// Info for drawing: position.
	// NOTE: these three fields must be first, so mobj_t can be used
	// anywhere that degenmobj_t is expected.
	float x, y, z;

	// More drawing info: to determine current sprite.
	angle_t angle;  // orientation
	angle_t vertangle;  // looking up or down

	// used to find patch_t and flip value
	int sprite;

	// frame and brightness
	short frame, bright;

	// current subsector
	struct subsector_s *subsector;

	// properties from extrafloor the thing is in
	struct region_properties_s *props;

	// The closest interval over all contacted Sectors.
	float floorz;
	float ceilingz;
	float dropoffz;

	// For movement checking.
	float radius;
	float height;

	// Momentum, used to update position.
	vec3_t mom;

	// Thing's health level
	float health;

	// This is the current speed of the object.
	// if fastparm, it is already calculated.
	float speed;
	int fuse;

	// If == validcount, already checked.
	int validcount;

	const mobjtype_c *info;

	// state tic counter
	int tics;
	int tic_skip;

	const state_t *state;
	const state_t *next_state;

	// flags (Old and New)
	int flags;
	int extendedflags;
	int hyperflags;

	// Movement direction, movement generation (zig-zagging).
	dirtype_e movedir;  // 0-7

	// when 0, select a new dir
	int movecount;

	// Reaction time: if non 0, don't attack yet.
	// Used by player to freeze a bit after teleporting.
	int reactiontime;

	// If >0, the target will be chased
	// no matter what (even if shot)
	int threshold;

	// Additional info record for player avatars only.
	struct player_s *player;

	// Player number last looked for.
	int lastlook;

	// For respawning.
	spawnpoint_t spawnpoint;

	float origheight;

	// current visibility and target visibility
	float visibility;
	float vis_target;

	// current attack to be made
	const atkdef_c *currentattack;

	// spread count for Ordered spreaders
	int spreadcount;

	// -ES- 1999/10/25 Reference Count. DO NOT TOUCH.
	// All the following mobj references should be set only
	// through P_MobjSetX, where X is the field name. This is useful because
	// it sets the pointer to NULL if the mobj is removed, this protects us
	// from a crash.
	int refcount;

	// source of the mobj, used for projectiles (i.e. the shooter)
	mobj_t * source;

	// target of the mobj
	mobj_t * target;

	// current spawned fire of the mobj
	mobj_t * tracer;

	// if exists, we are supporting/helping this object
	mobj_t * supportobj;
	int side;

	// objects that is above and below this one.  If there were several,
	// then the closest one (in Z) is chosen.  We are riding the below
	// object if the head height == our foot height.  We are being
	// ridden if our head == the above object's foot height.
	//
	mobj_t * above_mo;
	mobj_t * below_mo;

	// these delta values give what position from the ride_em thing's
	// center that we are sitting on.
	float ride_dx, ride_dy;

	// -AJA- 1999/09/25: Path support.
	struct rad_script_s *path_trigger;

	// if we're on a ladder, this is the linedef #, otherwise -1.
	int on_ladder;

	float dlight_qty;
	float dlight_target;

	// monster reload support: count the number of shots
	int shot_count;

	// hash values for TUNNEL missiles
	unsigned long tunnel_hash[2];

	// touch list: sectors this thing is in or touches
	struct touch_node_s *touch_sectors;

	// linked list (mobjlisthead)
	mobj_t *next, *prev;

	// Interaction info, by BLOCKMAP.
	// Links in blocks (if needed).
	mobj_t *bnext, *bprev;

	// More list: links in subsector (if needed)
	mobj_t *snext, *sprev;

	// One more: link in dynamic light blockmap
	mobj_t *dlnext, *dlprev;
};

// Item-in-Respawn-que Structure -ACB- 1998/07/30
typedef struct iteminque_s
{
	spawnpoint_t spawnpoint;
	int time;
	struct iteminque_s *next;
	struct iteminque_s *prev;
}
iteminque_t;

// useful macro for the vertical center of an object
#define MO_MIDZ(mo)  ((mo)->z + (mo)->height / 2)

#endif  // __P_MOBJ__
