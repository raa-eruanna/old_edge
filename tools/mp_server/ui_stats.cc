//------------------------------------------------------------------------
//  Statistics
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
#include "packet.h"
#include "ui_stats.h"
#include "ui_window.h"

//
// Stats Constructor
//
UI_Stats::UI_Stats(int x, int y, int w, int h, const char *label) :
    Fl_Group(x, y, w, h, label)
{
	// cancel the automatic `begin' in Fl_Group constructor
	end();
 
 	box(FL_THIN_UP_BOX);
	resizable(0);  // don't resize our children

	labeltype(FL_NORMAL_LABEL);
	labelfont(FL_HELVETICA_BOLD);
	align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT | FL_ALIGN_TOP);

	y += 38;

	// ---- numbers of clients and games ----

	clients = new Fl_Output(x+120, y+4, 50, 22, "Clients: ");
	clients->align(FL_ALIGN_LEFT);
	clients->value("0");
	add(clients);

	queued = new Fl_Output(x+120, y+32, 50, 22, "Games Queued: ");
	queued->align(FL_ALIGN_LEFT);
	queued->value("0");
	add(queued);

	played = new Fl_Output(x+120, y+60, 50, 22, "Games Played: ");
	played->align(FL_ALIGN_LEFT);
	played->value("0");
	add(played);

	// ---- packets read, written and buffered ----

	in_pks = new Fl_Output(x+300, y+4, 70, 22, "Input packets: ");
	in_pks->align(FL_ALIGN_LEFT);
	in_pks->value("0");
	add(in_pks);

	out_pks = new Fl_Output(x+300, y+32, 70, 22, "Output packets: ");
	out_pks->align(FL_ALIGN_LEFT);
	out_pks->value("0");
	add(out_pks);

	buf_pks = new Fl_Output(x+300, y+60, 70, 22, "Buffered packets: ");
	buf_pks->align(FL_ALIGN_LEFT);
	buf_pks->value("0");
	add(buf_pks);

	// ---- number of bytes read, written and buffered ----

	in_bytes = new Fl_Output(x+430, y+4, 100, 22, "Kbytes: ");
	in_bytes->align(FL_ALIGN_LEFT);
	in_bytes->value("0");
	add(in_bytes);

	out_bytes = new Fl_Output(x+430, y+32, 100, 22, "Kbytes: ");
	out_bytes->align(FL_ALIGN_LEFT);
	out_bytes->value("0");
	add(out_bytes);

	buf_bytes = new Fl_Output(x+430, y+60, 100, 22, "Kbytes: ");
	buf_bytes->align(FL_ALIGN_LEFT);
	buf_bytes->value("0");
	add(buf_bytes);
}


//
// Stats Destructor
//
UI_Stats::~UI_Stats()
{
}


//
// Stats Updater
//
void UI_Stats::Update()
{
	char num_str[40];

	// clients and games

	sprintf(num_str, "%d", total_clients);
	clients->value(num_str);
	
	sprintf(num_str, "%d", total_queued_games);
	queued->value(num_str);

	sprintf(num_str, "%d", total_played_games);
	played->value(num_str);

	// packet statistics

	sprintf(num_str, "%d", total_in_packets);
	in_pks->value(num_str);

	sprintf(num_str, "%1.1f", total_in_bytes / 1024.0f);
	in_bytes->value(num_str);

	sprintf(num_str, "%d", total_out_packets);
	out_pks->value(num_str);

	sprintf(num_str, "%1.1f", total_out_bytes / 1024.0f);
	out_bytes->value(num_str);

	sprintf(num_str, "%d", buffered_packets);
	buf_pks->value(num_str);

	sprintf(num_str, "%1.1f", buffered_bytes / 1024.0f);
	buf_bytes->value(num_str);
}
