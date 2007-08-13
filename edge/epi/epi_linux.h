//----------------------------------------------------------------------------
//  Linux EPI System Specifics
//----------------------------------------------------------------------------
//
//  Copyright (c) 2003-2007  The EDGE Team.
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
#ifndef __LINUX_EPI_HEADER__
#define __LINUX_EPI_HEADER__

// Sanity checking...
#ifdef __EPI_HEADER_SYSTEM_SPECIFIC__
#error "Two different system specific EPI headers included"
#else
#define __EPI_HEADER_SYSTEM_SPECIFIC__
#endif

#include <GL/gl.h>

//#define DIRSEPARATOR '/'

#define GCCATTR(xyz) __attribute__ (xyz)

#endif /* __LINUX_EPI_HEADER__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
