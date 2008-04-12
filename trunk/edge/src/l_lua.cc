//------------------------------------------------------------------------
//  LUA HUD code
//------------------------------------------------------------------------
//
//  Copyright (c) 2006-2008  The EDGE Team.
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

#include "i_defs.h"
#include "i_luainc.h"

#include "ddf/main.h"
#include "ddf/font.h"

#include "dm_state.h"
#include "e_main.h"
#include "g_game.h"
#include "l_lua.h"
#include "version.h"

#include "e_player.h"
#include "hu_font.h"
#include "r_draw.h"
#include "r_modes.h"
#include "w_wad.h"
#include "z_zone.h"

#include "r_misc.h"     //  R_Render
#include "r_automap.h"  // AM_Drawer


static lua_State *HUD_ST;

static bool has_loaded = false;

static player_t *cur_player = NULL;

static font_c *cur_font = NULL;
static colourmap_c *cur_colmap = NULL;
static float cur_scale;
static float cur_alpha;

static int hud_last_time = -1;

extern std::string w_map_title;


static void FrameSetup(void)
{
	fontdef_c *DEF = fontdefs.Lookup("DOOM");  // FIXME allow other default
	SYS_ASSERT(DEF);

	cur_font = hu_fonts.Lookup(DEF);
	SYS_ASSERT(cur_font);

	cur_colmap = NULL;
	cur_scale  = 1.0f;
	cur_alpha  = 1.0f;

	cur_player = players[displayplayer];


	int now_time = I_GetTime();
	int passed_time = 0;

	if (hud_last_time > 0 && hud_last_time <= now_time)
	{
		passed_time = MIN(now_time - hud_last_time, TICRATE);
	}

	hud_last_time = now_time;


	// setup some fields in 'hud' module

	lua_getglobal(HUD_ST, "hud");

	lua_pushinteger(HUD_ST, screen_hud);
	lua_setfield(HUD_ST, -2, "which");

	lua_pushboolean(HUD_ST, automapactive);
	lua_setfield(HUD_ST, -2, "automap");

	lua_pushinteger(HUD_ST, now_time);
	lua_setfield(HUD_ST, -2, "now_time");

	lua_pushinteger(HUD_ST, passed_time);
	lua_setfield(HUD_ST, -2, "passed_time");

	lua_pop(HUD_ST, 1);
}


static void DoDrawChar(float cx, float cy, char ch)
{
	const image_c *image = cur_font->CharImage(ch);

	if (! image)
		return;
	
	float sc_x = cur_scale;  // * aspect
	float sc_y = cur_scale;

	cx -= IM_OFFSETX(image) * sc_x;
	cy -= IM_OFFSETY(image) * sc_y;

	RGL_DrawImage(
	    FROM_320(cx),
		SCREENHEIGHT - FROM_200(cy + IM_HEIGHT(image) * sc_y),
		FROM_320(IM_WIDTH(image))  * sc_x,
		FROM_200(IM_HEIGHT(image)) * sc_y,
		image, 0.0f, 0.0f,
		IM_RIGHT(image), IM_TOP(image),
		cur_colmap, cur_alpha);
}

static void DoWriteText(float x, float y, const char *str)
{
	float cx = x;
	float cy = y;

	for (; *str; str++)
	{
		char ch = *str;

		if (ch == '\n')
		{
			cx = x;
			cy += 12.0f * cur_scale;  // FIXME: use font's height
			continue;
		}

		if (FROM_320(cx) >= SCREENWIDTH)
			continue;

		DoDrawChar(cx, cy, ch);

		cx += cur_font->CharWidth(ch) * cur_scale;
	}
}

static void DoWriteText_RightAlign(float x, float y, const char *str)
{
	float cx = x;
	float cy = y;

	for (int pos = strlen(str)-1; pos >= 0; pos--)
	{
		char ch = str[pos];

		if (ch == '\n')
		{
			cx = x;
			cy += 12.0f * cur_scale;  // FIXME: use font's height
			continue;
		}

		if (FROM_320(cx) >= SCREENWIDTH)
			continue;

		cx -= cur_font->CharWidth(ch) * cur_scale;

		DoDrawChar(cx, cy, ch);
	}
}


//------------------------------------------------------------------------
//  HUD MODULE
//------------------------------------------------------------------------


static rgbcol_t ParseColor(lua_State *L, int index)
{
	if (lua_isstring(L, index))
	{
		const char *name = lua_tostring(L, index);

		if (name[0] != '#')
			I_Error("Bad color in lua script (missing #) : %s\n", name);

		return (rgbcol_t) strtol(name+1, NULL, 16);
	}

	if (lua_istable(L, index))
	{
		// parse 'r', 'g', 'b' fields
		int r, g, b;

		lua_getfield(L, index, "r");
		if (! lua_isnumber(L, -1))
			I_Error("Bad color table in lua script (missing \"r\")\n");
		r = lua_tointeger(L, -1);

		lua_getfield(L, index, "g");
		if (! lua_isnumber(L, -1))
			I_Error("Bad color table in lua script (missing \"g\")\n");
		g = lua_tointeger(L, -1);

		lua_getfield(L, index, "b");
		if (! lua_isnumber(L, -1))
			I_Error("Bad color table in lua script (missing \"b\")\n");
		b = lua_tointeger(L, -1);

		lua_pop(L, 3);

		return RGB_MAKE(r, g, b);
	}

	I_Error("Bad color value in lua script!\n");
	return 0; /* NOT REACHED */
}


// hud.raw_debug_print(str)
//
static int HD_raw_debug_print(lua_State *L)
{
	int nargs = lua_gettop(L);

	if (nargs >= 1)
	{
		const char *res = luaL_checkstring(L,1);
		SYS_ASSERT(res);

		I_Debugf("%s", res);
	}

	return 0;
}


// hud.coord_sys(w, h)
//
static int HD_coord_sys(lua_State *L)
{
	int w = luaL_checkint(L, 1);
	int h = luaL_checkint(L, 2);

	if (w < 64 || h < 64)
		I_Error("Bad hud.coord_sys size: %dx%d\n", w, h);

	//... FIXME

	return 0;
}


// hud.game_mode()
//
static int HD_game_mode(lua_State *L)
{
	if (DEATHMATCH())
		lua_pushstring(L, "dm");
	else if (COOP_MATCH())
		lua_pushstring(L, "coop");
	else
		lua_pushstring(L, "sp");

	return 1;
}


// hud.game_name()
//
static int HD_game_name(lua_State *L)
{
	gamedef_c *g = currmap->episode;
	SYS_ASSERT(g);

	lua_pushstring(L, g->ddf.name.c_str());
	return 1;
}


// hud.map_name()
//
static int HD_map_name(lua_State *L)
{
	lua_pushstring(L, currmap->ddf.name.c_str());
	return 1;
}


// hud.map_title()
//
static int HD_map_title(lua_State *L)
{
	lua_pushstring(L, w_map_title.c_str());
	return 1;
}


// hud.text_font(name)
//
static int HD_text_font(lua_State *L)
{
	const char *font_name = luaL_checkstring(L, 1);

	fontdef_c *DEF = fontdefs.Lookup(font_name);
	SYS_ASSERT(DEF);

	cur_font = hu_fonts.Lookup(DEF);
	SYS_ASSERT(cur_font);

	return 0;
}


// hud.text_color(name)
//
static int HD_text_color(lua_State *L)
{
	// TODO: table colors { r=xxx,g=xxx,b=xxx }

	const char *col_str = luaL_checkstring(L, 1);

	if (strlen(col_str) == 0)
	{
		cur_colmap = NULL;
		return 0;
	}

	if (col_str[0] == '#')
		I_Error("hud.text_color: cannot use # colors yet!\n");

	cur_colmap = colourmaps.Lookup(col_str);

	if (! cur_colmap)
		I_Error("hud.text_color: no such colormap: %s\n", col_str);

	return 0;
}


// hud.set_scale(value)
//
static int HD_set_scale(lua_State *L)
{
	cur_scale = luaL_checknumber(L, 1);

	if (cur_scale <= 0)
		I_Error("hud.set_scale: Bad scale value: %1.3f\n", cur_scale);

	return 0;
}


// hud.set_alpha(value)
//
static int HD_set_alpha(lua_State *L)
{
	cur_alpha = luaL_checknumber(L, 1);

	return 0;
}


// hud.solid_box(x, y, w, h, color [,alpha])
//
static int HD_solid_box(lua_State *L)
{
	float x = luaL_checknumber(L, 1);
	float y = luaL_checknumber(L, 2);
	float w = luaL_checknumber(L, 3);
	float h = luaL_checknumber(L, 4);

	w = FROM_320(w); h = FROM_200(h);
	x = FROM_320(x); y = SCREENHEIGHT - FROM_200(y) - h;

	rgbcol_t rgb = ParseColor(L, 5);

	RGL_SolidBox((int)x, (int)y, I_ROUND(w), I_ROUND(h), rgb, cur_alpha);

	return 0;
}


// hud.solid_line(x1, y1, x2, y2, color [,alpha])
//
static int HD_solid_line(lua_State *L)
{
	float x1 = luaL_checknumber(L, 1);
	float y1 = luaL_checknumber(L, 2);
	float x2 = luaL_checknumber(L, 3);
	float y2 = luaL_checknumber(L, 4);

	x1 = FROM_320(x1); y1 = SCREENHEIGHT - FROM_200(y1);
	x2 = FROM_320(x2); y2 = SCREENHEIGHT - FROM_200(y2);

	rgbcol_t rgb = ParseColor(L, 5);

	RGL_SolidLine((int)x1, (int)y1, (int)x2, (int)y2, rgb, cur_alpha);

	return 0;
}


// hud.thin_box(x, y, w, h, color)
//
static int HD_thin_box(lua_State *L)
{
	float x = luaL_checknumber(L, 1);
	float y = luaL_checknumber(L, 2);
	float w = luaL_checknumber(L, 3);
	float h = luaL_checknumber(L, 4);

	w = FROM_320(w); h = FROM_200(h);
	x = FROM_320(x); y = SCREENHEIGHT - FROM_200(y) - h;

	rgbcol_t rgb = ParseColor(L, 5);

	RGL_ThinBox((int)x, (int)y, I_ROUND(w), I_ROUND(h), rgb, cur_alpha);

	return 0;
}


// hud.gradient_box(x, y, w, h, TL, BL, TR, BR)
//
static int HD_gradient_box(lua_State *L)
{
	float x = luaL_checknumber(L, 1);
	float y = luaL_checknumber(L, 2);
	float w = luaL_checknumber(L, 3);
	float h = luaL_checknumber(L, 4);

	w = FROM_320(w); h = FROM_200(h);
	x = FROM_320(x); y = SCREENHEIGHT - FROM_200(y) - h;

	rgbcol_t cols[4];

	cols[0] = ParseColor(L, 5);
	cols[1] = ParseColor(L, 6);
	cols[2] = ParseColor(L, 7);
	cols[3] = ParseColor(L, 8);

	RGL_GradientBox((int)x, (int)y, I_ROUND(w), I_ROUND(h), cols, cur_alpha);

	return 0;
}


// hud.draw_image(x, y, name)
//
static int HD_draw_image(lua_State *L)
{
	float x = luaL_checknumber(L, 1);
	float y = luaL_checknumber(L, 2);

	const char *name = luaL_checkstring(L, 3);

	const image_c *img = W_ImageLookup(name, INS_Graphic);
	if (img)
	{
		x -= IM_OFFSETX(img);
		y -= IM_OFFSETY(img);

		float w = IM_WIDTH(img);
		float h = IM_HEIGHT(img);

		w *= cur_scale;
		h *= cur_scale;

		w = FROM_320(w); h = FROM_200(h);
		x = FROM_320(x); y = SCREENHEIGHT - FROM_200(y) - h;

		RGL_DrawImage(x, y, w, h, img,
		              0, 0, IM_RIGHT(img), IM_TOP(img),
					  NULL, cur_alpha);
	}

	return 0;
}


// hud.stretch_image(x, y, w, h, name)
//
static int HD_stretch_image(lua_State *L)
{
	float x = luaL_checknumber(L, 1);
	float y = luaL_checknumber(L, 2);
	float w = luaL_checknumber(L, 3);
	float h = luaL_checknumber(L, 4);

	const char *name = luaL_checkstring(L, 5);

	const image_c *img = W_ImageLookup(name, INS_Graphic);
	if (img)
	{
		w = FROM_320(w); h = FROM_200(h);
		x = FROM_320(x); y = SCREENHEIGHT - FROM_200(y) - h;

		RGL_DrawImage(x, y, w, h, img,
		              0, 0, IM_RIGHT(img), IM_TOP(img),
					  NULL, cur_alpha);
	}

	return 0;
}


// hud.tile_image(x, y, w, h, name, [x_offset, y_offset])
//
static int HD_tile_image(lua_State *L)
{
	float x = luaL_checknumber(L, 1);
	float y = luaL_checknumber(L, 2);
	float w = luaL_checknumber(L, 3);
	float h = luaL_checknumber(L, 4);

	const char *name = luaL_checkstring(L, 5);

	float offset_x = 0;
	float offset_y = 0;

	if (lua_isnumber(L, 6))
		offset_x = lua_tonumber(L, 6);

	if (lua_isnumber(L, 7))
		offset_y = lua_tonumber(L, 7);

	const image_c *img = W_ImageLookup(name, INS_Texture);
	if (img)
	{
		offset_x /=  w;
		offset_y /= -h;

		float tx_scale = w / IM_TOTAL_WIDTH(img)  / cur_scale;
		float ty_scale = h / IM_TOTAL_HEIGHT(img) / cur_scale;

		w = FROM_320(w); h = FROM_200(h);
		x = FROM_320(x); y = SCREENHEIGHT - FROM_200(y) - h;

		RGL_DrawImage(x, y, w, h, img,
		              (offset_x) * tx_scale,
					  (offset_y) * ty_scale,
					  (offset_x + 1) * tx_scale,
					  (offset_y + 1) * ty_scale,
					  NULL, cur_alpha);
	}

	return 0;
}


// hud.draw_text(x, y, str)
//
static int HD_draw_text(lua_State *L)
{
	float x = luaL_checknumber(L, 1);
	float y = luaL_checknumber(L, 2);

	const char *str = luaL_checkstring(L, 3);

	DoWriteText(x, y, str);

	return 0;
}


// hud.draw_num2(x, y, len, num)
//
static int HD_draw_num2(lua_State *L)
{
	float x = luaL_checknumber(L, 1);
	float y = luaL_checknumber(L, 2);

	int len = luaL_checkint(L, 3);
	int num = luaL_checkint(L, 4);

	if (len < 1 || len > 20)
		I_Error("hud.draw_number: bad field length: %d\n", len);

	bool is_neg = false;

	if (num < 0 && len > 1)
	{
		is_neg = true; len--;
	}

	// build the integer backwards

	char buffer[200];
	char *pos = &buffer[sizeof(buffer)-4];

	*--pos = 0;

	if (num == 0)
	{
		*--pos = '0';
	}
	else
	{
		for (; num > 0 && len > 0; num /= 10, len--)
			*--pos = '0' + (num % 10);

		if (is_neg)
			*--pos = '-';
	}

	DoWriteText_RightAlign(x, y, pos);

	return 0;
}


// hud.render_world(x, y, w, h)
//
static int HD_render_world(lua_State *L)
{
	float x = luaL_checknumber(L, 1);
	float y = luaL_checknumber(L, 2);
	float w = luaL_checknumber(L, 3);
	float h = luaL_checknumber(L, 4);

	x = FROM_320(x); y = FROM_200(y);
	w = FROM_320(w); h = FROM_200(h);

 	R_Render((int)x, SCREENHEIGHT-(int)(y+h), I_ROUND(w), I_ROUND(h), cur_player->mo);

	return 0;
}


// hud.render_automap(x, y, w, h)
//
static int HD_render_automap(lua_State *L)
{
	float x = luaL_checknumber(L, 1);
	float y = luaL_checknumber(L, 2);
	float w = luaL_checknumber(L, 3);
	float h = luaL_checknumber(L, 4);

	x = FROM_320(x); y = FROM_200(y);
	w = FROM_320(w); h = FROM_200(h);

 	AM_Drawer((int)x, SCREENHEIGHT-(int)(y+h), I_ROUND(w), I_ROUND(h), cur_player->mo);

	return 0;
}


static const char * am_color_names[AM_NUM_COLORS] =
{
    "grid",     // AMCOL_Grid

    "wall",     // AMCOL_Wall
    "step",     // AMCOL_Step
    "ledge",    // AMCOL_Ledge
    "ceil",     // AMCOL_Ceil
    "secret",   // AMCOL_Secret
    "allmap",   // AMCOL_Allmap

    "player",   // AMCOL_Player
    "monster",  // AMCOL_Monster
    "corpse",   // AMCOL_Corpse
    "item",     // AMCOL_Item
    "missile",  // AMCOL_Missile
    "scenery"   // AMCOL_Scenery
};


// hud.automap_colors(table)
//
static int HD_automap_colors(lua_State *L)
{
	if (! lua_istable(L, 1))
		I_Error("hud.automap_colors() requires a table!\n");

	for (int which = 0; which < AM_NUM_COLORS; which++)
	{
		lua_getfield(L, 1, am_color_names[which]);
		
		if (! lua_isnil(L, -1))
		{
			AM_SetColor(which, ParseColor(L, -1));
		}

		lua_pop(L, 1);
	}

	return 0;
}


static const luaL_Reg hud_module[] =
{
	{ "raw_debug_print", HD_raw_debug_print },

	// query functions
    { "game_mode",       HD_game_mode },
    { "game_name",       HD_game_name },
    { "map_name",  	     HD_map_name },
    { "map_title",  	 HD_map_title },

	// set-state functions
    { "coord_sys",       HD_coord_sys   },
    { "text_font",       HD_text_font   },
    { "text_color",      HD_text_color  },
    { "set_scale",       HD_set_scale   },
    { "set_alpha",       HD_set_alpha   },
    { "automap_colors",  HD_automap_colors },

	// drawing functions
    { "solid_box",       HD_solid_box   },
    { "solid_line",      HD_solid_line  },
    { "thin_box",        HD_thin_box    },
    { "gradient_box",    HD_gradient_box },

    { "draw_image",      HD_draw_image  },
    { "stretch_image",   HD_stretch_image },
    { "tile_image",      HD_tile_image  },
    { "draw_text",       HD_draw_text   },
    { "draw_num2",       HD_draw_num2   },

    { "render_world",    HD_render_world   },
    { "render_automap",  HD_render_automap },

	{ NULL, NULL } // the end
};


//------------------------------------------------------------------------
//  PLAYER MODULE
//------------------------------------------------------------------------


// player.num_players()
//
static int PL_num_players(lua_State *L)
{
	lua_pushinteger(L, numplayers);
	return 1;
}


// player.set_who(index)
//
static int PL_set_who(lua_State *L)
{
	int index = luaL_checkint(L, 1);

	if (index < 0 || index >= numplayers)
		I_Error("player.set_who: bad index value: %d (numplayers=%d)\n", index, numplayers);

	if (index == 0)
	{
		cur_player = players[consoleplayer];
		return 0;
	}

	int who = displayplayer;

	for (; index > 1; index--)
	{
		do
		{
			who = (who + 1) % MAXPLAYERS;
		}
		while (players[who] == NULL);
	}

	cur_player = players[who];

	return 0;
}


// player.is_bot()
//
static int PL_is_bot(lua_State *L)
{
	lua_pushboolean(L, (cur_player->playerflags & PFL_Bot) ? 1 : 0);
	return 1;
}


// player.get_name()
//
static int PL_get_name(lua_State *L)
{
	lua_pushstring(L, cur_player->playername);
	return 1;
}


// player.health()
//
static int PL_health(lua_State *L)
{
	float h = cur_player->health * 100 / cur_player->mo->info->spawnhealth;

	lua_pushinteger(L, (int)floor(h + 0.99));
	return 1;
}


// player.armor(type)
//
static int PL_armor(lua_State *L)
{
	int kind = luaL_checkint(L, 1);

	if (kind < 1 || kind > NUMARMOUR)
		I_Error("player.armor: bad armor index: %d\n", kind);
	
	kind--;

	lua_pushinteger(L, (int)floor(cur_player->armours[kind] + 0.99));
	return 1;
}


// player.total_armor(type)
//
static int PL_total_armor(lua_State *L)
{
	lua_pushinteger(L, (int)floor(cur_player->totalarmour + 0.99));
	return 1;
}


// player.frags()
//
static int PL_frags(lua_State *L)
{
	lua_pushinteger(L, cur_player->frags);
	return 1;
}


// player.under_water()
//
static int PL_under_water(lua_State *L)
{
	lua_pushboolean(L, cur_player->underwater ? 1 : 0);
	return 1;
}


// player.on_ground()
//
static int PL_on_ground(lua_State *L)
{
	lua_pushboolean(L, (cur_player->mo->z <= cur_player->mo->floorz) ? 1 : 0);
	return 1;
}


// player.is_swimming()
//
static int PL_is_swimming(lua_State *L)
{
	lua_pushboolean(L, cur_player->swimming ? 1 : 0);
	return 1;
}


// player.is_jumping()
//
static int PL_is_jumping(lua_State *L)
{
	lua_pushboolean(L, (cur_player->jumpwait > 0) ? 1 : 0);
	return 1;
}


// player.is_crouching()
//
static int PL_is_crouching(lua_State *L)
{
	lua_pushboolean(L, (cur_player->mo->extendedflags & EF_CROUCHING) ? 1 : 0);
	return 1;
}


// player.is_attacking()
//
static int PL_is_attacking(lua_State *L)
{
	lua_pushboolean(L, (cur_player->attackdown[0] ||
	                    cur_player->attackdown[1]) ? 1 : 0);
	return 1;
}


// player.is_rampaging()
//
static int PL_is_rampaging(lua_State *L)
{
	lua_pushboolean(L, (cur_player->attackdown_count >= 70) ? 1 : 0);
	return 1;
}


// player.is_grinning()
//
static int PL_is_grinning(lua_State *L)
{
	lua_pushboolean(L, (cur_player->grin_count > 0) ? 1 : 0);
	return 1;
}


// player.is_using()
//
static int PL_is_using(lua_State *L)
{
	lua_pushboolean(L, cur_player->usedown ? 1 : 0);
	return 1;
}


// player.move_speed()
//
static int PL_move_speed(lua_State *L)
{
	lua_pushnumber(L, cur_player->actual_speed);
	return 1;
}


// player.air_in_lungs()
//
static int PL_air_in_lungs(lua_State *L)
{
	if (cur_player->air_in_lungs <= 0)
	{
		lua_pushnumber(L, 0.0f);
		return 1;
	}

	float value = cur_player->air_in_lungs * 100.0f /
	              cur_player->mo->info->lung_capacity;

	value = CLAMP(0.0f, value, 100.0f);

	lua_pushnumber(L, value);
	return 1;
}


// player.has_key(key)
//
static int PL_has_key(lua_State *L)
{
	int key = luaL_checkint(L, 1);

	if (key < 1 || key > 16)
		I_Error("player.has_key: bad key number: %d\n", key);

	key--;

	int value = (cur_player->cards & (1 << key)) ? 1 : 0;

	lua_pushboolean(L, value);
	return 1;
}


// player.has_power(key)
//
static int PL_has_power(lua_State *L)
{
	int power = luaL_checkint(L, 1);

	if (power < 1 || power > NUMPOWERS)
		I_Error("player.has_power: bad powerup number: %d\n", power);

	power--;

	int value = (cur_player->powers[power] > 0) ? 1 : 0;

	// special check for GOD mode
	if (power == PW_Invulnerable && (cur_player->cheats && CF_GODMODE))
		value = 1;

	lua_pushboolean(L, value);
	return 1;
}


// player.has_weapon_slot(slot)
//
static int PL_has_weapon_slot(lua_State *L)
{
	int slot = luaL_checkint(L, 1);

	if (slot < 0 || slot > 9)
		I_Error("player.has_weapon_slot: bad slot number: %d\n", slot);

	int value = cur_player->avail_weapons[slot] ? 1 : 0;

	lua_pushboolean(L, value);
	return 1;
}


static int PL_cur_weapon_slot(lua_State *L)
{
	int slot;

	if (cur_player->ready_wp < 0)
		slot = -1;
	else
		slot = cur_player->weapons[cur_player->ready_wp].info->bind_key;

	lua_pushinteger(L, slot);
	return 1;
}


// player.has_weapon(name)
//
static int PL_has_weapon(lua_State *L)
{
	const char * name = luaL_checkstring(L, 1);
	SYS_ASSERT(name);

	for (int j = 0; j < MAXWEAPONS; j++)
	{
		playerweapon_t *pw = &cur_player->weapons[j];

		if (pw->owned && ! (pw->flags & PLWEP_Removing))
		{
			if (DDF_CompareName(name, pw->info->ddf.name.c_str()) == 0)
			{
				lua_pushboolean(L, 1);
				return 1;
			}
		}
	}

	lua_pushboolean(L, 0);
	return 1;
}


static int PL_cur_weapon(lua_State *L)
{
	if (cur_player->pending_wp >= 0)
	{
		lua_pushstring(L, "change");
		return 1;
	}

	if (cur_player->ready_wp < 0)
	{
		lua_pushstring(L, "none");
		return 1;
	}

	weapondef_c *info = cur_player->weapons[cur_player->ready_wp].info;

	lua_pushstring(L, info->ddf.name.c_str());
	return 1;
}


// player.ammo(type)
//
static int PL_ammo(lua_State *L)
{
	int ammo = luaL_checkint(L, 1);

	if (ammo < 1 || ammo > NUMAMMO)
		I_Error("player.ammo: bad ammo number: %d\n", ammo);

	ammo--;

	lua_pushinteger(L, cur_player->ammo[ammo].num);
	return 1;
}


// player.ammomax(type)
//
static int PL_ammomax(lua_State *L)
{
	int ammo = luaL_checkint(L, 1);

	if (ammo < 1 || ammo > NUMAMMO)
		I_Error("player.ammomax: bad ammo number: %d\n", ammo);

	ammo--;

	lua_pushinteger(L, cur_player->ammo[ammo].max);
	return 1;
}


// player.main_ammo(clip)
//
static int PL_main_ammo(lua_State *L)
{
	int value = 0;

	if (cur_player->ready_wp >= 0)
	{
		playerweapon_t *pw = &cur_player->weapons[cur_player->ready_wp];

		if (pw->info->ammo[0] != AM_NoAmmo)
		{
			if (pw->info->show_clip)
			{
				SYS_ASSERT(pw->info->ammopershot[0] > 0);

				value = pw->clip_size[0] / pw->info->ammopershot[0];
			}
			else
			{
				value = cur_player->ammo[pw->info->ammo[0]].num;

				if (pw->info->clip_size[0] > 0)
					value += pw->clip_size[0];
			}
		}
	}

	lua_pushinteger(L, value);
	return 1;
}


// player.ammo_type(ATK)
//
static int PL_ammo_type(lua_State *L)
{
	int ATK = luaL_checkint(L, 1);

	if (ATK < 1 || ATK > 2)
		I_Error("player.ammo_type: bad attack number: %d\n", ATK);

	ATK--;

	int value = 0;

	if (cur_player->ready_wp >= 0)
	{
		playerweapon_t *pw = &cur_player->weapons[cur_player->ready_wp];
		
		value = 1 + (int) pw->info->ammo[ATK];
	}

	lua_pushinteger(L, value);
	return 1;
}


// player.ammo_pershot(ATK)
//
static int PL_ammo_pershot(lua_State *L)
{
	int ATK = luaL_checkint(L, 1);

	if (ATK < 1 || ATK > 2)
		I_Error("player.ammo_pershot: bad attack number: %d\n", ATK);

	ATK--;

	int value = 0;

	if (cur_player->ready_wp >= 0)
	{
		playerweapon_t *pw = &cur_player->weapons[cur_player->ready_wp];
		
		value = pw->info->ammopershot[ATK];
	}

	lua_pushinteger(L, value);
	return 1;
}


// player.clip_ammo(ATK)
//
static int PL_clip_ammo(lua_State *L)
{
	int ATK = luaL_checkint(L, 1);

	if (ATK < 1 || ATK > 2)
		I_Error("player.clip_ammo: bad attack number: %d\n", ATK);

	ATK--;

	int value = 0;

	if (cur_player->ready_wp >= 0)
	{
		playerweapon_t *pw = &cur_player->weapons[cur_player->ready_wp];

		value = pw->clip_size[ATK];
	}

	lua_pushinteger(L, value);
	return 1;
}


// player.clip_size(ATK)
//
static int PL_clip_size(lua_State *L)
{
	int ATK = luaL_checkint(L, 1);

	if (ATK < 1 || ATK > 2)
		I_Error("player.clip_size: bad attack number: %d\n", ATK);

	ATK--;

	int value = 0;

	if (cur_player->ready_wp >= 0)
	{
		playerweapon_t *pw = &cur_player->weapons[cur_player->ready_wp];

		value = pw->info->clip_size[ATK];
	}

	lua_pushinteger(L, value);
	return 1;
}


// player.clip_is_shared()
//
static int PL_clip_is_shared(lua_State *L)
{
	int value = 0;

	if (cur_player->ready_wp >= 0)
	{
		playerweapon_t *pw = &cur_player->weapons[cur_player->ready_wp];

		if (pw->info->shared_clip)
			value = 1;
	}

	lua_pushboolean(L, value);
	return 1;
}


// player.hurt_by()
//
static int PL_hurt_by(lua_State *L)
{
	if (cur_player->damagecount <= 0)
		return 0;  // return NIL

	if (cur_player->attacker)
	{
		lua_pushstring(L, "enemy");
		return 1;
	}

	// getting hurt because of your own damn stupidity
	lua_pushstring(L, "self");
	return 1;
}


// player.hurt_pain()
//
static int PL_hurt_pain(lua_State *L)
{
	lua_pushnumber(L, cur_player->damage_pain);
	return 1;
}


// player.hurt_dir()
//
static int PL_hurt_dir(lua_State *L)
{
	int dir = 0;

	if (cur_player->attacker && cur_player->attacker != cur_player->mo)
	{
		mobj_t *badguy = cur_player->attacker;
		mobj_t *pmo    = cur_player->mo;

		angle_t diff = R_PointToAngle(pmo->x, pmo->y, badguy->x, badguy->y) - pmo->angle;

		if (diff >= ANG45 && diff <= ANG135)
		{
			dir = -1;
		}
		else if (diff >= ANG225 && diff <= ANG315)
		{
			dir = +1;
		}
	}

	lua_pushinteger(L, dir);
	return 1;
}


static const luaL_Reg player_module[] =
{
	{ "num_players", PL_num_players },
	{ "set_who",     PL_set_who  },
    { "is_bot",      PL_is_bot   },
    { "get_name",    PL_get_name },

    { "health",      PL_health      },
    { "armor",       PL_armor       },
    { "total_armor", PL_total_armor },
    { "frags",       PL_frags       },
    { "ammo",        PL_ammo        },
    { "ammomax",     PL_ammomax     },

    { "is_swimming",     PL_is_swimming   },
    { "is_jumping",      PL_is_jumping    },
    { "is_crouching",    PL_is_crouching  },
    { "is_using",        PL_is_using      },
    { "is_attacking",    PL_is_attacking  },
    { "is_rampaging",    PL_is_rampaging  },
    { "is_grinning",     PL_is_grinning   },

    { "under_water",     PL_under_water   },
    { "on_ground",       PL_on_ground     },
    { "move_speed",      PL_move_speed    },
    { "air_in_lungs",    PL_air_in_lungs  },

    { "has_key",         PL_has_key   },
    { "has_power",       PL_has_power },
    { "has_weapon",      PL_has_weapon      },
    { "has_weapon_slot", PL_has_weapon_slot },
    { "cur_weapon",      PL_cur_weapon      },
    { "cur_weapon_slot", PL_cur_weapon_slot },

    { "main_ammo",       PL_main_ammo  },
    { "ammo_type",       PL_ammo_type  },
    { "ammo_pershot",    PL_ammo_pershot },
    { "clip_ammo",       PL_clip_ammo  },
    { "clip_size",       PL_clip_size  },
    { "clip_is_shared",  PL_clip_is_shared },

    { "hurt_by",         PL_hurt_by    },
    { "hurt_pain",       PL_hurt_pain  },
    { "hurt_dir",        PL_hurt_dir   },

	{ NULL, NULL } // the end
};


//------------------------------------------------------------------------


static int p_init_lua(lua_State *L)
{
	/* stop collector during initialization */
	lua_gc(L, LUA_GCSTOP, 0);
	{
		/* open libraries */

		luaL_openlibs(L);

		/* register our own modules */

		luaL_register(HUD_ST, "hud",    hud_module);
		luaL_register(HUD_ST, "player", player_module);

		// remove the tables which luaL_register created
		lua_pop(L, 2);
	}
	lua_gc(L, LUA_GCRESTART, 0);

	return 0;
}


void LU_Init(void)
{
	HUD_ST = lua_open();

	if (! HUD_ST)
		I_Error("LUA Init failed: cannot create new state");

	int status = lua_cpcall(HUD_ST, &p_init_lua, NULL);

	if (status != 0)
		I_Error("LUA Init failed (status %d)", status);

	// FIXME !!!! make sure script cannot load any DLLs or other scripts
}


void LU_Close(void)
{
	if (HUD_ST)
	{
		lua_close(HUD_ST);
		HUD_ST = NULL;
	}
}


void LU_LoadScripts(void)
{
	static const char *script_lumps[] =
	{
		"LUAUTIL0",  // edge.wad
		"LUAUTIL1",  // user - in a TC
		"LUAUTIL2",  // user - for a add-on

		"LUAHUD0",  // edge.wad
		"LUAHUD1",  // user - in a TC
		"LUAHUD2",  // user - for a add-on

		NULL  // end of list
	};

	for (int i = 0; script_lumps[i]; i++)
	{
		int lump = W_CheckNumForName(script_lumps[i]);

		if (lump < 0)
			continue;
	
		I_Printf("Loading LUA lump: %s...\n", script_lumps[i]);

		char *buffer = (char *)W_LoadLumpNum(lump);
		int length   = W_LumpLength(lump);

		int status = luaL_loadbuffer(HUD_ST, buffer, length, script_lumps[i]);

		if (status == 0)
			status = lua_pcall(HUD_ST, 0, 0, 0);

		if (status != 0)
		{
			const char *msg = lua_tolstring(HUD_ST, -1, NULL);

			I_Error("LUA scripts: failure loading '%s' lump (%d)\n%s",
			        script_lumps[i], status, msg);
		}

		has_loaded = true;

		Z_Free(buffer);
	}

	if (! has_loaded)
		I_Error("Missing required LUAHUDx lumps!\n");
}


void LU_BeginLevel(void)
{
	hud_last_time = -1;
}


void LU_RunHud(void)
{
	FrameSetup();


	lua_getglobal(HUD_ST, "stack_trace");

	if (lua_type(HUD_ST, -1) == LUA_TNIL)
		I_Error("LUA scripts: missing function '%s'\n", "stack_trace");

	lua_getglobal(HUD_ST, "hud");
	lua_getfield(HUD_ST, -1, "draw_all");

	if (lua_type(HUD_ST, -1) == LUA_TNIL)
		I_Error("LUA scripts: missing function 'hud.%s'\n", "draw_all");

	int nargs = 0;

	int status = lua_pcall(HUD_ST, nargs, 0, -3-nargs);
	if (status != 0)
	{
		const char *msg = lua_tolstring(HUD_ST, -1, NULL);

		I_Error("LUA script error:\n%s", msg);
	}

	// remove the traceback function and 'hud' table
	lua_pop(HUD_ST, 2);
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab