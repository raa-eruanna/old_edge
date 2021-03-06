//----------------------------------------------------------------------------
//  EDGE Main Menu Code
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
//
// See M_Option.C for text built menus.
//
// -KM- 1998/07/21 Add support for message input.
//

#include "i_defs.h"

#include "epi/str_format.h"

#include "ddf/game.h"
#include "ddf/level.h"
#include "ddf/language.h"
#include "ddf/sfx.h"

#include "con_main.h"
#include "e_input.h"
#include "e_keys.h"
#include "g_state.h"
#include "m_strings.h"
#include "e_main.h"
#include "g_game.h"
#include "f_interm.h"
#include "hu_draw.h"
#include "hu_stuff.h"
#include "m_argv.h"
#include "m_menu.h"
#include "m_misc.h"
#include "m_netgame.h"
#include "m_option.h"
#include "m_random.h"
#include "n_network.h"
#include "p_setup.h"
#include "am_map.h"
#include "r_local.h"
#include "hu_draw.h"
#include "r_modes.h"
#include "r_colormap.h"
#include "s_sound.h"
#include "s_music.h"
#include "sv_chunk.h"
#include "sv_main.h"
#include "w_wad.h"
#include "z_zone.h"


//
// defaulted values
//
int mouseSensitivity;  // has default.  Note: used only in platform code

// Show messages has default, 0 = off, 1 = on
cvar_c m_messages;
cvar_c m_language;
cvar_c m_screenhud;

static std::string msg_string;
static int msg_lastmenu;
static int msg_mode;

static std::string input_string;		

bool menuactive;

#define SKULLXOFF   -24
#define LINEHEIGHT   15  //!!!!

// timed message = no input from user
static bool msg_needsinput;

static void (* message_key_routine)(int response) = NULL;
static void (* message_input_routine)(const char *response) = NULL;

static int chosen_epi;

// SOUNDS
sfx_t * sfx_swtchn;
sfx_t * sfx_tink;
sfx_t * sfx_radio;
sfx_t * sfx_oof;
sfx_t * sfx_pstop;
sfx_t * sfx_stnmov;
sfx_t * sfx_pistol;
sfx_t * sfx_swtchx;
//
//  IMAGES USED
//
static const image_c *therm_l;
static const image_c *therm_m;
static const image_c *therm_r;
static const image_c *therm_o;

static const image_c *menu_loadg;
static const image_c *menu_saveg;
static const image_c *menu_svol;
static const image_c *menu_doom;
static const image_c *menu_newgame;
static const image_c *menu_skill;
static const image_c *menu_episode;
static const image_c *menu_skull[2];
static const image_c *menu_readthis[2];

//
//  SAVE STUFF
//
#define SAVESTRINGSIZE 	24

#define SAVE_SLOTS  8
#define SAVE_PAGES  100  // more would be rather unwieldy

// -1 = no quicksave slot picked!
int quickSaveSlot;
int quickSavePage;

// 25-6-98 KM Lots of save games... :-)
int save_page = 0;
int save_slot = 0;

// we are going to be entering a savegame string
static int saveStringEnter;

// which char we're editing
static int saveCharIndex;

// old save description before edit
static char saveOldString[SAVESTRINGSIZE];

typedef struct slot_extra_info_s
{
	bool empty;
	bool corrupt;

	char desc[SAVESTRINGSIZE];
	char timestr[32];
  
	char mapname[10];
	char gamename[32];
  
	int skill;
	int netgame;
	bool has_view;
}
slot_extra_info_t;

static slot_extra_info_t ex_slots[SAVE_SLOTS];

// 98-7-10 KM New defines for slider left.
// Part of savegame changes.
#define SLIDERLEFT  -1
#define SLIDERRIGHT -2


//
// MENU TYPEDEFS
//
typedef struct
{
	// 0 = no cursor here, 1 = ok, 2 = arrows ok
	int status;

  	// image for menu entry
	char patch_name[10];
	const image_c *image;

  	// choice = menu item #.
  	// if status = 2, choice can be SLIDERLEFT or SLIDERRIGHT
	void (* select_func)(int choice);

	// hotkey in menu
	char alpha_key;
}
menuitem_t;

typedef struct menu_s
{
	// # of menu items
	int numitems;

  // previous menu
	struct menu_s *prevMenu;

	// menu items
	menuitem_t *menuitems;

	// draw routine
	void (* draw_func)(void);

	// x,y of menu
	int x, y;

	// last item user was on in menu
	int lastOn;
}
menu_t;

// menu item skull is on
static int itemOn;

// skull animation counter
static int skullAnimCounter;

// which skull to draw
static int whichSkull;

// current menudef
static menu_t *currentMenu;

//
// PROTOTYPES
//
static void M_NewGame(int choice);
static void M_Episode(int choice);
static void M_ChooseSkill(int choice);
static void M_LoadGame(int choice);
static void M_SaveGame(int choice);

// 25-6-98 KM
static void M_LoadSavePage(int choice);
static void M_Options(int choice);
static void M_ReadThis(int choice);
static void M_ReadThis2(int choice);
void M_EndGame(int choice);

static void M_ChangeMessages(int choice);
static void M_SfxVol(int choice);
static void M_MusicVol(int choice);
// static void M_Sound(int choice);

static void M_FinishReadThis(int choice);
static void M_LoadSelect(int choice);
static void M_SaveSelect(int choice);
static void M_ReadSaveStrings(void);
static void M_QuickSave(void);
static void M_QuickLoad(void);

static void M_DrawMainMenu(void);
static void M_DrawReadThis1(void);
static void M_DrawReadThis2(void);
static void M_DrawNewGame(void);
static void M_DrawEpisode(void);
static void M_DrawSound(void);
static void M_DrawLoad(void);
static void M_DrawSave(void);

static void M_DrawSaveLoadBorder(float x, float y, int len);
static void M_SetupNextMenu(menu_t * menudef);
void M_ClearMenus(void);
void M_StartControlPanel(void);
// static void M_StopMessage(void);

//
// DOOM MENU
//
typedef enum
{
	newgame = 0,
	options,
	loadgame,
	savegame,
	readthis,
	quitdoom,
	main_end
}
main_e;

static menuitem_t MainMenu[] =
{
	{1, "M_NGAME",   NULL, M_NewGame,  'N'},
	{1, "M_OPTION",  NULL, M_Options,  'O'},
	{1, "M_LOADG",   NULL, M_LoadGame, 'L'},
	{1, "M_SAVEG",   NULL, M_SaveGame, 'S'},
	// Another hickup with Special edition.
	{1, "M_RDTHIS",  NULL, M_ReadThis, 'R'},
	{1, "M_QUITG",   NULL, M_QuitEDGE, 'Q'}
};

static menu_t MainDef =
{
	main_end,
	NULL,
	MainMenu,
	M_DrawMainMenu,
	97, 64,
	0
};

//
// EPISODE SELECT
//
// -KM- 1998/12/16 This is generated dynamically.
//
static menuitem_t *EpisodeMenu = NULL;

static menuitem_t DefaultEpiMenu =
{
	1,  // status
	"Working",  // name
	NULL,  // image
	NULL,  // select_func
	'w'  // alphakey
};

static menu_t EpiDef =
{
	0,  //ep_end,  // # of menu items
	&MainDef,  // previous menu
	&DefaultEpiMenu,  // menuitem_t ->
	M_DrawEpisode,  // drawing routine ->
	48, 63,  // x,y
	0  // lastOn
};

static menuitem_t SkillMenu[] =
{
	{1, "M_JKILL", NULL, M_ChooseSkill, 'P'},
	{1, "M_ROUGH", NULL, M_ChooseSkill, 'R'},
	{1, "M_HURT",  NULL, M_ChooseSkill, 'H'},
	{1, "M_ULTRA", NULL, M_ChooseSkill, 'U'},
	{1, "M_NMARE", NULL, M_ChooseSkill, 'N'}
};

static menu_t SkillDef =
{
	5,    // # of menu items
	&EpiDef,  // previous menu
	SkillMenu,  // menuitem_t ->
	M_DrawNewGame,  // drawing routine ->
	48, 63,  // x,y
	sk_medium  // lastOn
};

//
// OPTIONS MENU
//
typedef enum
{
	endgame,
	messages,
	scrnsize,
	option_empty1,
	mousesens,
	option_empty2,
	soundvol,
	opt_end
}
options_e;

//
// Read This! MENU 1 & 2
//

static menuitem_t ReadMenu1[] =
{
	{1, "", NULL, M_ReadThis2, 0}
};

static menu_t ReadDef1 =
{
	1,
	&MainDef,
	ReadMenu1,
	M_DrawReadThis1,
	280, 185,
	0
};

static menuitem_t ReadMenu2[] =
{
	{1, "", NULL, M_FinishReadThis, 0}
};

static menu_t ReadDef2 =
{
	1,
	&ReadDef1,
	ReadMenu2,
	M_DrawReadThis2,
	330, 175,
	0
};

//
// SOUND VOLUME MENU
//

static menuitem_t SoundMenu[4] =
{
	{  2, "M_SFXVOL", NULL, M_SfxVol,   'S' },
	{ -1, "", NULL, 0 },
	{  2, "M_MUSVOL", NULL, M_MusicVol, 'M' },
	{ -1, "", NULL, 0 }
};

static menu_t SoundDef =
{
	4,
	&MainDef,  ///  &OptionsDef,
	SoundMenu,
	M_DrawSound,
	80, 64,
	0
};

//
// LOAD GAME MENU
//
// Note: upto 10 slots per page
//
static menuitem_t LoadingMenu[] =
{
	{2, "", NULL, M_LoadSelect, '1'},
	{2, "", NULL, M_LoadSelect, '2'},
	{2, "", NULL, M_LoadSelect, '3'},
	{2, "", NULL, M_LoadSelect, '4'},
	{2, "", NULL, M_LoadSelect, '5'},
	{2, "", NULL, M_LoadSelect, '6'},
	{2, "", NULL, M_LoadSelect, '7'},
	{2, "", NULL, M_LoadSelect, '8'},
	{2, "", NULL, M_LoadSelect, '9'},
	{2, "", NULL, M_LoadSelect, '0'}
};

static menu_t LoadDef =
{
	SAVE_SLOTS,
	&MainDef,
	LoadingMenu,
	M_DrawLoad,
	30, 34,
	0
};

//
// SAVE GAME MENU
//
static menuitem_t SavingMenu[] =
{
	{2, "", NULL, M_SaveSelect, '1'},
	{2, "", NULL, M_SaveSelect, '2'},
	{2, "", NULL, M_SaveSelect, '3'},
	{2, "", NULL, M_SaveSelect, '4'},
	{2, "", NULL, M_SaveSelect, '5'},
	{2, "", NULL, M_SaveSelect, '6'},
	{2, "", NULL, M_SaveSelect, '7'},
	{2, "", NULL, M_SaveSelect, '8'},
	{2, "", NULL, M_SaveSelect, '9'},
	{2, "", NULL, M_SaveSelect, '0'}
};

static menu_t SaveDef =
{
	SAVE_SLOTS,
	&MainDef,
	SavingMenu,
	M_DrawSave,
	30, 34,
	0
};

// 98-7-10 KM Chooses the page of savegames to view
void M_LoadSavePage(int choice)
{
	switch (choice)
	{
		case SLIDERLEFT:
			// -AJA- could use `OOF' sound...
			if (save_page == 0)
				return;

			save_page--;
			break;
      
		case SLIDERRIGHT:
			if (save_page >= SAVE_PAGES-1)
				return;

			save_page++;
			break;
	}

	S_StartFX(sfx_swtchn);
	M_ReadSaveStrings();
}

//
// Read the strings from the savegame files
//
// 98-7-10 KM Savegame slots increased
//
void M_ReadSaveStrings(void)
{
	int i, version;
  
	saveglobals_t *globs;

	for (i=0; i < SAVE_SLOTS; i++)
	{
		ex_slots[i].empty = false;
		ex_slots[i].corrupt = true;

		ex_slots[i].skill = -1;
		ex_slots[i].netgame = -1;
		ex_slots[i].has_view = false;

		ex_slots[i].desc[0] = 0;
		ex_slots[i].timestr[0] = 0;
		ex_slots[i].mapname[0] = 0;
		ex_slots[i].gamename[0] = 0;
    
		std::string fn(G_FileNameFromSlot(save_page * SAVE_SLOTS + i));

		if (! SV_OpenReadFile(fn.c_str()))
		{
			ex_slots[i].empty = true;
			ex_slots[i].corrupt = false;
			continue;
		}

		if (! SV_VerifyHeader(&version))
		{
			SV_CloseReadFile();
			continue;
		}

		globs = SV_LoadGLOB();

		// close file now -- we only need the globals
		SV_CloseReadFile();

		if (! globs)
			continue;

		// --- pull info from global structure ---

		if (!globs->game || !globs->level || !globs->description)
		{
			SV_FreeGLOB(globs);
			continue;
		}

		ex_slots[i].corrupt = false;

		Z_StrNCpy(ex_slots[i].gamename, globs->game,  32-1);
		Z_StrNCpy(ex_slots[i].mapname,  globs->level, 10-1);

		Z_StrNCpy(ex_slots[i].desc, globs->description, SAVESTRINGSIZE-1);

		if (globs->desc_date)
			Z_StrNCpy(ex_slots[i].timestr, globs->desc_date, 32-1);

		ex_slots[i].skill   = globs->skill;
		ex_slots[i].netgame = globs->netgame;

		SV_FreeGLOB(globs);
    
#if 0
		// handle screenshot
		if (globs->view_pixels)
		{
			int x, y;
      
			for (y=0; y < 100; y++)
				for (x=0; x < 160; x++)
				{
					save_screenshot[x][y] = SV_GetShort();
				}
		}
#endif
	}

	// fix up descriptions
	for (i=0; i < SAVE_SLOTS; i++)
	{
		if (ex_slots[i].corrupt)
		{
			strncpy(ex_slots[i].desc, language["Corrupt_Slot"],
					SAVESTRINGSIZE - 1);
			continue;
		}
		else if (ex_slots[i].empty)
		{
			strncpy(ex_slots[i].desc, language["EmptySlot"],
					SAVESTRINGSIZE - 1);
			continue;
		}
	}
}

static void M_DrawSaveLoadCommon(int row, int row2)
{
	int y = LoadDef.y + LINEHEIGHT * row;

	slot_extra_info_t *info;

	char mbuffer[200];


	// -KM-  1998/06/25 This could quite possibly be replaced by some graphics...
	HUD_SetTextColor(T_WHITE);

	if (save_page > 0)
		HUD_DrawText(LoadDef.x - 4, y, "< PREV");

	if (save_page < SAVE_PAGES-1)
		HUD_DrawText(LoadDef.x + 172, y, "NEXT >");
 
	sprintf(mbuffer, "PAGE %d", save_page + 1);

 	HUD_SetAlignment(0, -1);
	HUD_DrawText(LoadDef.x + 94, y, mbuffer);
	HUD_SetAlignment();


	info = ex_slots + itemOn;
	SYS_ASSERT(0 <= itemOn && itemOn < SAVE_SLOTS);

	if (saveStringEnter || info->empty || info->corrupt)
		return;


	// show some info about the savegame

	HUD_SetAlignment(+1, -1);
	HUD_SetTextColor(T_GREEN);

	y = LoadDef.y + LINEHEIGHT * (row2 + 1);

	HUD_DrawText(310, y, info->timestr);

	y -= LINEHEIGHT;
    

	switch (info->skill)
	{
		case 1:  HUD_DrawText(310, y, "Too Young To Die"); break;
		case 2:  HUD_DrawText(310, y, "Not Too Rough"); break;
		case 3:  HUD_DrawText(310, y, "Hurt Me Plenty"); break;
		case 4:  HUD_DrawText(310, y, "Ultra Violence"); break;
		default: HUD_DrawText(310, y, "NIGHTMARE"); break;
	}

	y -= LINEHEIGHT;

  
	switch (info->netgame)
	{
		case 0:  HUD_DrawText(310, y, "SP MODE"); break;
		case 1:  HUD_DrawText(310, y, "COOP MODE"); break;
		default: HUD_DrawText(310, y, "DM MODE"); break;
	}
  
	y -= LINEHEIGHT;

  
	HUD_DrawText(310, y, info->mapname);

	HUD_SetAlpha();
	HUD_SetTextColor();
}

//
// 1998/07/10 KM Savegame slots increased
//
void M_DrawLoad(void)
{
	int i;

	HUD_DrawImage(72, 8, menu_loadg);
      
	for (i = 0; i < SAVE_SLOTS; i++)
		M_DrawSaveLoadBorder(LoadDef.x + 8, LoadDef.y + LINEHEIGHT * (i), 24);

	// draw screenshot ?

	for (i = 0; i < SAVE_SLOTS; i++)
		HUD_DrawText(// FIXME ex_slots[i].corrupt ? 3 : 0,
		             LoadDef.x + 8, LoadDef.y + LINEHEIGHT * (i),
					 ex_slots[i].desc);

	M_DrawSaveLoadCommon(i, i+1);
}


//
// Draw border for the savegame description
//
void M_DrawSaveLoadBorder(float x, float y, int len)
{
	const image_c *L = R_ImageLookup("M_LSLEFT");
	const image_c *C = R_ImageLookup("M_LSCNTR");
	const image_c *R = R_ImageLookup("M_LSRGHT");

	HUD_DrawImage(x - IM_WIDTH(L), y + 7, L);

	for (int i = 0; i < len; i++, x += IM_WIDTH(C))
		HUD_DrawImage(x, y + 7, C);

	HUD_DrawImage(x, y + 7, R);
}

//
// User wants to load this game
//
// 98-7-10 KM Savegame slots increased
//
void M_LoadSelect(int choice)
{
	if (choice < 0)
	{
		M_LoadSavePage(choice);
		return;
	}

	G_DeferredLoadGame(save_page * SAVE_SLOTS + choice);
	M_ClearMenus();
}

//
// Selected from DOOM menu
//
void M_LoadGame(int choice)
{
	if (netgame)
	{
		M_StartMessage(language["NoLoadInNetGame"], NULL, false);
		return;
	}

	M_SetupNextMenu(&LoadDef);
	M_ReadSaveStrings();
}

//
// 98-7-10 KM Savegame slots increased
//
void M_DrawSave(void)
{
	int i;

	HUD_DrawImage(72, 8, menu_saveg);

	for (i = 0; i < SAVE_SLOTS; i++)
	{
		int y = LoadDef.y + LINEHEIGHT * i;

		M_DrawSaveLoadBorder(LoadDef.x + 8, y, 24);

		if (saveStringEnter && i == save_slot)
		{
			HUD_SetTextColor(T_YELLOW);

			float len = HUD_StringWidth(ex_slots[save_slot].desc);

			HUD_DrawText(LoadDef.x + 8, y, ex_slots[i].desc);
			HUD_DrawText(LoadDef.x + len + 8, y, "_");

			HUD_SetTextColor();
		}
		else
			HUD_DrawText(LoadDef.x + 8, y, ex_slots[i].desc);
	}

	M_DrawSaveLoadCommon(i, i+1);
}

//
// M_Responder calls this when user is finished
//
// 98-7-10 KM Savegame slots increased
//
static void M_DoSave(int page, int slot)
{
	G_DeferredSaveGame(page * SAVE_SLOTS + slot, ex_slots[slot].desc);
	M_ClearMenus();

	// PICK QUICKSAVE SLOT YET?
	if (quickSaveSlot == -2)
	{
		quickSavePage = page;
		quickSaveSlot = slot;
	}

	LoadDef.lastOn = SaveDef.lastOn;
}

//
// User wants to save. Start string input for M_Responder
//
void M_SaveSelect(int choice)
{
	if (choice < 0)
	{
		M_LoadSavePage(choice);
		return;
	}

	// we are going to be intercepting all chars
	saveStringEnter = 1;

	save_slot = choice;
	strcpy(saveOldString, ex_slots[choice].desc);

	if (ex_slots[choice].empty)
		ex_slots[choice].desc[0] = 0;

	saveCharIndex = strlen(ex_slots[choice].desc);
}

//
// Selected from DOOM menu
//
void M_SaveGame(int choice)
{
	if (gamestate != GS_LEVEL)
	{
		M_StartMessage(language["SaveWhenNotPlaying"], NULL, false);
		return;
	}

	// -AJA- big cop-out here (add RTS menu stuff to savegame ?)
	if (rts_menuactive)
	{
		M_StartMessage("You can't save during an RTS menu.\n\npress a key.", NULL, false);
		return;
	}

	M_ReadSaveStrings();
	M_SetupNextMenu(&SaveDef);

	need_save_screenshot = true;
	save_screenshot_valid = false;
}

//
//   M_QuickSave
//

static void QuickSaveResponse(int ch)
{
	if (ch == 'Y')
	{
		M_DoSave(quickSavePage, quickSaveSlot);
		S_StartFX(sfx_swtchx);
	}
}

void M_QuickSave(void)
{
	if (gamestate != GS_LEVEL)
	{
		S_StartFX(sfx_oof);
		return;
	}

	if (quickSaveSlot < 0)
	{
		M_StartControlPanel();
		M_ReadSaveStrings();
		M_SetupNextMenu(&SaveDef);

		need_save_screenshot = true;
		save_screenshot_valid = false;

		quickSaveSlot = -2;  // means to pick a slot now
		return;
	}
	
	std::string s(epi::STR_Format(language["QuickSaveOver"],
				  ex_slots[quickSaveSlot].desc));

	M_StartMessage(s.c_str(), QuickSaveResponse, true);
}

static void QuickLoadResponse(int ch)
{
	if (ch == 'Y')
	{
		int tempsavepage = save_page;

		save_page = quickSavePage;
		M_LoadSelect(quickSaveSlot);

		save_page = tempsavepage;
		S_StartFX(sfx_swtchx);
	}
}

void M_QuickLoad(void)
{
	if (netgame)
	{
		M_StartMessage(language["NoQLoadInNet"], NULL, false);
		return;
	}

	if (quickSaveSlot < 0)
	{
		M_StartMessage(language["NoQuickSaveSlot"], NULL, false);
		return;
	}

	std::string s(epi::STR_Format(language["QuickLoad"],
					ex_slots[quickSaveSlot].desc));

	M_StartMessage(s.c_str(), QuickLoadResponse, true);
}

//
// Read This Menus
// Had a "quick hack to fix romero bug"
//
void M_DrawReadThis1(void)
{
	HUD_StretchImage(0, 0, 320, 200, menu_readthis[0]);
}

//
// Read This Menus - optional second page.
//
void M_DrawReadThis2(void)
{
	HUD_StretchImage(0, 0, 320, 200, menu_readthis[1]);
}


void M_DrawSound(void)
{
	HUD_DrawImage(60, 38, menu_svol);

	int sfx_slide_pos = I_ROUND(s_volume.f   * SND_SLIDER_NUM);
	int mus_slide_pos = I_ROUND(s_musicvol.f * SND_SLIDER_NUM);

	sfx_slide_pos = CLAMP(0, sfx_slide_pos, SND_SLIDER_NUM-1);
	mus_slide_pos = CLAMP(0, mus_slide_pos, SND_SLIDER_NUM-1);

	M_DrawThermo(SoundDef.x, SoundDef.y + LINEHEIGHT * 1, SND_SLIDER_NUM, sfx_slide_pos, 1);
	M_DrawThermo(SoundDef.x, SoundDef.y + LINEHEIGHT * 3, SND_SLIDER_NUM, mus_slide_pos, 1);
}

#if 0
void M_Sound(int choice)
{
	M_SetupNextMenu(&SoundDef);
}
#endif


#define VOLUME_STEP  (1.0f / SND_SLIDER_NUM)

// -ACB- 1999/10/10 Sound API Volume re-added
void M_SfxVol(int choice)
{
	switch (choice)
	{
		case SLIDERLEFT:
			s_volume = s_volume.f - VOLUME_STEP;
			break;

		case SLIDERRIGHT:
			s_volume = s_volume.f + VOLUME_STEP;
			break;
	}

	s_volume = CLAMP(0.0f, s_volume.f, 1.0f);

	S_ChangeSoundVolume();
}

// -ACB- 1999/10/07 Removed sound references: New Sound API
void M_MusicVol(int choice)
{
	switch (choice)
	{
		case SLIDERLEFT:
			s_musicvol = s_musicvol.f - VOLUME_STEP;
			break;

		case SLIDERRIGHT:
			s_musicvol = s_musicvol.f + VOLUME_STEP;
			break;
	}

	s_musicvol = CLAMP(0.0f, s_musicvol.f, 1.0f);

	S_ChangeMusicVolume();
}

void M_DrawMainMenu(void)
{
	HUD_DrawImage(94, 2, menu_doom);
}

void M_DrawNewGame(void)
{
	HUD_DrawImage(96, 14, menu_newgame);
	HUD_DrawImage(54, 38, menu_skill);
}

void M_NewGame(int choice)
{
	if (netgame)
	{
		M_StartMessage(language["NewNetGame"], NULL, false);
		return;
	}

	M_SetupNextMenu(&EpiDef);
}

//
//      M_Episode
//

// -KM- 1998/12/16 Generates EpiDef menu dynamically.
static void CreateEpisodeMenu(void)
{
	if (gamedefs.GetSize() == 0)
		I_Error("No defined episodes !\n");

	EpisodeMenu = Z_New(menuitem_t, gamedefs.GetSize());

	Z_Clear(EpisodeMenu, menuitem_t, gamedefs.GetSize());

	int e = 0;
	epi::array_iterator_c it;

	for (it = gamedefs.GetBaseIterator(); it.IsValid(); it++)
	{
		gamedef_c *g = ITERATOR_TO_TYPE(it, gamedef_c*);
		if (! g) continue;

		if (W_CheckNumForName(g->firstmap.c_str()) == -1)
			continue;

		EpisodeMenu[e].status = 1;
		EpisodeMenu[e].select_func = M_Episode;
		EpisodeMenu[e].image = NULL;
		EpisodeMenu[e].alpha_key = '1' + e;

		Z_StrNCpy(EpisodeMenu[e].patch_name, g->namegraphic.c_str(), 8);
		EpisodeMenu[e].patch_name[8] = 0;

		e++;
	}

	if (e == 0)
		I_Error("No available episodes !\n");

	EpiDef.numitems  = e;
	EpiDef.menuitems = EpisodeMenu;
}


void M_DrawEpisode(void)
{
	if (!EpisodeMenu)
		CreateEpisodeMenu();
    
	HUD_DrawImage(54, 38, menu_episode);
}

static void ReallyDoStartLevel(int skill, gamedef_c *g)
{
	newgame_params_c params;

	params.skill = skill;
	params.gametype = GT_Single;

	params.random_seed = I_PureRandom();

	params.SinglePlayer(0);

	params.map = G_LookupMap(g->firstmap.c_str());

	if (! params.map)
	{
		// 23-6-98 KM Fixed this.
		M_SetupNextMenu(&EpiDef);
		M_StartMessage(language["EpisodeNonExist"], NULL, false);
		return;
	}

	SYS_ASSERT(G_MapExists(params.map));
	SYS_ASSERT(params.map->episode);

	G_DeferredNewGame(params);

	M_ClearMenus();
}

static void DoStartLevel(int skill)
{
	// -KM- 1998/12/17 Clear the intermission.
	WI_Clear();
  
	// find episode
	gamedef_c *g = NULL;
	epi::array_iterator_c it;

	for (it = gamedefs.GetBaseIterator(); it.IsValid(); it++) 
	{ 
		g = ITERATOR_TO_TYPE(it, gamedef_c*);

		if (!strcmp(g->namegraphic.c_str(), EpisodeMenu[chosen_epi].patch_name))
		{
			break;
		}
	}

	// Sanity checking...
	if (! g)
	{
		I_Warning("Internal Error: no episode for '%s'.\n",
			EpisodeMenu[chosen_epi].patch_name);
		M_ClearMenus();
		return;
	}

	const mapdef_c * map = G_LookupMap(g->firstmap.c_str());
	if (! map)
	{
		I_Warning("Cannot find map for '%s' (episode %s)\n",
			g->firstmap.c_str(),
			EpisodeMenu[chosen_epi].patch_name);
		M_ClearMenus();
		return;
	}

	ReallyDoStartLevel(skill, g);
}

static void VerifyNightmare(int ch)
{
	if (ch == 'Y')
	{
		DoStartLevel(sk_nightmare);
	}
}

void M_ChooseSkill(int choice)
{
	if (choice == sk_nightmare)
	{
		M_StartMessage(language["NightMareCheck"], VerifyNightmare, true);
		return;
	}
	
	DoStartLevel(choice+1);
}

void M_Episode(int choice)
{
	chosen_epi = choice;
	M_SetupNextMenu(&SkillDef);
}

void M_Options(int choice)
{
//!!!!!	option_menuon = 1;
}

//
// Toggle messages on/off
//
void M_ChangeMessages(int choice)
{
	// warning: unused parameter `int choice'
	(void) choice;

	m_messages = m_messages.d ? 0 : 1;

	if (m_messages.d)
		CON_Printf("%s\n", language["MessagesOn"]);
	else
		CON_Printf("%s\n", language["MessagesOff"]);
}

static void EndGameResponse(int ch)
{
	if (ch == 'Y' || ch == KEYD_ENTER)
	{
		G_DeferredEndGame();

		currentMenu->lastOn = itemOn;
		M_ClearMenus();
	}
}

void M_EndGame(int choice)
{
	if (gamestate != GS_LEVEL)
	{
		S_StartFX(sfx_oof);
		return;
	}

	option_menuon  = 0;
	netgame_menuon = 0;

	if (netgame)
	{
		M_StartMessage(language["EndNetGame"], NULL, false);
		return;
	}

	M_StartMessage(language["EndGameCheck"], EndGameResponse, true);
}

void M_ReadThis(int choice)
{
	M_SetupNextMenu(&ReadDef1);
}

void M_ReadThis2(int choice)
{
	M_SetupNextMenu(&ReadDef2);
}

void M_FinishReadThis(int choice)
{
	M_SetupNextMenu(&MainDef);
}

void M_QuitFinally(void)
{
	// -ACB- 1999/09/20 New exit code order
	// Write the default config file first
	I_Printf("Saving defaults to: %s\n", cfgfile.c_str());

	M_SaveDefaults();

	I_Printf("Exiting...\n");

	E_EngineShutdown();
	I_SystemShutdown();

	I_DisplayExitScreen();
	I_CloseProgram(0);
}

//
// -KM- 1998/12/16 Handle sfx that don't exist in this version.
// -KM- 1999/01/31 Generate quitsounds from default.ldf
//
static void QuitResponse(int ch)
{
	if (! (ch == 'Y' || ch == KEYD_ENTER || ch == KEYD_F4))
		return;
		
	if (!netgame)
	{
		int numsounds = 0;
		char refname[16];
		char sound[16];
		int i, start;

		// Count the quit messages
		do
		{
			sprintf(refname, "QuitSnd%d", numsounds + 1);
			if (language.IsValidRef(refname))
				numsounds++;
			else
				break;
		}
		while (true);

		if (numsounds)
		{
			// cycle through all the quit sounds, until one of them exists
			// (some of the default quit sounds do not exist in DOOM 1)
			start = i = M_Random() % numsounds;
			do
			{
				sprintf(refname, "QuitSnd%d", i + 1);
				sprintf(sound, "DS%s", language[refname]);
				if (W_CheckNumForName(sound) != -1)
				{
					S_StartFX(sfxdefs.GetEffect(language[refname]));
					break;
				}
				i = (i + 1) % numsounds;
			}
			while (i != start);
		}
	}

	M_QuitFinally();
}

//
// -ACB- 1998/07/19 Removed offensive messages selection (to some people);
//     Better Random Selection.
//
// -KM- 1998/07/21 Reinstated counting quit messages, so adding them to dstrings.c
//                   is all you have to do.  Using P_Random for the random number
//                   automatically kills the demo sync...
//                   (hence M_Random()... -AJA-).
//
// -KM- 1998/07/31 Removed Limit. So there.
// -KM- 1999/01/31 Load quit messages from default.ldf
//
void M_QuitEDGE(int choice)
{
	char ref[64];

	std::string msg;

	int num_quitmessages = 0;

	// Count the quit messages
	do
	{
		num_quitmessages++;

		sprintf(ref, "QUITMSG%d", num_quitmessages);
	}
	while (language.IsValidRef(ref));

	// we stopped at one higher than the last
	num_quitmessages--;

	// -ACB- 2004/08/14 Allow fallback to just the "PressToQuit" message
	if (num_quitmessages > 0)
	{
		// Pick one at random
		sprintf(ref, "QUITMSG%d", 1 + (M_Random() % num_quitmessages));

		// Construct the quit message in full
		msg = epi::STR_Format("%s\n\n%s", language[ref], language["PressToQuit"]);
	}
	else
	{
		msg = std::string(language["PressToQuit"]);
	}

	// Trigger the message
	M_StartMessage(msg.c_str(), QuitResponse, true);
}


//----------------------------------------------------------------------------
//   MENU FUNCTIONS
//----------------------------------------------------------------------------

void M_DrawThermo(int x, int y, int thermWidth, int thermDot, int div)
{
	int i, basex = x;
	int step = (8 / div);

	// Note: the (step+1) here is for compatibility with the original
	// code.  It seems required to make the thermo bar tile properly.

	HUD_StretchImage(x, y, step+1, IM_HEIGHT(therm_l)/div, therm_l);

	for (i=0, x += step; i < thermWidth; i++, x += step)
	{
		HUD_StretchImage(x, y, step+1, IM_HEIGHT(therm_m)/div, therm_m);
	}

	HUD_StretchImage(x, y, step+1, IM_HEIGHT(therm_r)/div, therm_r);

	x = basex + step + thermDot * step;

	HUD_StretchImage(x, y, step+1, IM_HEIGHT(therm_o)/div, therm_o);
}

void M_StartMessage(const char *string, void (* routine)(int response), 
					bool input)
{
	msg_lastmenu = menuactive;
	msg_mode = 1;
	msg_string = std::string(string);
	message_key_routine = routine;
	message_input_routine = NULL;
	msg_needsinput = input;
	menuactive = true;
	CON_SetVisible(vs_notvisible);
	return;
}

//
// -KM- 1998/07/21 Call M_StartMesageInput to start a message that needs a
//                 string input. (You can convert it to a number if you want to.)
//                 
// string:  The prompt.
//
// routine: Format is void routine(char *s)  Routine will be called
//          with a pointer to the input in s.  s will be NULL if the user
//          pressed ESCAPE to cancel the input.
//
void M_StartMessageInput(const char *string,
						 void (* routine)(const char *response))
{
	msg_lastmenu = menuactive;
	msg_mode = 2;
	msg_string = std::string(string);
	message_input_routine = routine;
	message_key_routine = NULL;
	msg_needsinput = true;
	menuactive = true;
	CON_SetVisible(vs_notvisible);
	return;
}

#if 0
void M_StopMessage(void)
{
	menuactive = msg_lastmenu?true:false;
	msg_string.Clear();
	msg_mode = 0;
  
	if (!menuactive)
		save_screenshot_valid = false;
}
#endif

static void M_BumpGamma(void)
{
	int new_lev = 0;

	if (r_gamma.f < 1.1)
		new_lev = 1;
	else if (r_gamma.f < 1.25)
		new_lev = 2;
	else if (r_gamma.f < 1.45)
		new_lev = 3;
	else if (r_gamma.f < 1.8)
		new_lev = 4;
	else if (r_gamma.f < 2.2)
		new_lev = 5;

	// update cvar with new value
	if (new_lev == 5)
		r_gamma = 2.4f;
	else
		r_gamma = (float)(1.0 / (1.0 - new_lev/8.0));

	switch (new_lev)
	{
		case 0: CON_PlayerMessageLDF(consoleplayer, "GammaOff");        break;
		case 1: CON_PlayerMessageLDF(consoleplayer, "GammaLevelOne");   break;
		case 2: CON_PlayerMessageLDF(consoleplayer, "GammaLevelTwo");   break;
		case 3: CON_PlayerMessageLDF(consoleplayer, "GammaLevelThree"); break;
		case 4: CON_PlayerMessageLDF(consoleplayer, "GammaLevelFour");  break;
		case 5: CON_PlayerMessageLDF(consoleplayer, "GammaLevelFive");  break;

		default: break;
	}
}

//
// CONTROL PANEL
//

//
// -KM- 1998/09/01 Analogue binding, and hat support
//
bool M_Responder(event_t * ev)
{
	int i;

	if (ev->type != ev_keydown)
		return false;

	int ch = ev->value.key.sym;

	// -ACB- 1999/10/11 F1 is responsible for print screen at any time
	if (ch == KEYD_F1 || ch == KEYD_PRINT)
	{
		G_DeferredScreenShot();
		return true;
	}

	// Take care of any messages that need input
	// -KM- 1998/07/21 Message Input
	if (msg_mode == 1)
	{
		if (msg_needsinput == true &&
			!(ch == ' ' || ch == 'N' || ch == 'Y' || ch == KEYD_ESCAPE))
			return false;

		msg_mode = 0;
		// -KM- 1998/07/31 Moved this up here to fix bugs.
		menuactive = msg_lastmenu?true:false;

		if (message_key_routine)
			(* message_key_routine)(ch);

		S_StartFX(sfx_swtchx);
		return true;
	}
	else if (msg_mode == 2)
	{		
		if (ch == KEYD_ENTER)
		{
			menuactive = msg_lastmenu?true:false;
			msg_mode = 0;

			if (message_input_routine)
				(* message_input_routine)(input_string.c_str());

			input_string.clear();
			
			M_ClearMenus();
			S_StartFX(sfx_swtchx);
			return true;
		}

		if (ch == KEYD_ESCAPE)
		{
			menuactive = msg_lastmenu?true:false;
			msg_mode = 0;
      
			if (message_input_routine)
				(* message_input_routine)(NULL);

			input_string.clear();
			
			M_ClearMenus();
			S_StartFX(sfx_swtchx);
			return true;
		}

		if ((ch == KEYD_BACKSPACE || ch == KEYD_DELETE) && !input_string.empty())
		{
			std::string s = input_string.c_str();

			if (input_string.size() > 0)
			{
				input_string.resize(input_string.size() - 1);
			}

			return true;
		}
		
        // Use unicode encoding if available
        int unicode = ev->value.key.unicode;
        if (HU_IS_PRINTABLE(unicode))
            ch = unicode;
		else
			ch = toupper(ch);

		if (ch == '-')
			ch = '_';

		if (HU_IS_PRINTABLE(ch))  // FIXME: international characters ??
		{
			// Set the input_string only if fits
			if (input_string.size() < 64)
			{
				input_string += ch;
			}
		}

		return true;
	}

	// new options menu on - use that responder
//!!!!!	if (option_menuon)
//!!!!!		return M_OptResponder(ev, ch);

//!!!!!	if (netgame_menuon)
//!!!!!		return M_NetGameResponder(ev, ch);

	// Save Game string input
	if (saveStringEnter)
	{
		switch (ch)
		{
			case KEYD_BACKSPACE:
				if (saveCharIndex > 0)
				{
					saveCharIndex--;
					ex_slots[save_slot].desc[saveCharIndex] = 0;
				}
				break;

			case KEYD_ESCAPE:
				saveStringEnter = 0;
				strcpy(ex_slots[save_slot].desc, saveOldString);
				break;

			case KEYD_ENTER:
				saveStringEnter = 0;
				if (ex_slots[save_slot].desc[0])
					M_DoSave(save_page, save_slot);
				break;

			default:
				ch = toupper(ch);

				if (ch >= 32 && ch <= 127 &&
					saveCharIndex < SAVESTRINGSIZE - 1)
				{
					ex_slots[save_slot].desc[saveCharIndex++] = ch;
					ex_slots[save_slot].desc[saveCharIndex] = 0;
				}
				break;
		}
		return true;
	}

	// F-Keys
	if (!menuactive)
	{
		switch (ch)
		{
			case '-':  // Screen size down

				if (automapactive || chat_on)
					return false;

				m_screenhud = (m_screenhud.d - 1 + NUMHUD) % NUMHUD;

				S_StartFX(sfx_stnmov);
				return true;

			case '+':
			case '=':  // Screen size up

				if (automapactive || chat_on)
					return false;

				m_screenhud = (m_screenhud.d + 1) % NUMHUD;

				S_StartFX(sfx_stnmov);
				return true;

			case KEYD_F2:  // Save

				M_StartControlPanel();
				S_StartFX(sfx_swtchn);
				M_SaveGame(0);
				return true;

			case KEYD_F3:  // Load

				M_StartControlPanel();
				S_StartFX(sfx_swtchn);
				M_LoadGame(0);
				return true;

			case KEYD_F4:  // Sound Volume

				M_StartControlPanel();
				currentMenu = &SoundDef;
				itemOn = 0;
				S_StartFX(sfx_swtchn);
				return true;

			case KEYD_F5:  // Detail toggle, now loads options menu
				// -KM- 1998/07/31 F5 now loads options menu, detail is obsolete.

				S_StartFX(sfx_swtchn);
				M_StartControlPanel();
				M_Options(0);
				return true;

			case KEYD_F6:  // Quicksave

				S_StartFX(sfx_swtchn);
				M_QuickSave();
				return true;

			case KEYD_F7:  // End game

				S_StartFX(sfx_swtchn);
				M_EndGame(0);
				return true;

			case KEYD_F8:  // Toggle messages

				M_ChangeMessages(0);
				S_StartFX(sfx_swtchn);
				return true;

			case KEYD_F9:  // Quickload

				S_StartFX(sfx_swtchn);
				M_QuickLoad();
				return true;

			case KEYD_F10:  // Quit DOOM

				S_StartFX(sfx_swtchn);
				M_QuitEDGE(0);
				return true;

			case KEYD_F11:  // gamma toggle

				M_BumpGamma();

				// -AJA- 1999/07/03: removed PLAYPAL reference.
				return true;

		}

		// Pop-up menu?
		if (ch == KEYD_ESCAPE)
		{
			M_StartControlPanel();
			S_StartFX(sfx_swtchn);
			return true;
		}
		return false;
	}

	// Keys usable within menu
	switch (ch)
	{
		case KEYD_DOWNARROW:
		case KEYD_WHEEL_DN:
			do
			{
				if (itemOn + 1 > currentMenu->numitems - 1)
					itemOn = 0;
				else
					itemOn++;
				S_StartFX(sfx_pstop);
			}
			while (currentMenu->menuitems[itemOn].status == -1);
			return true;

		case KEYD_UPARROW:
		case KEYD_WHEEL_UP:
			do
			{
				if (itemOn == 0)
					itemOn = currentMenu->numitems - 1;
				else
					itemOn--;
				S_StartFX(sfx_pstop);
			}
			while (currentMenu->menuitems[itemOn].status == -1);
			return true;

		case KEYD_PGUP:
		case KEYD_LEFTARROW:
			if (currentMenu->menuitems[itemOn].select_func &&
				currentMenu->menuitems[itemOn].status == 2)
			{
				S_StartFX(sfx_stnmov);
				// 98-7-10 KM Use new defines
				(* currentMenu->menuitems[itemOn].select_func)(SLIDERLEFT);
			}
			return true;

		case KEYD_PGDN:
		case KEYD_RIGHTARROW:
			if (currentMenu->menuitems[itemOn].select_func &&
				currentMenu->menuitems[itemOn].status == 2)
			{
				S_StartFX(sfx_stnmov);
				// 98-7-10 KM Use new defines
				(* currentMenu->menuitems[itemOn].select_func)(SLIDERRIGHT);
			}
			return true;

		case KEYD_ENTER:
		case KEYD_MOUSE1:
			if (currentMenu->menuitems[itemOn].select_func &&
				currentMenu->menuitems[itemOn].status)
			{
				currentMenu->lastOn = itemOn;
				(* currentMenu->menuitems[itemOn].select_func)(itemOn);
				S_StartFX(sfx_pistol);
			}
			return true;

		case KEYD_ESCAPE:
		case KEYD_MOUSE2:
		case KEYD_MOUSE3:
			currentMenu->lastOn = itemOn;
			M_ClearMenus();
			S_StartFX(sfx_swtchx);
			return true;

		case KEYD_BACKSPACE:
			currentMenu->lastOn = itemOn;
			if (currentMenu->prevMenu)
			{
				currentMenu = currentMenu->prevMenu;
				itemOn = currentMenu->lastOn;
				S_StartFX(sfx_swtchn);
			}
			return true;

		default:
			for (i = itemOn + 1; i < currentMenu->numitems; i++)
				if (currentMenu->menuitems[i].alpha_key == ch)
				{
					itemOn = i;
					S_StartFX(sfx_pstop);
					return true;
				}
			for (i = 0; i <= itemOn; i++)
				if (currentMenu->menuitems[i].alpha_key == ch)
				{
					itemOn = i;
					S_StartFX(sfx_pstop);
					return true;
				}
			break;

	}

	return false;
}

void M_StartControlPanel(void)
{
	// intro might call this repeatedly
	if (menuactive)
		return;

	menuactive = true;
	CON_SetVisible(vs_notvisible);

	currentMenu = &MainDef;  // JDC
	itemOn = currentMenu->lastOn;  // JDC

//!!!!!	M_OptCheckNetgame();
}


static void DrawMessage(void)
{
	HUD_SetAlpha(0.64f);
	HUD_SolidBox(0, 0, 320, 200, T_BLACK);
	HUD_SetAlpha();

	std::string s(msg_string);

	s += input_string;

	if (msg_mode == 2)
		s += "_";
	
	HUD_SetAlignment(0, 0);
	HUD_DrawText(160, 100, s.c_str());
	HUD_SetAlignment();
}


void M_Drawer(void)
{
	HUD_Reset();

	short x, y;

	unsigned int i;
	unsigned int max;

	if (!menuactive)
		return;

	// Horiz. & Vertically center string and print it.
	if (msg_mode)
	{
		DrawMessage();
		return;
	}

	// new options menu enable, use that drawer instead
	if (option_menuon)
	{
//!!!!!		M_OptDrawer();
		return;
	}

	if (netgame_menuon)
	{
//!!!!!		M_NetGameDrawer();
		return;
	}

	HUD_SetAlpha(0.64f);
	HUD_SolidBox(0, 0, 320, 200, T_BLACK);
	HUD_SetAlpha();

	// call Draw routine
	if (currentMenu->draw_func)
		(* currentMenu->draw_func)();

	// DRAW MENU
	x = currentMenu->x;
	y = currentMenu->y;
	max = currentMenu->numitems;

	for (i = 0; i < max; i++, y += LINEHEIGHT)
	{
		// ignore blank lines
		if (! currentMenu->menuitems[i].patch_name[0])
			continue;

		if (! currentMenu->menuitems[i].image)
			currentMenu->menuitems[i].image = R_ImageLookup(
				currentMenu->menuitems[i].patch_name);

		const image_c *image = currentMenu->menuitems[i].image;

		HUD_DrawImage(x, y, image);
	}

	// DRAW SKULL
	{
		int sx = x + SKULLXOFF;
		int sy = currentMenu->y - 5 + itemOn * LINEHEIGHT;

		HUD_DrawImage(sx, sy, menu_skull[whichSkull]);
	}
}

void M_ClearMenus(void)
{
	// -AJA- 2007/12/24: save user changes ASAP (in case of crash)
	if (menuactive)
	{
		M_SaveDefaults();
	}

	menuactive = false;
	save_screenshot_valid = false;
}

void M_SetupNextMenu(menu_t * menudef)
{
	currentMenu = menudef;
	itemOn = currentMenu->lastOn;
}

void M_Ticker(void)
{
	// update language if it changed
	if (m_language.CheckModified())
		if (! language.Select(m_language.str))
			I_Printf("Unknown language: %s\n", m_language.str);

	if (option_menuon)
	{
//!!!!!		M_OptTicker();
		return;
	}

	if (netgame_menuon)
	{
//!!!!!		M_NetGameTicker();
		return;
	}

	if (--skullAnimCounter <= 0)
	{
		whichSkull ^= 1;
		skullAnimCounter = 8;
	}
}

void M_Init(void)
{
	E_ProgressMessage(language["MiscInfo"]);

	currentMenu = &MainDef;
	menuactive = false;
	itemOn = currentMenu->lastOn;
	whichSkull = 0;
	skullAnimCounter = 10;
	msg_mode = 0;
	msg_string.clear();
	msg_lastmenu = menuactive;
	quickSaveSlot = -1;

	// lookup required images
	therm_l = R_ImageLookup("M_THERML");
	therm_m = R_ImageLookup("M_THERMM");
	therm_r = R_ImageLookup("M_THERMR");
	therm_o = R_ImageLookup("M_THERMO");

	menu_loadg    = R_ImageLookup("M_LOADG");
	menu_saveg    = R_ImageLookup("M_SAVEG");
	menu_svol     = R_ImageLookup("M_SVOL");
	menu_newgame  = R_ImageLookup("M_NEWG");
	menu_skill    = R_ImageLookup("M_SKILL");
	menu_episode  = R_ImageLookup("M_EPISOD");
	menu_skull[0] = R_ImageLookup("M_SKULL1");
	menu_skull[1] = R_ImageLookup("M_SKULL2");

	if (W_CheckNumForName("M_HTIC") >= 0)
		menu_doom = R_ImageLookup("M_HTIC");
	else
		menu_doom = R_ImageLookup("M_DOOM");

	// Here we could catch other version dependencies,
	//  like HELP1/2, and four episodes.
	//    if (W_CheckNumForName("M_EPI4") < 0)
	//      EpiDef.numitems -= 2;
	//    else if (W_CheckNumForName("M_EPI5") < 0)
	//      EpiDef.numitems--;

	if (W_CheckNumForName("HELP") >= 0)
		menu_readthis[0] = R_ImageLookup("HELP");
	else
		menu_readthis[0] = R_ImageLookup("HELP1");

	if (W_CheckNumForName("HELP2") >= 0)
		menu_readthis[1] = R_ImageLookup("HELP2");
	else
	{
		menu_readthis[1] = R_ImageLookup("CREDIT");

		// This is used because DOOM 2 had only one HELP
		//  page. I use CREDIT as second page now, but
		//  kept this hack for educational purposes.

		MainMenu[readthis] = MainMenu[quitdoom];
		MainDef.numitems--;
		MainDef.y += 8; // FIXME
		SkillDef.prevMenu = &MainDef;
		ReadDef1.draw_func = M_DrawReadThis1;
		ReadDef1.x = 330;
		ReadDef1.y = 165;
		ReadMenu1[0].select_func = M_FinishReadThis;
	}

	sfx_swtchn = sfxdefs.GetEffect("SWTCHN");
	sfx_tink   = sfxdefs.GetEffect("TINK");
	sfx_radio  = sfxdefs.GetEffect("RADIO");
	sfx_oof    = sfxdefs.GetEffect("OOF");
	sfx_pstop  = sfxdefs.GetEffect("PSTOP");
	sfx_stnmov = sfxdefs.GetEffect("STNMOV");
	sfx_pistol = sfxdefs.GetEffect("PISTOL");
	sfx_swtchx = sfxdefs.GetEffect("SWTCHX");

//!!!!!	M_OptMenuInit();
//!!!!!	M_NetGameInit();
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
