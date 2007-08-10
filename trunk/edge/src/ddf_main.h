//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Main)
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

#ifndef __DDF_MAIN_H__
#define __DDF_MAIN_H__

#include "epi/math_crc.h"
#include "epi/utility.h"

#include "dm_defs.h"
#include "ddf_type.h"

#define DEBUG_DDF  0

// Forward declarations
struct mobj_s;
struct sfx_s;

class atkdef_c;
class colourmap_c;
class gamedef_c;
class mapdef_c;
class mobjtype_c;
class pl_entry_c;
class weapondef_c;


class ddf_base_c
{
public:
	ddf_base_c();	
	ddf_base_c(const ddf_base_c &rhs);
	~ddf_base_c();	

private:
	void Copy(const ddf_base_c &src);

public:
	void Default(void);
	void SetUniqueName(const char *prefix, int num);
	
	ddf_base_c& operator= (const ddf_base_c &rhs);

	epi::strent_c name;
	int number;	
	epi::crc32_c crc;
};

// Our lumpname class
#define LUMPNAME_SIZE 10

class lumpname_c
{
public:
	lumpname_c() { Clear(); }
	lumpname_c(lumpname_c &rhs) { Set(rhs.data); }
	~lumpname_c() {};

private:
	char data[LUMPNAME_SIZE];

public:
	void Clear() { data[0] = '\0'; }

	const char* GetString() const { return data; }

	inline bool IsEmpty() const { return data[0] == '\0'; }

	void Set(const char *s) 
	{
		int i;

		for (i=0; i<(LUMPNAME_SIZE-1) && *s; i++, s++)
			data[i] = *s;

		data[i] = '\0';
	}

	lumpname_c& operator=(lumpname_c &rhs) 
	{
		if (&rhs != this) 
			Set(rhs.data);
			 
		return *this; 
	}
	
	char operator[](int idx) const { return data[idx]; }
	operator const char* () const { return data; }
};

class mobj_strref_c
{
public:
	mobj_strref_c() : name(), def(NULL) { }
	mobj_strref_c(const char *s) : name(s), def(NULL) { }
	mobj_strref_c(const mobj_strref_c &rhs) : name(rhs.name), def(NULL) { }
	~mobj_strref_c() {};

private:
	epi::strent_c name;

	const mobjtype_c *def;

public:
	const char *GetName() const { return name.GetString(); }

	const mobjtype_c *GetRef();
	// Note: this returns NULL if not found, in which case you should
	// produce an error, since future calls will do the search again.

	mobj_strref_c& operator= (mobj_strref_c &rhs) 
	{
		if (&rhs != this) 
		{
			name = rhs.name;
			def = NULL;
		}
			 
		return *this; 
	}
};


#include "ddf_mobj.h"
#include "ddf_atk.h"
#include "ddf_stat.h"
#include "ddf_weap.h"

#include "ddf_line.h"
#include "ddf_levl.h"
#include "ddf_game.h"

#include "ddf_mus.h"
#include "ddf_sfx.h"
#include "ddf_lang.h"


// Misc playsim constants
#define CEILSPEED   		1.0f
#define FLOORSPEED  		1.0f

#define GRAVITY     		8.0f
#define FRICTION    		0.9063f
#define VISCOSITY   		0.0f
#define DRAG        		0.99f
#define RIDE_FRICTION    	0.7f
#define LADDER_FRICTION  	0.8f

#define STOPSPEED   		0.07f
#define OOF_SPEED   		20.0f


// Info for the JUMP action
typedef struct act_jump_info_s
{
	// chance value
	percent_t chance; 
}
act_jump_info_t;


// ------------------------------------------------------------------
// -------------------------EXTERNALISATIONS-------------------------
// ------------------------------------------------------------------

void DDF_Init(void);
void DDF_CleanUp(void);
bool DDF_MainParseCondition(const char *str, condition_check_t *cond);
void DDF_MainGetWhenAppear(const char *info, void *storage);
void DDF_MainGetRGB(const char *info, void *storage);
bool DDF_MainDecodeBrackets(const char *info, char *outer, char *inner, int buf_len);
const char *DDF_MainDecodeList(const char *info, char divider, bool simple);
void DDF_GetLumpNameForFile(const char *filename, char *lumpname);

int DDF_CompareName(const char *A, const char *B);

bool DDF_CheckSprites(int st_low, int st_high);
bool DDF_WeaponIsUpgrade(weapondef_c *weap, weapondef_c *old);

bool DDF_IsBoomLineType(int num);
bool DDF_IsBoomSectorType(int num);
void DDF_BoomClearGenTypes(void);
linetype_c *DDF_BoomGetGenLine(int number);
sectortype_c *DDF_BoomGetGenSector(int number);

#endif /* __DDF_MAIN_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
