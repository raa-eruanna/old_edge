//----------------------------------------------------------------------------
//  EDGE Radius Trigger Parsing
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

#include "i_defs.h"
#include "rad_trig.h"

#include "dm_defs.h"
#include "dm_state.h"
#include "hu_lib.h"
#include "hu_stuff.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_swap.h"
#include "p_local.h"
#include "p_spec.h"
#include "r_defs.h"
#include "s_sound.h"
#include "v_res.h"
#include "w_wad.h"
#include "z_zone.h"
#include "version.h"


int rts_version;  // global

typedef struct define_s
{
	// next in list
	struct define_s *next;

	char *name;
	char *value;
}
define_t;

typedef struct rts_parser_s
{
	// needed level:
	//   -1 : don't care
	//    0 : outside any block
	//    1 : within START_MAP block
	//    2 : within RADIUSTRIGGER block
	int level;

	// name
	char *name;

	// number of parameters
	int min_pars, max_pars;

	// parser function
	void (* parser)(int pnum, const char ** pars);
}
rts_parser_t;


int rad_cur_linenum;
char *rad_cur_filename;
static char *rad_cur_linedata = NULL;

static char tokenbuf[4096];

// -AJA- 1999/09/12: Made all these static.  The variable `defines'
//       was clashing with the one in ddf_main.c.  ARGH !

// Define List
static define_t *defines;

// Determine whether the code blocks are started and terminated.
static int rad_cur_level = 0;

// For checking when #VERSION is used
static bool rad_has_start_map;

static const char *rad_level_names[3] =
{ "outer area", "map area", "trigger area" };

// Location of current script
static rad_script_t *this_rad;
static char *this_map = NULL;

// Pending state info for current script
static int pending_wait_tics = 0;
static char *pending_label = NULL;

// Default tip properties (position, colour, etc)
static s_tip_prop_t default_tip_props =
{ -1, -1, -1, -1, NULL, -1.0f, 0 };


int RAD_StringHashFunc(const char *s)
{
	int r = 0;
	int c;

	while (*s)
	{
		r *= 36;
		if (*s >= 'a' && *s <= 'z')
			c = *s - 'a';
		else if (*s >= 'A' && *s <= 'Z')
			c = *s - 'A';
		else if (*s >= '0' && *s <= '9')
			c = *s - '0' + 'Z' - 'A' + 1;
		else
			c = *s;
		r += c % 36;
		s++;
	}

	return r;
}

//
// RAD_Error
//
void RAD_Error(const char *err, ...)
{
	va_list argptr;
	char buffer[2048];
	char *pos;

	buffer[2047] = 0;

	// put actual message on first line
	va_start(argptr, err);
	vsprintf(buffer, err, argptr);
	va_end(argptr);

	pos = buffer + strlen(buffer);

	sprintf(pos, "Error occurred near line %d of %s\n", rad_cur_linenum, 
		rad_cur_filename);
	pos += strlen(pos);

	if (rad_cur_linedata)
	{
		sprintf(pos, "Line contents: %s\n", rad_cur_linedata);
		pos += strlen(pos);
	}

	// check for buffer overflow
	DEV_ASSERT(buffer[2047] == 0, ("Buffer overflow in RAD_Error"));

	// add a blank line for readability under DOS/Linux.  Two linefeeds
	// because the cursor may be at the end of a line with dots.
	I_Printf("\n\n");

	I_Error("%s", buffer);
}

void RAD_Warning(const char *err, ...)
{
	va_list argptr;
	char buffer[1024];

	if (no_warnings)
		return;

	va_start(argptr, err);
	vsprintf(buffer, err, argptr);
	va_end(argptr);

	I_Warning("\n");
	I_Warning("Found problem near line %d of %s\n", rad_cur_linenum, 
		rad_cur_filename);

	if (rad_cur_linedata)
		I_Warning("with line contents: %s\n", rad_cur_linedata);

	I_Warning("%s", buffer);
}

void RAD_WarnError(const char *err, ...)
{
	va_list argptr;
	char buffer[1024];

	va_start(argptr, err);
	vsprintf(buffer, err, argptr);
	va_end(argptr);

	if (strict_errors)
		RAD_Error("%s", buffer);
	else
		RAD_Warning("%s", buffer);
}

void RAD_WarnError2(int ver, const char *err, ...)
{
	va_list argptr;
	char buffer[1024];

	va_start(argptr, err);
	vsprintf(buffer, err, argptr);
	va_end(argptr);

	if (strict_errors || (rts_version >= ver && ! lax_errors))
		RAD_Error("%s", buffer);
	else
		RAD_Warning("%s", buffer);
}

static void RAD_ErrorSetLineData(const char *data)
{
	if (rad_cur_linedata)
		Z_Free(rad_cur_linedata);

	rad_cur_linedata = Z_StrDup(data);
}

static void RAD_ErrorClearLineData(void)
{
	if (rad_cur_linedata)
	{
		Z_Free(rad_cur_linedata);
		rad_cur_linedata = NULL;
	}
}


// Searches through the #defines namespace for a match and returns
// its value if it exists.
static bool CheckForDefine(const char *s, char ** val)
{
	define_t *tempnode = defines;

	for (; tempnode; tempnode = tempnode->next)
	{
		if (strcmp(s, tempnode->name) == 0)
		{
			*val = Z_StrDup(tempnode->value);
			return true;
		}
	}
	return false;
}

static void RAD_CheckForInt(const char *value, int *retvalue)
{
	const char *pos = value;
	int count = 0;
	int length = strlen(value);

	if (strchr(value, '%'))
		RAD_Error("Parameter '%s' should not be a percentage.\n", value);

	// Accomodate for "-" as you could have -5 or something like that.
	if (*pos == '-')
	{
		count++;
		pos++;
	}
	while (isdigit(*pos++))
		count++;

	// Is the value an integer?
	if (length != count)
		RAD_Error("Parameter '%s' is not of numeric type.\n", value);

	*retvalue = atoi(value);
}

static void RAD_CheckForFloat(const char *value, float *retvalue)
{
	if (strchr(value, '%'))
		RAD_Error("Parameter '%s' should not be a percentage.\n", value);

	if (sscanf(value, "%f", retvalue) != 1)
		RAD_Error("Parameter '%s' is not of numeric type.\n", value);
}

//
// RAD_CheckForPercent
//
// Reads percentages (0%..100%).
//
static void RAD_CheckForPercent(const char *info, void *storage)
{
	char s[101];
	char *p;
	float f;

	// just check that the string is valid
	Z_StrNCpy(s, info, 100);
	for (p = s; isdigit(*p) || *p == '.'; p++)
	{ /* nothing here */ }

	// the number must be followed by %
	if (*p != '%')
		RAD_Error("Parameter '%s' is not of percent type.\n", info);
	*p = 0;

	RAD_CheckForFloat(s, &f);
	if (f < 0.0f || f > 100.0f)
		RAD_Error("Percentage out of range: %s\n", info);

	*(percent_t *)storage = f / 100.0f;
}

//
// RAD_CheckForPercentAny
//
// Like the above routine, but don't limit to 0..100%.
//
static void RAD_CheckForPercentAny(const char *info, void *storage)
{
	char s[101];
	char *p;
	float f;

	// just check that the string is valid
	Z_StrNCpy(s, info, 100);
	for (p = s; isdigit(*p) || *p == '.'; p++)
	{ /* nothing here */ }

	// the number must be followed by %
	if (*p != '%')
		RAD_Error("Parameter '%s' is not of percent type.\n", info);
	*p = 0;

	RAD_CheckForFloat(s, &f);

	*(percent_t *)storage = f / 100.0f;
}

// -ES- Copied from DDF_MainGetTime.
// FIXME: Collect all functions that are common to DDF and RTS,
// and move them to a new module for RTS+DDF common code.
static void RAD_CheckForTime(const char *info, void *storage)
{
	float val;
	int *dest = (int *)storage;
	int i;
	char *s;

	DEV_ASSERT2(info && storage);

	// -ES- 1999/09/14 MAXT means that time should be maximal.
	if (!stricmp(info, "maxt"))
	{
		*dest = INT_MAX; // -ACB- 1999/09/22 Standards, Please.
		return;
	}

	s = strchr(info, 'T');

	if (!s)
		s = strchr(info, 't');

	if (s)
	{
		i = s-info;
		s = (char*)I_TmpMalloc(i + 1);
		Z_StrNCpy(s, info, i);
		RAD_CheckForInt(s, (int*)storage);
		I_TmpFree(s);
		return;
	}

	if (sscanf(info, "%f", &val) != 1)
	{
		I_Warning("Bad time value `%s'.\n", info);
		return;
	}

	*dest = (int)(val * (float)TICRATE);
}

static armour_type_e RAD_CheckForArmourType(const char *info)
{
	if (DDF_CompareName(info, "GREEN") == 0)
		return ARMOUR_Green;
	else if (DDF_CompareName(info, "BLUE") == 0)
		return ARMOUR_Blue;
	else if (DDF_CompareName(info, "YELLOW") == 0)
		return ARMOUR_Yellow;
	else if (DDF_CompareName(info, "RED") == 0)
		return ARMOUR_Red;

	RAD_Error("Unknown armour type: %s\n", info);
	return ARMOUR_Green; // (0 - No such thing as ARMOUR_None)
}

static changetex_type_e RAD_CheckForChangetexType(const char *info)
{
	if (DDF_CompareName(info, "LEFT_UPPER") == 0)
		return CHTEX_LeftUpper;
	else if (DDF_CompareName(info, "LEFT_MIDDLE") == 0)
		return CHTEX_LeftMiddle;
	else if (DDF_CompareName(info, "LEFT_LOWER") == 0)
		return CHTEX_LeftLower;
	if (DDF_CompareName(info, "RIGHT_UPPER") == 0)
		return CHTEX_RightUpper;
	else if (DDF_CompareName(info, "RIGHT_MIDDLE") == 0)
		return CHTEX_RightMiddle;
	else if (DDF_CompareName(info, "RIGHT_LOWER") == 0)
		return CHTEX_RightLower;
	else if (DDF_CompareName(info, "FLOOR") == 0)
		return CHTEX_Floor;
	else if (DDF_CompareName(info, "CEILING") == 0)
		return CHTEX_Ceiling;
	else if (DDF_CompareName(info, "SKY") == 0)
		return CHTEX_Sky;

	RAD_Error("Unknown ChangeTex type `%s'\n", info);
	return CHTEX_RightUpper; // (0 - No such thing as CHTEX_None)
}

//
// RAD_UnquoteString
//
// Remove the quotes from the given string, returning a newly
// allocated string.
//
static char *RAD_UnquoteString(const char *s)
{
	int tokenlen = 0;

	// skip initial quote
	s++;

	while (*s != '"')
	{
#ifdef DEVELOPERS
		if (*s == 0)
			I_Error("INTERNAL ERROR: bad string.\n");
#endif

		// -AJA- 1999/09/07: check for \n. Only temporary, awaiting bison...
		if (s[0] == '\\' && toupper(s[1]) == 'N')
		{
			tokenbuf[tokenlen++] = '\n';
			s += 2;
			continue;
		}

		tokenbuf[tokenlen++] = *s++;
	}

	tokenbuf[tokenlen] = 0;
	return Z_StrDup(tokenbuf);
}

static bool CheckForBoolean(const char *s)
{
	if (strcmp(s, "TRUE") == 0 || strcmp(s, "1") == 0)
		return true;

	if (strcmp(s, "FALSE") == 0 || strcmp(s, "0") == 0)
		return false;

	// Nope, it's an error.
	RAD_Error("Bad boolean value (should be TRUE or FALSE): %s\n", s);
	return false;
}

static void DoParsePlayerSet(const char *info, unsigned long *set)
{
	const char *p = info;
	const char *next;
	int num;

	*set = 0;

	if (DDF_CompareName(info, "ALL") == 0)
	{
		*set = ~0;
		return;
	}

	for (;;)
	{
		if (! isdigit(p[0]))
			RAD_Error("Bad number in set of players: %s\n", info);

		num = strtol(p, (char **) &next, 10);

		*set |= (1 << (num-1));

		p = next;

		if (p[0] == 0)
			break;

		if (p[0] != ':')
			RAD_Error("Missing `:' in set of players: %s\n", info);

		p++;
	}
}

// AddStateToScript
//
// Adds a new action state to the tail of the current set of states
// for the given radius trigger.
//
static void AddStateToScript(rad_script_t *R, int tics,
							 void (* action)(struct rad_trigger_s *R, mobj_t *actor, void *param), 
							 void *param)
{
	rts_state_t *state;

	state = Z_ClearNew(rts_state_t, 1);

	state->tics = tics;
	state->action = action;
	state->param = param;

	state->tics += pending_wait_tics;
	state->label = pending_label;

	pending_wait_tics = 0;
	pending_label = NULL;

	// link it in
	state->next = NULL;
	state->prev = R->last_state;

	if (R->last_state)
		R->last_state->next = state;
	else
		R->first_state = state;

	R->last_state = state;
}

//
// ClearOneScripts
//
static void ClearOneScript(rad_script_t *scr)
{
	Z_Free(scr->mapid);

	while (scr->boss_trig)
	{
		s_ondeath_t *cur = scr->boss_trig;
		scr->boss_trig = cur->next;

		Z_Free(cur);
	}

	while (scr->height_trig)
	{
		s_onheight_t *cur = scr->height_trig;
		scr->height_trig = cur->next;

		Z_Free(cur);
	}

	// free all states
	while (scr->first_state)
	{
		rts_state_t *cur = scr->first_state;
		scr->first_state = cur->next;

		if (cur->param)
			Z_Free(cur->param);

		Z_Free(cur);
	}
}

// ClearPreviousScripts
//
// Removes any radius triggers for a given map when start_map is used.
// Thus triggers in later RTS files/lumps replace those in earlier RTS
// files/lumps in the specified level.
// 
static void ClearPreviousScripts(const char *mapid)
{
	rad_script_t *scr, *next;

	for (scr=r_scripts; scr; scr=next)
	{
		next = scr->next;

		if (strcmp(scr->mapid, mapid) == 0)
		{
			// unlink and free it
			if (scr->next)
				scr->next->prev = scr->prev;

			if (scr->prev)
				scr->prev->next = scr->next;
			else
				r_scripts = scr->next;

			ClearOneScript(scr);

			Z_Free(scr);
		}
	}
}

// ClearAllScripts
//
// Removes all radius triggers from all maps.
// 
static void ClearAllScripts(void)
{
	while (r_scripts)
	{
		rad_script_t *scr = r_scripts;
		r_scripts = scr->next;

		ClearOneScript(scr);

		Z_Free(scr);
	}
}

//
// RAD_ComputeScriptCRC
//
static void RAD_ComputeScriptCRC(rad_script_t *scr)
{
	int flags;

	CRC32_Init(&scr->crc);

	// Note: the mapid doesn't belong in the CRC

	if (scr->script_name)
		CRC32_ProcessStr(&scr->crc, scr->script_name);

	CRC32_ProcessInt(&scr->crc, scr->tag);
	CRC32_ProcessInt(&scr->crc, scr->appear);
	CRC32_ProcessInt(&scr->crc, scr->min_players);
	CRC32_ProcessInt(&scr->crc, scr->max_players);
	CRC32_ProcessInt(&scr->crc, scr->repeat_count);

	CRC32_ProcessInt(&scr->crc, (int)scr->x);
	CRC32_ProcessInt(&scr->crc, (int)scr->y);
	CRC32_ProcessInt(&scr->crc, (int)scr->z);
	CRC32_ProcessInt(&scr->crc, (int)scr->rad_x);
	CRC32_ProcessInt(&scr->crc, (int)scr->rad_y);
	CRC32_ProcessInt(&scr->crc, (int)scr->rad_z);

	// lastly handle miscellaneous parts

#undef M_FLAG
#define M_FLAG(bit, cond)  \
	if cond { flags |= (1 << (bit)); }

	flags = 0;

	M_FLAG(0, (scr->tagged_disabled));
	M_FLAG(1, (scr->tagged_use));
	M_FLAG(2, (scr->tagged_independent));
	M_FLAG(3, (scr->tagged_immediate));

	M_FLAG(4, (scr->boss_trig != NULL));
	M_FLAG(5, (scr->height_trig != NULL));
	M_FLAG(6, (scr->cond_trig != NULL));
	M_FLAG(7, (scr->next_in_path != NULL));

	CRC32_ProcessInt(&scr->crc, flags);

	// Q/ add in states ?  
	// A/ Nah.

	CRC32_Done(&scr->crc);
}

#undef M_FLAG

// RAD_CollectParameters
//
// Collect the parameters from the line into an array of strings
// `pars', which can hold at most `max' string pointers.
// 
// -AJA- 2000/01/02: Moved #define handling to here.
//
static void RAD_CollectParameters(const char *line, int *pnum, 
								  char ** pars, int max)
{
	int tokenlen = -1;
	bool in_string = false;

	*pnum = 0;

	for (;;)
	{
		int ch = *line;

		if (in_string)
		{
			if (ch == 0)
				RAD_Error("Nonterminated string found.\n");

			if (ch == '"')
				in_string = false;

			tokenbuf[tokenlen++] = ch;
			line++;
			continue;
		}

		if (tokenlen >= 0)
		{
			// end of token ?
			if (ch == 0 || isspace(ch) ||
				ch == ';' || (line[0] == '/' && line[1] == '/'))
			{
				tokenbuf[tokenlen] = 0;
				tokenlen = -1;

				if (*pnum >= max)
					RAD_Error("Too many tokens on line\n");

				// check for defines 
				if (! CheckForDefine(tokenbuf, &pars[*pnum]))
					pars[*pnum] = Z_StrDup(tokenbuf);

				*pnum += 1;

				// end of line ?
				if (ch == 0 || ch == ';' || (line[0] == '/' && line[1] == '/'))
					break;

				line++;
				continue;
			}

			tokenbuf[tokenlen++] = ch;
			line++;
			continue;
		}

		// end of line ?
		if (ch == 0 || ch == ';' || (line[0] == '/' && line[1] == '/'))
			break;

		if (isspace(ch))
		{
			line++;
			continue;
		}

		// string ?
		if (ch == '"')
			in_string = true;

		// must be token
		tokenbuf[0] = ch;
		tokenlen = 1;
		line++;
		continue;
	}
}

// RAD_FreeParameters
//
// Free previously collected parameters.
// 
static void RAD_FreeParameters(int pnum, char **pars)
{
	while (pnum > 0)
	{
		Z_Free(pars[--pnum]);
	}
}


// ---- Primitive Parsers ----------------------------------------------

static void RAD_ParseVersion(int pnum, const char **pars)
{
	// #Version <vers>

	if (rad_has_start_map)
		RAD_Error("The #VERSION directive must appear before all scripts.\n");

	float vers;

	RAD_CheckForFloat(pars[1], &vers);

	if (vers < 0.99f || vers > 9.99f)
		RAD_Error("Illegal #VERSION number.\n");

	int decimal = (int)(100.0f * vers + 0.5f);

	rts_version = ((decimal / 100) << 8) |
				  (((decimal / 10) % 10) << 4) | (decimal % 10);

	// backwards compat (old scripts have #VERSION 1.1 in them)
	if (rts_version < 0x123)
		return;

	if (rts_version > EDGEVER)
		RAD_Error("This version of EDGE cannot handle this RTS script\n");
}

static void RAD_ParseClearAll(int pnum, const char **pars)
{
	// #ClearAll

	ClearAllScripts();
}

static void RAD_ParseDefine(int pnum, const char **pars)
{
	// #Define <identifier> <num>

	define_t *newdef;

	newdef = Z_ClearNew(define_t, 1);

	newdef->name  = Z_StrDup(pars[1]);
	newdef->value = Z_StrDup(pars[2]);

	// link it in
	newdef->next = defines;
	defines = newdef;
}

static void RAD_ParseStartMap(int pnum, const char **pars)
{
	// Start_Map <map>

	if (rad_cur_level != 0)
		RAD_Error("%s found, but previous END_MAP missing !\n", pars[0]);

	// -AJA- 1999/08/02: New scripts replace old ones.
	ClearPreviousScripts(pars[1]);

	this_map = Z_StrDup(pars[1]);
	strupr(this_map);

	rad_cur_level++;
	rad_has_start_map = true;
}

static void RAD_ParseRadiusTrigger(int pnum, const char **pars)
{
	// RadiusTrigger <x> <y> <radius>
	// RadiusTrigger <x> <y> <radius> <low z> <high z>
	//
	// RectTrigger <x1> <y1> <x2> <y2>
	// RectTrigger <x1> <y1> <x2> <y2> <z1> <z2>

	// -AJA- 1999/09/12: Reworked for having Z-restricted triggers.

	if (rad_cur_level == 2)
		RAD_Error("%s found, but previous END_RADIUSTRIGGER missing !\n",
		pars[0]);

	if (rad_cur_level == 0)
		RAD_Error("%s found, but without any START_MAP !\n", pars[0]);

	// Set the node up, from now on we can use rscript as it points
	// to the new node.

	this_rad = Z_ClearNew(rad_script_t, 1);

	// set defaults
	this_rad->z = 0;
	this_rad->rad_z = -1;
	this_rad->appear = DEFAULT_APPEAR;
	this_rad->min_players = 0;
	this_rad->max_players = INT_MAX;
	this_rad->netmode = RNET_Separate;
	this_rad->what_players = ~0;  // "ALL"
	this_rad->absolute_req_players = 1;
	this_rad->repeat_count = -1;
	this_rad->repeat_delay = 0;

	pending_wait_tics = 0;
	pending_label = NULL;

	if (DDF_CompareName("RECT_TRIGGER", pars[0]) == 0)
	{
		float x1, y1, x2, y2, z1, z2;

		if (pnum == 6)
			RAD_Error("%s: Wrong number of parameters.\n", pars[0]);

		RAD_CheckForFloat(pars[1], &x1);
		RAD_CheckForFloat(pars[2], &y1);
		RAD_CheckForFloat(pars[3], &x2);
		RAD_CheckForFloat(pars[4], &y2);

		if (x1 > x2)
			RAD_WarnError2(0x128, "%s: bad X range %1.1f to %1.1f\n", pars[0], x1, x2);
		if (y1 > y2)
			RAD_WarnError2(0x128, "%s: bad Y range %1.1f to %1.1f\n", pars[0], y1, y2);

		this_rad->x = (float)(x1 + x2) / 2.0f;
		this_rad->y = (float)(y1 + y2) / 2.0f;
		this_rad->rad_x = (float)fabs(x1 - x2) / 2.0f;
		this_rad->rad_y = (float)fabs(y1 - y2) / 2.0f;

		if (pnum >= 7)
		{
			RAD_CheckForFloat(pars[5], &z1);
			RAD_CheckForFloat(pars[6], &z2);

			if (z1 > z2 + 1)
				RAD_WarnError2(0x128, "%s: bad height range %1.1f to %1.1f\n",
				pars[0], z1, z2);

			this_rad->z = (z1 + z2) / 2.0f;
			this_rad->rad_z = fabs(z1 - z2) / 2.0f;
		}
	}
	else
	{
		if (pnum == 5)
			RAD_Error("%s: Wrong number of parameters.\n", pars[0]);

		RAD_CheckForFloat(pars[1], &this_rad->x);
		RAD_CheckForFloat(pars[2], &this_rad->y);
		RAD_CheckForFloat(pars[3], &this_rad->rad_x);

		this_rad->rad_y = this_rad->rad_x;

		if (pnum >= 6)
		{
			float z1, z2;

			RAD_CheckForFloat(pars[4], &z1);
			RAD_CheckForFloat(pars[5], &z2);

			if (z1 > z2)
				RAD_WarnError2(0x128, "%s: bad height range %1.1f to %1.1f\n",
				pars[0], z1, z2);

			this_rad->z = (z1 + z2) / 2.0f;
			this_rad->rad_z = fabs(z1 - z2) / 2.0f;
		}
	}

	// link it in
	this_rad->next = r_scripts;
	this_rad->prev = NULL;

	if (r_scripts)
		r_scripts->prev = this_rad;

	r_scripts = this_rad;

	rad_cur_level++;
}

static void RAD_ParseEndRadiusTrigger(int pnum, const char **pars)
{
	// End_RadiusTrigger

	if (rad_cur_level != 2)
		RAD_Error("%s found, but without any RADIUSTRIGGER !\n", pars[0]);

	// --- check stuff ---

	// handle any pending WAIT or LABEL values
	if (pending_wait_tics > 0 || pending_label)
	{
		AddStateToScript(this_rad, 0, RAD_ActNOP, NULL);
	}

	this_rad->mapid = Z_StrDup(this_map);
	RAD_ComputeScriptCRC(this_rad);
	this_rad = NULL;

	rad_itemsread++;
	rad_cur_level--;
}

static void RAD_ParseEndMap(int pnum, const char **pars)
{
	// End_Map

	if (rad_cur_level == 2)
		RAD_Error("%s found, but previous END_RADIUSTRIGGER missing !\n",
		pars[0]);

	if (rad_cur_level == 0)
		RAD_Error("%s found, but without any START_MAP !\n", pars[0]);

	this_map = NULL;

	rad_cur_level--;
}

static void RAD_ParseName(int pnum, const char **pars)
{
	// Name <name>

	if (this_rad->script_name)
		RAD_Error("Script already has a name: `%s'\n", this_rad->script_name);

	this_rad->script_name = Z_StrDup(pars[1]);
}

static void RAD_ParseTag(int pnum, const char **pars)
{
	// Tag <number>

	if (this_rad->tag != 0)
		RAD_Error("Script already has a tag: `%d'\n", this_rad->tag);

	RAD_CheckForInt(pars[1], &this_rad->tag);
}

static void RAD_ParseWhenAppear(int pnum, const char **pars)
{
	// When_Appear 1:2:3:4:5:SP:COOP:DM

	DDF_MainGetWhenAppear(pars[1], &this_rad->appear);
}

static void RAD_ParseWhenPlayerNum(int pnum, const char **pars)
{
	// When_Player_Num <num>
	// When_Player_Num <min> <max>

	RAD_CheckForInt(pars[1], &this_rad->min_players);
	this_rad->max_players = this_rad->min_players;

	if (pnum >= 3)
		RAD_CheckForInt(pars[2], &this_rad->max_players);

	if (this_rad->min_players < 0 || 
		this_rad->min_players > this_rad->max_players)
	{
		RAD_Error("Illegal playernum range: %d..%d\n",
			this_rad->min_players, this_rad->max_players);
	}
}

static void RAD_ParseNetMode(int pnum, const char **pars)
{
	// Net_Mode SEPARATE
	// Net_Mode SEPARATE <player set>
	//
	// Net_Mode ABSOLUTE
	// Net_Mode ABSOLUTE <min players>

	if (DDF_CompareName(pars[1], "SEPARATE") == 0)
	{
		this_rad->netmode = RNET_Separate;

		if (pnum >= 3)
			DoParsePlayerSet(pars[2], &this_rad->what_players);

		return;
	}

	if (DDF_CompareName(pars[1], "ABSOLUTE") == 0)
	{
		this_rad->netmode = RNET_Absolute;

		if (pnum >= 3)
		{
			if (DDF_CompareName(pars[2], "ALL") == 0)
				this_rad->absolute_req_players = -1;
			else
				RAD_CheckForInt(pars[2], &this_rad->absolute_req_players);
		}

		return;
	}

	RAD_Error("%s: unknown mode `%s'\n", pars[0], pars[1]);
}

static void RAD_ParseTaggedRepeatable(int pnum, const char **pars)
{
	// Tagged_Repeatable
	// Tagged_Repeatable <num repetitions>
	// Tagged_Repeatable <num repetitions> <delay>

	if (this_rad->repeat_count >= 0)
		RAD_Error("%s: can only be used once.\n", pars[0]);

	if (pnum >= 2)
		RAD_CheckForInt(pars[1], &this_rad->repeat_count);
	else
		this_rad->repeat_count = REPEAT_FOREVER;

	// -ES- 2000/03/03 Changed to RAD_CheckForTime.
	if (pnum >= 3)
		RAD_CheckForTime(pars[2], &this_rad->repeat_delay);
	else
		this_rad->repeat_delay = 1;
}

static void RAD_ParseTaggedUse(int pnum, const char **pars)
{
	// Tagged_Use

	this_rad->tagged_use = true;
}

static void RAD_ParseTaggedIndependent(int pnum, const char **pars)
{
	// Tagged_Independent

	this_rad->tagged_independent = true;
}

static void RAD_ParseTaggedImmediate(int pnum, const char **pars)
{
	// Tagged_Immediate

	this_rad->tagged_immediate = true;
}

static void RAD_ParseTaggedPlayerSpecific(int pnum, const char **pars)
{
	// Tagged_Player_Specific

	if (this_rad->netmode != RNET_Separate)
		RAD_Error("%s can only be used with NET_MODE SEPARATE\n", pars[0]);

	this_rad->tagged_player_specific = true;
}

static void RAD_ParseTaggedDisabled(int pnum, const char **pars)
{
	// Tagged_Disabled

	this_rad->tagged_disabled = true;
}

static void RAD_ParseTaggedPath(int pnum, const char **pars)
{
	// Tagged_Path  <next node>

	rts_path_t *path = Z_ClearNew(rts_path_t, 1);

	path->next = this_rad->next_in_path;
	path->name = Z_StrDup(pars[1]);

	this_rad->next_in_path = path;
	this_rad->next_path_total += 1;
}

static void RAD_ParsePathEvent(int pnum, const char **pars)
{
	// Path_Event  <label>

	const char *div;
	int i;

	if (this_rad->path_event_label)
		RAD_Error("%s: Can only be used once per trigger.\n", pars[0]);

	// parse the label name
	div = strchr(pars[1], ':');

	i = div ? (div - pars[1]) : strlen(pars[1]);

	if (i <= 0)
		RAD_Error("%s: Bad label `%s'.\n", pars[0], pars[2]);

	this_rad->path_event_label = Z_New(const char, i + 1);
	Z_StrNCpy((char *)this_rad->path_event_label, pars[1], i);

	this_rad->path_event_offset = div ? MAX(0, atoi(div+1) - 1) : 0;
}

static void RAD_ParseOnDeath(int pnum, const char **pars)
{
	// OnDeath <thing type>
	// OnDeath <thing type> <threshhold>

	s_ondeath_t *cond;

	cond = Z_ClearNew(s_ondeath_t, 1);
	cond->threshhold = 0;

	// get map thing
	if (pars[1][0] == '-' || pars[1][0] == '+' || isdigit(pars[1][0]))
	{
		RAD_CheckForInt(pars[1], &cond->thing_type);
	}
	else
		cond->thing_name = Z_StrDup(pars[1]);

	if (pnum >= 3)
	{
		RAD_CheckForInt(pars[2], &cond->threshhold);
	}

	// link it into list of ONDEATH conditions
	cond->next = this_rad->boss_trig;
	this_rad->boss_trig = cond;
}

static void RAD_ParseOnHeight(int pnum, const char **pars)
{
	// OnHeight <low Z> <high Z>
	// OnHeight <low Z> <high Z> <sector num>

	s_onheight_t *cond;

	cond = Z_ClearNew(s_onheight_t, 1);

	cond->sec_num = -1;

	RAD_CheckForFloat(pars[1], &cond->z1);
	RAD_CheckForFloat(pars[2], &cond->z2);

	if (cond->z1 > cond->z2)
		RAD_Error("%s: bad height range %1.1f..%1.1f\n", pars[0],
		cond->z1, cond->z2);

	// get sector reference
	if (pnum >= 4)
	{
		RAD_CheckForInt(pars[3], &cond->sec_num);
	}

	// link it into list of ONHEIGHT conditions
	cond->next = this_rad->height_trig;
	this_rad->height_trig = cond;
}

static void RAD_ParseOnCondition(int pnum, const char **pars)
{
	// OnCondition  <condition>

	condition_check_t *cond;

	cond = Z_ClearNew(condition_check_t, 1);

	if (! DDF_MainParseCondition(pars[1], cond))
	{
		Z_Free(cond);
		return;
	}

	// link it into list of ONCONDITION list
	cond->next = this_rad->cond_trig;
	this_rad->cond_trig = cond;
}

static void RAD_ParseLabel(int pnum, const char **pars)
{
	// Label <label>

	if (pending_label)
		RAD_Error("State already has a label: `%s'\n", pending_label);

	// handle any pending WAIT value
	if (pending_wait_tics > 0)
		AddStateToScript(this_rad, 0, RAD_ActNOP, NULL);

	pending_label = Z_StrDup(pars[1]);
}

static void RAD_ParseEnableScript(int pnum, const char **pars)
{
	// Enable_Script  <script name>
	// Disable_Script <script name>

	s_enabler_t *t;

	t = Z_ClearNew(s_enabler_t, 1);

	t->script_name = Z_StrDup(pars[1]);
	t->new_disabled = DDF_CompareName("DISABLE_SCRIPT", pars[0]) == 0;

	AddStateToScript(this_rad, 0, RAD_ActEnableScript, t);
}

static void RAD_ParseEnableTagged(int pnum, const char **pars)
{
	// Enable_Tagged  <tag num>
	// Disable_Tagged <tag num>

	s_enabler_t *t;

	t = Z_ClearNew(s_enabler_t, 1);

	RAD_CheckForInt(pars[1], &t->tag);

	if (t->tag <= 0)
		RAD_Error("Bad tag value: %s\n", pars[1]);

	t->new_disabled = DDF_CompareName("DISABLE_TAGGED", pars[0]) == 0;

	AddStateToScript(this_rad, 0, RAD_ActEnableScript, t);
}

static void RAD_ParseExitLevel(int pnum, const char **pars)
{
	// ExitLevel
	// ExitLevel <wait tics>

	s_exit_t *exit;

	exit = Z_ClearNew(s_exit_t, 1);
	exit->exittime = 10;

	if (pnum >= 2)
	{
		RAD_CheckForTime(pars[1], &exit->exittime);
	}

	AddStateToScript(this_rad, 0, RAD_ActExitLevel, exit);
}

static void RAD_ParseTip(int pnum, const char **pars)
{
	// Tip "<text>"
	// Tip "<text>" <time>
	// Tip "<text>" <time> <has sound>
	// Tip "<text>" <time> <has sound> <scale>
	//
	// (likewise for Tip_LDF)
	// (likewise for Tip_Graphic)

	s_tip_t *tip;

	tip = Z_ClearNew(s_tip_t, 1);

	tip->display_time = 3 * TICRATE;
	tip->playsound = false;
	tip->gfx_scale = 1.0f;

	if (DDF_CompareName(pars[0], "TIP_GRAPHIC") == 0)
	{
		tip->tip_graphic = Z_StrDup(pars[1]);
	}
	else if (DDF_CompareName(pars[0], "TIP_LDF") == 0)
	{
		tip->tip_ldf = Z_StrDup(pars[1]);
	}
	else if (pars[1][0] == '"')
	{
		tip->tip_text = RAD_UnquoteString(pars[1]);
	}
	else
		RAD_Error("Needed string for TIP command.\n");

	if (pnum >= 3)
		RAD_CheckForTime(pars[2], &tip->display_time);

	if (pnum >= 4)
		tip->playsound = CheckForBoolean(pars[3]);

	if (pnum >= 5)
	{
		if (rts_version < 0x128)
			RAD_Error("%s: Scaling only available with #VERSION 1.28 or higher.\n", pars[0]);

		if (! tip->tip_graphic)
			RAD_Error("%s: scale value only works with TIP_GRAPHIC.\n", pars[0]);

		RAD_CheckForFloat(pars[4], &tip->gfx_scale);
	}

	AddStateToScript(this_rad, 0, RAD_ActTip, tip);
}

static void RAD_ParseTipSlot(int pnum, const char ** pars)
{
	// Tip_Slot <slotnum>

	s_tip_prop_t *tp;

	tp = Z_New(s_tip_prop_t, 1);
	tp[0] = default_tip_props;

	RAD_CheckForInt(pars[1], &tp->slot_num);

	if (tp->slot_num < 1 || tp->slot_num > MAXTIPSLOT)
		RAD_Error("Bad tip slot `%d' -- must be between 1-%d\n",
		tp->slot_num, MAXTIPSLOT);

	tp->slot_num--;

	AddStateToScript(this_rad, 0, RAD_ActTipProps, tp);
}

static void RAD_ParseTipPos(int pnum, const char ** pars)
{
	// Tip_Set_Pos <x> <y>
	// Tip_Set_Pos <x> <y> <time>

	s_tip_prop_t *tp;

	tp = Z_New(s_tip_prop_t, 1);
	tp[0] = default_tip_props;

	RAD_CheckForPercentAny(pars[1], &tp->x_pos);
	RAD_CheckForPercentAny(pars[2], &tp->y_pos);

	if (pnum >= 4)
		RAD_CheckForTime(pars[3], &tp->time);

	AddStateToScript(this_rad, 0, RAD_ActTipProps, tp);
}

static void RAD_ParseTipColour(int pnum, const char ** pars)
{
	// Tip_Set_Colour <colmap ref>
	// Tip_Set_Colour <colmap ref> <time>

	s_tip_prop_t *tp;

	tp = Z_New(s_tip_prop_t, 1);
	tp[0] = default_tip_props;

	tp->colourmap_name = Z_StrDup(pars[1]);

	if (pnum >= 3)
		RAD_CheckForTime(pars[2], &tp->time);

	AddStateToScript(this_rad, 0, RAD_ActTipProps, tp);
}

static void RAD_ParseTipTrans(int pnum, const char ** pars)
{
	// Tip_Set_Trans <translucency>
	// Tip_Set_Trans <translucency> <time>

	s_tip_prop_t *tp;

	tp = Z_New(s_tip_prop_t, 1);
	tp[0] = default_tip_props;

	RAD_CheckForPercent(pars[1], &tp->translucency);

	if (pnum >= 3)
		RAD_CheckForTime(pars[2], &tp->time);

	AddStateToScript(this_rad, 0, RAD_ActTipProps, tp);
}

static void RAD_ParseTipAlign(int pnum, const char ** pars)
{
	// Tip_Set_Align  CENTER/LEFT

	s_tip_prop_t *tp;

	tp = Z_New(s_tip_prop_t, 1);
	tp[0] = default_tip_props;

	if (DDF_CompareName(pars[1], "CENTER") == 0 ||
		DDF_CompareName(pars[1], "CENTRE") == 0)
	{
		tp->left_just = 0;
	}
	else if (DDF_CompareName(pars[1], "LEFT") == 0)
	{
		tp->left_just = 1;
	}
	else
	{
		RAD_WarnError2(0x128, "TIP_POS: unknown justify method `%s'\n", pars[1]);
	}

	AddStateToScript(this_rad, 0, RAD_ActTipProps, tp);
}

static void RAD_ParseSpawnThing(int pnum, const char **pars)
{
	// SpawnThing <thingid>
	// SpawnThing <thingid> <angle>
	// SpawnThing <thingid> <x> <y>
	// SpawnThing <thingid> <x> <y> <angle>
	// SpawnThing <thingid> <x> <y> <angle> <z>
	// SpawnThing <thingid> <x> <y> <angle> <z> <slope>
	//
	// (likewise for SpawnThing_Ambush)
	// (likewise for SpawnThing_Flash)
	//
	// -ACB- 1998/08/06 Use mobjinfo_t linked list
	// -AJA- 1999/09/11: Extra fields for Z and slope.

	// -AJA- 1999/09/11: Reworked for spawning things at Z.

	s_thing_t *t;
	const char *angle_str;
	int val;

	t = Z_ClearNew(s_thing_t, 1);

	// set defaults
	t->x = this_rad->x;
	t->y = this_rad->y;

	if (this_rad->rad_z < 0)
		t->z = ONFLOORZ;
	else
		t->z = this_rad->z - this_rad->rad_z;

	t->ambush = DDF_CompareName("SPAWNTHING_AMBUSH", pars[0]) == 0;
	t->spawn_effect = DDF_CompareName("SPAWNTHING_FLASH", pars[0]) == 0;

	// get map thing
	if (pars[1][0] == '-' || pars[1][0] == '+' || isdigit(pars[1][0]))
	{
		RAD_CheckForInt(pars[1], &t->thing_type);
	}
	else
		t->thing_name = Z_StrDup(pars[1]);

	// get angle
	angle_str = (pnum == 3) ? pars[2] : 
	(pnum >= 5) ? pars[4] : NULL;

	if (angle_str) 
	{
		RAD_CheckForInt(angle_str, &val);

		if (ABS(val) <= 360)
			t->angle = FLOAT_2_ANG((float) val);
		else
			t->angle = val << 16;
	}

	// check for x & y, z, slope

	if (pnum >= 4)
	{
		RAD_CheckForFloat(pars[2], &t->x);
		RAD_CheckForFloat(pars[3], &t->y);
	}
	if (pnum >= 6)
	{
		RAD_CheckForFloat(pars[5], &t->z);
	}
	if (pnum >= 7)
	{
		RAD_CheckForFloat(pars[6], &t->slope);

		// FIXME: Merge with DDF_MainGetSlope someday.
		t->slope /= 45.0f;
	}

	AddStateToScript(this_rad, 0, RAD_ActSpawnThing, t);
}

static void RAD_ParsePlaySound(int pnum, const char **pars)
{
	// PlaySound <soundid>
	// PlaySound <soundid> <x> <y>
	// PlaySound <soundid> <x> <y> <z>
	//
	// PlaySound_BossMan <soundid>
	// PlaySound_BossMan <soundid> <x> <y>
	// PlaySound_BossMan <soundid> <x> <y> <z>
	//
	// -AJA- 1999/09/12: Reworked for playing sound at specific Z.

	s_sound_t *t;

	if (pnum == 3)
		RAD_Error("%s: Wrong number of parameters.\n", pars[0]);

	t = Z_ClearNew(s_sound_t, 1);

	if (DDF_CompareName(pars[0], "PLAYSOUND_BOSSMAN") == 0)
		t->kind = PSOUND_BossMan;
	else
		t->kind = PSOUND_Normal;

	t->soundid = DDF_SfxLookupSound(pars[1]);

	t->x = this_rad->x;
	t->y = this_rad->y;
	t->z = (this_rad->rad_z < 0) ? ONFLOORZ : this_rad->z;

	if (pnum >= 4)
	{
		RAD_CheckForFloat(pars[2], &t->x);
		RAD_CheckForFloat(pars[3], &t->y);
	}

	if (pnum >= 5)
	{
		RAD_CheckForFloat(pars[4], &t->z);
	}

	AddStateToScript(this_rad, 0, RAD_ActPlaySound, t);
}

static void RAD_ParseKillSound(int pnum, const char **pars)
{
	// KillSound

	AddStateToScript(this_rad, 0, RAD_ActKillSound, NULL);
}

static void RAD_ParseChangeMusic(int pnum, const char **pars)
{
	// ChangeMusic <playlist num>

	s_music_t *music;

	music = Z_ClearNew(s_music_t, 1);

	RAD_CheckForInt(pars[1], &music->playnum);

	music->looping = true;

	AddStateToScript(this_rad, 0, RAD_ActChangeMusic, music);
}

static void RAD_ParseDamagePlayer(int pnum, const char **pars)
{
	// DamagePlayer <amount>

	s_damagep_t *t;

	t = Z_ClearNew(s_damagep_t, 1);

	RAD_CheckForFloat(pars[1], &t->damage_amount);

	AddStateToScript(this_rad, 0, RAD_ActDamagePlayers, t);
}

// FIXME: use the benefit system
static void RAD_ParseHealPlayer(int pnum, const char **pars)
{
	// HealPlayer <amount>
	// HealPlayer <amount> <limit>

	s_healp_t *heal;

	heal = Z_ClearNew(s_healp_t, 1);

	RAD_CheckForFloat(pars[1], &heal->heal_amount);

	if (pnum < 3)
		heal->limit = MAXHEALTH;
	else
		RAD_CheckForFloat(pars[2], &heal->limit);

	if (heal->limit < 0 || heal->limit > MAXHEALTH)
		RAD_Error("Health limit out of range: %1.1f\n", heal->limit);

	if (heal->heal_amount < 0 || heal->heal_amount > heal->limit)
		RAD_Error("Health value out of range: %1.1f\n", heal->heal_amount);

	AddStateToScript(this_rad, 0, RAD_ActHealPlayers, heal);
}

// FIXME: use the benefit system
static void RAD_ParseGiveArmour(int pnum, const char **pars)
{
	// GiveArmour <type> <amount>
	// GiveArmour <type> <amount> <limit>

	s_armour_t *armour;

	armour = Z_ClearNew(s_armour_t, 1);

	armour->type = RAD_CheckForArmourType(pars[1]);

	RAD_CheckForFloat(pars[2], &armour->armour_amount);

	if (pnum < 4)
		armour->limit = MAXARMOUR;
	else
		RAD_CheckForFloat(pars[3], &armour->limit);

	if (armour->limit < 0 || armour->limit > MAXARMOUR)
		RAD_Error("Armour limit out of range: %1.1f\n", armour->limit);

	if (armour->armour_amount < 0 || armour->armour_amount > armour->limit)
		RAD_Error("Armour value out of range: %1.1f\n", armour->armour_amount);

	AddStateToScript(this_rad, 0, RAD_ActArmourPlayers, armour);
}

static void RAD_ParseGiveLoseBenefit(int pnum, const char **pars)
{
	// Give_Benefit  <benefit>
	//   or
	// Lose_Benefit  <benefit>

	s_benefit_t *sb;

	sb = Z_ClearNew(s_benefit_t, 1);

	if (DDF_CompareName(pars[0], "LOSE_BENEFIT") == 0)
		sb->lose_it = true;

	DDF_MobjGetBenefit(pars[1], &sb->benefit);

	AddStateToScript(this_rad, 0, RAD_ActBenefitPlayers, sb);
}

static void RAD_ParseDamageMonsters(int pnum, const char **pars)
{
	// Damage_Monsters <monster> <amount>

	s_damage_monsters_t *mon;

	mon = Z_ClearNew(s_damage_monsters_t, 1);

	// get monster type
	if (pars[1][0] == '-' || pars[1][0] == '+' || isdigit(pars[1][0]))
	{
		RAD_CheckForInt(pars[1], &mon->thing_type);
	}
	else if (DDF_CompareName(pars[1], "ANY") == 0)
		mon->thing_type = -1;
	else
		mon->thing_name = Z_StrDup(pars[1]);

	RAD_CheckForFloat(pars[2], &mon->damage_amount);

	AddStateToScript(this_rad, 0, RAD_ActDamageMonsters, mon);
}

static void RAD_ParseThingEvent(int pnum, const char **pars)
{
	// Thing_Event <thing> <label>

	s_thing_event_t *tev;
	const char *div;
	int i;

	tev = Z_ClearNew(s_thing_event_t, 1);

	// parse the object type
	if (pars[1][0] == '-' || pars[1][0] == '+' || isdigit(pars[1][0]))
		RAD_CheckForInt(pars[1], &tev->thing_type);
	else
		tev->thing_name = Z_StrDup(pars[1]);

	// parse the label name
	div = strchr(pars[2], ':');

	i = div ? (div - pars[2]) : strlen(pars[2]);

	if (i <= 0)
		RAD_Error("%s: Bad label `%s'.\n", pars[0], pars[2]);

	tev->label = Z_New(const char, i + 1);
	Z_StrNCpy((char *)tev->label, pars[2], i);

	tev->offset = div ? MAX(0, atoi(div+1) - 1) : 0;

	AddStateToScript(this_rad, 0, RAD_ActThingEvent, tev);
}

static void RAD_ParseSkill(int pnum, const char **pars)
{
	// Skill <skill> <respawn> <fastmonsters>

	s_skill_t *skill;
	int val;

	skill = Z_ClearNew(s_skill_t, 1);

	RAD_CheckForInt(pars[1], &val);

	skill->skill = (skill_t) (val - 1);
	skill->respawn = CheckForBoolean(pars[2]);
	skill->fastmonsters = CheckForBoolean(pars[3]);

	AddStateToScript(this_rad, 0, RAD_ActSkill, skill);
}

static void RAD_ParseGotoMap(int pnum, const char **pars)
{
	// GotoMap <map name>
	// GotoMap <map name> SKIP_ALL

	s_gotomap_t *go;

	go = Z_ClearNew(s_gotomap_t, 1);

	go->map_name = Z_StrDup(pars[1]);

	if (pnum >= 3)
	{
		if (DDF_CompareName(pars[2], "SKIP_ALL") == 0)
		{
			if (rts_version < 0x128)
				RAD_Error("%s: SKIP_ALL only available with #VERSION 1.28 or higher.\n", pars[0]);
			go->skip_all = true;
		}
		else
			RAD_WarnError2(0x128, "%s: expected `SKIP_ALL' but got `%s'.\n",
			pars[0], pars[2]);
	}

	AddStateToScript(this_rad, 0, RAD_ActGotoMap, go);
}

static void RAD_ParseMoveSector(int pnum, const char **pars)
{
	// MoveSector <tag> <amount> <ceil or floor>
	// MoveSector <tag> <amount> <ceil or floor> ABSOLUTE
	// 
	// backwards compatibility:
	//   SectorV <sector> <amount> <ceil or floor>

	s_movesector_t *secv;

	secv = Z_ClearNew(s_movesector_t, 1);
	secv->relative = true;

	RAD_CheckForInt(pars[1], &secv->tag);
	RAD_CheckForFloat(pars[2], &secv->value);

	if (DDF_CompareName(pars[3], "FLOOR") == 0)
		secv->is_ceiling = 0;
	else if (DDF_CompareName(pars[3], "CEILING") == 0)
		secv->is_ceiling = 1;
	else
		secv->is_ceiling = !CheckForBoolean(pars[3]);

	if (DDF_CompareName(pars[0], "SECTORV") == 0)
	{
		secv->secnum = secv->tag;
		secv->tag = 0;
	}
	else  // MOVE_SECTOR
	{
		if (secv->tag == 0)
			RAD_Error("%s: Invalid tag number: %d\n", pars[0], secv->tag);

		if (pnum >= 5)
		{
			if (DDF_CompareName(pars[4], "ABSOLUTE") == 0)
				secv->relative = false;
			else
				RAD_WarnError2(0x128, "%s: expected `ABSOLUTE' but got `%s'.\n",
				pars[0], pars[4]);
		}
	}

	AddStateToScript(this_rad, 0, RAD_ActMoveSector, secv);
}

static void RAD_ParseLightSector(int pnum, const char **pars)
{
	// LightSector <tag> <amount>
	// LightSector <tag> <amount> ABSOLUTE
	// 
	// backwards compatibility:
	//   SectorL <sector> <amount>

	s_lightsector_t *secl;

	secl = Z_ClearNew(s_lightsector_t, 1);
	secl->relative = true;

	RAD_CheckForInt(pars[1], &secl->tag);
	RAD_CheckForFloat(pars[2], &secl->value);

	if (DDF_CompareName(pars[0], "SECTORL") == 0)
	{
		secl->secnum = secl->tag;
		secl->tag = 0;
	}
	else  // LIGHT_SECTOR
	{
		if (secl->tag == 0)
			RAD_Error("%s: Invalid tag number: %d\n", pars[0], secl->tag);

		if (pnum >= 4)
		{
			if (DDF_CompareName(pars[3], "ABSOLUTE") == 0)
				secl->relative = false;
			else
				RAD_WarnError2(0x128, "%s: expected `ABSOLUTE' but got `%s'.\n",
				pars[0], pars[3]);
		}
	}

	AddStateToScript(this_rad, 0, RAD_ActLightSector, secl);
}

static void RAD_ParseActivateLinetype(int pnum, const char **pars)
{
	// Activate_LineType <linetype> <tag>

	s_lineactivator_t *lineact;

	lineact = Z_ClearNew(s_lineactivator_t, 1);

	RAD_CheckForInt(pars[1], &lineact->typenum);
	RAD_CheckForInt(pars[2], &lineact->tag);

	AddStateToScript(this_rad, 0, RAD_ActActivateLinetype, lineact);
}

static void RAD_ParseUnblockLines(int pnum, const char **pars)
{
	// Unblock_Lines <tag>

	s_lineunblocker_t *lineact;

	lineact = Z_ClearNew(s_lineunblocker_t, 1);

	RAD_CheckForInt(pars[1], &lineact->tag);

	AddStateToScript(this_rad, 0, RAD_ActUnblockLines, lineact);
}

static void RAD_ParseWait(int pnum, const char **pars)
{
	// Wait <time>

	int tics;

	RAD_CheckForTime(pars[1], &tics);

	if (tics <= 0)
		RAD_Error("%s: Invalid time: %d\n", pars[0], tics);

	pending_wait_tics += tics;
}

static void RAD_ParseJump(int pnum, const char **pars)
{
	// Jump <label>
	// Jump <label> <random chance>

	s_jump_t *jump;

	jump = Z_ClearNew(s_jump_t, 1);

	jump->label = Z_StrDup(pars[1]);
	jump->random_chance = PERCENT_MAKE(100);

	if (pnum >= 3)
		RAD_CheckForPercent(pars[2], &jump->random_chance);

	AddStateToScript(this_rad, 0, RAD_ActJump, jump);
}

static void RAD_ParseSleep(int pnum, const char **pars)
{
	// Sleep

	AddStateToScript(this_rad, 0, RAD_ActSleep, NULL);
}

static void RAD_ParseRetrigger(int pnum, const char **pars)
{
	// Retrigger

	if (! this_rad->tagged_independent)
		RAD_Error("%s can only be used with TAGGED_INDEPENDENT.\n", pars[0]);

	AddStateToScript(this_rad, 0, RAD_ActRetrigger, NULL);
}

static void RAD_ParseChangeTex(int pnum, const char **pars)
{
	// Change_Tex <where> <texname>
	// Change_Tex <where> <texname> <tag>
	// Change_Tex <where> <texname> <tag> <subtag>

	s_changetex_t *ctex;

	if (strlen(pars[2]) > 8)
		RAD_Error("%s: Texture name too long: %s\n", pars[0], pars[2]);

	ctex = Z_ClearNew(s_changetex_t, 1);

	ctex->what = RAD_CheckForChangetexType(pars[1]);
	ctex->tag = ctex->subtag = 0;

	strcpy(ctex->texname, pars[2]);

	if (pnum >= 4)
		RAD_CheckForInt(pars[3], &ctex->tag);

	if (pnum >= 5)
		RAD_CheckForInt(pars[4], &ctex->subtag);

	AddStateToScript(this_rad, 0, RAD_ActChangeTex, ctex);
}


//  PARSER TABLE

static rts_parser_t radtrig_parsers[] =
{
	// directives...
	{-1, "#DEFINE",  3,3, RAD_ParseDefine},
	{0, "#VERSION",  2,2, RAD_ParseVersion},
	{0, "#CLEARALL", 1,1, RAD_ParseClearAll},

	// basics...
	{-1, "START_MAP", 2,2, RAD_ParseStartMap},
	{-1, "RADIUS_TRIGGER", 4,6, RAD_ParseRadiusTrigger},
	{-1, "RECT_TRIGGER", 5,7, RAD_ParseRadiusTrigger},
	{-1, "END_RADIUSTRIGGER", 1,1, RAD_ParseEndRadiusTrigger},
	{-1, "END_MAP",  1,1, RAD_ParseEndMap},

	// properties...
	{2, "NAME", 2,2, RAD_ParseName},
	{2, "TAG",  2,2, RAD_ParseTag},
	{2, "WHEN_APPEAR", 2,2, RAD_ParseWhenAppear},
	{2, "WHEN_PLAYER_NUM",   2,3, RAD_ParseWhenPlayerNum},
	{2, "NET_MODE", 2,3, RAD_ParseNetMode},
	{2, "TAGGED_REPEATABLE", 1,3, RAD_ParseTaggedRepeatable},
	{2, "TAGGED_USE", 1,1, RAD_ParseTaggedUse},
	{2, "TAGGED_INDEPENDENT", 1,1, RAD_ParseTaggedIndependent},
	{2, "TAGGED_IMMEDIATE",   1,1, RAD_ParseTaggedImmediate},
	{2, "TAGGED_PLAYER_SPECIFIC", 1,1, RAD_ParseTaggedPlayerSpecific},
	{2, "TAGGED_DISABLED", 1,1, RAD_ParseTaggedDisabled},
	{2, "TAGGED_PATH", 2,2, RAD_ParseTaggedPath},
	{2, "PATH_EVENT", 2,2, RAD_ParsePathEvent},
	{2, "ONDEATH",  2,3, RAD_ParseOnDeath},
	{2, "ONHEIGHT", 3,4, RAD_ParseOnHeight},
	{2, "ONCONDITION",  2,2, RAD_ParseOnCondition},
	{2, "LABEL", 2,2, RAD_ParseLabel},

	// actions...
	{2, "TIP",     2,5, RAD_ParseTip},
	{2, "TIP_LDF", 2,5, RAD_ParseTip},
	{2, "TIP_GRAPHIC", 2,5, RAD_ParseTip},
	{2, "TIP_SLOT",    2,2, RAD_ParseTipSlot},
	{2, "TIP_SET_POS",    3,4, RAD_ParseTipPos},
	{2, "TIP_SET_COLOUR", 2,3, RAD_ParseTipColour},
	{2, "TIP_SET_TRANS",  2,3, RAD_ParseTipTrans},
	{2, "TIP_SET_ALIGN",  2,2, RAD_ParseTipAlign},
	{2, "EXITLEVEL", 1,2, RAD_ParseExitLevel},
	{2, "SPAWNTHING", 2,7, RAD_ParseSpawnThing},
	{2, "SPAWNTHING_AMBUSH", 2,7, RAD_ParseSpawnThing},
	{2, "SPAWNTHING_FLASH",  2,7, RAD_ParseSpawnThing},
	{2, "PLAYSOUND", 2,5, RAD_ParsePlaySound},
	{2, "PLAYSOUND_BOSSMAN", 2,5, RAD_ParsePlaySound},
	{2, "KILLSOUND", 1,1, RAD_ParseKillSound},
	{2, "HEALPLAYER",   2,3, RAD_ParseHealPlayer},
	{2, "GIVEARMOUR",   3,4, RAD_ParseGiveArmour},
	{2, "DAMAGEPLAYER", 2,2, RAD_ParseDamagePlayer},
	{2, "GIVE_BENEFIT", 2,2, RAD_ParseGiveLoseBenefit},
	{2, "LOSE_BENEFIT", 2,2, RAD_ParseGiveLoseBenefit},
	{2, "DAMAGE_MONSTERS", 3,3, RAD_ParseDamageMonsters},
	{2, "THING_EVENT", 3,3, RAD_ParseThingEvent},
	{2, "SKILL",   4,4, RAD_ParseSkill},
	{2, "GOTOMAP", 2,3, RAD_ParseGotoMap},
	{2, "MOVE_SECTOR", 4,5, RAD_ParseMoveSector},
	{2, "LIGHT_SECTOR", 3,4, RAD_ParseLightSector},
	{2, "ENABLE_SCRIPT",  2,2, RAD_ParseEnableScript},
	{2, "DISABLE_SCRIPT", 2,2, RAD_ParseEnableScript},
	{2, "ENABLE_TAGGED",  2,2, RAD_ParseEnableTagged},
	{2, "DISABLE_TAGGED", 2,2, RAD_ParseEnableTagged},
	{2, "ACTIVATE_LINETYPE", 3,3, RAD_ParseActivateLinetype},
	{2, "UNBLOCK_LINES", 2,2, RAD_ParseUnblockLines},
	{2, "WAIT",  2,2, RAD_ParseWait},
	{2, "JUMP",  2,3, RAD_ParseJump},
	{2, "SLEEP", 1,1, RAD_ParseSleep},
	{2, "RETRIGGER", 1,1, RAD_ParseRetrigger},
	{2, "CHANGE_TEX", 3,5, RAD_ParseChangeTex},
	{2, "CHANGE_MUSIC", 2,2, RAD_ParseChangeMusic},

	// deprecated primitives
	{2, "!SECTORV", 4,4, RAD_ParseMoveSector},
	{2, "!SECTORL", 3,3, RAD_ParseLightSector},

	// that's all, folks.
	{0, NULL, 0,0, NULL}
};

//
// Primitive Parser
//
void RAD_ParseLine(char *s)
{
	int pnum;
	char *pars[16];
	rts_parser_t *cur;

	RAD_ErrorSetLineData(s);

	RAD_CollectParameters(s, &pnum, pars, 16);

	if (pnum == 0)
	{
		RAD_ErrorClearLineData();
		return;
	}

	for (cur = radtrig_parsers; cur->name != NULL; cur++)
	{
		const char *cur_name = cur->name;
		bool obsolete = false;

		if (cur_name[0] == '!')
		{
			cur_name++;
			obsolete = true;

		}

		if (DDF_CompareName(pars[0], cur_name) != 0)
			continue;

		if (obsolete)
		{
			if (rts_version >= 0x128)
				RAD_Error("%s: is not supported with #VERSION 1.28 or higher.\n", cur_name);

			if (no_obsoletes)
				RAD_WarnError("The rts %s command is obsolete !\n", cur_name);
		}

		// check level
		if (cur->level >= 0)
		{
			if (cur->level != rad_cur_level)
			{
				RAD_Error("RTS command `%s' used in wrong place "
					"(found in %s, should be in %s).\n", pars[0],
					rad_level_names[rad_cur_level],
					rad_level_names[cur->level]);

				RAD_FreeParameters(pnum, pars);
				RAD_ErrorClearLineData();

				return;
			}
		}

		// check number of parameters.  Too many is live-with-able, but
		// not enough is fatal.

		if (pnum < cur->min_pars)
			RAD_Error("%s: Not enough parameters.\n", cur->name);

		if (pnum > cur->max_pars)
			RAD_WarnError("%s: Too many parameters.\n", cur->name);

		// found it, invoke the parser function
		(* cur->parser)(pnum, (const char **) pars);

		RAD_FreeParameters(pnum, pars);
		RAD_ErrorClearLineData();

		return;
	}

	RAD_WarnError2(0x128, "Unknown primitive: %s\n", pars[0]);

	RAD_FreeParameters(pnum, pars);
	RAD_ErrorClearLineData();
}

void RAD_ParserBegin(void)
{
	rad_cur_level = 0;
	rad_has_start_map = false;

	rts_version = 0x127;
}

void RAD_ParserDone(void)
{
	if (rad_cur_level >= 2)
		RAD_Error("RADIUSTRIGGER: block not terminated !\n");

	if (rad_cur_level == 1)
		RAD_Error("START_MAP: block not terminated !\n");
}

