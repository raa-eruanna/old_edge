//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Skies)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2003  The EDGE Team.
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

// this conditional applies to the whole file
#ifdef USE_GL

#include "i_defs.h"

#include "am_map.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "e_search.h"
#include "m_bbox.h"
#include "m_random.h"
#include "p_local.h"
#include "p_mobj.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_sky.h"
#include "r_state.h"
#include "r_things.h"
#include "r2_defs.h"
#include "rgl_defs.h"
#include "v_colour.h"
#include "v_ctx.h"
#include "v_res.h"
#include "w_textur.h"
#include "w_wad.h"
#include "z_zone.h"


#define DEBUG  0


#define RGB_RED(rgbcol)  ((float)((rgbcol >> 16) & 0xFF) / 255.0f)
#define RGB_GRN(rgbcol)  ((float)((rgbcol >>  8) & 0xFF) / 255.0f)
#define RGB_BLU(rgbcol)  ((float)((rgbcol      ) & 0xFF) / 255.0f)


#define Z_NEAR  1.0f
#define Z_FAR   32000.0f

static bool has_drawn_sky = false;


//
// RGL_SetupSkyMatrices
//
void RGL_SetupSkyMatrices(void)
{
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();

	glLoadIdentity();
	glFrustum(-1.0f, +1.0f, -1.0f, +1.0f, Z_NEAR, Z_FAR);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	glLoadIdentity();
	glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
	// glRotatef(0.0f, 0.0f, 0.0f, 1.0f);
}

//
// RGL_RevertSkyMatrices
//
void RGL_RevertSkyMatrices(void)
{
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}

//
// RGL_BeginSky
//
void RGL_BeginSky(void)
{
	has_drawn_sky = false;

	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDisable(GL_TEXTURE_2D);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEdgeFlag(GL_TRUE);
}

//
// RGL_FinishSky
//
void RGL_FinishSky(void)
{
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	// draw sky picture, but DON'T affect the depth buffering

	if (has_drawn_sky)
	{
		glEnable(GL_TEXTURE_2D);

		glDepthFunc(GL_GREATER);
		glDepthMask(GL_FALSE);

		RGL_DrawSkyBackground();

		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_TRUE);

		glDisable(GL_TEXTURE_2D);
	}
}

//
// RGL_DrawSkyBackground
//
void RGL_DrawSkyBackground(void)
{
#if 0  // sky way #1, behind everything
	if (has_drawn_sky)
		return;

	has_drawn_sky = true;
#endif

	RGL_SetupSkyMatrices();

	int x, y, w, h;
	float right, bottom;

	const cached_image_t *cim;
	GLuint tex_id;

	float mlook_rad;
	float top_L, bottom_L;
	float base_a = ANG_2_FLOAT(viewangle);

	int sx1, sy1, sx2, sy2;  // screen coords
	float tx1, tx2, ty, bx1, bx2, by;  // tex coords

	DEV_ASSERT2(sky_image);
	cim = W_ImageCache(sky_image, IMG_OGL, 0, true);
	tex_id = W_ImageGetOGL(cim);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tex_id);

	w = sky_image->actual_w;
	h = sky_image->actual_h;
	right  = w / (float)sky_image->total_w;
	bottom = h / (float)sky_image->total_h;

	// sky is always 100% bright
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	sx1 = viewwindowx;
	sx2 = viewwindowx + viewwindowwidth;

	sy1 = viewwindowy;
	sy2 = viewwindowy + viewwindowheight;

	// compute sky horizontally tex coords
	mlook_rad = atan(viewvertangle);

	if (mlook_rad >= M_PI/4)
		top_L = 90;
	else if (mlook_rad <= -M_PI/4)
		top_L = 45;
	else
	{
		// d is just the distance horizontally forward from the eye to
		// the top edge of the view rectangle.
		float d = M_ROOT2 * sin(M_PI/2 - (mlook_rad + M_PI/4));

		top_L = 90 - ANG_2_FLOAT(M_ATan(d));
	}

	if (mlook_rad <= -M_PI/4)
		bottom_L = 90;
	else if (mlook_rad >= M_PI/4)
		bottom_L = 45;
	else
	{
		// d is just the distance horizontally forward from the eye to
		// the bottom edge of the view rectangle.
		float d = M_ROOT2 * sin(M_PI/2 - (M_PI/4 - mlook_rad));

		bottom_L = 90 - ANG_2_FLOAT(M_ATan(d));
	}

	if (! level_flags.stretchsky)
	{
		top_L = bottom_L = 90;
	}

	top_L /= 2.0f;
	bottom_L /= 2.0f;

	tx1 = (base_a + top_L) / (w > 256 ? 360.0f : 180.0f);
	tx2 = (base_a - top_L) / (w > 256 ? 360.0f : 180.0f);

	bx1 = (base_a + bottom_L) / (w > 256 ? 360.0f : 180.0f);
	bx2 = (base_a - bottom_L) / (w > 256 ? 360.0f : 180.0f);

	// compute sky vertical tex coords
	{
		float top_a    = (M_PI/2 - mlook_rad - M_PI/4) / M_PI;
		float bottom_a = (M_PI/2 - mlook_rad + M_PI/4) / M_PI;

		if (top_a < 0)
			top_a = 0;

		if (bottom_a > 1.0f)
			bottom_a = 1.0f;

		DEV_ASSERT2(bottom_a > top_a);

		ty = top_a * bottom;
		by = bottom_a * bottom;
	}

	glBegin(GL_QUADS);

	// divide screen into many squares, to reduce distortion
	for (y=0; y < 8; y++)
	{
		for (x=0; x < 8; x++)
		{
			int xa = sx1 + (sx2 - sx1) * x     / 8;
			int xb = sx1 + (sx2 - sx1) * (x+1) / 8;

			int ya = sy1 + (sy2 - sy1) * y     / 8;
			int yb = sy1 + (sy2 - sy1) * (y+1) / 8;

			float la = tx1 + (bx1 - tx1) * y     / 8;
			float ra = tx2 + (bx2 - tx2) * y     / 8;
			float lb = tx1 + (bx1 - tx1) * (y+1) / 8;
			float rb = tx2 + (bx2 - tx2) * (y+1) / 8;

			float txa = la + (ra - la) * x     / 8;
			float bxa = lb + (rb - lb) * x     / 8;
			float txb = la + (ra - la) * (x+1) / 8;
			float bxb = lb + (rb - lb) * (x+1) / 8;

			float tya = ty + (by - ty) * y     / 8;
			float bya = ty + (by - ty) * (y+1) / 8;

#if 1
			// sky way #3, using depth-buffer

			float pxa = xa / (float)SCREENWIDTH  * 2.0f - 1.0f;
			float pxb = xb / (float)SCREENWIDTH  * 2.0f - 1.0f;
			float pya = ya / (float)SCREENHEIGHT * 2.0f - 1.0f;
			float pyb = yb / (float)SCREENHEIGHT * 2.0f - 1.0f;

			float dist = Z_FAR * 0.99;

			pxa *= dist; pya *= dist;
			pxb *= dist; pyb *= dist;

			glTexCoord2f(txa, 1.0f - bottom * tya);
			glVertex3f(pxa, pya, dist);

			glTexCoord2f(txb, 1.0f - bottom * tya);
			glVertex3f(pxb, pya, dist);

			glTexCoord2f(bxb, 1.0f - bottom * bya);
			glVertex3f(pxb, pyb, dist);

			glTexCoord2f(bxa, 1.0f - bottom * bya);
			glVertex3f(pxa, pyb, dist);
#endif

#if 0
			// sky way #1, behind everything
		
			glTexCoord2f(txa, 1.0f - bottom * tya);
			glVertex2i(xa, SCREENHEIGHT - ya);

			glTexCoord2f(txb, 1.0f - bottom * tya);
			glVertex2i(xb, SCREENHEIGHT - ya);

			glTexCoord2f(bxb, 1.0f - bottom * bya);
			glVertex2i(xb, SCREENHEIGHT - yb);

			glTexCoord2f(bxa, 1.0f - bottom * bya);
			glVertex2i(xa, SCREENHEIGHT - yb);
#endif
		}
	}

	glEnd();

	glDisable(GL_TEXTURE_2D);

	W_ImageDone(cim);

	RGL_RevertSkyMatrices();
}


#if 0  // sky way #2, polygons

static INLINE void CalcSkyTexCoord(float x, float y, float z, 
								   float *tx, float *ty)
{
	float dist;
	float tile_num = 1;
	float base_val, angle_val;

	angle_t angle;

	x -= viewx;
	y -= viewy;
	z -= viewz;

	// OPTIMISE
	dist = (float)sqrt(x*x + y*y + z*z);

	if (dist < 1) 
		dist = 1;

	x /= dist;
	y /= dist;
	z /= dist;

	angle = viewangle - R_PointToAngle(0, 0, x, y);

	base_val = ANG_2_FLOAT(viewangle);

	if (angle < ANG180)
		angle_val = ANG_2_FLOAT(angle);
	else
		angle_val = ANG_2_FLOAT(angle) - 360.0f;

	(*tx) = tile_num * (base_val - angle_val) /
		(sky_image->actual_w > 256 ? 360.0f : 180.0f);
	(*ty) = tile_num * ((1 + z) / 2);  /// * IM_BOTTOM(sky_image);
	return;

#if 0  // EXPERIMENTAL
	dist = sqrt(x*x + z*z);

	if (dist > 0.01f)
	{
		x /= dist;
		z /= dist;
	}

	x = (1 + x) / 2;
	z = (1 + z) / 2;

	if (0)  // sky_image->actual_w > 256)
	{
		x /= 2;

		if (y < 0)
			x = 1.0f - x;
	}
	else
	{
		if (y < 0)
			x = -x;
	}

	(*tx) = x;
	(*ty) = z * IM_BOTTOM(sky_image);
#endif
}

typedef struct sky_data_s
{
	vec3_t normal;
}
sky_data_t;

void SkyPolyCoordFunc(vec3_t *src, local_gl_vert_t *vert, void *d)
{
	sky_data_t *data = (sky_data_t *)d;

	float tx, ty;

	CalcSkyTexCoord(src->x, src->y, src->z, &tx, &ty);

	SET_COLOR(1.0f, 1.0f, 1.0f, 1.0f);
	SET_TEXCOORD(tx, ty);
	SET_NORMAL(data->normal.x, data->normal.y, data->normal.z);
	SET_EDGE_FLAG(GL_TRUE);
	SET_VERTEX(src->x, src->y, src->z);
}
#endif

//
// RGL_DrawSkyPlane
//
void RGL_DrawSkyPlane(subsector_t *sub, float h)
{
	seg_t *seg;

	glNormal3f(0, 0, (viewz > h) ? 1.0f : -1.0f);

	glBegin(GL_POLYGON);

	for (seg=sub->segs; seg; seg=seg->sub_next)
	{
		glVertex3f(seg->v1->x, seg->v1->y, h);
	}

	glEnd();

	has_drawn_sky = true;

#if 0  // sky way #2, polygons.

	raw_polyquad_t *poly;
	sky_data_t data;

	seg_t *seg;

	GLuint tex_id;
	const cached_image_t *cim;

	int num_vert, i;

	// count number of actual vertices
	for (seg=sub->segs, num_vert=0; seg; seg=seg->sub_next, num_vert++)
	{ /* nothing here */ }

	if (num_vert > MAX_PLVERT)
		num_vert = MAX_PLVERT;

	DEV_ASSERT2(sky_image);
	cim = W_ImageCache(sky_image, IMG_OGL, 0, true);
	tex_id = W_ImageGetOGL(cim);

	// normally this is wrong -- W_LockImagesOGL saves us though.
	W_ImageDone(cim);

	data.normal.x = 0;
	data.normal.y = 0;
	data.normal.z = (viewz > h) ? 1.0f : -1.0f;

	// create PolyQuad and transfer vertices

	poly = RGL_NewPolyQuad(num_vert, false);

	for (seg=sub->segs, i=0; seg && (i < MAX_PLVERT); 
		seg=seg->sub_next, i++)
	{
		PQ_ADD_VERT(poly, seg->v1->x, seg->v1->y, h);
	}

	RGL_BoundPolyQuad(poly);
	RGL_SplitPolyQuadLOD(poly, 1, 128 >> detail_level);

	RGL_RenderPolyQuad(poly, &data, &SkyPolyCoordFunc, tex_id,
		false, false);

	RGL_FreePolyQuad(poly);
#endif
}

//
// RGL_DrawSkyWall
//
void RGL_DrawSkyWall(seg_t *seg, float h1, float h2)
{
	float x1 = seg->v1->x;
	float y1 = seg->v1->y;
	float x2 = seg->v2->x;
	float y2 = seg->v2->y;

	glNormal3f(y2 - y1, x1 - x2, 0);

	glBegin(GL_QUADS);

	glVertex3f(x1, y1, h1);
	glVertex3f(x1, y1, h2);
	glVertex3f(x2, y2, h2);
	glVertex3f(x2, y2, h1);

	glEnd();

	has_drawn_sky = true;

#if 0  // sky way #2, polygons.

	float x1 = seg->v1->x;
	float y1 = seg->v1->y;
	float x2 = seg->v2->x;
	float y2 = seg->v2->y;

	GLuint tex_id;
	const cached_image_t *cim;

	sky_data_t data;
	raw_polyquad_t *poly;

	poly = RGL_NewPolyQuad(4, true);

	PQ_ADD_VERT(poly, x1, y1, h1);
	PQ_ADD_VERT(poly, x1, y1, h2);
	PQ_ADD_VERT(poly, x2, y2, h1);
	PQ_ADD_VERT(poly, x2, y2, h2);

	// get texture id for sky
	DEV_ASSERT2(sky_image);
	cim = W_ImageCache(sky_image, IMG_OGL, 0, true);
	tex_id = W_ImageGetOGL(cim);

	// normally this is wrong -- W_LockImagesOGL saves us though.
	W_ImageDone(cim);

	data.normal.x = y2 - y1;
	data.normal.y = x1 - x2;
	data.normal.z = 0;

	RGL_BoundPolyQuad(poly);
	RGL_SplitPolyQuadLOD(poly, 1, 128 >> detail_level);

	RGL_RenderPolyQuad(poly, &data, &SkyPolyCoordFunc, tex_id,
		false, false);

	RGL_FreePolyQuad(poly);
#endif
}


#endif  // USE_GL
