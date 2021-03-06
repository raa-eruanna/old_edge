//------------------------------------------------------------------------
//  Networking bits
//------------------------------------------------------------------------
//
//  Edge MultiPlayer Server (C) 2005  Andrew Apted
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

#include "includes.h"

#include "buffer.h"
#include "client.h"
#include "game.h"
#include "lib_argv.h"
#include "network.h"
#include "packet.h"
#include "protocol.h"
#include "sys_net.h"
#include "ui_log.h"


NLsocket bcast_socket;
NLsocket conn_socket;

NLaddress local_addr;
NLint local_port;

NLint main_group;

NLmutex  global_lock;  // lock the client/game structures

int cur_net_time;

volatile bool net_quit = false;
volatile bool net_failure = false;

static char net_fail_message[512];

static NLtime base_time;

static int last_update_time = -1;


const char *GetAddrName(const NLaddress *addr)
{
	static char name_buf[NL_MAX_STRING_LENGTH];

	return nlAddrToString(addr, name_buf);
}

const char *GetNetFailureMsg(void)
{
	return net_fail_message;
}

const char *GetNLErrorStr(void)
{
	int err = nlGetError();

	if (err == NL_NO_ERROR)
		return "(Unknown Problem)";

	static char err_buf[512];

	if (err == NL_SYSTEM_ERROR)
	{
		err = nlGetSystemError();
		sprintf(err_buf, "%s", nlGetSystemErrorStr(err));
	}
	else
	{
		sprintf(err_buf, "HawkNL: %s", nlGetErrorStr(err));
	}

	// remove newlines and other rubbish
	for (int i = 0; err_buf[i] && (unsigned)i < sizeof(err_buf); i++)
	{
		if (! isprint(err_buf[i]))
			err_buf[i] = ' ';
	}

	return err_buf;
}

static bool MakeBroadcastAddress(NLaddress *dest, const NLaddress *src)
{
	// changes the last value, e.g. 10.0.0.4 --> 10.0.0.255

	static char name_buf[NL_MAX_STRING_LENGTH];

	char *begin = nlAddrToString(src, name_buf);

	char *pos = strrchr(begin, '.');

	if (! pos)
		return false;
	
	strcpy(pos + 1, "255");

	return (nlStringToAddr(begin, dest) == NL_TRUE);
}

static void InitNetTime(void)
{
	nlTime(&base_time);

	// initialise random-number generator too

	unsigned int net_random = base_time.seconds ^ (base_time.useconds * 229);
	DebugPrintf("Global Random Seed = 0x%x\n", net_random);

	srand(net_random);
}

//
// GetNetTime
//
// Returns a time value which increases by 1 for every 10 milliseconds
// (i.e. 100HZ timebase).  Values start at zero.  For 32-bit integers,
// this will overflow in about 248 days.
//
int GetNetTime(void)
{
	NLtime current;

	nlTime(&current);

	NLlong sec_diff  = current.seconds  - base_time.seconds;
	NLlong usec_diff = current.useconds - base_time.useconds;

	return int(sec_diff * 100 + usec_diff / 10000);
}

static bool DetermineLocalAddr(void)
{
	// explicitly given via command-line option?
	int p, num_p;

	p = ArgvFind('l', "local", &num_p);

	if (p >= 0)
	{
		if (num_p < 1)
		{
			fl_alert("-local option: missing address");
			exit(5);
	    }

		if (! nlStringToAddr(arg_list[p+1], &local_addr))
		{
			fl_alert("Bad local address '%s'\n(%s)", arg_list[p+1],
				GetNLErrorStr());
			exit(5);
		}

		return true;
	}

	int count;
	NLaddress *all_addrs = nlGetAllLocalAddr(&count);

	if (all_addrs && count > 0)
	{
		for (int i = 0; i < count; i++)
		{
			const char *addr_name = GetAddrName(all_addrs +i);

			DebugPrintf("ALL-Local[%d] = %s\n", i, addr_name);

			if (strncmp(addr_name, "127.", 4) == 0 ||
				strncmp(addr_name, "0.0.", 4) == 0 ||
				strncmp(addr_name, "255.255.", 8) == 0)
			{
				continue;
			}

			// found a valid one
			memcpy(&local_addr, all_addrs + i, sizeof(local_addr));
			return true;
		}
	}

#ifdef LINUX
	const char *local_ip = I_LocalIPAddrString("eth0");
	if (! local_ip)
		local_ip = I_LocalIPAddrString("eth1");
	if (! local_ip)
		local_ip = I_LocalIPAddrString("ppp0");

	if (local_ip)
	{
		DebugPrintf("LINUX-Local-IP = %s\n", local_ip);

		nlStringToAddr(local_ip, &local_addr);
		return true;
	}
#endif

	return false;
}

//
// NetInit
//
// Calls fl_alert if something goes wrong.
//
void NetInit(void)
{
	nlMutexInit(&global_lock);

	I_IgnoreBrokenPipes();

	nlHint(NL_REUSE_ADDRESS, NL_TRUE);
	nlHint(NL_TCP_NO_DELAY,  NL_TRUE);

	InitNetTime();

    DebugPrintf("NL_VERSION: %s\n", nlGetString(NL_VERSION));
  	DebugPrintf("NL_NETWORK_TYPES: %s\n", nlGetString(NL_NETWORK_TYPES));
 
    if (! nlSelectNetwork(NL_IP))
	{
		fl_alert("Hawk network library: unable to select IP networking:\n(%s)",
				 GetNLErrorStr());
        exit(5); //!!!!
	}

	DebugPrintf("NL_SOCKET_TYPES: %s\n\n", nlGetString(NL_CONNECTION_TYPES));

	DetermineLocalAddr();

	// override default port
	local_port = MPS_DEF_PORT;

	int num_p;
	int p = ArgvFind('p', "port", &num_p);

	if (p >= 0)
	{
		if (num_p < 1)
		{
			fl_alert("-port option: missing number");
			exit(5);
	    }

		local_port = atoi(arg_list[p+1]);

		if (local_port <= 0 || local_port > 65535)
		{
			fl_alert("-port option: invalid value '%s'", arg_list[p+1]);
			exit(5);
		}
	}

	// Must create BROADCAST socket using special broadcast address
	NLaddress bcast_addr;
	MakeBroadcastAddress(&bcast_addr, &local_addr);
	nlSetLocalAddr(&bcast_addr);

	bcast_socket = nlOpen(local_port, NL_BROADCAST);

	if (bcast_socket == NL_INVALID)
	{
		LogPrintf(2, "Unable to create UDP broadcast socket on port %d:\n(%s)\n\n",
			local_port, GetNLErrorStr());
		/* leave socket as NL_INVALID */
	}

	// Set real local address (for listen socket)
	nlSetLocalAddr(&local_addr);

	conn_socket = nlOpen(local_port, NL_RELIABLE_PACKETS);

	if (conn_socket == NL_INVALID)
	{
		fl_alert("Unable to create TCP socket on port %d:\n(%s)", local_port,
				 GetNLErrorStr());
		exit(5); //!!!!
	}

	DebugPrintf("Broadcast socket  = 0x%x\n", bcast_socket);
	DebugPrintf("Connection socket = 0x%x\n", conn_socket);

	if (nlListen(conn_socket) != NL_TRUE)
	{
		fl_alert("Unable to listen on TCP socket:\n(%s)", GetNLErrorStr());
		exit(5); //!!!!
	}

	main_group = nlGroupCreate();

	if (main_group == NL_INVALID)
	{
		fl_alert("Out of memory (cannot create socket group)");
		exit(5);
	}

	nlGroupAddSocket(main_group, conn_socket);

	if (bcast_socket != NL_INVALID)
		nlGroupAddSocket(main_group, bcast_socket);

	NLaddress addr;

    nlGetLocalAddr(conn_socket, &addr);
    LogPrintf(0, "Server address: %s\n\n", GetAddrName(&addr));
}

//------------------------------------------------------------------------

static void HandleTimeouts()
{
	// LOCK STRUCTURES
	nlMutexLock(&global_lock);

	cur_net_time = GetNetTime();

	if (cur_net_time > last_update_time)
	{
		last_update_time = cur_net_time;

		BufferRetryWrites();
		ClientTimeouts();
		GameTimeouts();

		// FIXME: copy data structures for GUI (every 1 second)
	}

	// UNLOCK STRUCTURES
	nlMutexUnlock(&global_lock);
}

static void HandleBroadcast(void)
{
	SYS_ASSERT(bcast_socket != NL_INVALID);

	packet_c pk; 
	pk.Clear();

	while (pk.Read(bcast_socket))
	{
		NLaddress remote_addr;
		nlGetRemoteAddr(bcast_socket, &remote_addr);

		DebugPrintf("BROADCAST PACKET: type [%c%c] data_len %d addr %s\n",
				pk.hd().type[0], pk.hd().type[1], pk.hd().data_len,
				GetAddrName(&remote_addr));

		if (pk.CheckType("bd"))   // broadcast discovery
		{
			PK_broadcast_discovery_UDP(&pk, &remote_addr);
		}
		else
		{
			LogPrintf(1, "Unknown broadcast packet: [%c%c] addr %s\n",
				pk.hd().type[0], pk.hd().type[1],
				GetAddrName(&remote_addr));
		}
	}
}

static void HandleConnection(void)
{
	NLsocket NEW_SOCK = nlAcceptConnection(conn_socket);

	if (NEW_SOCK == NL_INVALID)
	{
		LogPrintf(0, "Unable to accept TCP connection: %s\n",
				GetNLErrorStr());
		return;
	}

	DebugPrintf("Accepted new connection, socket = 0x%x\n", NEW_SOCK);

	CreateNewClient(NEW_SOCK);
}

static void NormalPacket(NLsocket CUR_SOCK)
{
	packet_c pk; 
	pk.Clear();

	while (pk.Read(CUR_SOCK))
	{
		NLaddress remote_addr;
		nlGetRemoteAddr(CUR_SOCK, &remote_addr);

		DebugPrintf("NORMAL PACKET: type [%c%c] data_len %d addr %s\n",
				pk.hd().type[0], pk.hd().type[1], pk.hd().data_len,
				GetAddrName(&remote_addr));

///UDP	nlSetRemoteAddr(socks[0], &remote_addr);

		if (pk.CheckType("cs"))   // connect to server
		{
			PK_connect_to_server(&pk, CUR_SOCK);
			continue;
		}

		// must validate the client field (the only exceptions are the
		// connect-to-server and the broadcast-discovery packets).

		// FIXME: CUR_SOCK !!!
		if (! VerifyClient(pk.hd().client, CUR_SOCK, &remote_addr))
		{
			LogPrintf(2, "Client %d verify failed: packet [%c%c]\n",
					pk.hd().client, pk.hd().type[0], pk.hd().type[1]);
			continue;
		}

		// any traffic from a client proves they still exist
		cur_net_time = GetNetTime();

		clients[pk.hd().client]->BumpDieTime();

		if (pk.CheckType("ls"))   // leave server
		{
			PK_leave_server(&pk);
		}
		else if (pk.CheckType("ka"))  // keep-alive
		{
			PK_keep_alive(&pk);
		}
		else if (pk.CheckType("qc"))  // query client
		{
			PK_query_client(&pk);
		}
		else if (pk.CheckType("qg"))  // query game
		{
			PK_query_game(&pk);
		}
		else if (pk.CheckType("ng"))  // new game
		{
			PK_new_game(&pk);
		}
		else if (pk.CheckType("jq"))  // join queue
		{
			PK_join_queue(&pk);
		}
		else if (pk.CheckType("lg"))  // leave game
		{
			PK_leave_game(&pk);
		}
		else if (pk.CheckType("vp"))  // play game, dammit!
		{
			PK_vote_to_play(&pk);
		}
		else if (pk.CheckType("tc"))  // send ticcmd
		{
			PK_ticcmd(&pk);
		}
		else if (pk.CheckType("tr"))  // tic retransmit
		{
			PK_tic_retransmit(&pk);
		}
		else if (pk.CheckType("ms"))  // send message
		{
			PK_message(&pk);
		}
		else
		{
			LogPrintf(1, "Unknown packet: [%c%c] addr %s\n",
					pk.hd().type[0], pk.hd().type[1],
					GetAddrName(&remote_addr));
		}
	}
}

//
// NetRun
//
// Main networking thread is here.
// Listens to net_quit variable if main program wants to quit.
// Sets net_failure variable if an exception is caught.
//
void *NetRun(void *data)
{
	net_fail_message[0] = 0;

	try
	{
		const int SOCK_LIM = 256;
		NLsocket socks[SOCK_LIM];

		while (! net_quit)
		{
			HandleTimeouts();

			int num = nlPollGroup(main_group, NL_READ_STATUS, socks, SOCK_LIM, 10 /* millis */);

			if (num < 1)
				continue;

			packet_c pk; 
			pk.Clear();

			for (int idx = 0; idx < num; idx++)
			{
				NLsocket CUR_SOCK = socks[idx];

				if (bcast_socket != NL_INVALID && CUR_SOCK == bcast_socket)
					HandleBroadcast();
				else if (CUR_SOCK == conn_socket)
					HandleConnection();
				else
					NormalPacket(CUR_SOCK);
			}
		}
	}
	catch (assert_fail_c err)
	{
		net_failure = true;
		strcpy(net_fail_message, err.GetMessage());
		return net_fail_message;
	}
	catch (...)
	{
		net_failure = true;
		strcpy(net_fail_message, "Unknown problem in networking code.");
		return net_fail_message;
	}

	return NULL;
}

