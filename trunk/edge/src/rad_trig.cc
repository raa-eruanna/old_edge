//----------------------------------------------------------------------------
//  EDGE Radius Trigger / Tip Code
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
//
// -KM- 1998/11/25 Fixed problems created by DDF.
//   Radius Triggers can be added to wad files.  RSCRIPT is the lump.
//   Tip function can handle graphics.
//   New functions: ondeath, #version
//   Radius Triggers with radius < 0 affect entire map.
//   Radius triggers used to save compatibility with hacks in Doom/Doom2 
//       (eg MAP07, E2M8, E3M8, MAP32 etc..)
//
// -AJA- 1999/10/23: Began work on a state model for RTS actions.
//
// -AJA- 1999/10/24: Split off actions into rad_act.c, and structures
//       into the rad_main.h file.
//
// -AJA- 2000/01/04: Split off parsing code into rad_pars.c.
//

#include "i_defs.h"
#include "rad_trig.h"

#include "dm_defs.h"
#include "dm_state.h"
#include "e_main.h"
#include "hu_lib.h"
#include "hu_stuff.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_random.h"
#include "m_swap.h"
#include "p_local.h"
#include "p_spec.h"
#include "r_defs.h"
#include "s_sound.h"
#include "v_res.h"
#include "w_wad.h"
#include "z_zone.h"


// Static Scripts.  Never change once all scripts have been read in.
rad_script_t *r_scripts = NULL;

// Dynamic Triggers.  These only exist for the current level.
rad_trigger_t *r_triggers = NULL;

// # Triggers
int rad_itemsread = 0;

// Current RTS file or lump being parsed.
static byte *rad_memfile;
static byte *rad_memfile_end;
static byte *rad_memptr;
static int rad_memfile_size;


//
// RAD_FindScriptByName
//
rad_script_t * RAD_FindScriptByName(const char *map_name, const char *name)
{
  rad_script_t *scr;

  for (scr=r_scripts; scr; scr=scr->next)
  {
    if (scr->script_name == NULL)
      continue;

    if (strcmp(scr->mapid, map_name) != 0)
      continue;

    if (DDF_CompareName(scr->script_name, name) == 0)
      return scr;
  }

  I_Error("RTS: No such script `%s' on map %s.\n", name, map_name);
  return NULL;
}

//
// RAD_FindTriggerByName
//
rad_trigger_t * RAD_FindTriggerByName(const char *name)
{
  rad_trigger_t *trig;

  for (trig=r_triggers; trig; trig=trig->next)
  {
    if (trig->info->script_name == NULL)
      continue;

    if (DDF_CompareName(trig->info->script_name, name) == 0)
      return trig;
  }

  I_Warning("RTS: No such trigger `%s'.\n", name);
  return NULL;
}

//
// RAD_FindTriggerByScript
//
rad_trigger_t * RAD_FindTriggerByScript(const rad_script_t *scr)
{
  rad_trigger_t *trig;

  for (trig=r_triggers; trig; trig=trig->next)
  {
    if (trig->info == scr)
      return trig;
  }

  return NULL;  // no worries if none.
}

//
// RAD_FindStateByLabel
//
rts_state_t * RAD_FindStateByLabel(rad_script_t *scr, char *label)
{
  rts_state_t *st;

  for (st=scr->first_state; st; st=st->next)
  {
    if (st->label == NULL)
      continue;

    if (DDF_CompareName(st->label, label) == 0)
      return st;
  }

  // NOTE: no error message, unlike the other Find funcs
  return NULL;
}

//
// RAD_EnableByTag
//
// Looks for all current triggers with the given tag number, and
// either enables them or disables them (based on `disable').
// Actor can be NULL.
//
void RAD_EnableByTag(mobj_t *actor, int tag, bool disable)
{
  rad_trigger_t *trig;

  if (tag <= 0)
    I_Error("INTERNAL ERROR: RAD_EnableByTag: bad tag %d\n", tag);

  for (trig=r_triggers; trig; trig=trig->next)
  {
    if (trig->info->tag == tag)
      break;
  }

  // were there any ?
  if (! trig)
    return;
  
  for (; trig; trig=trig->tag_next)
  {
    if (disable)
      trig->disabled = true;
    else
      trig->disabled = false;
  }
}

bool RAD_WithinRadius(mobj_t * mo, rad_script_t * r)
{
  if (r->rad_x >= 0 && fabs(r->x - mo->x) > r->rad_x + mo->radius)
    return false;

  if (r->rad_y >= 0 && fabs(r->y - mo->y) > r->rad_y + mo->radius)
    return false;

  if (r->rad_z >= 0 && fabs(r->z - MO_MIDZ(mo)) > r->rad_z + mo->height/2)
  {
    return false;
  }

  return true;
}

//
// RAD_CheckBossTrig
//
static bool RAD_CheckBossTrig(rad_trigger_t *trig, s_ondeath_t *cond)
{
  mobj_t *mo;

  int count = 0;

  // lookup thing type if we haven't already done so
  if (! cond->cached_info)
  {
    if (cond->thing_name)
      cond->cached_info = DDF_MobjLookup(cond->thing_name);
    else
    {
      cond->cached_info = DDF_MobjLookupNum(cond->thing_type);
      
      if (cond->cached_info == NULL)
        I_Error("RTS ONDEATH: Unknown thing type %d.\n",
          cond->thing_type);
    }
  }
  
  // scan the remaining mobjs to see if all bosses are dead
  for (mo=mobjlisthead; mo != NULL; mo=mo->next)
  {
    if (mo->info == cond->cached_info && mo->health > 0)
    {
      count++;
    
      if (count > cond->threshhold)
        return false;
    }
  }

  return true;
}

//
// RAD_CheckHeightTrig
//
static bool RAD_CheckHeightTrig(rad_trigger_t *trig, 
    s_onheight_t *cond)
{
  float h;

  // lookup sector if we haven't already done so
  if (! cond->cached_sector)
  {
    if (cond->sec_num >= 0)
    {
      if (cond->sec_num >= numsectors)
        I_Error("RTS ONHEIGHT: no such sector %d.\n", cond->sec_num);
      
      cond->cached_sector = & sectors[cond->sec_num];
    }
    else
    {
      cond->cached_sector = R_PointInSubsector(trig->info->x, 
          trig->info->y)->sector;
    }
  }

  h = cond->cached_sector->f_h;

  return (cond->z1 <= h && h <= cond->z2);
}

bool RAD_CheckReachedTrigger(mobj_t * thing)
{
  rad_script_t * scr = (rad_script_t *) thing->path_trigger;
  rad_trigger_t * trig;

  rts_path_t *path;
  int choice;

  if (! RAD_WithinRadius(thing, scr))
    return false;

  // Thing has reached this path node. Update so it starts following
  // the next node.  Handle any PATH_EVENT too.  Enable the associated
  // trigger (could be none if there were no states).

  trig = RAD_FindTriggerByScript(scr);

  if (trig)
    trig->disabled = false;

  if (scr->path_event_label)
  {
    statenum_t state = P_MobjFindLabel(thing, scr->path_event_label);

    if (state)
      P_SetMobjStateDeferred(thing, state + scr->path_event_offset, 0);
  }
 
  if (scr->next_path_total == 0)
  {
    thing->path_trigger = NULL;
    return true;
  }
  else if (scr->next_path_total == 1)
    choice = 0;
  else
    choice = P_Random() % scr->next_path_total;

  path = scr->next_in_path;
  DEV_ASSERT2(path);

  for (; choice > 0; choice--)
  {
    path = path->next;
    DEV_ASSERT2(path);
  }

  if (! path->cached_scr)
    path->cached_scr = RAD_FindScriptByName(scr->mapid, path->name);
 
  DEV_ASSERT2(path->cached_scr);

  thing->path_trigger = path->cached_scr;
  return true;
}

static void DoRemoveTrigger(rad_trigger_t *trig)
{
  // handle tag linkage
  if (trig->tag_next)
    trig->tag_next->tag_prev = trig->tag_prev;

  if (trig->tag_prev)
    trig->tag_prev->tag_next = trig->tag_next;

  // unlink and free it
  if (trig->next)
    trig->next->prev = trig->prev;

  if (trig->prev)
    trig->prev->next = trig->next;
  else
    r_triggers = trig->next;

  S_AddToFreeQueue((mobj_t *)&trig->soundorg, (void *)trig);
}

// Called by P_PlayerThink
// Radius Trigger Event handler.
//
void RAD_DoRadiTrigger(player_t * p)
{
  rad_trigger_t *trig, *next;

  // Start looking through the trigger list.
  for (trig=r_triggers; trig; trig=next)
  {
    next = trig->next;

    // Don't process, if disabled
    if (trig->disabled)
      continue;

    // Handle repeat delay (from TAGGED_REPEATABLE).  This must be
    // done *before* all the condition checks, and that's what makes
    // it different from `wait_tics'.
    //
    if (trig->repeat_delay > 0)
    {
      trig->repeat_delay--;
      continue;
    }
 
    // Independent, means you don't have to stay within the trigger
    // radius for it to operate, It will operate on it's own.

    if (! (trig->info->tagged_independent && trig->activated))
    {
      // Immediate triggers are just that. Immediate.
      // Not within range so skip it.
      //
      if (!trig->info->tagged_immediate && 
          !RAD_WithinRadius(p->mo, trig->info))
        continue;

      // Check for use key trigger.
      if (trig->info->tagged_use && !p->usedown)
        continue;
    
      // height check...
      if (trig->info->height_trig)
      {
        s_onheight_t *cur;

        for (cur=trig->info->height_trig; cur; cur=cur->next)
          if (! RAD_CheckHeightTrig(trig, cur))
            break;
        
        // if they all succeeded, then cur will be NULL...
        if (cur)
          continue;
      }

      // ondeath check...
      if (trig->info->boss_trig)
      {
        s_ondeath_t *cur;

        for (cur=trig->info->boss_trig; cur; cur=cur->next)
          if (! RAD_CheckBossTrig(trig, cur))
            break;
        
        // if they all succeeded, then cur will be NULL...
        if (cur)
          continue;
      }

      // condition check...
      if (trig->info->cond_trig)
      {
        if (! G_CheckConditions(p->mo, trig->info->cond_trig))
          continue;
      }

      trig->activated = true;
    }

    // If we are waiting, decrement count and skip it.
    // Note that we must do this *after* all the condition checks.
    //
    if (trig->wait_tics > 0)
    {
      trig->wait_tics--;
      continue;
    }
 
    // Execute the commands
    while (trig->wait_tics == 0)
    {
      rts_state_t *state = trig->state;

      // move to next state.  We do this NOW since the action itself
      // may want to change the trigger's state (to support GOTO type
      // actions and other possibilities).
      //
      trig->state = trig->state->next;

      (*state->action)(trig, p->mo, state->param);

      if (trig->state == NULL || trig->disabled)
        break;

      trig->wait_tics += trig->state->tics;
    }

    if (trig->state)
      continue;

    // we've reached the end of the states.  Delete the trigger unless
    // it is Tagged_Repeatable and has some more repeats left.
    //
	if (trig->info->repeat_count != REPEAT_FOREVER)
		trig->repeats_left--;

    if (trig->repeats_left > 0)
    {
      trig->state = trig->info->first_state;
      trig->wait_tics = trig->state->tics;
      trig->repeat_delay = trig->info->repeat_delay;
      continue;
    }

    DoRemoveTrigger(trig);
  }
}

//
// RAD_GroupTriggerTags
//
// Called from RAD_SpawnTriggers to set the tag_next & tag_prev fields
// of each rad_trigger_t, keeping all triggers with the same tag in a
// linked list for faster handling.
//
// Also sets up `soundorg'.
//
void RAD_GroupTriggerTags(rad_trigger_t *trig)
{
  rad_trigger_t *cur;

  trig->soundorg.x = trig->info->x;
  trig->soundorg.y = trig->info->y;
  trig->soundorg.z = 0.0f;
     
  trig->tag_next = trig->tag_prev = NULL;

  // find first trigger with the same tag #
  for (cur=r_triggers; cur; cur=cur->next)
  {
    if (cur == trig)
      continue;

    if (cur->info->tag == trig->info->tag)
      break;
  }

  if (! cur)
    return;

  // link it in

  trig->tag_next = cur;
  trig->tag_prev = cur->tag_prev;

  if (cur->tag_prev)
    cur->tag_prev->tag_next = trig;
  
  cur->tag_prev = trig;
}

//
// RAD_SpawnTriggers
//
void RAD_SpawnTriggers(char *map_name)
{
  rad_script_t *scr;
  rad_trigger_t *trig;

#ifdef DEVELOPERS
  if (r_triggers)
    I_Error("RAD_SpawnTriggers without RAD_ClearTriggers\n");
#endif

  for (scr=r_scripts; scr; scr=scr->next)
  {
    // This is from a different map!
    if (strcmp(map_name, scr->mapid) != 0)
      continue;

    // -AJA- 1999/09/25: Added skill checks.
    if (! G_CheckWhenAppear(scr->appear))
      continue;

    // -AJA- 2000/02/03: Added player num checks.
    if (doomcom->numplayers < scr->min_players ||
        doomcom->numplayers > scr->max_players)
    {
      continue;
    }
    
    // ignore empty scripts (e.g. path nodes)
    if (! scr->first_state)
      continue;

    // OK, spawn new dynamic trigger
    trig = Z_ClearNew(rad_trigger_t, 1);

    trig->info = scr;
    trig->disabled = scr->tagged_disabled;
    trig->repeats_left = (scr->repeat_count < 0 || 
	                      scr->repeat_count == REPEAT_FOREVER) ? 1 : scr->repeat_count;
    trig->repeat_delay = 0;
    trig->tip_slot = 0;

    RAD_GroupTriggerTags(trig);

    // initialise state machine
    trig->state = scr->first_state;
    trig->wait_tics = scr->first_state->tics;

    // link it in
    trig->next = r_triggers;
    trig->prev = NULL;

    if (r_triggers)
      r_triggers->prev = trig;
    
    r_triggers = trig;
  }
}

//
// RAD_ClearCachedInfo
//
static void RAD_ClearCachedInfo(void)
{
  rad_script_t *scr;
  s_ondeath_t *d_cur;
  s_onheight_t *h_cur;

  for (scr=r_scripts; scr; scr=scr->next)
  {
    // clear ONDEATH cached info
    for (d_cur=scr->boss_trig; d_cur; d_cur=d_cur->next)
    {
      d_cur->cached_info = NULL;
    }

    // clear ONHEIGHT cached info
    for (h_cur=scr->height_trig; h_cur; h_cur=h_cur->next)
    {
      h_cur->cached_sector = NULL;
    }
  }
}

//
// RAD_ClearTriggers
//
void RAD_ClearTriggers(void)
{
  // remove all dynamic triggers
  while (r_triggers)
  {
    rad_trigger_t *trig = r_triggers;
    r_triggers = trig->next;

    Z_Free(trig);
  }

  RAD_ClearCachedInfo();
  RAD_ResetTips();
}

//
// Loads the script file into memory for parsing.
//
// -AJA- 2000/01/04: written, based on DDF_MainCacheFile
// -AJA- FIXME: merge them both into a single utility routine.
//       (BETTER: a single utility parsing module).
//
static void RAD_MainCacheFile(const char *filename)
{
  FILE *file;

  // open the file
  file = fopen(filename, "rb");

  if (file == NULL)
    I_Error("\nRAD_MainReadFile: Unable to open: '%s'", filename);

  // get to the end of the file
  fseek(file, 0, SEEK_END);

  // get the size
  rad_memfile_size = ftell(file);

  // reset to beginning
  fseek(file, 0, SEEK_SET);

  // malloc the size
  rad_memfile = Z_New(byte, rad_memfile_size + 1);
  rad_memfile_end = &rad_memfile[rad_memfile_size];

  // read the goodies
  fread(rad_memfile, 1, rad_memfile_size, file);

  // null Terminated string.
  rad_memfile[rad_memfile_size] = 0;

  // close the file
  fclose(file);
}

//
// RAD_ParseScript
//
// -ACB- 1998/07/10 Renamed function and used I_Print for functions,
//                  Version displayed at all times.
//
static void RAD_ParseScript(bool dots)
{
  char str[MAXSTRLEN];
  int n = 0;

  RAD_ParserBegin();
  
  rad_cur_linenum = 1;
  rad_memptr = rad_memfile;

  while (rad_memptr < rad_memfile_end)
  {
    int sp;

    for (sp=0; rad_memptr < rad_memfile_end && rad_memptr[0] != '\n'; 
         rad_memptr++)
    {
      // ignore carriage returns
      if (rad_memptr[0] == '\r')
        continue;

      if (sp < MAXSTRLEN-1)
        str[sp++] = rad_memptr[0];
    }

    // skip trailing EOLN
    rad_memptr++;

    str[sp] = 0;

#if (DEBUG_RTS)
    L_WriteDebug("RTS LINE: `%s'\n", str);
#endif

    if (sp > 0)
    {
      // turn it into upper case
      strupr(str);

      RAD_ParseLine(str);

      if (dots && n != rad_itemsread)
        I_Printf(".");

      n = rad_itemsread;
    }

    rad_cur_linenum++;
  }

  RAD_ParserDone();
}

//
// RAD_LoadLump
//
void RAD_LoadLump(void *data, int size)
{
  L_WriteDebug("RTS: Loading LUMP (size=%d)\n", size);

  rad_cur_filename = "RSCRIPT LUMP";
  
  rad_memfile_size = size;
  rad_memfile = Z_New(byte, size + 1);
  rad_memfile_end = &rad_memfile[size];

  Z_MoveData(rad_memfile, (byte *)data, byte, size);

  // Null Terminated string.
  rad_memfile[size] = 0;

  // OK we have the file in memory.  Parse it to death :-)
  RAD_ParseScript(false);

  Z_Free(rad_memfile);
}

//
// RAD_LoadFile
//
static void RAD_LoadFile(const char *name, bool dots)
{
  DEV_ASSERT2(name);
  
  L_WriteDebug("RTS: Loading File %s\n", name);

  rad_cur_filename = (char *) name;

  RAD_MainCacheFile(name);

  // OK we have the file in memory.  Parse it to death :-)
  RAD_ParseScript(dots);

  Z_Free(rad_memfile);
}

//
// RAD_Init
//
bool RAD_Init(void)
{
  char *filename;

  RAD_InitTips();

  I_Printf("Radius Triggers v%i.%i", PARSERV / PARSERVFIX, 
      PARSERV % PARSERVFIX);

  if (external_ddf)
  {
    filename = M_ComposeFileName(ddfdir, "edge.scr");
    RAD_LoadFile(filename, true);
    Z_Free(filename);
  }

  I_Printf("\n");

  return true;
}

//
// RAD_LoadParam
//
bool RAD_LoadParam(void)
{
  const char *par;
  char *filename;

  // -KM- 1998/11/25 Check for command line script loading
  par = M_GetParm("-script");

  if (par)
  {
    filename = M_ComposeFileName(gamedir, par);
    RAD_LoadFile(filename, false);
    Z_Free(filename);
  }

  return true;
}

