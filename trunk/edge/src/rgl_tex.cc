//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Textures)
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

// this conditional applies to the whole file
#ifdef USE_GL

#include "i_defs.h"

#include "dm_defs.h"
#include "dm_state.h"
#include "e_search.h"
#include "m_bbox.h"
#include "m_random.h"
#include "p_local.h"
#include "p_mobj.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_sky.h"
#include "r_state.h"
#include "r_things.h"
#include "r2_defs.h"
#include "rgl_defs.h"
#include "v_colour.h"
#include "v_res.h"
#include "w_textur.h"
#include "w_wad.h"
#include "z_zone.h"

/* NOTE: texture handling now done by image system */

/* --- AJA: test for a logo --- */

static const int logo_w = 228;
static const int logo_h = 58;

static const byte logo_data[] =
{
	228,166,
	153,166, 1,180, 1,166, 3,130, 8,40, 4,130, 79,166, 26,130, 
	54,166, 2,180, 16,166, 5,180, 30,166, 3,130, 10,40, 3,130, 
	11,166, 1,180, 62,166, 45,130, 4,166, 38,130, 1,166, 4,180, 
	9,166, 1,180, 2,166, 34,130, 2,166, 1,130, 11,40, 2,130, 
	1,166, 20,130, 5,166, 6,130, 41,166, 89,130, 3,166, 3,180, 
	8,166, 36,130, 1,166, 2,130, 11,40, 3,166, 44,130, 27,166, 
	5,130, 40,40, 8,130, 35,40, 4,130, 3,166, 3,180, 1,166, 
	2,180, 1,166, 4,130, 35,40, 1,130, 11,40, 2,130, 1,166, 
	3,130, 39,40, 4,130, 25,166, 4,130, 87,40, 4,130, 3,166, 
	1,180, 2,166, 4,130, 43,40, 2,166, 4,40, 2,130, 46,40, 
	2,130, 23,166, 3,130, 91,40, 3,130, 3,166, 4,130, 45,40, 
	2,166, 53,40, 3,130, 21,166, 2,130, 93,40, 3,130, 1,166, 
	3,130, 47,40, 1,130, 1,166, 54,40, 3,130, 19,166, 3,130, 
	94,40, 5,130, 48,40, 1,130, 1,166, 4,40, 1,130, 50,40, 
	3,130, 1,180, 17,166, 1,130, 1,166, 1,130, 95,40, 4,130, 
	48,40, 1,130, 1,166, 4,40, 1,130, 50,40, 3,130, 20,166, 
	1,130, 48,40, 2,130, 97,40, 2,166, 4,40, 1,166, 50,40, 
	1,130, 3,166, 1,180, 14,166, 1,180, 3,166, 1,130, 4,40, 
	1,130, 4,166, 38,130, 1,40, 2,130, 2,40, 1,130, 4,166, 
	32,130, 16,40, 41,130, 1,40, 2,166, 4,40, 1,166, 4,40, 
	1,130, 4,166, 37,130, 4,40, 1,130, 3,166, 1,180, 14,166, 
	1,180, 1,130, 1,166, 2,130, 5,40, 1,166, 1,180, 3,166, 
	35,180, 1,166, 1,130, 1,40, 2,130, 2,40, 1,130, 5,166, 
	30,180, 1,166, 1,130, 14,40, 1,130, 1,166, 39,180, 1,166, 
	1,130, 1,166, 1,130, 4,40, 1,166, 4,40, 1,130, 1,180, 
	3,166, 35,180, 2,166, 2,130, 2,40, 1,166, 1,130, 1,166, 
	1,130, 16,166, 1,130, 2,166, 1,130, 5,40, 1,166, 39,40, 
	3,130, 1,166, 3,40, 1,130, 1,166, 33,40, 1,130, 1,180, 
	1,166, 1,130, 12,40, 1,130, 1,180, 1,166, 38,40, 1,130, 
	1,166, 2,180, 1,130, 4,40, 1,166, 4,40, 2,130, 38,40, 
	4,130, 1,40, 2,166, 1,130, 1,166, 1,130, 14,166, 1,180, 
	1,166, 1,130, 1,166, 2,130, 4,40, 1,130, 1,166, 1,130, 
	37,40, 5,130, 3,40, 1,130, 1,166, 6,40, 25,130, 3,40, 
	1,130, 1,180, 1,166, 1,130, 10,40, 1,130, 1,180, 1,166, 
	1,130, 2,40, 6,130, 32,40, 1,130, 1,166, 1,130, 4,40, 
	1,166, 4,40, 2,166, 36,40, 7,130, 1,166, 2,130, 1,166, 
	1,130, 14,166, 1,180, 1,166, 4,130, 4,40, 1,130, 1,180, 
	1,130, 3,40, 38,130, 4,40, 1,130, 1,180, 4,40, 30,130, 
	1,40, 1,130, 1,180, 1,166, 1,130, 8,40, 1,130, 2,166, 
	1,130, 1,40, 37,130, 4,40, 2,130, 4,40, 1,130, 4,40, 
	2,166, 4,40, 37,130, 3,166, 2,130, 1,166, 1,130, 14,166, 
	1,180, 1,166, 2,130, 6,40, 1,130, 1,180, 5,40, 29,130, 
	7,166, 1,130, 4,40, 1,130, 1,180, 5,40, 26,166, 4,130, 
	1,40, 1,130, 2,166, 1,130, 6,40, 1,130, 1,180, 1,166, 
	2,40, 4,130, 5,166, 1,130, 10,40, 18,130, 4,40, 1,130, 
	1,166, 4,40, 1,130, 4,40, 2,166, 5,40, 29,130, 8,166, 
	3,130, 1,166, 2,130, 1,180, 12,166, 1,180, 1,166, 3,130, 
	6,40, 1,130, 1,180, 36,40, 1,130, 4,166, 1,130, 4,40, 
	1,130, 1,180, 5,40, 2,130, 25,166, 2,130, 4,40, 1,180, 
	1,166, 2,40, 2,130, 1,40, 1,130, 2,166, 3,40, 3,130, 
	5,166, 1,130, 33,40, 1,130, 1,166, 9,40, 2,166, 36,40, 
	1,130, 4,166, 1,130, 1,40, 1,130, 2,166, 1,130, 1,166, 
	1,180, 13,166, 3,130, 7,40, 1,130, 1,180, 37,40, 4,166, 
	5,40, 1,130, 1,180, 5,40, 2,130, 22,166, 1,130, 3,166, 
	1,130, 4,40, 1,130, 1,166, 2,40, 2,130, 1,40, 1,130, 
	1,166, 5,40, 1,130, 2,166, 1,130, 3,166, 1,130, 33,40, 
	1,130, 1,166, 9,40, 2,166, 36,40, 1,130, 2,166, 4,130, 
	2,166, 1,130, 14,166, 1,180, 1,166, 2,130, 8,40, 1,130, 
	1,180, 38,40, 3,130, 5,40, 1,130, 1,180, 5,40, 2,130, 
	1,166, 22,130, 1,40, 1,130, 1,166, 1,130, 4,40, 1,130, 
	1,166, 2,40, 2,130, 1,40, 1,130, 1,166, 5,40, 1,130, 
	1,166, 2,40, 4,130, 33,40, 1,130, 1,166, 9,40, 2,166, 
	37,40, 3,130, 3,166, 2,130, 15,166, 1,180, 3,130, 8,40, 
	1,130, 1,180, 38,40, 2,166, 2,130, 4,40, 1,130, 1,180, 
	5,40, 2,130, 2,166, 20,130, 2,166, 3,130, 4,40, 1,130, 
	1,166, 2,40, 2,130, 1,40, 1,130, 1,166, 5,40, 2,130, 
	1,40, 2,130, 2,166, 1,130, 5,40, 1,166, 1,130, 26,40, 
	2,166, 2,40, 1,130, 6,40, 2,166, 37,40, 1,130, 3,166, 
	3,130, 15,166, 1,180, 1,166, 1,130, 1,166, 1,130, 8,40, 
	1,130, 1,166, 1,130, 37,40, 2,166, 2,130, 4,40, 1,130, 
	1,180, 5,40, 2,130, 22,166, 1,130, 2,166, 2,130, 4,40, 
	1,130, 1,166, 2,40, 2,130, 1,40, 1,130, 1,166, 5,40, 
	4,130, 1,166, 3,130, 4,40, 1,130, 1,166, 1,130, 26,40, 
	2,166, 1,40, 2,130, 6,40, 2,166, 37,40, 1,130, 4,166, 
	3,180, 14,166, 1,180, 1,130, 2,166, 1,130, 5,40, 4,130, 
	1,166, 3,130, 29,40, 1,130, 4,40, 1,130, 2,166, 1,130, 
	5,40, 1,130, 1,180, 5,40, 5,130, 19,166, 1,130, 3,166, 
	1,130, 4,40, 1,130, 1,166, 2,40, 2,130, 1,40, 1,130, 
	1,166, 5,40, 1,166, 1,130, 2,166, 1,180, 1,166, 1,130, 
	1,166, 5,40, 2,166, 2,130, 22,40, 1,130, 1,166, 1,180, 
	4,130, 2,40, 4,130, 2,166, 3,130, 28,40, 1,130, 5,40, 
	1,130, 2,166, 1,130, 1,180, 18,166, 1,130, 3,166, 2,40, 
	1,130, 1,40, 1,130, 4,166, 1,180, 32,166, 2,130, 3,40, 
	1,130, 2,166, 1,130, 5,40, 1,130, 1,180, 5,40, 1,166, 
	4,130, 1,166, 1,180, 17,166, 1,130, 1,166, 1,130, 1,166, 
	1,130, 4,40, 1,130, 1,166, 2,40, 2,130, 1,40, 1,130, 
	1,166, 5,40, 1,166, 1,130, 6,166, 5,40, 1,166, 1,180, 
	25,166, 1,180, 1,166, 4,130, 1,40, 1,130, 4,166, 1,180, 
	33,166, 1,130, 1,40, 1,130, 2,40, 1,166, 1,130, 1,166, 
	1,130, 1,180, 18,166, 1,130, 1,166, 1,130, 1,166, 1,130, 
	1,40, 2,130, 2,166, 7,130, 30,166, 2,130, 2,40, 1,166, 
	1,130, 1,166, 1,130, 5,40, 1,130, 1,180, 5,40, 1,166, 
	4,130, 1,166, 1,180, 17,166, 1,130, 1,166, 1,130, 1,166, 
	1,130, 4,40, 1,130, 1,166, 2,40, 2,130, 1,40, 1,130, 
	1,166, 5,40, 1,166, 1,130, 4,166, 1,130, 1,166, 5,40, 
	2,166, 1,130, 24,166, 1,130, 1,40, 4,130, 1,40, 1,130, 
	2,166, 6,130, 30,166, 2,130, 2,40, 1,130, 1,166, 1,130, 
	1,166, 1,130, 1,180, 18,166, 1,130, 1,166, 2,130, 1,166, 
	4,130, 4,40, 1,166, 32,40, 3,130, 1,40, 1,130, 1,166, 
	1,130, 1,166, 2,130, 4,40, 1,130, 1,180, 5,40, 1,166, 
	4,130, 1,166, 1,180, 17,166, 1,130, 1,166, 1,130, 1,166, 
	1,130, 4,40, 1,130, 1,166, 2,40, 2,130, 1,40, 1,130, 
	1,166, 5,40, 1,166, 1,130, 4,166, 1,130, 1,166, 1,130, 
	1,40, 1,130, 1,40, 1,130, 1,166, 1,130, 26,40, 7,130, 
	4,40, 2,130, 32,40, 4,130, 1,166, 2,130, 1,166, 1,130, 
	1,180, 18,166, 1,130, 1,166, 2,130, 2,166, 3,130, 3,40, 
	1,130, 1,180, 1,130, 3,40, 32,130, 2,166, 1,130, 1,166, 
	2,130, 4,40, 1,130, 1,180, 5,40, 1,166, 4,130, 1,166, 
	1,180, 17,166, 1,130, 1,166, 1,130, 1,166, 1,130, 4,40, 
	1,130, 1,166, 2,40, 2,130, 1,40, 1,130, 1,166, 5,40, 
	1,166, 1,130, 6,166, 1,130, 1,40, 3,130, 1,166, 1,130, 
	1,40, 28,130, 2,166, 2,130, 4,40, 2,166, 4,40, 31,130, 
	2,166, 2,130, 1,166, 1,130, 19,166, 2,130, 1,166, 2,130, 
	2,166, 1,130, 4,40, 1,130, 1,180, 1,130, 3,40, 28,130, 
	5,166, 1,130, 2,166, 2,130, 4,40, 1,130, 1,180, 5,40, 
	1,166, 4,130, 19,166, 1,130, 1,166, 1,130, 1,166, 1,130, 
	4,40, 1,130, 1,166, 2,40, 2,130, 1,40, 1,130, 1,166, 
	5,40, 1,166, 1,130, 4,166, 2,130, 1,166, 1,130, 1,40, 
	2,130, 1,166, 27,130, 5,166, 1,130, 5,40, 2,166, 4,40, 
	28,130, 4,166, 2,130, 1,166, 2,130, 1,180, 17,166, 1,180, 
	1,166, 1,130, 2,166, 1,40, 1,130, 1,166, 1,130, 4,40, 
	1,130, 1,180, 5,40, 1,130, 28,166, 2,130, 1,40, 1,130, 
	2,166, 2,130, 4,40, 1,130, 1,180, 5,40, 1,166, 4,130, 
	1,166, 1,180, 17,166, 1,130, 1,166, 1,130, 1,166, 1,130, 
	4,40, 1,130, 1,166, 2,40, 2,130, 1,40, 1,130, 1,166, 
	5,40, 1,166, 1,130, 2,166, 1,180, 2,166, 1,130, 2,166, 
	7,130, 29,166, 1,130, 5,40, 2,166, 4,40, 1,130, 29,166, 
	2,130, 1,40, 2,166, 1,130, 1,166, 1,180, 18,166, 1,180, 
	1,166, 1,130, 1,166, 1,130, 1,40, 2,130, 4,40, 1,130, 
	1,180, 5,40, 1,130, 4,166, 18,180, 6,166, 2,40, 2,130, 
	1,166, 3,130, 4,40, 1,130, 1,180, 5,40, 1,166, 4,130, 
	1,166, 1,180, 15,166, 1,180, 1,166, 1,130, 1,166, 1,130, 
	1,166, 1,130, 4,40, 1,130, 1,166, 2,40, 2,130, 1,40, 
	1,130, 1,166, 5,40, 1,166, 1,130, 2,166, 1,180, 2,166, 
	2,130, 3,166, 3,130, 3,166, 1,130, 3,166, 14,180, 6,166, 
	1,130, 2,40, 2,130, 5,40, 2,166, 5,40, 1,166, 1,130, 
	2,166, 18,180, 6,166, 1,130, 2,40, 1,130, 1,166, 1,130, 
	1,166, 1,180, 12,166, 1,180, 8,166, 1,130, 2,166, 3,130, 
	4,40, 1,130, 1,180, 5,40, 3,130, 23,166, 1,130, 1,40, 
	3,130, 3,166, 3,130, 4,40, 1,130, 1,180, 5,40, 1,166, 
	4,130, 1,166, 1,180, 15,166, 1,180, 1,166, 1,130, 1,166, 
	1,130, 1,166, 1,130, 4,40, 1,130, 1,166, 2,40, 2,130, 
	1,40, 1,130, 1,166, 5,40, 1,166, 1,130, 2,166, 1,180, 
	1,130, 2,166, 3,130, 5,166, 4,130, 17,166, 3,130, 1,40, 
	4,130, 1,40, 1,130, 5,40, 2,166, 5,40, 1,166, 1,130, 
	23,166, 1,130, 2,40, 3,130, 1,166, 2,130, 1,180, 23,166, 
	1,130, 2,166, 2,130, 4,40, 1,130, 1,180, 5,40, 28,130, 
	4,166, 1,130, 1,166, 3,130, 4,40, 1,130, 1,180, 5,40, 
	1,166, 4,130, 1,166, 1,180, 17,166, 1,130, 1,166, 1,130, 
	1,166, 1,130, 4,40, 1,130, 1,166, 2,40, 2,130, 1,40, 
	1,130, 1,166, 5,40, 1,166, 1,130, 2,166, 1,180, 1,166, 
	1,130, 1,166, 1,130, 2,40, 2,130, 1,166, 2,130, 2,40, 
	23,130, 4,166, 2,130, 5,40, 2,166, 5,40, 27,130, 4,166, 
	2,130, 24,166, 1,180, 3,166, 2,130, 4,40, 1,130, 1,180, 
	5,40, 3,130, 1,166, 26,130, 1,40, 2,130, 1,166, 3,130, 
	4,40, 1,130, 1,180, 5,40, 5,130, 1,166, 3,180, 1,166, 
	3,180, 2,166, 7,180, 2,166, 1,130, 1,166, 1,130, 1,166, 
	1,130, 4,40, 1,130, 1,166, 2,40, 2,130, 1,40, 1,130, 
	1,166, 5,40, 1,166, 1,130, 1,166, 1,130, 1,180, 1,166, 
	2,130, 1,166, 9,130, 2,166, 16,130, 4,40, 2,130, 2,166, 
	2,130, 1,166, 1,130, 4,40, 2,166, 4,40, 3,130, 1,166, 
	26,130, 2,40, 1,130, 1,166, 1,180, 23,166, 1,180, 3,166, 
	2,130, 4,40, 1,130, 1,180, 5,40, 4,130, 25,40, 1,130, 
	2,166, 1,130, 1,166, 3,130, 4,40, 1,130, 1,166, 5,40, 
	5,130, 19,166, 1,130, 1,166, 3,130, 4,40, 1,130, 1,166, 
	2,40, 2,130, 1,40, 1,130, 1,166, 5,40, 4,130, 2,180, 
	1,166, 1,130, 4,166, 3,130, 4,166, 12,40, 2,130, 1,166, 
	9,40, 3,130, 1,166, 5,40, 2,166, 4,40, 5,130, 24,40, 
	1,130, 3,166, 2,180, 23,166, 1,180, 1,166, 1,130, 1,166, 
	2,130, 4,40, 1,130, 1,180, 5,40, 3,130, 26,166, 2,180, 
	1,166, 1,130, 1,166, 3,130, 4,40, 1,130, 1,166, 5,40, 
	5,130, 19,166, 3,130, 6,40, 1,130, 1,166, 2,40, 2,130, 
	1,40, 1,130, 1,166, 6,40, 3,130, 4,166, 3,130, 5,166, 
	3,130, 14,166, 1,130, 9,40, 1,130, 1,166, 1,130, 1,166, 
	5,40, 2,166, 4,40, 3,130, 1,166, 1,130, 24,166, 2,180, 
	4,166, 2,180, 1,166, 2,180, 18,166, 1,180, 1,166, 1,130, 
	1,166, 2,130, 4,40, 1,130, 1,180, 5,40, 3,130, 28,166, 
	2,130, 1,166, 3,130, 4,40, 1,130, 1,180, 5,40, 3,130, 
	1,166, 19,130, 3,166, 1,130, 6,40, 1,130, 1,166, 2,40, 
	2,130, 1,40, 1,130, 1,166, 6,40, 6,130, 6,166, 1,180, 
	18,166, 1,130, 11,40, 3,166, 5,40, 2,166, 4,40, 3,130, 
	27,166, 6,130, 4,166, 1,180, 17,166, 1,180, 1,166, 1,130, 
	1,166, 2,130, 4,40, 1,130, 1,180, 5,40, 1,166, 2,130, 
	1,166, 33,130, 4,40, 1,130, 1,166, 5,40, 3,130, 1,166, 
	20,130, 1,166, 2,130, 6,40, 1,130, 1,166, 2,40, 2,130, 
	1,40, 1,130, 1,166, 7,40, 2,130, 1,166, 28,130, 11,40, 
	1,130, 2,166, 5,40, 2,166, 5,40, 1,166, 1,130, 1,166, 
	35,130, 19,166, 1,180, 1,166, 1,130, 1,166, 2,130, 4,40, 
	1,130, 1,180, 5,40, 3,130, 32,40, 2,130, 4,40, 1,130, 
	1,166, 6,40, 2,130, 30,40, 1,130, 1,166, 4,130, 1,40, 
	1,130, 1,166, 49,40, 3,166, 5,40, 2,166, 5,40, 3,130, 
	32,40, 4,130, 2,166, 1,180, 15,166, 1,180, 1,166, 1,130, 
	1,166, 2,130, 4,40, 1,130, 1,180, 46,40, 1,130, 1,166, 
	38,40, 2,166, 4,130, 1,40, 1,130, 1,180, 1,130, 41,40, 
	1,130, 1,166, 1,130, 4,40, 1,130, 2,166, 5,40, 2,166, 
	42,40, 4,130, 1,166, 3,180, 12,166, 1,180, 1,166, 1,130, 
	1,166, 2,130, 4,40, 1,130, 1,180, 46,40, 1,130, 1,180, 
	37,40, 1,130, 1,180, 1,166, 2,130, 1,166, 1,130, 1,40, 
	1,130, 1,180, 1,166, 41,40, 1,130, 1,166, 1,130, 4,40, 
	1,130, 1,180, 1,130, 5,40, 2,166, 43,40, 3,130, 2,166, 
	3,180, 11,166, 1,180, 1,166, 1,130, 2,166, 1,130, 4,40, 
	1,130, 1,180, 43,40, 1,130, 2,40, 1,130, 1,180, 1,130, 
	35,40, 1,130, 1,180, 1,166, 1,40, 7,130, 1,180, 1,166, 
	40,40, 1,130, 1,180, 1,130, 4,40, 1,130, 1,180, 1,130, 
	5,40, 2,166, 44,40, 3,130, 1,166, 4,180, 10,166, 1,180, 
	1,166, 1,130, 3,166, 4,40, 1,130, 1,180, 1,130, 42,40, 
	1,166, 2,40, 1,130, 1,180, 1,166, 34,40, 1,130, 2,166, 
	2,40, 6,130, 1,40, 1,166, 1,180, 1,166, 39,40, 1,130, 
	1,180, 1,130, 1,40, 1,130, 1,40, 4,130, 5,40, 2,166, 
	44,40, 3,130, 2,166, 3,180, 10,166, 1,180, 1,166, 1,130, 
	1,166, 1,130, 1,166, 2,40, 1,130, 1,40, 1,130, 1,180, 
	1,130, 42,40, 2,130, 1,40, 2,130, 1,180, 1,130, 32,40, 
	1,130, 1,166, 1,180, 1,130, 1,40, 3,130, 1,166, 3,130, 
	2,40, 1,166, 1,180, 1,130, 37,40, 1,130, 1,180, 1,166, 
	1,40, 2,130, 1,40, 2,166, 1,40, 2,166, 4,40, 2,166, 
	44,40, 1,130, 1,166, 2,130, 1,166, 3,180, 10,166, 1,180, 
	1,166, 1,130, 1,166, 1,130, 1,166, 1,130, 1,40, 3,130, 
	2,166, 1,130, 42,40, 1,166, 2,130, 1,40, 1,166, 1,180, 
	1,130, 30,40, 1,130, 1,166, 1,180, 1,130, 1,40, 1,130, 
	1,40, 1,130, 3,166, 4,130, 1,40, 1,130, 1,180, 1,166, 
	34,40, 1,130, 1,166, 1,180, 1,166, 1,40, 2,130, 1,40, 
	1,130, 1,166, 3,130, 1,166, 2,40, 1,130, 1,40, 1,130, 
	1,180, 1,130, 43,40, 1,130, 2,166, 1,130, 2,166, 2,180, 
	12,166, 1,130, 4,166, 2,40, 1,130, 1,40, 1,130, 1,180, 
	1,166, 39,130, 3,40, 1,166, 2,130, 2,40, 1,166, 1,180, 
	1,166, 28,130, 1,166, 2,180, 1,130, 1,40, 3,130, 5,166, 
	3,130, 2,40, 1,166, 1,180, 1,166, 33,130, 1,166, 1,180, 
	1,130, 1,40, 2,130, 1,40, 1,130, 2,166, 1,130, 1,166, 
	1,130, 1,166, 1,130, 1,40, 2,130, 1,40, 1,166, 1,180, 
	1,166, 38,130, 4,40, 2,130, 1,166, 1,130, 2,166, 3,180, 
	10,166, 1,180, 1,130, 1,166, 2,130, 1,166, 1,130, 1,40, 
	2,130, 1,40, 1,130, 1,166, 37,180, 1,166, 1,130, 1,40, 
	1,130, 1,40, 4,130, 2,40, 1,130, 1,166, 29,180, 1,166, 
	1,130, 1,40, 3,130, 1,166, 1,180, 3,130, 2,166, 3,130, 
	2,40, 2,166, 33,180, 1,166, 1,130, 2,40, 1,130, 1,40, 
	1,130, 2,166, 1,130, 2,166, 1,130, 2,166, 1,130, 1,40, 
	1,130, 1,40, 1,130, 1,166, 38,180, 1,166, 1,40, 1,130, 
	2,40, 1,166, 1,130, 1,166, 1,130, 2,166, 3,180, 1,166, 
	1,180, 8,166, 1,180, 2,130, 1,166, 1,130, 1,166, 1,180, 
	4,130, 39,40, 2,130, 3,40, 2,166, 3,130, 34,40, 3,130, 
	2,166, 1,130, 3,40, 1,130, 2,166, 3,130, 1,40, 1,130, 
	35,40, 1,130, 1,40, 3,130, 2,166, 2,130, 2,166, 2,130, 
	2,166, 3,130, 39,40, 4,130, 1,40, 1,130, 1,166, 1,130, 
	1,166, 1,130, 2,166, 3,180, 10,166, 1,180, 1,166, 1,130, 
	1,166, 2,130, 2,166, 47,130, 3,166, 38,130, 2,166, 1,130, 
	1,40, 1,130, 2,166, 1,40, 1,130, 2,166, 43,130, 2,166, 
	1,130, 1,40, 3,166, 3,130, 2,166, 46,130, 2,166, 1,130, 
	1,166, 1,130, 2,166, 3,180, 11,166, 1,180, 4,130, 1,40, 
	3,166, 43,130, 7,166, 34,130, 3,166, 1,130, 1,40, 1,130, 
	4,166, 2,130, 3,166, 39,130, 3,166, 2,40, 2,166, 1,130, 
	2,166, 3,130, 3,166, 43,130, 2,166, 2,130, 1,166, 1,130, 
	2,166, 3,180, 14,166, 2,130, 2,40, 1,130, 45,166, 1,130, 
	3,40, 37,166, 2,130, 1,40, 3,130, 1,40, 1,130, 2,166, 
	3,130, 41,166, 1,130, 1,40, 1,130, 2,166, 3,130, 2,166, 
	1,130, 1,40, 2,130, 43,166, 1,180, 1,166, 2,130, 1,166, 
	2,130, 2,166, 3,180, 12,166, 2,180, 1,166, 3,130, 2,40, 
	1,130, 42,166, 4,130, 3,40, 34,166, 1,130, 2,40, 3,130, 
	2,166, 2,130, 2,166, 3,40, 39,166, 3,40, 2,166, 2,130, 
	2,166, 1,40, 2,166, 1,130, 2,40, 1,130, 43,166, 2,130, 
	2,166, 1,130, 2,166, 3,180, 15,166, 1,180, 3,166, 2,130, 
	1,40, 3,130, 35,166, 2,130, 2,40, 1,130, 3,166, 7,130, 
	26,166, 9,130, 4,166, 2,130, 2,166, 6,130, 31,166, 7,130, 
	1,166, 1,130, 1,40, 3,166, 1,130, 1,40, 2,166, 7,130, 
	35,166, 2,130, 2,40, 1,130, 2,166, 1,130, 2,166, 4,180, 
	15,166, 1,180, 1,166, 2,130, 3,166, 42,130, 2,166, 3,130, 
	3,166, 33,130, 3,166, 1,130, 2,166, 2,180, 2,166, 1,130, 
	1,40, 3,166, 38,130, 3,166, 1,130, 1,40, 1,130, 2,166, 
	1,180, 2,166, 1,130, 2,40, 1,130, 2,166, 42,130, 2,166, 
	3,130, 1,166, 5,180, 16,166, 2,180, 1,166, 46,130, 1,40, 
	42,130, 2,166, 4,180, 2,166, 2,130, 2,40, 43,130, 3,166, 
	2,180, 2,166, 48,130, 1,40, 1,130, 3,166, 4,180, 19,166, 
	3,180, 143,166, 6,180, 51,166, 4,180, 231,166, 230,166, 
	
	0, 0  // THE END
};

static const int init_w = 216;
static const int init_h = 27;

static const byte init_data[] =
{
	200,0, 200,0, 200,0, 213,0, 1,28, 3,85, 1,28, 
	211,0, 1,226, 4,255, 1,198, 1,28, 209,0, 4,85, 1,141, 
	1,255, 1,226, 1,28, 213,0, 1,113, 1,255, 1,113, 213,0, 
	1,28, 1,255, 1,170, 48,0, 1,85, 8,170, 1,141, 3,0, 
	1,28, 4,170, 1,85, 2,0, 1,141, 3,170, 1,141, 2,0, 
	1,28, 10,170, 1,85, 6,0, 1,85, 1,198, 2,255, 1,198, 
	1,141, 1,56, 4,0, 1,85, 10,170, 1,28, 3,0, 1,28, 
	1,170, 2,255, 1,226, 1,141, 1,28, 1,0, 2,170, 1,141, 
	3,0, 1,141, 9,170, 1,85, 3,0, 1,85, 10,170, 1,28, 
	2,0, 1,56, 1,255, 1,113, 1,170, 1,198, 2,255, 1,198, 
	1,141, 1,28, 5,0, 1,141, 9,170, 1,141, 2,0, 1,28, 
	4,170, 1,85, 2,0, 1,141, 3,170, 1,141, 6,0, 2,85, 
	1,56, 2,0, 1,255, 1,170, 7,0, 1,85, 1,226, 1,255, 
	1,198, 1,28, 9,0, 1,28, 1,198, 1,255, 1,226, 1,85, 
	10,0, 1,141, 2,255, 1,141, 8,0, 1,141, 8,255, 1,226, 
	3,0, 1,56, 4,255, 1,141, 2,0, 1,226, 3,255, 1,226, 
	2,0, 1,56, 10,255, 1,141, 5,0, 1,85, 1,255, 1,226, 
	2,170, 3,255, 1,170, 3,0, 1,141, 10,255, 1,56, 2,0, 
	1,28, 1,226, 1,255, 1,226, 1,170, 1,198, 2,255, 1,170, 
	2,255, 1,226, 3,0, 1,226, 9,255, 1,141, 3,0, 1,141, 
	10,255, 1,56, 2,0, 1,85, 3,255, 1,226, 2,170, 1,226, 
	1,255, 1,226, 1,28, 4,0, 1,226, 9,255, 1,226, 2,0, 
	1,56, 4,255, 1,141, 2,0, 1,226, 3,255, 1,226, 4,0, 
	1,113, 4,255, 1,226, 1,85, 1,255, 1,170, 7,0, 1,226, 
	3,255, 1,141, 9,0, 1,141, 3,255, 1,226, 9,0, 1,56, 
	4,255, 1,56, 11,0, 1,170, 1,255, 9,0, 1,170, 1,255, 
	5,0, 1,85, 1,255, 1,85, 8,0, 1,170, 1,255, 10,0, 
	1,226, 1,255, 1,28, 3,0, 1,85, 2,198, 8,0, 1,255, 
	1,170, 7,0, 1,141, 1,255, 1,170, 3,0, 1,28, 1,141, 
	2,255, 1,85, 8,0, 1,170, 1,255, 13,0, 1,255, 1,170, 
	7,0, 1,85, 1,255, 1,226, 1,56, 4,0, 1,85, 1,255, 
	1,113, 8,0, 1,85, 1,255, 1,85, 8,0, 1,170, 1,255, 
	5,0, 1,85, 1,255, 1,85, 4,0, 1,141, 2,255, 1,141, 
	1,85, 1,113, 1,198, 2,255, 1,170, 7,0, 1,226, 3,255, 
	1,141, 9,0, 1,141, 3,255, 1,226, 9,0, 1,56, 4,255, 
	1,56, 11,0, 1,170, 1,255, 9,0, 1,170, 1,255, 5,0, 
	1,85, 1,255, 1,85, 8,0, 1,170, 1,255, 10,0, 1,255, 
	1,170, 15,0, 1,255, 1,170, 7,0, 1,170, 1,255, 1,85, 
	5,0, 1,141, 1,255, 1,85, 8,0, 1,170, 1,255, 13,0, 
	1,255, 1,170, 7,0, 1,56, 1,255, 1,28, 6,0, 1,255, 
	1,170, 8,0, 1,85, 1,255, 1,85, 8,0, 1,170, 1,255, 
	5,0, 1,85, 1,255, 1,85, 3,0, 1,28, 2,255, 1,28, 
	4,0, 1,170, 1,255, 1,170, 7,0, 1,85, 1,226, 1,255, 
	1,198, 1,28, 9,0, 1,28, 1,198, 1,255, 1,226, 1,85, 
	10,0, 1,141, 2,255, 1,141, 12,0, 1,170, 1,255, 9,0, 
	1,170, 1,255, 5,0, 1,85, 1,255, 1,85, 8,0, 1,170, 
	1,255, 10,0, 1,255, 1,170, 15,0, 1,255, 1,170, 7,0, 
	1,141, 1,255, 1,170, 5,0, 1,85, 1,255, 1,85, 8,0, 
	1,170, 1,255, 13,0, 1,255, 1,170, 11,0, 1,56, 3,85, 
	1,170, 1,255, 1,141, 8,0, 1,85, 1,255, 1,85, 8,0, 
	1,170, 1,255, 5,0, 1,85, 1,255, 1,85, 3,0, 1,113, 
	1,255, 1,141, 5,0, 1,28, 1,255, 1,170, 52,0, 1,170, 
	1,255, 9,0, 1,170, 1,255, 5,0, 1,85, 1,255, 1,85, 
	8,0, 1,170, 1,255, 10,0, 1,255, 1,170, 15,0, 1,255, 
	1,170, 7,0, 1,28, 1,226, 1,255, 1,226, 1,141, 2,85, 
	1,113, 1,198, 1,255, 1,85, 8,0, 1,170, 1,255, 13,0, 
	1,255, 1,170, 8,0, 1,28, 1,170, 6,255, 1,226, 1,28, 
	8,0, 1,85, 1,255, 1,85, 8,0, 1,170, 1,255, 5,0, 
	1,85, 1,255, 1,85, 3,0, 1,170, 1,255, 1,85, 6,0, 
	1,255, 1,170, 52,0, 1,170, 1,255, 9,0, 1,170, 1,255, 
	5,0, 1,85, 1,255, 1,85, 8,0, 1,170, 1,255, 10,0, 
	1,255, 1,170, 15,0, 1,255, 1,170, 8,0, 1,28, 1,198, 
	7,255, 1,85, 8,0, 1,170, 1,255, 13,0, 1,255, 1,170, 
	8,0, 1,226, 2,255, 1,226, 3,170, 1,113, 10,0, 1,85, 
	1,255, 1,85, 8,0, 1,170, 1,255, 5,0, 1,85, 1,255, 
	1,85, 3,0, 1,170, 1,255, 1,85, 6,0, 1,255, 1,170, 
	52,0, 1,170, 1,255, 9,0, 1,170, 1,255, 1,28, 4,0, 
	1,85, 1,255, 1,85, 8,0, 1,170, 1,255, 10,0, 1,255, 
	1,170, 15,0, 1,255, 1,170, 11,0, 3,85, 1,56, 1,85, 
	1,255, 1,85, 8,0, 1,170, 1,255, 13,0, 1,255, 1,170, 
	7,0, 1,85, 1,255, 1,170, 5,0, 1,28, 1,56, 9,0, 
	1,85, 1,255, 1,85, 8,0, 1,170, 1,255, 1,28, 4,0, 
	1,85, 1,255, 1,85, 3,0, 1,85, 1,255, 1,198, 5,0, 
	1,56, 1,255, 1,170, 52,0, 1,170, 1,255, 9,0, 1,170, 
	1,255, 1,170, 4,0, 1,198, 1,255, 1,28, 8,0, 1,170, 
	1,255, 7,0, 1,56, 2,85, 1,255, 1,198, 5,85, 1,28, 
	9,0, 1,255, 1,170, 9,0, 1,85, 1,28, 4,0, 1,170, 
	1,255, 1,56, 8,0, 1,170, 1,255, 13,0, 1,255, 1,170, 
	7,0, 1,85, 1,255, 1,113, 4,0, 1,28, 1,226, 1,255, 
	9,0, 1,85, 1,255, 1,85, 8,0, 1,170, 1,255, 1,170, 
	4,0, 1,198, 1,255, 1,28, 4,0, 1,226, 1,255, 1,113, 
	3,0, 1,28, 1,226, 1,255, 1,170, 52,0, 1,170, 1,255, 
	7,0, 1,28, 1,170, 1,226, 2,255, 1,198, 1,113, 1,85, 
	1,170, 1,255, 1,198, 6,0, 1,141, 2,170, 1,226, 1,255, 
	7,0, 10,255, 1,170, 5,0, 1,28, 3,170, 1,255, 1,170, 
	8,0, 1,85, 2,255, 1,170, 2,85, 1,170, 1,255, 1,226, 
	9,0, 1,170, 1,255, 9,0, 1,28, 3,170, 1,255, 1,170, 
	8,0, 1,226, 1,255, 1,141, 2,85, 1,141, 3,255, 6,0, 
	1,85, 2,170, 1,198, 1,255, 1,85, 6,0, 1,28, 1,170, 
	1,226, 2,255, 1,198, 1,113, 1,85, 1,170, 1,255, 1,198, 
	5,0, 1,56, 1,226, 1,255, 1,226, 1,170, 1,198, 1,255, 
	1,226, 1,255, 1,226, 1,170, 1,28, 50,0, 1,170, 1,255, 
	7,0, 1,56, 3,255, 1,56, 1,226, 3,255, 1,198, 1,28, 
	6,0, 1,226, 4,255, 7,0, 1,56, 2,85, 1,255, 1,198, 
	5,85, 1,28, 5,0, 1,56, 4,255, 1,170, 9,0, 1,141, 
	5,255, 1,226, 1,28, 9,0, 1,170, 1,255, 9,0, 1,56, 
	4,255, 1,170, 8,0, 1,28, 1,198, 4,255, 2,226, 1,255, 
	6,0, 1,141, 4,255, 1,85, 6,0, 1,56, 3,255, 1,56, 
	1,226, 3,255, 1,198, 1,28, 6,0, 1,28, 1,141, 1,226, 
	1,255, 1,226, 1,141, 1,28, 3,255, 1,56, 50,0, 1,170, 
	1,255, 13,0, 1,56, 1,85, 1,56, 23,0, 1,255, 1,170, 
	28,0, 3,85, 1,56, 11,0, 1,170, 1,255, 25,0, 1,28, 
	2,85, 1,56, 1,0, 1,28, 1,56, 24,0, 1,56, 1,85, 
	1,56, 65,0, 1,28, 3,85, 1,198, 1,255, 3,85, 1,56, 
	35,0, 1,255, 1,170, 43,0, 1,170, 1,255, 124,0, 1,170, 
	9,255, 23,0, 1,141, 1,170, 1,28, 9,0, 1,255, 1,170, 
	14,0, 1,28, 1,170, 1,141, 26,0, 1,170, 1,255, 12,0, 
	1,28, 1,170, 1,141, 26,0, 1,85, 1,170, 1,85, 80,0, 
	1,28, 8,85, 1,56, 23,0, 2,255, 1,85, 9,0, 1,56, 
	1,28, 14,0, 1,85, 2,255, 22,0, 1,56, 5,255, 12,0, 
	1,85, 2,255, 26,0, 1,170, 1,255, 1,170, 113,0, 2,255, 
	1,85, 25,0, 1,85, 2,255, 22,0, 1,28, 5,170, 12,0, 
	1,85, 2,255, 26,0, 1,170, 1,255, 1,170, 113,0, 1,56, 
	1,85, 27,0, 1,85, 1,56, 41,0, 1,85, 1,56, 26,0, 
	1,28, 1,85, 1,28, 253,0, 254,0, 

	0,0  // THE END
};

static byte *Unpacker(const byte *src, int w, int h)
{
	byte *buffer = new byte[w * h];
	byte *dest = buffer;

	for (; *src; src += 2)
	{
		for (int j=0; j < src[0]; j++)
			*dest++ = src[1];
	}

	DEV_ASSERT2((dest - buffer) == (w * h));

	return buffer;
}

const byte *RGL_LogoImage(int *w, int *h)
{
	static byte *unpacked_logo = NULL;

	if (! unpacked_logo)
		unpacked_logo = Unpacker(logo_data, logo_w, logo_h);

	*w = logo_w;
	*h = logo_h;

	return unpacked_logo;
}

const byte *RGL_InitImage(int *w, int *h)
{
	static byte *unpacked_logo = NULL;

	if (! unpacked_logo)
		unpacked_logo = Unpacker(init_data, init_w, init_h);

	*w = init_w;
	*h = init_h;

	return unpacked_logo;
}

#endif  // USE_GL
