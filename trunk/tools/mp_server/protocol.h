//------------------------------------------------------------------------
//  Protocol structures
//------------------------------------------------------------------------
//
//  Edge MultiPlayer Server (C) 2004  Andrew Apted
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

#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#define MP_PROTOCOL_VER  0x070  /* 0.7.0 */

//
// common packet header
//
// types are an alphanumeric pair, generally Verb+Noun.  They should
// start with lowercase for packets from the client (e.g. "cs"), and
// uppercase for ones from the server (e.g. "Er").  Data length does
// not include the header, e.g. data_len = sizeof(connect_proto_t).
//
typedef struct header_proto_s
{
	char type[2];

	s16_t flags;
	s16_t data_len;
	s16_t client;
	s16_t request;   // use to match requests with replies

	s16_t reserved;  // (future expansion)

	void ByteSwap();
}
header_proto_t;


//
// ERROR ("Er")
//
// Error types are a lowercase alphanumeric pair.
// Current error codes:
//
//   ac : Already Connected
//   bc : Bad Client info
//   bg : Bad Game info
//   np : Not Playiing
//   nq : Not in Queue
//   ps : Packet too Short (data_len)
//   pl : Packet too Long  (data_len)
//   xc : non-eXisting Client
//   xg : non-eXisting Game
//
typedef struct error_proto_s
{
	char type[2];

	u32_t reserved;

	static const int ERR_STR_MAX = 200;

	char message[1];  // as big as needed (upto ERR_STR_MAX)

	void ByteSwap();
}
error_proto_t;


/* BROADCAST-DISCOVERY ("bd") has no data */


//
// Client information
//
typedef struct client_info_s
{
	static const int NAME_LEN = 16;

	char name[NAME_LEN];

	u32_t reserved[2];

	/* --- query output --- */

	s16_t game;

	byte state;

	enum  // state values:
	{
		CS_NotExist = 'N',
		CS_Browsing = 'B',
		CS_Queueing = 'Q',
		CS_Voted    = 'V',  // implies Queueing
		CS_Playing  = 'P'
	};

	void ByteSwap();
}
client_info_t;

//
// CONNECT ("cs")
//
typedef struct connect_proto_s
{
	s16_t protocol_ver;  // three-digit hex: 0x025 means 0.2.5

	client_info_t info;

	void ByteSwap();
}
connect_proto_t;


/* LEAVE-SERVER ("ls") has no data */

/* KEEP-ALIVE ("ka") has no data */


//
// QUERY-CLIENT ("qc")
//
typedef struct query_client_proto_s 
{
	s16_t first_client;
	byte  count;

	s16_t total_clients;  // output value
	s16_t reserved;

	static const int CLIENT_FIT = 16;

	client_info_t info[1];  // upto CLIENT_FIT structs

	void ByteSwap(bool do_info);
}
query_client_proto_t;


//
// Game information
//
typedef struct game_info_s
{
	static const int ENGINE_STR_MAX = 12;
	static const int GAME_STR_MAX   = 12;
	static const int LEVEL_STR_MAX  = 8;

	char engine_name[ENGINE_STR_MAX]; // e.g. EDGE129 (Note: includes version)
	char game_name[GAME_STR_MAX];     // e.g. DOOM2
	char level_name[LEVEL_STR_MAX];   // e.g. MAP01

	char mode;
	byte skill;  // 1 to 5

	enum  // mode values:
	{
		MD_Coop = 'C',
		MD_DeathMatch = 'D',
		MD_AltDeath = 'A'
	};

	s16_t min_players;
	s16_t num_bots;

	u32_t features;
	u32_t reserved;  // (future expansion)

	enum  // feature bitflags:
	{
		FT_Jumping    = (1 << 0),
		FT_Crouching  = (1 << 1),
		FT_Zooming    = (1 << 2),
		FT_VertLook   = (1 << 3),
		FT_AutoAim    = (1 << 4),

		// compatibility mode:
		FT_BoomCompat = (1 << 30)
	};

	u16_t wad_checksum;  // checksum over all loaded wads
	u16_t def_checksum;  // checksum over all definitions

	/* --- query output --- */

	s16_t num_players;
	s16_t num_votes;

	byte state;

	enum  // state values:
	{
		GS_NotExist = 'N',
		GS_Queued   = 'Q',
		GS_Playing  = 'P'
	};

	void ByteSwap();
}
game_info_t;

//
// NEW-GAME ("ng")
//
typedef struct new_game_proto_s
{
	game_info_t info;

	s16_t game;  // output game ID

	void ByteSwap();
}
new_game_proto_t;


//
// QUERY-GAME ("qg")
//
typedef struct query_game_proto_s
{
	s16_t first_game;
	byte  count;

	s16_t total_games;  // out value
	s16_t reserved;

	static const int GAME_FIT = 8;

	game_info_t info[1];  // upto GAME_FIT structures

	void ByteSwap(bool do_info);
}
query_game_proto_t;


//
// JOIN-QUEUE ("jq")
//
typedef struct join_queue_proto_s
{
	s16_t game;
	s16_t reserved;

	void ByteSwap();
}
join_queue_proto_t;


/* LEAVE-GAME ("lg") has no data */

/* VOTE-to-PLAY ("vp") has no data */


//
// PLAY-GAME ("Pg")
//
typedef struct play_game_proto_s
{
	static const int PLAYER_MAX = 30;

	byte real_players;
	byte bots_each;  // how many bots handled by each client

	u32_t random_seed;
	u32_t reserved;

	byte first_player;
	byte count;

	// client IDs for each player (upto PLAYER_MAX).  No bots here.
	s16_t client_list[1];

	void ByteSwap();
}
play_game_proto_t;


//
// raw TICCMD (contents are engine-specific)
//
// engines may use less that what's here.
//
typedef struct raw_ticcmd_s
{
	u16_t shorts[4];
	u8_t  bytes [8];

	// only clients need to byte-swap, the server doesn't care
	// about the contents of ticcmds.
	void ByteSwap();
}
raw_ticcmd_t;


//
// TICCMD ("tc")
//
// Holds the ticcmds send from client to server.  'count' is the
// number of tics (starting at tic_counter).  Bot ticcmds must
// follow the real player's ticcmds, for example when count is
// three and have two bots:
//
//    (tic +0)  Player,Bot0,Bot1,
//    (tic +1)  Player,Bot0,Bot1,
//    (tic +2)  Player,Bot0,Bot1.
//
typedef struct ticcmd_proto_s
{
	u32_t tic_counter;
	byte  count;

	s16_t reserved;

	static const int TICCMD_FIT = 24;

	raw_ticcmd_t tic_cmds[1];  // upto TICCMD_FIT commands

	void ByteSwap();
}
ticcmd_proto_t;


//
// TIC-GROUP ("Tg")
//
// Holds the ticcmds for _ONE_ single tic.  Bot ticcmds must follow
// each player, for example with two players and three bots each:
//
//    Player0, Bot0, Bot1, Bot2,
//    Player1, Bot3, Bot4, Bot5.
//
typedef struct tic_group_proto_s
{
	u32_t tic_counter;

	byte first_player;
	byte count;

	s16_t reserved;

	static const int TICCMD_FIT = 24;

	raw_ticcmd_t tic_cmds[1];  // as big as needed (upto TICCMD_FIT)

	void ByteSwap();
}
tic_group_proto_t;


//
// MESSAGE ("ms")
//
typedef struct message_proto_s
{
	s16_t dest;

	enum  // special destinations
	{
		D_ALL_BROWSING = -1,
		D_ALL_NOT_PLAYING = -2,
		D_OTHER_QUEUERS = -3,
		D_OTHER_PLAYERS = -4,
		D_ABSOLUTELY_EVERYONE = -5,  // try to avoid this!
	};

	void ByteSwap();

	char msg_data[1];  /* data is engine-specific (we don't care!) */
}
message_proto_t;

#endif /* __PROTOCOL_H__ */
