//----------------------------------------------------------------------------
//  EDGE Data
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
// DESCRIPTION:
//  all external data is defined here
//  most of the data is loaded into different structures at run time
//  some internal structures shared by many modules are here
//

#ifndef __DOOMDATA__
#define __DOOMDATA__

// The most basic types we use, portability.
#include "dm_type.h"

// Some global defines, that configure the game.
#include "dm_defs.h"

//
// Map level types.
// The following data structures define the persistent format
// used in the lumps of the WAD files.
//

// Lump order in a map WAD: each map needs a couple of lumps
// to provide a complete scene geometry description.
enum
{
   ML_LABEL=0,   // A separator name, ExMx or MAPxx
   ML_THINGS,    // Monsters, items..
   ML_LINEDEFS,  // LineDefs, from editing
   ML_SIDEDEFS,  // SideDefs, from editing
   ML_VERTEXES,  // Vertices, edited and BSP splits generated
   ML_SEGS,      // LineSegs, from LineDefs split by BSP
   ML_SSECTORS,  // SubSectors, list of LineSegs
   ML_NODES,     // BSP nodes
   ML_SECTORS,   // Sectors, from editing
   ML_REJECT,    // LUT, sector-sector visibility 
   ML_BLOCKMAP   // LUT, motion clipping, walls/grid element
};

// -AJA- 1999/12/20: Lump order from "GL-Friendly Nodes" specs.
enum
{
   ML_GL_LABEL=0,  // A separator name, GL_ExMx or GL_MAPxx
   ML_GL_VERT,     // Extra Vertices
   ML_GL_SEGS,     // Segs, from linedefs & minisegs
   ML_GL_SSECT,    // SubSectors, list of segs
   ML_GL_NODES     // GL BSP nodes
};

// A single Vertex.
typedef struct
{
  short x;
  short y;
}
mapvertex_t;

// A SideDef, defining the visual appearance of a wall,
// by setting textures and offsets.
typedef struct
{
  short textureoffset;
  short rowoffset;
  char toptexture[8];
  char bottomtexture[8];
  char midtexture[8];
  // Front sector, towards viewer.
  short sector;
}
mapsidedef_t;

// A LineDef, as used for editing, and as input
// to the BSP builder.
typedef struct
{
  short v1;
  short v2;
  short flags;
  short special;
  short tag;
  // sidenum[1] will be -1 if one sided
  short sidenum[2];
}
maplinedef_t;

//
// LineDef attributes.
//

typedef enum
{
  // Solid, is an obstacle.
  ML_Blocking = 0x0001,

  // Blocks monsters only.
  ML_BlockMonsters = 0x0002,

  // Backside will not be present at all if not two sided.
  ML_TwoSided = 0x0004,

  // If a texture is pegged, the texture will have
  // the end exposed to air held constant at the
  // top or bottom of the texture (stairs or pulled
  // down things) and will move with a height change
  // of one of the neighbor sectors.
  // Unpegged textures allways have the first row of
  // the texture at the top pixel of the line for both
  // top and bottom textures (use next to windows).

  // upper texture unpegged
  ML_UpperUnpegged = 0x0008,

  // lower texture unpegged
  ML_LowerUnpegged = 0x0010,

  // In AutoMap: don't map as two sided: IT'S A SECRET!
  ML_Secret = 0x0020,

  // Sound rendering: don't let sound cross two of these.
  ML_SoundBlock = 0x0040,

  // Don't draw on the automap at all.
  ML_DontDraw = 0x0080,

  // Set if already seen, thus drawn in automap.
  ML_Mapped = 0x0100,

  // -AJA- 1999/08/16: This one is from Boom. Allows multiple lines to
  //       be pushed simultaneously.
  ML_PassThru = 0x0200,

  // -AJA- These three from XDoom.  Translucent one doesn't do
  //       anything at present.
  ML_Translucent = 0x0400,
  ML_ShootBlock  = 0x0800,
  ML_SightBlock  = 0x1000,

  // --- internal flags ---

}
lineflag_e;

// Sector definition, from editing.
typedef struct
{
  short floorheight;
  short ceilingheight;
  char floorpic[8];
  char ceilingpic[8];
  short lightlevel;
  short special;
  short tag;
}
mapsector_t;

// SubSector, as generated by BSP.
typedef struct
{
  short numsegs;
  // Index of first one, segs are stored sequentially.
  short firstseg;
}
mapsubsector_t;

// LineSeg, generated by splitting LineDefs
// using partition lines selected by BSP builder.
typedef struct mapseg_t
{
  short v1;
  short v2;
  short angle;
  short linedef;
  short side;
  short offset;
}
mapseg_t;

// -AJA- 1999/12/20: New kind of seg, contained in the GL_SEGS lump
//       and conforming to the "GL-friendly Nodes" specifications.

// Indicates a GL-specific vertex
#define	SF_GL_VERTEX	0x8000

typedef struct map_glseg_s
{
  // start & end vertices
  unsigned short v1;
  unsigned short v2;

  // linedef, or -1 for minisegs
  short linedef;

  // side on linedef: 0 for right, 1 for left
  short side;

  // corresponding partner seg, or -1 on one-sided walls
  short partner;
}
map_glseg_t;

// -AJA- 2000/07/01: New kind of vertex for V2.0 of the "GL Nodes"
//      specifications.  These vertices are in 16.16 fixed point.
typedef struct
{
  long x;
  long y;
}
map_gl2vertex_t;

// BSP node structure.

// Indicate a leaf.
#define	NF_SUBSECTOR	0x8000

typedef struct
{
  // Partition line from (x,y) to x+dx,y+dy)
  short x;
  short y;
  short dx;
  short dy;

  // Bounding box for each child,
  // clip against view frustum.
  short bbox[2][4];

  // If NF_SUBSECTOR its a subsector,
  // else it's a node of another subtree.
  unsigned short children[2];

}
mapnode_t;

// Thing definition, position, orientation and type,
// plus skill/visibility flags and attributes.
typedef struct
{
  short x;
  short y;
  short angle;
  short type;
  short options;
}
mapthing_t;

// Wad header definition
typedef struct wad_header_s
{
  // should be "IWAD" or "PWAD".
  char identification[4];
  long numlumps;
  long infotableofs;
}
wad_header_t;

// Wad table entry
typedef struct wad_entry_s
{
  long pos;
  long size;
  char name[8];
}
wad_entry_t;

// Patches.
//
// A patch holds one or more columns.
// Patches are used for sprites and all masked pictures,
// and we compose textures from the TEXTURE1/2 lists
// of patches.
//
typedef struct patch_s
{
  // bounding box size 
  short width;
  short height;

  // pixels to the left of origin 
  short leftoffset;

  // pixels below the origin 
  short topoffset;

  int columnofs[1];  // only [width] used
}
patch_t;

//
// Texture definition.
//
// Each texture is composed of one or more patches,
// with patches being lumps stored in the WAD.
//
// The lumps are referenced by number, and patched into the rectangular
// texture space using origin and possibly other attributes.
//
typedef struct
{
  short originx;
  short originy;
  short patch;
  short stepdir;
  short colourmap;
}
mappatch_t;

//
// Texture definition.
//
// A DOOM wall texture is a list of patches which are to be combined in a
// predefined order.
//
// Removing the obsolete columndirectory fails because this defines how a
// texture is stored in the wad file.
//
typedef struct
{
  char name[8];
  int masked;
  short width;
  short height;
  int columndirectory;
  short patchcount;
  mappatch_t patches[1];
}
maptexture_t;

#endif // __DOOMDATA__
