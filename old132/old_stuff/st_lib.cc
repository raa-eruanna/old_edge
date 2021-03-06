//----------------------------------------------------------------------------
//  EDGE Status Bar Library Code
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2008  The EDGE Team.
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

#include "i_defs.h"
#include "st_lib.h"

#include "dm_defs.h"
#include "dm_state.h"
#include "st_stuff.h"
#include "r_local.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_modes.h"
#include "r_image.h"
#include "w_wad.h"

void STLIB_Init(void)
{
	/* does nothing */
}

void STLIB_InitNum(st_number_t * n, int x, int y, 
				   const image_c ** digits, const image_c *minus, int *num, 
				   int width)
{
	n->x = x;
	n->y = y;
	n->oldnum = 0;
	n->width = width;
	n->num = num;
	n->digits = digits;
	n->minus = minus;
	n->colmap = NULL;  // text_red_map;
}

void STLIB_InitFloat(st_float * n, int x, int y, 
					 const image_c ** digits, float *num, int width)
{
	STLIB_InitNum(&n->num, x,y, digits,NULL, NULL, width);
	n->f = num;
}

static void DrawDigit(float x, float y, const image_c *image, 
		const colourmap_c *map)
{
	float w = IM_WIDTH(image);
	float h = IM_HEIGHT(image);

    RGL_DrawImage(
			FROM_320(x-IM_OFFSETX(image)),
            SCREENHEIGHT - FROM_200(y-IM_OFFSETY(image)) - FROM_200(h),
            FROM_320(w), FROM_200(h), image,
			0, 0, IM_RIGHT(image), IM_TOP(image),
			map, 1.0f);
}

/*
#define DrawDigit(X,Y,Image,Map)  \
	RGL_DrawImage(FROM_320((X)-IM_OFFSETX(Image)), \
	SCREENHEIGHT-FROM_200((Y)-IM_OFFSETY(Image)-FROM_200(IM_HEIGHT(Image)), \
	FROM_320(IM_WIDTH(Image)), FROM_200(IM_HEIGHT(Image)),  \
	(Image),0,0,IM_RIGHT(Image),IM_TOP(Image),(Map),1.0f)
*/

static void DrawNum(st_number_t * n)
{
	int numdigits = n->width;
	int num = *n->num;
	int x;

	bool neg = false;

	n->oldnum = *n->num;

	// if non-number, do not draw it
	if (num == 1994)
		return;

	if (num < 0)
	{
		neg = true;

		num = -num;
		numdigits--;
	}

#if 0
	if (numdigits == 1 && num > 9)
		num = 9;
	else if (numdigits == 2 && num > 99)
		num = 99;
	else if (numdigits == 3 && num > 999)
		num = 999;
	else if (numdigits == 4 && num > 9999)
		num = 9999;
#endif

	x = n->x;

	// in the special case of 0, you draw 0
	if (num == 0)
	{
		x -= I_ROUND(IM_WIDTH(n->digits[0]));
		DrawDigit(x, n->y, n->digits[0], n->colmap);
	}
	else
	{
		SYS_ASSERT(num > 0);

		// draw the new number
		for (; num && (numdigits > 0); num /= 10, numdigits--)
		{
			x -= I_ROUND(IM_WIDTH(n->digits[num % 10]));  // XXX
			DrawDigit(x, n->y, n->digits[num % 10], n->colmap);
		}
	}

	if (neg && n->minus)
	{
		x -= I_ROUND(IM_WIDTH(n->minus));
		DrawDigit(x, n->y, n->minus, n->colmap);
	}
}

void STLIB_DrawNum(st_number_t * n)
{
	DrawNum(n);
}

void STLIB_DrawFloat(st_float * n)
{
	int i = (int) *n->f;

	// HACK: Display 1 for numbers between 0 and 1. This is just because a
	// health of 0.3 otherwise would be displayed as 0%, which would make it
	// seem like you were a living dead.

	if (*n->f > 0 && *n->f < 1.0f)
		i = 1;

	n->num.num = &i;
	STLIB_DrawNum(&n->num);
	n->num.num = NULL;
}

void STLIB_InitPercent(st_percent_t * p, int x, int y, 
					   const image_c ** digits, const image_c *percsign,
					   float *num)
{
	STLIB_InitFloat(&p->f, x, y, digits, num, 3);
	p->percsign = percsign;
}

void STLIB_DrawPercent(st_percent_t * per)
{
	st_number_t *num = &per->f.num;

	DrawDigit(num->x, num->y, per->percsign, num->colmap);

	STLIB_DrawFloat(&per->f);
}

void STLIB_InitMultIcon(st_multicon_t * i, int x, int y, 
						const image_c ** icons, int *inum)
{
	i->x = x;
	i->y = y;
	i->oldinum = -1;
	i->inum = inum;
	i->icons = icons;
}

void STLIB_DrawMultIcon(st_multicon_t * mi)
{
	const image_c *image;

	if (*mi->inum != -1)
	{
		image = mi->icons[*mi->inum];

		RGL_ImageEasy320(mi->x, mi->y, image);

		mi->oldinum = *mi->inum;
	}
}

void STLIB_InitBinIcon(st_binicon_t * b, int x, int y, 
					   const image_c * icon, bool * val)
{
	b->x = x;
	b->y = y;
	b->oldval = 0;
	b->val = val;
	b->icon = icon;
}

void STLIB_DrawBinIcon(st_binicon_t * bi)
{
	if (*bi->val)
		RGL_ImageEasy320(bi->x, bi->y, bi->icon);

	bi->oldval = *bi->val;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
