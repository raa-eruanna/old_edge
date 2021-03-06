//----------------------------------------------------------------------------
//  EDGE Player Handling
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#include "i_defs.h"

#include <time.h>
#include <limits.h>

#include "epi/endianess.h"
#include "epi/path.h"
#include "epi/str_format.h"

#include "ddf/game.h"
#include "ddf/level.h"
#include "ddf/language.h"
#include "ddf/sfx.h"

#include "am_map.h"
#include "con_main.h"
#include "m_strings.h"
#include "e_input.h"
#include "e_main.h"
#include "f_finale.h"
#include "g_game.h"
#include "g_state.h"
#include "hu_stuff.h"
#include "m_argv.h"
#include "m_cheat.h"
#include "m_menu.h"
#include "m_random.h"
#include "n_network.h"
#include "p_bot.h"
#include "p_hubs.h"
#include "p_setup.h"
#include "p_tick.h"
#include "rad_trig.h"
#include "r_sky.h"
#include "r_modes.h"
#include "s_sound.h"
#include "s_music.h"
#include "sv_chunk.h"
#include "sv_main.h"
#include "r_colormap.h"
#include "version.h"
#include "vm_coal.h"
#include "w_wad.h"
#include "f_interm.h"
#include "z_zone.h"


static void G_DoReborn(player_t *p);

static void G_DoNewGame(void);
static void G_DoLoadGame(void);
static void G_DoCompleted(void);
static void G_DoSaveGame(void);
static void G_DoEndGame(void);

gamestate_e gamestate = GS_NOTHING;

gameaction_e gameaction = ga_nothing;

bool paused = false;

// for comparative timing purposes 
bool nodrawers;
bool noblit;

// if true, load all graphics at start 
bool precache = true;

int starttime;

// -KM- 1998/11/25 Exit time is the time when the level will actually finish
// after hitting the exit switch/killing the boss.  So that you see the
// switch change or the boss die.

int exittime = INT_MAX;
bool exit_skipall = false;  // -AJA- temporary (maybe become "exit_mode")


cvar_c g_skill;
cvar_c g_gametype;

// -ACB- 2004/05/25 We need to store our current/next mapdefs
const mapdef_c *currmap = NULL;
const mapdef_c *nextmap = NULL;

// combination of MPF_XXX flags
int map_features;


//--------------------------------------------

static int defer_load_slot;
static int defer_save_slot;
static char defer_save_desc[32];

// deferred stuff...
static newgame_params_c *defer_params = NULL;


// user preferences
cvar_c g_mlook;
cvar_c g_autoaim;
cvar_c g_jumping;
cvar_c g_crouching;
cvar_c g_true3d;
cvar_c g_noextra;
cvar_c g_moreblood;
cvar_c g_fastmon;
cvar_c g_passmissile;
cvar_c g_weaponkick;
cvar_c g_weaponswitch;

cvar_c debug_nomonsters;

cvar_c edge_compat;

// FIXME: these probably should be done another way
cvar_c g_itemrespawn;
cvar_c g_teamdamage;


//
// REQUIRED STATE:
//   (a) currmap, map_features
//   (b) players[], numplayers (etc)
//   (c) gameskill + gametype
//
//   ??  exittime
//
void G_DoLoadLevel(void)
{
	if (currmap == NULL)
		I_Error("G_DoLoadLevel: No Current Map selected");

	// Set the sky map.
	//
	// First thing, we have a dummy sky texture name, a flat. The data is
	// in the WAD only because we look for an actual index, instead of simply
	// setting one.
	//
	// -ACB- 1998/08/09 Reference current map for sky name.

	sky_image = R_ImageLookup(currmap->sky, INS_Texture);

	gamestate = GS_NOTHING; //FIXME: needed ???

	// -AJA- FIXME: this background camera stuff is a mess
	background_camera_mo = NULL;

	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		player_t *p = players[pnum];
		if (! p) continue;

		if (p->playerstate == PST_DEAD || (map_features & MPF_ResetPlayer))
		{
			p->playerstate = PST_REBORN;
		}

		p->frags = 0;
	}

	//???  level_flags.item_respawn

	if (M_CheckParm("-respawn"))
		map_features |= MPF_MonRespawn;

	//
	// Note: It should be noted that only the gameskill is
	// passed as the level is already defined in currmap,
	// The method for changing currmap, is using by
	// G_DeferredNewGame.
	//
	// -ACB- 1998/08/09 New P_SetupLevel
	// -KM- 1998/11/25 P_SetupLevel accepts the autotag
	//
	RAD_ClearTriggers();
	RAD_FinishMenu(0);

	wi_stats.kills = wi_stats.items = wi_stats.secret = 0;

	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		player_t *p = players[pnum];
		if (! p) continue;

		p->killcount = p->secretcount = p->itemcount = 0;
		p->mo = NULL;
	}

	// Initial height of PointOfView will be set by player think.
	players[consoleplayer]->viewz = FLO_UNUSED;

	leveltime = 0;
	
	P_SetupLevel();

	RAD_SpawnTriggers(currmap->name.c_str());

	starttime = I_GetTime();
	exittime = INT_MAX;
	exit_skipall = false;

	HU_BeginLevel();

	BOT_BeginLevel();

	gamestate = GS_LEVEL;

	CON_SetVisible(vs_notvisible);

	// clear cmd building stuff
	E_ClearInput();

	paused = false;
}


//
// G_Responder  
//
// Get info needed to make ticcmd_ts for the players.
// 
bool G_Responder(event_t * ev)
{
	// any other key pops up menu if in demos
	if (gameaction == ga_nothing && (gamestate == GS_TITLESCREEN))
	{
		if (ev->type == ev_keydown)
		{
			M_StartControlPanel();
			S_StartFX(sfx_swtchn, SNCAT_UI);
			return true;
		}

		return false;
	}

	if (ev->type == ev_keydown && ev->value.key.sym == KEYD_F12)
	{
		// 25-6-98 KM Allow spy mode for demos even in deathmatch
		if (gamestate == GS_LEVEL && !DEATHMATCH()) //!!!! DEBUGGING
		{
			G_ToggleDisplayPlayer();
			return true;
		}
	}

	if (ev->type == ev_keydown && ev->value.key.sym == KEYD_PAUSE && !netgame)
	{
		paused = !paused;

		if (paused)
		{
			S_PauseMusic();
			S_PauseSound();
			I_GrabCursor(false);
		}
		else
		{
			S_ResumeMusic();
			S_ResumeSound();
			I_GrabCursor(true);
		}

		// explicit as probably killed the initial effect
		S_StartFX(sfx_swtchn, SNCAT_UI);
		return true;
	}

	if (gamestate == GS_LEVEL)
	{
		if (RAD_Responder(ev))
			return true;  // RTS system ate it

		if (AM_Responder(ev))
			return true;  // automap ate it 

		if (HU_Responder(ev))
			return true;  // chat ate the event

		if (M_CheatResponder(ev))
			return true;  // cheat code at it
	}

	if (gamestate == GS_FINALE)
	{
		if (F_Responder(ev))
			return true;  // finale ate the event 
	}

	return INP_Responder(ev);
}


static bool CheckPlayersReborn(void)
{
	// returns TRUE if should reload the level

	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		player_t *p = players[pnum];

		if (!p || p->playerstate != PST_REBORN)
			continue;

		if (SP_MATCH())
			return true;

		G_DoReborn(p);
	}

	return false;
}


void G_BigStuff(void)
{
	// do things to change the game state
	while (gameaction != ga_nothing)
	{
		gameaction_e action = gameaction;
		gameaction = ga_nothing;

		switch (action)
		{
			case ga_newgame:
				G_DoNewGame();
				break;

			case ga_loadlevel:
				G_DoLoadLevel();
				G_SpawnInitialPlayers();
				break;

			case ga_loadgame:
				G_DoLoadGame();
				break;

			case ga_savegame:
				G_DoSaveGame();
				break;

			case ga_playdemo:
				// G_DoPlayDemo();
				break;

			case ga_recorddemo:
				// G_DoRecordDemo();
				break;

			case ga_intermission:
				G_DoCompleted();
				break;

			case ga_finale:
				SYS_ASSERT(nextmap);
				currmap = nextmap;
				map_features = currmap->features | currmap->episode->features;

				F_StartFinale(&currmap->f_pre, ga_loadlevel);
				break;

			case ga_endgame:
				G_DoEndGame();
				break;

			default:
				I_Error("G_BigStuff: Unknown gameaction %d", gameaction);
				break;
		}
	}
}

void G_Ticker(void)
{
	// animate flats and textures globally
	R_UpdateImageAnims();

	// do main actions
	switch (gamestate)
	{
		case GS_TITLESCREEN:
			E_TitleTicker();
			break;

		case GS_LEVEL:
			// get commands, check consistency,
			// and build new consistency check.
			N_TiccmdTicker();

			P_Ticker();
			AM_Ticker();
			HU_Ticker();
			RAD_Ticker();

			// do player reborns if needed
			if (CheckPlayersReborn())
			{
				E_ForceWipe();
				gameaction = ga_loadlevel;
			}
			break;

		case GS_INTERMISSION:
			N_TiccmdTicker();
			WI_Ticker();
			break;

		case GS_FINALE:
			N_TiccmdTicker();
			F_Ticker();
			break;

		default:
			break;
	}
}


static void G_DoReborn(player_t *p)
{
	// first disassociate the corpse (if any)
	if (p->mo)
		p->mo->player = NULL;

	// spawn at random spot if in death match 
	if (DEATHMATCH())
		G_DeathMatchSpawnPlayer(p);
	else
		G_CoopSpawnPlayer(p); // respawn at the start
}

void G_SpawnInitialPlayers(void)
{
	// spawn the active players
	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		player_t *p = players[pnum];
		if (! p) continue;

		G_DoReborn(p);

		if (!DEATHMATCH())
			G_SpawnVoodooDolls(p);
	}

	// check for missing player start.
	if (players[consoleplayer]->mo == NULL)
		I_Error("Missing player start !\n");

	G_SetDisplayPlayer(consoleplayer); // view the guy you are playing
}

void G_DeferredScreenShot(void)
{
	m_screenshot_required = true;
}

// -KM- 1998/11/25 Added time param which is the time to wait before
//  actually exiting level.
void G_ExitLevel(int time)
{
	nextmap = G_LookupMap(currmap->nextmapname);
	exittime = leveltime + time;
	exit_skipall = false;
}

// -ACB- 1998/08/08 We don't have support for the german edition
//                  removed the check for map31.
void G_SecretExitLevel(int time)
{
	nextmap = G_LookupMap(currmap->secretmapname);
	exittime = leveltime + time;
	exit_skipall = false;
}

void G_ExitToLevel(char *name, int time, bool skip_all)
{
	nextmap = G_LookupMap(name);
	exittime = leveltime + time;
	exit_skipall = skip_all;
}

void G_ExitToHub(const char *map_name, int tag)
{
	// FIXME !!! G_ExitToHub
}

void G_ExitToHub(int map_number, int tag)
{
	SYS_ASSERT(currmap);

	char name_buf[32];

	// bit hackish: decided whether to use MAP## or E#M#
	if (currmap->name[0] == 'E')
	{
		sprintf(name_buf, "E%dM%d", 1+(map_number/10), map_number%10);
	}
	else
		sprintf(name_buf, "MAP%02d", map_number);

	G_ExitToHub(name_buf, tag);
}

// 
// REQUIRED STATE:
//   (a) currmap, nextmap
//   (b) players[]
//   (c) leveltime
//   (d) exit_skipall
//   (e) wi_stats.kills (etc)
// 
static void G_DoCompleted(void)
{
	SYS_ASSERT(currmap);

	E_ForceWipe();

	exittime = INT_MAX;

	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		player_t *p = players[pnum];
		if (! p) continue;

		p->leveltime = leveltime;

		// take away cards and stuff
		G_PlayerFinishLevel(p);
	}

	if (automapactive)
		AM_Stop();

	if (rts_menuactive)
		RAD_FinishMenu(0);

	BOT_EndLevel();

	// handle "no stat" levels
	if (currmap->wistyle == WISTYLE_None || exit_skipall)
	{
		automapactive = false;

		if (exit_skipall && nextmap)
		{
			currmap = nextmap;
			map_features = currmap->features | currmap->episode->features;
			gameaction = ga_loadlevel;
		}
		else
		{
			F_StartFinale(&currmap->f_end, nextmap ? ga_finale : ga_nothing);
		}

		return;
	}

	automapactive = false;

	wi_stats.cur  = currmap;
	wi_stats.next = nextmap;

	gamestate = GS_INTERMISSION;

	WI_Start();
}


std::string G_FileNameFromSlot(int slot)
{
	// Creates a savegame file name.

	if (slot < 0) // HUB Hack
	{
		int hub_index = -1 - slot;

		std::string temp(epi::STR_Format("%s%02d.%s", HUBBASE, hub_index, SAVEGAMEEXT));

		return epi::PATH_Join(hub_dir.c_str(), temp.c_str());
	}

    std::string temp(epi::STR_Format("%s%04d.%s", SAVEGAMEBASE, slot + 1, SAVEGAMEEXT));

	return epi::PATH_Join(save_dir.c_str(), temp.c_str());
}


void G_DeferredLoadGame(int slot)
{
	// Can be called by the startup code or the menu task. 

	defer_load_slot = slot;
	gameaction = ga_loadgame;
}

void G_DeferredLoadHub(int hub_index)
{
	defer_load_slot = -1 - hub_index;
	gameaction = ga_loadgame;
}


//
// REQUIRED STATE:
//   (a) defer_load_slot
//
//   ?? nothing else ??
//
static void G_DoLoadGame(void)
{
	E_ForceWipe();

#if 0  // DEBUGGING CODE
	SV_DumpSaveGame(defer_load_slot);
	return;
#endif

	// Try to open		
	std::string fn(G_FileNameFromSlot(defer_load_slot));

	if (! SV_OpenReadFile(fn.c_str()))
	{
		I_Printf("LOAD-GAME: cannot open %s\n", fn.c_str());
		return;
	}
	
	int version;

	if (! SV_VerifyHeader(&version) || ! SV_VerifyContents())
	{
		I_Printf("LOAD-GAME: Savegame is corrupt !\n");
		SV_CloseReadFile();
		return;
	}

	bool is_hub = (defer_load_slot < 0);

	SV_BeginLoad(is_hub);

	saveglobals_t *globs = SV_LoadGLOB();

	if (!globs)
		I_Error("LOAD-GAME: Bad savegame file (no GLOB)\n");

	// --- pull info from global structure ---

	newgame_params_c params;

	params.map = G_LookupMap(globs->level);
	if (! params.map)
		I_Error("LOAD-GAME: No such map %s !  Check WADS\n", globs->level);

	SYS_ASSERT(params.map->episode);

	params.skill    = globs->skill + 1;
	params.gametype = 0;  //??? (globs->netgame >= 2) ? (globs->netgame - 1) : 0;

	params.random_seed = globs->p_random;

	// this player is a dummy one, replaced during actual load
	params.SinglePlayer(0);

	G_InitNew(params);

	map_features = globs->map_features;
	edge_compat  = globs->edge_compat;

	G_DoLoadLevel();

	// -- Check LEVEL consistency (crc) --
	//
	// FIXME: ideally we shouldn't bomb out, just display an error box

	if (globs->mapsector.count != numsectors ||
		globs->mapsector.crc != mapsector_CRC.crc ||
		globs->mapline.count != numlines ||
		globs->mapline.crc != mapline_CRC.crc ||
		globs->mapthing.count != mapthing_NUM ||
		globs->mapthing.crc != mapthing_CRC.crc)
	{
		SV_CloseReadFile();

		I_Error("LOAD-GAME: Level data does not match !  Check WADs\n");
	}

	if (! is_hub)
	{
		leveltime   = globs->level_time;
		exittime    = globs->exit_time;

		wi_stats.kills  = globs->total_kills;
		wi_stats.items  = globs->total_items;
		wi_stats.secret = globs->total_secrets;
	}

	if (globs->sky_image)  // backwards compat (sky_image added 2003/12/19)
		sky_image = globs->sky_image;

	// clear line/sector lookup caches, in case level_flags.edge_compat
	// has changed.
	DDF_BoomClearGenTypes();

	if (SV_LoadEverything() && SV_GetError() == 0)
	{
		/* all went well */ 
	}
	else
	{
		// something went horribly wrong...
		// FIXME (oneday) : show message & go back to title screen

		I_Error("Bad Save Game !\n");
	}

	SV_FreeGLOB(globs);

	SV_FinishLoad();
	SV_CloseReadFile();

	if (! is_hub)
	{
		std::string fn_base = epi::PATH_GetBasename(fn.c_str());

		HUB_CopyHubsForLoadgame(fn_base.c_str());
	}

	V_SetPalette(PALETTE_NORMAL, 0);

	HU_Start();
}

//
// G_DeferredSaveGame
//
// Called by the menu task.
// Description is a 24 byte text string 
//
void G_DeferredSaveGame(int slot, const char *description)
{
	defer_save_slot = slot;
	strcpy(defer_save_desc, description);

	gameaction = ga_savegame;
}

static void G_DoSaveGame(void)
{
	time_t cur_time;
	char timebuf[100];

	std::string fn(G_FileNameFromSlot(defer_save_slot));
	
	if (! SV_OpenWriteFile(fn.c_str(), (EDGEVERHEX << 8) | EDGEPATCH))
	{
		I_Error("Unable to create savegame file: %s\n", fn.c_str());
		return; /* NOT REACHED */
	}

	saveglobals_t *globs = SV_NewGLOB();

	// --- fill in global structure ---

	globs->game  = SV_DupString(currmap->episode_name.c_str());
	globs->level = SV_DupString(currmap->name.c_str());

	globs->skill = g_skill.d - 1;
	globs->netgame = netgame ? (g_gametype.d + 1) : 0;
	globs->map_features = map_features;
	globs->edge_compat = edge_compat.d;

	globs->p_random = P_ReadRandomState();

	globs->console_player = consoleplayer; // NB: not used

	globs->level_time = leveltime;
	globs->exit_time  = exittime;

	globs->total_kills   = wi_stats.kills;
	globs->total_items   = wi_stats.items;
	globs->total_secrets = wi_stats.secret;

	globs->sky_image = sky_image;

	time(&cur_time);
	strftime(timebuf, 99, "%I:%M %p  %d/%b/%Y", localtime(&cur_time));

	if (timebuf[0] == '0' && isdigit(timebuf[1]))
		timebuf[0] = ' ';

	globs->description = SV_DupString(defer_save_desc);
	globs->desc_date   = SV_DupString(timebuf);

	globs->mapsector.count = numsectors;
	globs->mapsector.crc = mapsector_CRC.crc;
	globs->mapline.count = numlines;
	globs->mapline.crc = mapline_CRC.crc;
	globs->mapthing.count = mapthing_NUM;
	globs->mapthing.crc = mapthing_CRC.crc;

	// FIXME: store DDF CRC values too...

	SV_BeginSave();

	SV_SaveGLOB(globs);
	SV_SaveEverything();

	SV_FreeGLOB(globs);

	SV_FinishSave();
	SV_CloseWriteFile();

	std::string fn_base = epi::PATH_GetBasename(fn.c_str());

	HUB_CopyHubsForSavegame(fn_base.c_str());

	defer_save_desc[0] = 0;

	CON_Printf("%s", language["GameSaved"]);
}

//
// G_InitNew Stuff
//
// Can be called by the startup code or the menu task.
// consoleplayer, displayplayer, players[] are setup.
//

//---> newgame_params_c class

newgame_params_c::newgame_params_c() :
	skill(sk_medium), gametype(GT_Single),
	map(NULL), random_seed(0),
	total_players(0)
{
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		players[i] = PFL_NOPLAYER;
		nodes[i]   = NULL;
	}
}

newgame_params_c::newgame_params_c(const newgame_params_c& src)
{
	skill = src.skill;
	gametype = src.gametype;

	map = src.map;

	random_seed   = src.random_seed;
	total_players = src.total_players;

	for (int i = 0; i < MAXPLAYERS; i++)
	{
		players[i] = src.players[i];
		nodes[i] = src.nodes[i];
	}
}

newgame_params_c::~newgame_params_c()
{ }

void newgame_params_c::SinglePlayer(int num_bots)
{
	total_players = 1 + num_bots;
	players[0] = PFL_Zero;  // i.e. !BOT and !NETWORK
	nodes[0]   = NULL;

	for (int pnum = 1; pnum <= num_bots; pnum++)
	{
		players[pnum] = PFL_Bot;
		nodes[pnum]   = NULL;
	}
}

//??  void newgame_params_c::CopyFlags(const gameflags_t *F)
//??  {
//??  	if (flags)
//??  		delete flags;
//??  
//??  	flags = new gameflags_t;
//??  
//??  	memcpy(flags, F, sizeof(gameflags_t));
//??  }

//
// This is the procedure that changes the currmap
// at the start of the game and outside the normal
// progression of the game. All thats needed is the
// skill and the name (The name in the DDF File itself).
//
void G_DeferredNewGame(newgame_params_c& params)
{
	SYS_ASSERT(params.map);

	defer_params = new newgame_params_c(params);

	gameaction = ga_newgame;
}

bool G_MapExists(const mapdef_c *map)
{
	return (W_CheckNumForName(map->lump) >= 0);
}


//
// REQUIRED STATE:
//   (a) defer_params
//
static void G_DoNewGame(void)
{
	E_ForceWipe();

	SYS_ASSERT(defer_params);

	quickSaveSlot = -1;

	G_InitNew(*defer_params);

	delete defer_params;
	defer_params = NULL;

	// -AJA- 2003/10/09: support for pre-level briefing screen on first map.
	//       FIXME: kludgy. All this game logic desperately needs rethinking.
	F_StartFinale(&currmap->f_pre, ga_loadlevel);
}

//
// -ACB- 1998/07/12 Removed Lost Soul/Spectre Ability stuff
// -ACB- 1998/08/10 Inits new game without the need for gamemap or episode.
// -ACB- 1998/09/06 Removed remarked code.
// -KM- 1998/12/21 Added mapdef param so no need for defered init new
//   which was conflicting with net games.
//
// REQUIRED STATE:
//   ?? nothing ??
//
void G_InitNew(newgame_params_c& params)
{
	// --- create players ---

	P_DestroyAllPlayers();

//????	HUB_DestroyAll();

	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		if (params.players[pnum] == PFL_NOPLAYER)
			continue;

		P_CreatePlayer(pnum, (params.players[pnum] & PFL_Bot) ? true : false);

		if (consoleplayer < 0 && ! (params.players[pnum] & PFL_Bot) &&
			! (params.players[pnum] & PFL_Network))
		{
			G_SetConsolePlayer(pnum);
		}

		players[pnum]->node = params.nodes[pnum];
	}

	if (numplayers != params.total_players)
		I_Error("Internal Error: G_InitNew: player miscount (%d != %d)\n",
			numplayers, params.total_players);

	if (consoleplayer < 0)
		I_Error("Internal Error: G_InitNew: no local players!\n");

	G_SetDisplayPlayer(consoleplayer);

	if (paused)
	{
		paused = false;
		S_ResumeMusic(); // -ACB- 1999/10/07 New Music API
		S_ResumeSound();  
	}

	currmap = params.map;

	map_features = currmap->features | currmap->episode->features;

	if (params.skill > sk_nightmare)
		params.skill = sk_nightmare;

	if (params.skill == sk_nightmare)
	{
		map_features |= MPF_FastMon;
		map_features |= MPF_MonRespawn;
	}

	P_WriteRandomState(params.random_seed);

	automapactive = false;

	g_skill    = params.skill;
	g_gametype = params.gametype;

// L_WriteDebug("G_InitNew: Deathmatch %d Skill %d\n", params.deathmatch, (int)params.skill);

//??  	// copy global flags into the level-specific flags
//??  	if (params.flags)
//??  		level_flags = *params.flags;
//??  	else
//??  		level_flags = global_flags;

	N_ResetTics();
}

void G_DeferredEndGame(void)
{
	if (gamestate == GS_LEVEL || gamestate == GS_INTERMISSION ||
	    gamestate == GS_FINALE)
	{
		gameaction = ga_endgame;
	}
}

// 
// REQUIRED STATE:
//    ?? nothing ??
// 
static void G_DoEndGame(void)
{
	E_ForceWipe();

	P_DestroyAllPlayers();
	HUB_DestroyAll();

	if (gamestate == GS_LEVEL)
	{
		BOT_EndLevel();

		// FIXME: P_ShutdownLevel()
	}

	gamestate = GS_NOTHING;

	V_SetPalette(PALETTE_NORMAL, 0);

	E_StartTitle();
}


bool G_CheckWhenAppear(when_appear_e appear)
{
	int use_skill = CLAMP(1,g_skill.d,5) - 1;

	if (! (appear & (1 << use_skill)))
		return false;

	if (SP_MATCH() && !(appear & WNAP_Single))
		return false;

	if (COOP_MATCH() && !(appear & WNAP_Coop))
		return false;

	if (DEATHMATCH() && !(appear & WNAP_DeathMatch))
		return false;

	return true;
}


mapdef_c *G_LookupMap(const char *refname)
{
	mapdef_c *m = mapdefs.Lookup(refname);

	if (m && G_MapExists(m))
		return m;

	// -AJA- handle numbers (like original DOOM)
	if (strlen(refname) <= 2 && isdigit(refname[0]) &&
		(!refname[1] || isdigit(refname[1])))
	{
		int num = atoi(refname);
		char new_ref[20];

		// try MAP## first
		sprintf(new_ref, "MAP%02d", num);

		m = mapdefs.Lookup(new_ref);
		if (m && G_MapExists(m))
			return m;

		// otherwise try E#M#
		if (1 <= num && num <= 9) num = num + 10;
		sprintf(new_ref, "E%dM%d", num/10, num%10);

		m = mapdefs.Lookup(new_ref);
		if (m && G_MapExists(m))
			return m;
	}

	return NULL;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
