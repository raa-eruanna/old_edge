//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Unit system)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2007  The EDGE Team.
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

#ifndef __R_UNITS_H__
#define __R_UNITS_H__

#define MAX_PLVERT  64

extern bool use_lighting;
extern bool use_color_material;
extern bool dumb_sky;

// a single vertex to pass to the GL 
typedef struct local_gl_vert_s
{
	GLfloat x, y, z;
	GLfloat col[4];
	GLfloat tx[2], ty[2];
	GLfloat nx, ny, nz;
	GLboolean edge;
}
local_gl_vert_t;

void RGL_InitUnits(void);
void RGL_SoftInitUnits(void);

void RGL_StartUnits(bool solid);
void RGL_FinishUnits(void);
void RGL_DrawUnits(void);

typedef enum
{
	BL_Masked  = 0x0001,  // drop fragments when alpha == 0
	BL_Alpha   = 0x0002,  // alpha-blend with the framebuffer
	BL_Add     = 0x0004,  // additive-blend with the framebuffer
	BL_NoZBuf  = 0x0020,  // don't update the Z buffer
	BL_ClampY  = 0x0040,
}
blending_mode_e;


local_gl_vert_t *RGL_BeginUnit(GLuint shape, int max_vert, 
		                       GLuint env1, GLuint tex1,
							   GLuint env2, GLuint tex2,
							   int pass, int blending);
void RGL_EndUnit(int actual_vert);

///--- void RGL_SendRawVector(const local_gl_vert_t *V);

// utility macros
#define SET_COLOR(R,G,B,A)  \
	do { vert->col[0] = (R); vert->col[1] = (G); vert->col[2] = (B);  \
	vert->col[3] = (A); } while(0)

#define SET_TEXCOORD(X,Y)  \
	do { vert->tx[0] = (X); vert->ty[0] = (Y); } while(0)

#define SET_TEX2COORD(X,Y)  \
	do { vert->tx[1] = (X); vert->ty[1] = (Y); } while(0)

#define SET_NORMAL(X,Y,Z)  \
	do { vert->nx = (X); vert->ny = (Y); vert->nz = (Z); } while(0)

#define SET_EDGE_FLAG(E)  \
	do { vert->edge = (E); } while(0)

#define SET_VERTEX(X,Y,Z)  \
	do { vert->x = (X); vert->y = (Y); vert->z = (Z); } while(0)


#endif /* __R_UNITS_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
