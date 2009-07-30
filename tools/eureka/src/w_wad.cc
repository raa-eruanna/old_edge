//------------------------------------------------------------------------
//  WAD Reading / Writing
//------------------------------------------------------------------------
//
//  Eureka DOOM Editor
//
//  Copyright (C) 2001-2009 Andrew Apted
//  Copyright (C) 1997-2003 André Majorel et al
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
//
//  Based on Yadex which incorporated code from DEU 5.21 that was put
//  in the public domain in 1994 by Raphaël Quinet and Brendon Wyber.
//
//------------------------------------------------------------------------

#include "main.h"

#include "lib_crc.h"
#include "w_rawdef.h"
#include "w_wad.h"


std::vector< Wad_file* > master_dir;

Wad_file * editing_wad;


Lump_c::Lump_c(Wad_file *_par, const char *_nam, int _start, int _len) :
	parent(_par), l_start(_start), l_length(_len)
{
	name = strdup(_nam);
}

Lump_c::Lump_c(Wad_file *_par, const struct raw_wad_entry_s *entry) :
	parent(_par)
{
	// handle the entry name, which can lack a terminating NUL
	char *buffer = (char *)malloc(10);
	SYS_ASSERT(buffer);

	memcpy(buffer, entry->name, 8);
	buffer[8] = 0;

	name = buffer;

	l_start  = LE_U32(entry->pos);
	l_length = LE_U32(entry->size);

//	DebugPrintf("new lump '%s' @ %d len:%d\n", name, l_start, l_length);
}

Lump_c::~Lump_c()
{
	free((void*)name);
}


bool Lump_c::Seek(int offset)
{
	SYS_ASSERT(offset >= 0);

	return (fseek(parent->fp, l_start + offset, SEEK_SET) == 0);
}

bool Lump_c::Read(void *data, int len)
{
	SYS_ASSERT(data && len > 0);

	return (fread(data, len, 1, parent->fp) == 1);
}


//------------------------------------------------------------------------


Wad_file::Wad_file(FILE * file) : fp(file), directory(),
	dir_start(0), dir_count(0), dir_crc(0),
	levels(), patches(), sprites(), flats(), tex_info(NULL),
	holes()
{
}

Wad_file::~Wad_file()
{
	// TODO free stuff
}


Wad_file * Wad_file::Open(const char *filename)
{
	FILE *fp = fopen(filename, "rw");
	if (! fp)
		return NULL;

	// FIXME : determine total size (seek to end)

	Wad_file *w = new Wad_file(fp);

	w->ReadDirectory();

	return w;
}

Wad_file * Wad_file::Create(const char *filename)
{
	FILE *fp = fopen(filename, "rw");
	if (! fp)
		return NULL;

	Wad_file *w = new Wad_file(fp);

	// write out base header
	raw_wad_header_t header;

	memset(&header, 0, sizeof(header));
	memcpy(header.ident, "PWAD", 4);

	fwrite(&header, sizeof(header), 1, fp);
	fflush(fp);

	return w;
}


Lump_c * Wad_file::GetLump(short index)
{
	SYS_ASSERT(0 <= index && index < NumLumps());
	SYS_ASSERT(directory[index]);

	return directory[index];
}


Lump_c * Wad_file::FindLump(const char *name)
{
	for (int k = 0; k < NumLumps(); k++)
		if (y_stricmp(directory[k]->name, name) == 0)
			return directory[k];

	return NULL;
}


Lump_c * Wad_file::FindLumpInLevel(const char *name, short level)
{
	SYS_ASSERT(0 <= level && level < (short)levels.size());

	// determine how far past the level marker (MAP01 etc) to search
	short last = levels[level] + 14;

	if (last >= NumLumps())
		last = NumLumps() - 1;
	
	// assumes levels[] are in increasing lump order!
	if (level+1 < (short)levels.size())
		if (last >= levels[level+1])
			last = levels[level+1] - 1;

	for (short k = levels[level]+1; k <= last; k++)
	{
		SYS_ASSERT(0 <= k && k < NumLumps());

		if (y_stricmp(directory[k]->name, name) == 0)
			return directory[k];
	}

	return NULL;
}


short Wad_file::FindLevel(const char *name)
{
	for (int k = 0; k < (int)levels.size(); k++)
	{
		short index = levels[k];

		SYS_ASSERT(0 <= index && index < NumLumps());
		SYS_ASSERT(directory[index]);

		if (y_stricmp(directory[index]->name, name) == 0)
			return k;
	}

	return -1;  // not found
}


void Wad_file::ReadDirectory()
{
	// TODO: no fatal errors

	// TODO: rewind(fp);

	// FIXME: read header in ::Open()
	raw_wad_header_t header;

	if (fread(&header, sizeof(header), 1, fp) != 1)
		FatalError("Error reading WAD header.\n");

	// TODO: check ident for PWAD or IWAD

	dir_start = LE_U32(header.dir_start);
	dir_count = LE_U32(header.num_entries);

	if (dir_count > 32000)
		FatalError("Bad WAD header, too many entries (%d)\n", dir_count);

	crc32_c checksum;

	if (fseek(fp, dir_start, SEEK_SET) != 0)
		FatalError("Error seeking to WAD directory.\n");
	
	for (int i = 0; i < dir_count; i++)
	{
		raw_wad_entry_t entry;

		if (fread(&entry, sizeof(entry), 1, fp) != 1)
			FatalError("Error reading WAD directory.\n");

		// update the checksum with each _RAW_ entry
		checksum.AddBlock((u8_t *) &entry, sizeof(entry));

		Lump_c *lump = new Lump_c(this, &entry);

		// TODO: check if entry is valid

		directory.push_back(lump);

		// FIXME: handle the namespaces (S_START..S_END etc)
	}

	dir_crc = checksum.raw;

	LogPrintf("Loaded directory. crc = %08x\n", dir_crc);
}


//------------------------------------------------------------------------


short WAD_FindEditLevel(const char *name)
{
	for (int i = (int)master_dir.size()-1; i >= 0; i--)
	{
		editing_wad = master_dir[i];

		short level = editing_wad->FindLevel(name);
		if (level >= 0)
			return level;
	}

	// not found
	editing_wad = NULL;
	return -1;
}


Lump_c * WAD_FindLump(const char *name)
{
	for (int i = (int)master_dir.size()-1; i >= 0; i--)
	{
		Lump_c *L = master_dir[i]->FindLump(name);
		if (L)
			return L;
	}

	return NULL;  // not found
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
