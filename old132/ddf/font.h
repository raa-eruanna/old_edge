//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Fonts)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
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

#ifndef __DDF_FONT__
#define __DDF_FONT__

#include "epi/utility.h"

#include "main.h"

//
// -AJA- 2004/11/13: Fonts.ddf
//
typedef enum
{
	FNTYP_UNSET = 0,

	FNTYP_Patch = 1,  // font is made up of individual patches
	FNTYP_Image = 2,  // font consists of one big image (16x16 chars)
}
fonttype_e;


class fontpatch_c
{
public:
	int char1, char2;  // range

	epi::strent_c patch1;

	fontpatch_c *next;  // link in list

public:
	 fontpatch_c(int _ch1, int _ch2, const char *_pat1);
	~fontpatch_c();
};


class fontdef_c
{
public:
	epi::strent_c name;

	fonttype_e type;

	fontpatch_c *patches;
	epi::strent_c missing_patch;

	epi::strent_c image_name;

public:
	 fontdef_c();
	~fontdef_c();

	void Default(void);
	void CopyDetail(const fontdef_c &src);

private:
	// disable copy construct and assignment operator
	explicit fontdef_c(fontdef_c &rhs) { }
	fontdef_c& operator=(fontdef_c &rhs) { return *this; }
};


fontdef_c * DDF_LookupFont(const char *refname);


#endif  /* __DDF_FONT__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
