//----------------------------------------------------------------------------
//  EDGE Video Context
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2002  The EDGE Team.
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

#ifndef __V_CTX__
#define __V_CTX__

#include "dm_type.h"
#include "ddf_main.h"
#include "w_image.h"

//
// Video Context
//
// This is the video context that contains high-level drawing
// operations (like "render a scene").  Details about column drawers
// or whatever are intentionally ignored here, they are low-level
// (e.g. column drawers aren't even used by the OpenGL system).  This
// interface is used by "Drawer" functions in the layer system to draw
// the stuff contained in layers.
//
// Note: the drawing routines don't take into account the dirty matrix
// and/or solid rectangles.  Use the functions in v_toplev for this.

typedef struct vid_view_s vid_view_t;  //!!! Not yet defined

typedef struct video_context_s
{
	// Video output is double buffered, or in some way requires
	// everything to be re-drawn at each frame.  When true, the dirty
	// matrix logic cannot be used.
  
	boolean_t double_buffered;

	// This routine should inform the lower level system(s) that the
	// screen has changed size/depth.  New size/depth is given.  Must be
	// called before any rendering has occurred (e.g. just before
	// I_StartFrame).

	void (* NewScreenSize)(int width, int height, int bpp);
  
	// Rendering routine, which renders a scene using the given view
	// (which is precomputed from an in-game object).  Coordinates are
	// inclusive.
  
	void (* RenderScene)(int x, int y, int w, int h, vid_view_t *view);
  
	// Begin/End a sequence of drawing for a particular layer.  The
	// coordinates to BeginDraw are the bounding box the future drawing
	// operations must clip to.  Coordinates are inclusive.  It is
	// illegal to call the drawing operations (DrawImage, SolidBox and
	// SolidLine) outside of a Begin/End sequence.
  
	void (* BeginDraw)(int x1, int y1, int x2, int y2);
	void (* EndDraw)(void);
  
	// Draws an image into a rectangular area of the screen.  Screen
	// coordinates are inclusive.  Alpha ranges from 0.0 (invisible) to
	// 1.0 (totally opaque).  The colmap parameter is a colourmap to use
	// to shade the image (especially font characters), or NULL to draw
	// the image as-is.  If the texture coordinates lie outside of the
	// [0-1] range, then the image will be tiled.  Proper tiling is only
	// guaranteed for power-of-two sized images.  The drawing will be
	// clipped to the current clipping rectangle.
  
	void (* DrawImage)(int x, int y, int w, int h, const image_t *image,
					   float_t tx1, float_t ty1, float_t tx2, float_t ty2,
					   const colourmap_t *colmap, float_t alpha);
 
	// Draw a solid colour box (possibly translucent) in the given
	// rectangle.  Coordinates are inclusive.  Alpha ranges from 0
	// (invisible) to 255 (totally opaque).  Colour is a palette index
	// (0-255).  Drawing will be clipped to the current clipping
	// rectangle.
  
	void (* SolidBox)(int x, int y, int w, int h,
					  int colour, float_t alpha);
 
	// Draw a solid colour line (possibly translucent) between the two
	// end points.  Coordinates are inclusive.  Used for the automap.
	// Colour is a palette index (0-255).  Drawing will be clipped to
	// the current clipping rectangle.
  
	void (* SolidLine)(int x1, int y1, int x2, int y2, int colour);
  
	// Read the contents of the screen into the given buffer, which is
	// in RGB format (3 bytes per pixel).  Buffer must be large enough
	// for the data.  Coordinates are inclusive.  This routine may only
	// be called after screen is fully rendered but _before_ the
	// I_FinishFrame() function has been called.

	void (* ReadScreen)(int x, int y, int w, int h, byte *rgb_buffer);
}
video_context_t;


// Global video context.  There is only ever one.
extern video_context_t vctx;

// Convenience macros
#define VCTX_Image(X,Y,W,H,Image)                                   \
    vctx.DrawImage((X)-(Image)->offset_x,                           \
                   (Y)-(Image)->offset_y,                           \
                   (W),(H),                                         \
                   (Image), 0, 0,                                   \
                   IM_RIGHT(Image),IM_BOTTOM(Image), NULL, 1.0)


#define VCTX_Image320(X,Y,W,H,Image)                                \
    vctx.DrawImage(FROM_320((X)-(Image)->offset_x),                 \
                   FROM_200((Y)-(Image)->offset_y),                 \
                   FROM_320(W), FROM_200(H),                        \
                   (Image), 0, 0,                                   \
                   IM_RIGHT(Image), IM_BOTTOM(Image), NULL, 1.0)


#define VCTX_ImageEasy(X,Y,Image)                                   \
    vctx.DrawImage((X)-(Image)->offset_x,                           \
                   (Y)-(Image)->offset_y,                           \
                   IM_WIDTH(Image), IM_HEIGHT(Image),               \
                   (Image), 0, 0,                                   \
                   IM_RIGHT(Image), IM_BOTTOM(Image), NULL, 1.0)

#define VCTX_ImageEasy320(X,Y,Image)                                       \
    vctx.DrawImage(FROM_320((X)-(Image)->offset_x),                        \
                   FROM_200((Y)-(Image)->offset_y),                        \
                   FROM_320(IM_WIDTH(Image)), FROM_200(IM_HEIGHT(Image)),  \
                   (Image), 0, 0,                                          \
                   IM_RIGHT(Image), IM_BOTTOM(Image), NULL, 1.0)

#endif  // __V_CTX__





