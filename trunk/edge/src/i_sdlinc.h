//----------------------------------------------------------------------------
//  EDGE SDL System Internal header
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2005  The EDGE Team.
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

#ifndef __SDL_SYSTEM_INTERNAL_H__
#define __SDL_SYSTEM_INTERNAL_H__

#ifdef MACOSX
#include <SDL.h>
#else
#include <SDL/SDL.h>
#endif 

typedef enum
{
	APP_STATE_ACTIVE       = 0x1,
	APP_STATE_PENDING_QUIT = 0x2
}
app_state_flags_e;

void I_CentreMouse();

extern int app_state;
extern bool use_grab;
extern bool use_warp_mouse;

#endif /* __SDL_SYSTEM_INTERNAL_H__ */
