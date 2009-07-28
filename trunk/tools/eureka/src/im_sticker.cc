//------------------------------------------------------------------------
//  STICKER (Images)
//------------------------------------------------------------------------
//
//  Eureka DOOM Editor
//
//  Copyright (C) 2001-2009 Andrew Apted
//  Copyright (C) 1997-2003 Andr� Majorel et al
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
//------------------------------------------------------------------------
//
//  Based on Yadex which incorporated code from DEU 5.21 that was put
//  in the public domain in 1994 by Rapha�l Quinet and Brendon Wyber.
//
//------------------------------------------------------------------------

#include "main.h"

#include "im_color.h"
#include "r_misc.h"
#include "im_img.h"
#include "im_sticker.h"



class Sticker_priv
{
public :
    Sticker_priv ();
    ~Sticker_priv ();
    void clear ();
    void load (const Img &img, bool opaque);
    bool opaque;

    Fl_RGB_Image *rgb;
};


/*
 *  Sticker::Sticker - create an empty Sticker
 */
Sticker::Sticker ()
{
	priv = new Sticker_priv ();
}


/*
 *  Sticker::Sticker - create a Sticker from an Img
 */
Sticker::Sticker (const Img& img, bool opaque)
{
	priv = new Sticker_priv ();
	priv->load (img, opaque);
}


/*
 *  Sticker::~Sticker - destroy a Sticker
 */
Sticker::~Sticker ()
{
	delete priv;
} 


/*
 *  Sticker::is_clear - tells whether a sprite is "empty"
 */
bool Sticker::is_clear ()
{
	return ! priv->rgb;
}


/*
 *  Sticker::clear - clear a Sticker
 */
void Sticker::clear ()
{
	priv->clear ();
}


/*
 *  Sticker::load - load an Img into a Sticker
 */
void Sticker::load (const Img& img, bool opaque)
{
	priv->load (img, opaque);
}


/*
 *  Sticker::draw - draw a Sticker
 */
void Sticker::draw (char grav, int x, int y)
{
	if (! priv->rgb)
		return;

	int x0 = (grav == 'c') ? x - priv->rgb->w() / 2 : x;
	int y0 = (grav == 'c') ? y - priv->rgb->h() / 2 : y;

	priv->rgb->draw(x0, y0);
}


//------------------------------------------------------------


Sticker_priv::Sticker_priv () : rgb(NULL)
{
}


Sticker_priv::~Sticker_priv ()
{
	clear ();
}


void Sticker_priv::clear ()
{
	if (rgb)
	{
		delete rgb;
		rgb = NULL;
	}
}


void Sticker_priv::load (const Img& img, bool opaque)
{
	this->opaque = opaque;

	clear();

	int w = img.width ();
	int h = img.height ();

	if (w< 1 || h< 1)
		return;  // Can't create Pixmaps with null dimensions...


	byte *buf = new byte[w * h * 4];

	for (int y = 0; y < h; y++)
	for (int x = 0; x < w; x++)
	{
		img_pixel_t pix = img.buf() [y*w+x];

		u32_t col = game_colour[pix];

		byte *ptr = buf + ((y*w+x) * 4);

		ptr[0] = ((col >> 24) & 0xFF);
		ptr[1] = ((col >> 16) & 0xFF);
		ptr[2] = ((col >>  8) & 0xFF);
		ptr[3] = 255; // FIXME
	}

	rgb = new Fl_RGB_Image(buf, w, h, 4, 0);

  //??? delete buf;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab