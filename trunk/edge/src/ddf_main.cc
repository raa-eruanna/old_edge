//----------------------------------------------------------------------------
//  EDGE Data Definition Files Code (Main)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2005  The EDGE Team.
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

#include "ddf_main.h"

#include "e_search.h"
#include "ddf_locl.h"
#include "ddf_colm.h"
#include "dm_state.h"
#include "dstrings.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_inline.h"
#include "m_math.h"
#include "m_misc.h"
#include "p_action.h"
#include "p_mobj.h"
#include "r_things.h"
#include "z_zone.h"
#include "version.h"

#include <epi/filesystem.h>

#include <ctype.h>
#include <limits.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG_DDFREAD  0

int ddf_version;  // global
bool boom_conflict;

static readchar_t DDF_MainProcessChar(char character, epi::string_c& buffer, int status);

//
// DDF_Error
//
// -AJA- 1999/10/27: written.
//
int cur_ddf_line_num;
epi::string_c cur_ddf_filename;
epi::string_c cur_ddf_entryname;
epi::string_c cur_ddf_linedata;

void DDF_Error(const char *err, ...)
{
	va_list argptr;
	char buffer[2048];
	char *pos;

	buffer[2047] = 0;

	// put actual error message on first line
	va_start(argptr, err);
	vsprintf(buffer, err, argptr);
	va_end(argptr);
 
	pos = buffer + strlen(buffer);

	if (!cur_ddf_filename.IsEmpty())
	{
		sprintf(pos, "Error occurred near line %d of %s\n", 
				cur_ddf_line_num, cur_ddf_filename.GetString());
				
		pos += strlen(pos);
	}

	if (!cur_ddf_entryname.IsEmpty())
	{
		sprintf(pos, "Error occurred in entry: %s\n", 
				cur_ddf_entryname.GetString());
				
		pos += strlen(pos);
	}

	if (!cur_ddf_linedata.IsEmpty())
	{
		sprintf(pos, "Line contents: %s\n", 
				cur_ddf_linedata.GetString());
				
		pos += strlen(pos);
	}

	// check for buffer overflow
	DEV_ASSERT(buffer[2047] == 0, ("Buffer overflow in DDF_Error"));
  
	// add a blank line for readability under DOS/Linux.
	I_Printf("\n");
 
	I_Error("%s", buffer);
}

void DDF_Warning(const char *err, ...)
{
	va_list argptr;
	char buffer[1024];

	if (no_warnings)
		return;

	va_start(argptr, err);
	vsprintf(buffer, err, argptr);
	va_end(argptr);

	I_Warning("\n");

	if (!cur_ddf_filename.IsEmpty())
	{
		I_Warning("Found problem near line %d of %s\n", 
				  cur_ddf_line_num, cur_ddf_filename.GetString());
	}

	if (!cur_ddf_entryname.IsEmpty())
	{
		I_Warning("occurred in entry: %s\n", 
				  cur_ddf_entryname.GetString());
	}
	
	if (!cur_ddf_linedata.IsEmpty())
	{
		I_Warning("with line contents: %s\n", 
				  cur_ddf_linedata.GetString());
	}
	
	I_Warning("%s", buffer);
}

void DDF_WarnError(const char *err, ...)
{
	va_list argptr;
	char buffer[1024];

	va_start(argptr, err);
	vsprintf(buffer, err, argptr);
	va_end(argptr);

	if (strict_errors)
		DDF_Error("%s", buffer);
	else
		DDF_Warning("%s", buffer);
}

void DDF_WarnError2(int ver, const char *err, ...)
{
	va_list argptr;
	char buffer[1024];

	va_start(argptr, err);
	vsprintf(buffer, err, argptr);
	va_end(argptr);

	if (strict_errors || (ddf_version >= ver && ! lax_errors))
		DDF_Error("%s", buffer);
	else
		DDF_Warning("%s", buffer);
}

void DDF_Obsolete(const char *err, ...)
{
	va_list argptr;
	char buffer[1024];

	va_start(argptr, err);
	vsprintf(buffer, err, argptr);
	va_end(argptr);

	if (strict_errors || (ddf_version >= 0x128 && ! lax_errors))
		DDF_Error("%s", buffer);
	else if (no_obsoletes)
		DDF_Warning("%s", buffer);
}

void DDF_Init(void)
{
	DDF_StateInit();
	DDF_LanguageInit();
	DDF_SFXInit();
	DDF_ColmapInit();
	DDF_ImageInit();
	DDF_FontInit();
	DDF_StyleInit();
	DDF_AttackInit(); 
	DDF_WeaponInit();
	DDF_MobjInit();
	DDF_LinedefInit();
	DDF_SectorInit();
	DDF_SwitchInit();
	DDF_AnimInit();
	DDF_GameInit();
	DDF_LevelInit();
	DDF_MusicPlaylistInit();
}

// -KM- 1999/01/29 Fixed #define system.
typedef struct
{
	char *name;
	char *value;
}
define_t;

// -AJA- 1999/09/12: Made these static.  The variable `defines' was
//       clashing with the one in rad_trig.c.  Ugh.

static define_t *defines = NULL;
static int numDefines = 0;

static void DDF_MainAddDefine(char *name, char *value)
{
	int i;

	for (i = 0; i < numDefines; i++)
	{
		if (!strcmp(defines[i].name, name))
			DDF_Error("Redefinition of '%s'\n", name);
	}

	Z_Resize(defines, define_t, numDefines + 1);

	defines[numDefines].name = name;
	defines[numDefines++].value = value;
}

static const char *DDF_MainGetDefine(const char *name)
{
	int i;

	for (i = 0; i < numDefines; i++)
	{
		if (!strcmp(defines[i].name, name))
			return defines[i].value;
	}

	// Not a define.
	return name;
}

//
// DDF_MainCleanup
//
// This goes through the information loaded via DDF and matchs any
// info stored as references.
//
void DDF_CleanUp(void)
{
	DDF_LanguageCleanUp();
	DDF_ImageCleanUp();
	DDF_FontCleanUp();
	DDF_StyleCleanUp();
	DDF_MobjCleanUp();
	DDF_AttackCleanUp();
	DDF_StateCleanUp();
	DDF_LinedefCleanUp();
	DDF_SFXCleanUp();
	DDF_ColmapCleanUp();
	DDF_WeaponCleanUp();
	DDF_SectorCleanUp();
	DDF_SwitchCleanUp();
	DDF_AnimCleanUp();
	DDF_GameCleanUp();
	DDF_LevelCleanUp();
	DDF_MusicPlaylistCleanUp();
	
	// -AJA- FIXME: this belongs elsewhere...
	currmap = mapdefs[0];
}

static const char *tag_conversion_table[] =
{
    "ANIMATIONS",  "DDFANIM",
    "ATTACKS",     "DDFATK",
    "COLOURMAPS",  "DDFCOLM",
    "FONTS",       "DDFFONT",
    "GAMES",       "DDFGAME",
    "IMAGES",      "DDFIMAGE",
    "LANGUAGES",   "DDFLANG",
    "LEVELS",      "DDFLEVL",
    "LINES",       "DDFLINE",
    "PLAYLISTS",   "DDFPLAY",
    "SECTORS",     "DDFSECT",
    "SOUNDS",      "DDFSFX",
    "STYLES",      "DDFSTYLE",
    "SWITCHES",    "DDFSWTH",
    "THINGS",      "DDFTHING",
    "WEAPONS",     "DDFWEAP",

	NULL, NULL
};

void DDF_GetLumpNameForFile(const char *filename, char *lumpname)
{
	FILE *fp = fopen(filename, "r");
	
	if (!fp)
		I_Error("Couldn't open DDF file: %s\n", filename);

	bool in_comment = false;

	for (;;)
	{
		int ch = fgetc(fp);

		if (ch == EOF || ferror(fp))
			break;

		if (ch == '/' || ch == '#')  // skip directives too
		{
			in_comment = true;
			continue;
		}

		if (in_comment)
		{
			if (ch == '\n' || ch == '\r')
				in_comment = false;
			continue;
		}
		
		if (ch == '[')
			break;

		if (ch != '<')
			continue;

		// found start of <XYZ> tag, read it in

		char tag_buf[40];
		int len = 0;

		for (;;)
		{
			ch = fgetc(fp);

			if (ch == EOF || ferror(fp) || ch == '>')
				break;

			tag_buf[len++] = toupper(ch);

			if (len+2 >= (int)sizeof(tag_buf))
				break;
		}

		tag_buf[len] = 0;

		if (len > 0)
		{
			for (int i = 0; tag_conversion_table[i]; i += 2)
			{
				if (strcmp(tag_buf, tag_conversion_table[i]) == 0)
				{
					strcpy(lumpname, tag_conversion_table[i+1]);
					fclose(fp);

					return;  // SUCCESS!
				}
			}

			fclose(fp);
			I_Error("Unknown marker <%s> in DDF file: %s\n", tag_buf, filename);
		}
		break;
	}

	fclose(fp);
	I_Error("Missing <..> marker in DDF file: %s\n", filename);
}

void DDF_SetBoomConflict(bool enabled)
{
	boom_conflict = enabled;
}

// -KM- 1998/12/16 This loads the ddf file into memory for parsing.
// -AJA- Returns NULL if no such file exists (with a warning).

static void *DDF_MainCacheFile(readinfo_t * readinfo)
{
	FILE *file;
	char *memfile;
	epi::string_c filename;
	size_t size;

	if (!readinfo->filename)
		I_Error("DDF_MainReadFile: No file to read");

    filename = epi::the_filesystem->JoinPath(ddf_dir.GetString(), readinfo->filename);

	file = fopen(filename.GetString(), "rb");
	if (file == NULL)
	{
		I_Warning("DDF_MainReadFile: Unable to open: '%s'\n", filename.GetString());
		return NULL;
	}

#if (DEBUG_DDFREAD)
	L_WriteDebug("\nDDF Parser Output:\n");
#endif

	// get to the end of the file
	fseek(file, 0, SEEK_END);

	// get the size
	size = ftell(file);

	// reset to beginning
	fseek(file, 0, SEEK_SET);

	// malloc the size
	memfile = new char[size + 1];

	fread(memfile, sizeof(char), size, file);
	memfile[size] = 0;

	// close the file
	fclose(file);

	readinfo->memsize = size;
	return (void *)memfile;
}

static void DDF_ParseVersion(const char *str, int len)
{
	if (len <= 0 || ! isspace(*str))
		DDF_Error("Badly formed #VERSION directive.\n");
	
	for (; isspace(*str) && len > 0; str++, len--)
	{ }

	if (len < 4 || ! isdigit(str[0]) || str[1] != '.' ||
		! isdigit(str[2]) || ! isdigit(str[3]))
	{
		DDF_Error("Badly formed #VERSION directive.\n");
	}

	ddf_version = ((str[0] - '0') << 8) |
	              ((str[2] - '0') << 4) | (str[3] - '0');

	if (ddf_version < 0x123)
		DDF_Error("Illegal #VERSION number.\n");

	if (ddf_version > EDGEVER)
		DDF_Error("This version of EDGE cannot handle this DDF.\n");
}


//
// Description of the DDF Parser:
//
// The DDF Parser is a simple reader that is very limited in error checking,
// however it can adapt to most tasks, as is required for the variety of stuff
// need to be loaded in order to configure the EDGE Engine.
//
// The parser will read an ascii file, character by character an interpret each
// depending in which mode it is in; Unless an error is encountered or a called
// procedure stops the parser, it will read everything until EOF is encountered.
//
// When the parser function is called, a pointer to a readinfo_t is passed and
// contains all the info needed, it contains:
//
// * filename              - filename to be read, returns error if NULL
// * DDF_MainCheckName     - function called when a def has been just been started
// * DDF_MainCheckCmd      - function called when we need to check a command
// * DDF_MainCreateEntry   - function called when a def has been completed
// * DDF_MainFinishingCode - function called when EOF is read
// * currentcmdlist        - Current list of commands
//
// Also when commands are referenced, they use currentcmdlist, which is a pointer
// to a list of entries, the entries are formatted like this:
//
// * name - name of command
// * routine - function called to interpret info
// * numeric - void pointer to an value (possibly used by routine)
//
// name is compared with the read command, to see if it matchs.
// routine called to interpret info, if command name matches read command.
// numeric is used if a numeric value needs to be changed, by routine.
//
// The different parser modes are:
//  waiting_newdef
//  reading_newdef
//  reading_command
//  reading_data
//  reading_remark
//  reading_string
//
// 'waiting_newdef' is only set at the start of the code, At this point every
// character with the exception of DEFSTART is ignored. When DEFSTART is
// encounted, the parser will switch to reading_newdef. DEFSTART the parser
// will only switches modes and sets firstgo to false.
//
// 'reading_newdef' reads all alphanumeric characters and the '_' character - which
// substitudes for a space character (whitespace is ignored) - until DEFSTOP is read.
// DEFSTOP passes the read string to DDF_MainCheckName and then clears the string.
// Mode reading_command is now set. All read stuff is passed to char *buffer.
//
// 'reading_command' picks out all the alphabetic characters and passes them to
// buffer as soon as COMMANDREAD is encountered; DDF_MainReadCmd looks through
// for a matching command, if none is found a fatal error is returned. If a matching
// command is found, this function returns a command reference number to command ref
// and sets the mode to reading_data. if DEFSTART is encountered the procedure will
// clear the buffer, run DDF_MainCreateEntry (called this as it reflects that in Items
// & Scenery if starts a new mobj type, in truth it can do anything procedure wise) and
// then switch mode to reading_newdef.
//
// 'reading_data' passes alphanumeric characters, plus a few other characters that
// are also needed. It continues to feed buffer until a SEPARATOR or a TERMINATOR is
// found. The difference between SEPARATOR and TERMINATOR is that a TERMINATOR refs
// the cmdlist to find the routine to use and then sets the mode to reading_command,
// whereas SEPARATOR refs the cmdlist to find the routine and a looks for more data
// on the same command. This is how the multiple states and specials are defined.
//
// 'reading_remark' does not process any chars except REMARKSTOP, everything else is
// ignored. This mode is only set when REMARKSTART is found, when this happens the
// current mode is held in formerstatus, which is restored when REMARKSTOP is found.
//
// 'reading_string' is set when the parser is going through data (reading_data) and
// encounters STRINGSTART and only stops on a STRINGSTOP. When reading_string,
// everything that is an ASCII char is read (which the exception of STRINGSTOP) and
// passed to the buffer. REMARKS are ignored in when reading_string and the case is
// take notice of here.
//
// The maximum size of BUFFER is set in the BUFFERSIZE define.
//
// DDF_MainReadFile & DDF_MainProcessChar handle the main processing of the file, all
// the procedures in the other DDF files (which the exceptions of the Inits) are
// called directly or indirectly. DDF_MainReadFile handles to opening, closing and
// calling of procedures, DDF_MainProcessChar makes sense from the character read
// from the file.
//

//
// DDF_MainReadFile
//
// -ACB- 1998/08/10 Added the string reading code
// -ACB- 1998/09/28 DDF_ReadFunction Localised here
// -AJA- 1999/10/02 Recursive { } comments.
// -ES- 2000/02/29 Added
//
bool DDF_MainReadFile(readinfo_t * readinfo)
{
	char *name;
	char *value = NULL;
	epi::string_c buffer;
	char character;
	char *memfile;
	char *memfileptr;
	int status, formerstatus;
	int response;
	int size;
	int comment_level;
	int bracket_level;
	bool firstgo;
	
	epi::strent_c current_cmd;
	
	int current_index = 0;
	int entry_count = 0;
  
#if (DEBUG_DDFREAD)
	char charcount = 0;
#endif

	ddf_version = 0x127;

	status = waiting_tag;
	formerstatus = readstatus_invalid;
	comment_level = 0;
	bracket_level = 0;
	firstgo = true;

	cur_ddf_line_num = 1;

	if (!readinfo->memfile && !readinfo->filename)
		I_Error("DDF_MainReadFile: No file to read\n");

	if (!readinfo->memfile)
	{
		readinfo->memfile = (char*)DDF_MainCacheFile(readinfo);

		// no file ?  No worries, we'll get it from edge.wad...
		if (!readinfo->memfile)
			return false;
      
		cur_ddf_filename = readinfo->filename;
	}
	else
	{
		cur_ddf_filename = readinfo->lumpname;
	}

	memfileptr = memfile = readinfo->memfile;
	size = readinfo->memsize;

	// -ACB- 1998/09/12 Copy file to memory: Read until end. Speed optimisation.
	while (memfileptr < &memfile[size])
	{
		// -KM- 1998/12/16 Added #define command to ddf files.
		if (!strnicmp(memfileptr, "#DEFINE", 7))
		{
			bool line = false;

			memfileptr += 8;
			name = memfileptr;

			while (*memfileptr != ' ' && memfileptr < &memfile[size])
				memfileptr++;

			if (memfileptr < &memfile[size])
			{
				*memfileptr++ = 0;
				value = memfileptr;
			}
			else
			{
				DDF_Error("#DEFINE '%s' as what?!\n", name);
			}

			while (memfileptr < &memfile[size])
			{
				if (*memfileptr == '\r')
					*memfileptr = ' ';
				if (*memfileptr == '\\')
					line = true;
				if (*memfileptr == '\n' && !line)
					break;
				memfileptr++;
			}

			if (*memfileptr == '\n')
				cur_ddf_line_num++;

			*memfileptr++ = 0;

			DDF_MainAddDefine(name, value);

			buffer.Empty();
			continue;
		}

		// -AJA- 1999/10/27: Not the greatest place for it, but detect //
		//       comments here and ignore them.  Ow the pain of long
		//       identifier names...  Ow the pain of &memfile[size] :-)
    
		if (comment_level == 0 && status != reading_string &&
			memfileptr+1 < &memfile[size] &&
			memfileptr[0] == '/' && memfileptr[1] == '/')
		{
			while (memfileptr < &memfile[size] && *memfileptr != '\n')
				memfileptr++;

			if (memfileptr >= &memfile[size])
				break;
		}
    
		character = *memfileptr++;

		if (character == '\n')
		{
			int l_len;

			cur_ddf_line_num++;

			// -AJA- 2000/03/21: determine linedata.  Ouch.
			for (l_len=0; &memfileptr[l_len] < &memfile[size] &&
					 memfileptr[l_len] != '\n' && memfileptr[l_len] != '\r'; l_len++)
			{ }


			cur_ddf_linedata.Empty();
			cur_ddf_linedata.AddChars(memfileptr, 0, l_len);
			
			// -AJA- 2001/05/21: handle directives (lines beginning with #).
			// This code is more hackitude -- to be fixed when the whole
			// parsing code gets the overhaul it needs.
      
			if (strnicmp(memfileptr, "#CLEARALL", 9) == 0)
			{
				if (! firstgo)
					DDF_Error("#CLEARALL cannot be used inside an entry !\n");

				(* readinfo->clear_all)();

				memfileptr += l_len;
				continue;
			}

			if (strnicmp(memfileptr, "#VERSION", 8) == 0)
			{
				if (! firstgo)
					DDF_Error("#VERSION cannot be used inside an entry !\n");

				DDF_ParseVersion(memfileptr + 8, l_len - 8);

				memfileptr += l_len;
				continue;
			}
		}

		response = DDF_MainProcessChar(character, buffer, status);

		switch (response)
		{
			case remark_start:
				if (ddf_version >= 0x129)
					DDF_Warning("Comments in { } deprecated, use // instead.\n");

				if (comment_level == 0)
				{
					formerstatus = status;
					status = reading_remark;
				}
				comment_level++;
				break;

			case remark_stop:
				comment_level--;
				if (comment_level == 0)
				{
					status = formerstatus;
				}
				break;

			case command_read:
				buffer.ToUpper();

				if (!buffer.IsEmpty())
					current_cmd.Set(buffer.GetString());
				else
					current_cmd.Clear();
					
				DEV_ASSERT2(current_index == 0);

				buffer.Empty();
				status = reading_data;
				break;

			case tag_start:
				status = reading_tag;
				break;

			case tag_stop:
				if (buffer.CompareNoCase(readinfo->tag))
					DDF_Error("Start tag <%s> expected, found <%s>!\n", 
							  readinfo->tag, buffer.GetString());
				
				status = waiting_newdef;
				buffer.Empty();
				break;

			case def_start:
				if (bracket_level > 0)
					DDF_Error("Unclosed () brackets detected.\n");
         
				entry_count++;

				if (firstgo)
				{
					firstgo = false;
					status = reading_newdef;
				}
				else
				{
					cur_ddf_linedata.Empty();

					// finish off previous entry
					(* readinfo->finish_entry)();

					buffer.Empty();
					
					status = reading_newdef;

					cur_ddf_entryname.Empty();
				}
				break;

			case def_stop:
				buffer.ToUpper();	 // <-- Do we need to do this anymore?

				cur_ddf_entryname.Format("[%s]", buffer.GetString());

				(* readinfo->start_entry)(buffer.GetString());
         
				buffer.Empty();
				status = reading_command;
				break;

				// -AJA- 2000/10/02: support for () brackets
			case group_start:
				if (status == reading_data || status == reading_command)
					bracket_level++;
				break;

			case group_stop:
				if (status == reading_data || status == reading_command)
				{
					bracket_level--;
					if (bracket_level < 0)
						DDF_Error("Unexpected `)' bracket.\n");
				}
				break;

			case separator:
				if (bracket_level > 0)
				{
					buffer.AddChar(SEPARATOR);
					break;
				}

				if (current_cmd.IsEmpty())
					DDF_Error("Unexpected comma `,'.\n");

				if (firstgo)
					DDF_WarnError2(0x128, "Command %s used outside of any entry\n");
				else
				{  
					(* readinfo->parse_field)(current_cmd.GetString(), 
											  DDF_MainGetDefine(buffer), current_index, false);
					current_index++;
				}

				buffer.Empty();
				break;

				// -ACB- 1998/08/10 String Handling
			case string_start:
				status = reading_string;
				break;

				// -ACB- 1998/08/10 String Handling
			case string_stop:
				status = reading_data;
				break;

			case terminator:
				if (current_cmd.IsEmpty())
					DDF_Error("Unexpected semicolon `;'.\n");

				if (bracket_level > 0)
					DDF_Error("Missing ')' bracket in ddf command.\n");

				(* readinfo->parse_field)(current_cmd.GetString(), 
										  DDF_MainGetDefine(buffer.GetString()), current_index, true);
				current_index = 0;

				buffer.Empty();
				status = reading_command;
				break;

			case property_read:
				DDF_WarnError2(0x128, "Badly formed command: Unexpected semicolon `;'\n");
				break;

			case nothing:
				break;

			case ok_char:
#if (DEBUG_DDFREAD)
				charcount++;
				L_WriteDebug("%c", character);
				if (charcount == 75)
				{
					charcount = 0;
					L_WriteDebug("\n");
				}
#endif
				break;

			default:
				break;
		}
	}

	current_cmd.Clear();
	cur_ddf_linedata.Empty();

	// -AJA- 1999/10/21: check for unclosed comments
	if (comment_level > 0)
		DDF_Error("Unclosed comments detected.\n");

	if (bracket_level > 0)
		DDF_Error("Unclosed () brackets detected.\n");

	if (status == reading_tag)
		DDF_Error("Unclosed <> brackets detected.\n");

	if (status == reading_newdef)
		DDF_Error("Unclosed [] brackets detected.\n");
	
	if (status == reading_data || status == reading_string)
		DDF_WarnError2(0x128, "Unfinished DDF command on last line.\n");

	// if firstgo is true, nothing was defined
	if (!firstgo)
		(* readinfo->finish_entry)();

	cur_ddf_entryname.Empty();
	cur_ddf_filename.Empty();

	if (defines)
	{
		Z_Free(defines);
		numDefines = 0;
		defines = NULL;
	}

	if (readinfo->filename)
	{
		delete [] memfile;
	}

	return true;
}

//
// DDF_MainProcessChar
//
// 1998/08/10 Added String reading code.
//
readchar_t DDF_MainProcessChar(char character, epi::string_c& buffer, int status)
{
	//int len;

	// -ACB- 1998/08/11 Used for detecting formatting in a string
	static bool formatchar = false;

	// With the exception of reading_string, whitespace is ignored and
	// a SUBSPACE is replaced by a space.
	if (status != reading_string)
	{
		if (isspace(character))
			return nothing;

		if (character == SUBSPACE)
			character = SPACE;
	}
	else  // check for formatting char in a string
	{
		if (!formatchar && character == '\\')
		{
			formatchar = true;
			return nothing;
		}
	}

	// -AJA- 1999/09/26: Handle unmatched '}' better.
	if (status != reading_string && character == REMARKSTART)
		return remark_start;
  
	if (status == reading_remark && character == REMARKSTOP)
		return remark_stop;

	if (status != reading_string && character == REMARKSTOP)
		DDF_Error("DDF: Encountered '}' without previous '{'.\n");

	switch (status)
	{
		case reading_remark:
			return nothing;

			// -ES- 2000/02/29 Added tag check.
		case waiting_tag:
			if (character == TAGSTART)
				return tag_start;
			else
				DDF_Error("DDF: File must start with a tag!\n");
			break;

		case reading_tag:
			if (character == TAGSTOP)
				return tag_stop;
			else
			{
				buffer.AddChar(character);
				return ok_char;
			}

		case waiting_newdef:
			if (character == DEFSTART)
				return def_start;
			else
				return nothing;

		case reading_newdef:
			if (character == DEFSTOP)
			{
				return def_stop;
			}
			else if ((isalnum(character)) || (character == SPACE) ||
					 (character == DIVIDE))
			{
				buffer.AddChar(character);
				return ok_char;
			}
			return nothing;

		case reading_command:
			if (character == COMMANDREAD)
			{
				return command_read;
			}
			else if (character == TERMINATOR)
			{
				return property_read;
			}
			else if (character == DEFSTART)
			{
				return def_start;
			}
			else if (isalnum(character) || character == SPACE ||
					 character == '(' || character == ')' ||
					 character == '.')
			{
				buffer.AddChar(character);
				return ok_char;
			}
			return nothing;

			// -ACB- 1998/08/10 Check for string start
		case reading_data:
			if (character == STRINGSTART)
				return string_start;
      
			if (character == TERMINATOR)
				return terminator;
      
			if (character == SEPARATOR)
				return separator;
      
			if (character == GROUPSTART)
			{
				buffer.AddChar(character);
				return group_start;
			}
      
			if (character == GROUPSTOP)
			{
				buffer.AddChar(character);
				return group_stop;
			}
      
			// Sprite Data - more than a few exceptions....
			if (isalnum(character) || character == SPACE || character == '-' ||
				character == ':' || character == '.' || character == '[' ||
				character == ']' || character == '\\' || character == '!' ||
				character == '#' || character == '%' || 
				character == '@' || character == '?')
			{
				buffer.AddChar(toupper(character));
				return ok_char;
			}
			else if (isprint(character))
				DDF_WarnError("DDF: Illegal character '%c' found.\n", character);

			break;

		case reading_string:  // -ACB- 1998/08/10 New string handling
			// -KM- 1999/01/29 Fixed nasty bug where \" would be recognised as
			//  string end over quote mark.  One of the level text used this.
			if (formatchar)
			{
				// -ACB- 1998/08/11 Formatting check: Carriage-return.
				if (character == 'n')
				{
					buffer.AddChar('\n');
					formatchar = false;
					return ok_char;
				}
				else if (character == '\"')    // -KM- 1998/10/29 Also recognise quote
				{
					buffer.AddChar('\"');
					formatchar = false;
					return ok_char;
				}
				else if (character == '\\') // -ACB- 1999/11/24 Double backslash means directory
				{
					buffer.AddChar(DIRSEPARATOR);
					formatchar = false;
					return ok_char;
				}
				else // -ACB- 1999/11/24 Any other characters are treated in the norm
				{
					buffer.AddChar(character);
					formatchar = false;
					return ok_char;
				}

			}
			else if (character == STRINGSTOP)
			{
				return string_stop;
			}
			else if (character == '\n')
			{
				cur_ddf_line_num--;
				DDF_WarnError2(0x128, "Unclosed string detected.\n");

				cur_ddf_line_num++;
				return nothing;
			}
			// -KM- 1998/10/29 Removed ascii check, allow foreign characters (?)
			// -ES- HEY! Swedish is not foreign!
			else
			{
				buffer.AddChar(character);
				return ok_char;
			}

		default:  // doh!
			I_Error("DDF_MainProcessChar: INTERNAL ERROR: "
					"Bad status value %d !\n", status);
			break;
	}

	return nothing;
}

//
// DDF_MainGetNumeric
//
// Get numeric value directly from the file
//
void DDF_MainGetNumeric(const char *info, void *storage)
{
	int *dest = (int *)storage;

	DEV_ASSERT2(info && storage);

	if (isalpha(info[0]))
	{
		DDF_WarnError2(0x128, "Bad numeric value: %s\n", info);
		return;
	}

	// -KM- 1999/01/29 strtol accepts hex and decimal.
	*dest = strtol(info, NULL, 0);  // straight conversion - no messin'
}

//
// DDF_MainGetBoolean
//
// Get true/false from the file
//
// -KM- 1998/09/01 Gets a true/false value
//
void DDF_MainGetBoolean(const char *info, void *storage)
{
	bool *dest = (bool *)storage;

	DEV_ASSERT2(info && storage);

	if ((stricmp(info, "TRUE") == 0) || (strcmp(info, "1") == 0))
	{
		*dest = true;
		return;
	}

	if ((stricmp(info, "FALSE") == 0) || (strcmp(info, "0") == 0))
	{
		*dest = false;
		return;
	}

	DDF_Error("Bad boolean value: %s\n", info);
}

//
// DDF_MainGetString
//
// Get String value directly from the file
//
// -KM- 1998/07/31 Needed a string argument.  Based on DDF_MainGetNumeric.
// -AJA- 2000/02/09: Free any existing string (potential memory leak).
// -ACB- 2004/07/26: Use epi::strent_c
//
void DDF_MainGetString(const char *info, void *storage)
{
	epi::strent_c *dest = (epi::strent_c *)storage;

	DEV_ASSERT2(info && storage);

	dest->Set(info);
}


//
// DDF_MainParseSubField
//
// Check if the sub-command exists, and call the parser function if it
// does (and return true), otherwise return false.  For sub-commands,
// the storage pointer
//
bool DDF_MainParseSubField(const commandlist_t *sub_comms,
								const char *field, const char *contents, char *stor_base,
    char *dummy_base, const char *base_command)
{
	int i, len;
	bool obsolete = false;
	const char *name = NULL;

	for (i=0; sub_comms[i].name; i++)
	{
		name = sub_comms[i].name;
		obsolete = false;

		if (name[0] == '!')
		{
			name++;
			obsolete = true;
		}
    
		// handle sub-fields within sub-fields
		if (name[0] == '*')
		{
			name++;

			len = strlen(name);
			DEV_ASSERT2(len > 0);

			if (strncmp(field, name, len) == 0 && field[len] == '.' && 
				isalnum(field[len+1]))
			{
				// found the sub-field reference, recurse !

				int offset = ((char *) sub_comms[i].storage) - dummy_base;
        
				return DDF_MainParseSubField(sub_comms[i].sub_comms,
						field + len + 1, contents, stor_base + offset,
						(char *)sub_comms[i].sub_dummy_base, name);
			}

			continue;
		}

		if (DDF_CompareName(field, name) == 0)
			break;
	}

	if (!sub_comms[i].name)
		return false;

	if (obsolete)
		DDF_Obsolete("The ddf %s.%s command is obsolete !\n", base_command, name);

	// found it, so call parse routine

	DEV_ASSERT2(sub_comms[i].parse_command);

	int offset = ((char *) sub_comms[i].storage) - dummy_base;

	(* sub_comms[i].parse_command)(contents, stor_base + offset);

	return true;
}

//
// DDF_MainParseField
//
// Check if the command exists, and call the parser function if it
// does (and return true), otherwise return false.
//
bool DDF_MainParseField(const commandlist_t *commands, 
							 const char *field, const char *contents)
{
	int i, len;
	bool obsolete = false;
	const char *name = NULL;

	for (i=0; commands[i].name; i++)
	{
		name = commands[i].name;
		obsolete = false;

		if (name[0] == '!')
		{
			name++;
			obsolete = true;
		}
    
		// handle subfields
		if (name[0] == '*')
		{
			name++;

			len = strlen(name);
			DEV_ASSERT2(len > 0);

			if (strncmp(field, name, len) == 0 && field[len] == '.' && 
				isalnum(field[len+1]))
			{
				// found the sub-field reference
				return DDF_MainParseSubField( commands[i].sub_comms, 
                        field + len + 1, contents, (char*)commands[i].storage,
                        (char*)commands[i].sub_dummy_base, name);
			}
      
			continue;
		}

		if (DDF_CompareName(field, name) == 0)
			break;
	}

	if (!commands[i].name)
		return false;

	if (obsolete)
		DDF_Obsolete("The ddf %s command is obsolete !\n", name);

	// found it, so call parse routine

	DEV_ASSERT2(commands[i].parse_command);

	(* commands[i].parse_command)(contents, commands[i].storage);

	return true;
}

//
// DDF_MainGetInlineStr10
//
// Gets the string and checks the length to see if is not more than 9.

void DDF_MainGetInlineStr10(const char *info, void *storage)
{
	char *str = (char *)storage;

	DEV_ASSERT2(info && storage);

	if (strlen(info) > 9)
		DDF_Error("Name %s too long (must be 9 characters or less)\n", info);

	strcpy(str, info);
}

//
// DDF_MainGetInlineStr32
//
// Gets the string and checks the length to see if is not more than 31.

void DDF_MainGetInlineStr32(const char *info, void *storage)
{
	char *str = (char *)storage;

	DEV_ASSERT2(info && storage);

	if (strlen(info) > 31)
		DDF_Error("Name %s too long (must be 31 characters or less)\n", info);

	strcpy(str, info);
}

void DDF_MainRefAttack(const char *info, void *storage)
{
	atkdef_c **dest = (atkdef_c **)storage;

	DEV_ASSERT2(info && storage);

	*dest = (atkdef_c*)atkdefs.Lookup(info);
	if (*dest == NULL)
		DDF_WarnError2(0x128, "Unknown Attack: %s\n", info);
}

//
// DDF_MainLookupDirector
//
int DDF_MainLookupDirector(const mobjtype_c *info, const char *ref)
{
	int i, state, offset;
	epi::string_c director;
	const char *div;

	// find `:'
	div = strchr(ref, DIVIDE);

	i = div ? (div - ref) : strlen(ref);

	if (i <= 0)
		DDF_Error("Bad Director `%s' : Nothing after divide\n", ref);

	director.Empty();
	director.AddChars(ref, 0, i);

	state = DDF_StateFindLabel(info->first_state, info->last_state, director);

	offset = div ? MAX(0, atoi(div + 1) - 1) : 0;

	// FIXME: check for overflow
	return state + offset;
}

void DDF_MainGetFloat(const char *info, void *storage)
{
	float *dest = (float *)storage;

	DEV_ASSERT2(info && storage);

	if (sscanf(info, "%f", dest) != 1)
		DDF_Error("Bad floating point value: %s\n", info);
}

// -AJA- 1999/09/11: Added DDF_MainGetAngle and DDF_MainGetSlope.

void DDF_MainGetAngle(const char *info, void *storage)
{
	float val;
	angle_t *dest = (angle_t *)storage;

	DEV_ASSERT2(info && storage);

	if (sscanf(info, "%f", &val) != 1)
		DDF_Error("Bad angle value: %s\n", info);

	if ((int) val == 360)
		val = 359.5;
	else if (val > 360.0f)
		DDF_WarnError2(0x129, "Angle '%s' too large (must be less than 360)\n", info);

	*dest = FLOAT_2_ANG(val);
}

void DDF_MainGetSlope(const char *info, void *storage)
{
	float val;
	float *dest = (float *)storage;

	DEV_ASSERT2(info && storage);

	if (sscanf(info, "%f", &val) != 1)
		DDF_Error("Bad slope value: %s\n", info);

	if (val > +89.5f)
		val = +89.5f;
	if (val < -89.5f)
		val = -89.5f;

	*dest = M_Tan(FLOAT_2_ANG(val));
}

//
// DDF_MainGetPercent
//
// Reads percentages (0%..100%).
//
void DDF_MainGetPercent(const char *info, void *storage)
{
	percent_t *dest = (percent_t *)storage;
	char s[101];
	char *p;
	float f;

	// check that the string is valid
	Z_StrNCpy(s, info, 100);
	for (p = s; isdigit(*p) || *p == '.'; p++)
	{ /* do nothing */ }

	// the number must be followed by %
	if (*p != '%')
	{
		DDF_WarnError2(0x128, "Bad percent value '%s': Should be a number followed by %%\n", info);
		// -AJA- 2001/01/27: backwards compatibility
		DDF_MainGetFloat(s, &f);
		*dest = MAX(0, MIN(1, f));
		return;
	}

	*p = 0;
  
	DDF_MainGetFloat(s, &f);
	if (f < 0.0f || f > 100.0f)
		DDF_Error("Bad percent value '%s': Must be between 0%% and 100%%\n", s);

	*dest = f / 100.0f;
}

//
// DDF_MainGetPercentAny
//
// Like the above routine, but allows percentages outside of the
// 0-100% range (which is useful in same instances).
//
void DDF_MainGetPercentAny(const char *info, void *storage)
{
	percent_t *dest = (percent_t *)storage;
	char s[101];
	char *p;
	float f;

	// check that the string is valid
	Z_StrNCpy(s, info, 100);
	for (p = s; isdigit(*p) || *p == '.'; p++)
	{ /* do nothing */ }

	// the number must be followed by %
	if (*p != '%')
	{
		DDF_WarnError2(0x128, "Bad percent value '%s': Should be a number followed by %%\n", info);
		// -AJA- 2001/01/27: backwards compatibility
		DDF_MainGetFloat(s, dest);
		return;
	}

	*p = 0;
  
	DDF_MainGetFloat(s, &f);

	*dest = f / 100.0f;
}

// -KM- 1998/09/27 You can end a number with T to specify tics; ie 35T
// means 35 tics while 3.5 means 3.5 seconds.

void DDF_MainGetTime(const char *info, void *storage)
{
	float val;
	int *dest = (int *)storage;

	DEV_ASSERT2(info && storage);

	// -ES- 1999/09/14 MAXT means that time should be maximal.
	if (!stricmp(info, "maxt"))
	{
		*dest = INT_MAX; // -ACB- 1999/09/22 Standards, Please.
		return;
	}

	if (strchr(info, 'T'))
	{
		DDF_MainGetNumeric(info, storage);
		return;
	}

	if (sscanf(info, "%f", &val) != 1)
		DDF_Error("Bad time value: %s\n", info);

	*dest = (int)(val * (float)TICRATE);
}

//
// DDF_DummyFunction
//
void DDF_DummyFunction(const char *info, void *storage)
{
	/* does nothing */
}

//
// DDF_MainGetColourmap
//
void DDF_MainGetColourmap(const char *info, void *storage)
{
	const colourmap_c **result = (const colourmap_c **)storage;

	*result = colourmaps.Lookup(info);
	if (*result == NULL)
		DDF_Error("DDF_MainGetColourmap: No such colourmap '%s'\n", info);
	
}

//
// DDF_MainGetRGB
//
void DDF_MainGetRGB(const char *info, void *storage)
{
	rgbcol_t *result = (rgbcol_t *)storage;
	int r, g, b;

	DEV_ASSERT2(info && storage);

	if (DDF_CompareName(info, "NONE") == 0)
	{
		*result = RGB_NO_VALUE;
		return;
	}

	if (sscanf(info, " #%2x%2x%2x ", &r, &g, &b) != 3)
		DDF_Error("Bad RGB colour value: %s\n", info);

	*result = (r << 16) | (g << 8) | b;

	// silently change if matches the "none specified" value
	if (*result == RGB_NO_VALUE)
		*result ^= 0x010101;
}

//
// DDF_MainGetWhenAppear
//
// Syntax:  [ '!' ]  [ SKILL ]  ':'  [ NETMODE ]
//
// SKILL = digit { ':' digit }  |  digit '-' digit.
// NETMODE = 'sp'  |  'coop'  |  'dm'.
//
// When no skill was specified, it's as though all were specified.
// Same for the netmode.
//
// -AJA- 2004/10/28: Dodgy-est crap ever, now with ranges and negation.
//
void DDF_MainGetWhenAppear(const char *info, void *storage)
{
	when_appear_e *result = (when_appear_e *)storage;

	*result = WNAP_None;

	bool negate = (info[0] == '!');

	const char *range = strstr(info, "-");

	if (range)
	{
		if (range <= info   || range[+1] == 0  ||
			range[-1] < '1' || range[-1] > '5' ||
			range[+1] < '1' || range[+1] > '5' ||
			range[-1] > range[+1])
		{
			DDF_Error("Bad range in WHEN_APPEAR value: %s\n", info);
			return;
		}

		for (char sk = '1'; sk <= '5'; sk++)
			if (range[-1] <= sk && sk <= range[+1])
				*result = (when_appear_e)(*result | (WNAP_SkillLevel1 << (sk - '1')));
	}
	else
	{
		if (strstr(info, "1"))
			*result = (when_appear_e)(*result | WNAP_SkillLevel1);

		if (strstr(info, "2"))
			*result = (when_appear_e)(*result | WNAP_SkillLevel2);

		if (strstr(info, "3"))
			*result = (when_appear_e)(*result | WNAP_SkillLevel3);

		if (strstr(info, "4"))
			*result = (when_appear_e)(*result | WNAP_SkillLevel4);

		if (strstr(info, "5"))
			*result = (when_appear_e)(*result | WNAP_SkillLevel5);
	}

	if (strstr(info, "SP") || strstr(info, "sp"))
		*result = (when_appear_e)(*result| WNAP_Single);

	if (strstr(info, "COOP") || strstr(info, "coop"))
		*result = (when_appear_e)(*result | WNAP_Coop);

	if (strstr(info, "DM") || strstr(info, "dm"))
		*result = (when_appear_e)(*result | WNAP_DeathMatch);

	// allow more human readable strings...

	if (negate)
		*result = (when_appear_e)(*result ^ (WNAP_SkillBits | WNAP_NetBits));

	if ((*result & WNAP_SkillBits) == 0)
		*result = (when_appear_e)(*result | WNAP_SkillBits);

	if ((*result & WNAP_NetBits) == 0)
		*result = (when_appear_e)(*result | WNAP_NetBits);
}

#if 0  // DEBUGGING ONLY
void Test_ParseWhenAppear(void)
{
	when_appear_e val;

	DDF_MainGetWhenAppear("1", &val);  printf("WNAP '1' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("3", &val);  printf("WNAP '3' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("5", &val);  printf("WNAP '5' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("7", &val);  printf("WNAP '7' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("1:2", &val);  printf("WNAP '1:2' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("5:3:1", &val);  printf("WNAP '5:3:1' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("1-3", &val);  printf("WNAP '1-3' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("4-5", &val);  printf("WNAP '4-5' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("0-2", &val);  printf("WNAP '0-2' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("3-6", &val);  printf("WNAP '3-6' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("5-1", &val);  printf("WNAP '5-1' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("sp", &val);  printf("WNAP 'sp' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("coop", &val);  printf("WNAP 'coop' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("dm", &val);  printf("WNAP 'dm' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("sp:coop", &val);  printf("WNAP 'sp:coop' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("sp:dm", &val);  printf("WNAP 'sp:dm' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("sp:coop:dm", &val);  printf("WNAP 'sp:coop:dm' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("sp:3", &val);  printf("WNAP 'sp:3' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("sp:3:4", &val);  printf("WNAP 'sp:3:4' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("sp:3-5", &val);  printf("WNAP 'sp:3-5' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("sp:dm:3", &val);  printf("WNAP 'sp:dm:3' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("sp:dm:3:4", &val);  printf("WNAP 'sp:dm:3:4' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("sp:dm:3-5", &val);  printf("WNAP 'sp:dm:3-5' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("DM:COOP:SP:3", &val);  printf("WNAP 'DM:COOP:SP:3' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("DM:COOP:SP:3:4", &val);  printf("WNAP 'DM:COOP:SP:3:4' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("DM:COOP:SP:3-5", &val);  printf("WNAP 'DM:COOP:SP:3-5' --> 0x%04x\n", val);

	printf("------------------------------------------------------------\n");

	DDF_MainGetWhenAppear("!1", &val);  printf("WNAP '!1' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!3", &val);  printf("WNAP '!3' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!5", &val);  printf("WNAP '!5' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!7", &val);  printf("WNAP '!7' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("!1:2", &val);  printf("WNAP '!1:2' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!5:3:1", &val);  printf("WNAP '!5:3:1' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("!1-3", &val);  printf("WNAP '!1-3' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!4-5", &val);  printf("WNAP '!4-5' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!0-2", &val);  printf("WNAP '!0-2' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!3-6", &val);  printf("WNAP '!3-6' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!5-1", &val);  printf("WNAP '!5-1' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("!sp", &val);  printf("WNAP '!sp' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!coop", &val);  printf("WNAP '!coop' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!dm", &val);  printf("WNAP '!dm' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!sp:coop", &val);  printf("WNAP '!sp:coop' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!sp:dm", &val);  printf("WNAP '!sp:dm' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!sp:coop:dm", &val);  printf("WNAP '!sp:coop:dm' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("!sp:3", &val);  printf("WNAP '!sp:3' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!sp:3:4", &val);  printf("WNAP '!sp:3:4' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!sp:3-5", &val);  printf("WNAP '!sp:3-5' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("!sp:dm:3", &val);  printf("WNAP '!sp:dm:3' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!sp:dm:3:4", &val);  printf("WNAP '!sp:dm:3:4' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!sp:dm:3-5", &val);  printf("WNAP '!sp:dm:3-5' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("!DM:COOP:SP:3", &val);  printf("WNAP '!DM:COOP:SP:3' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!DM:COOP:SP:3:4", &val);  printf("WNAP '!DM:COOP:SP:3:4' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!DM:COOP:SP:3-5", &val);  printf("WNAP '!DM:COOP:SP:3-5' --> 0x%04x\n", val);
}
#endif

//
// DDF_MainGetBitSet
//
void DDF_MainGetBitSet(const char *info, void *storage)
{
	bitset_t *result = (bitset_t *)storage;
	int start, end;

	DEV_ASSERT2(info && storage);

	// allow a numeric value
	if (sscanf(info, " %i ", result) == 1)
		return;

	*result = BITSET_EMPTY;

	for (; *info; info++)
	{
		if (*info < 'A' || *info > 'Z')
			continue;
    
		start = end = (*info) - 'A';

		// handle ranges
		if (info[1] == '-' && 'A' <= info[2] && info[2] <= 'Z' &&
			info[2] >= info[0])
		{
			end = info[2] - 'A';
		}

		for (; start <= end; start++)
			(*result) |= (1 << start);
	}
}

static int FindSpecialFlag(const char *prefix, const char *name,
						   const specflags_t *flag_set)
{
	int i;
	char try_name[512];

	for (i=0; flag_set[i].name; i++)
	{
		const char *current = flag_set[i].name;
		bool obsolete = false;

		if (current[0] == '!')
		{
			current++;
			obsolete = true;
		}
    
		sprintf(try_name, "%s%s", prefix, current);
    
		if (DDF_CompareName(name, try_name) == 0)
		{
			if (obsolete)
				DDF_Obsolete("The ddf flag `%s' is obsolete !\n", try_name);

			return i;
		}
	}

	return -1;
}

checkflag_result_e DDF_MainCheckSpecialFlag(const char *name,
							 const specflags_t *flag_set, int *flag_value, 
							 bool allow_prefixes, bool allow_user)
{
	int index;
	int negate = 0;
	int user = 0;

	// try plain name...
	index = FindSpecialFlag("", name, flag_set);
  
	if (allow_prefixes)
	{
		// try name with ENABLE_ prefix...
		if (index == -1)
		{
			index = FindSpecialFlag("ENABLE_", name, flag_set);
		}

		// try name with NO_ prefix...
		if (index == -1)
		{
			negate = 1;
			index = FindSpecialFlag("NO_", name, flag_set);
		}

		// try name with NOT_ prefix...
		if (index == -1)
		{
			negate = 1;
			index = FindSpecialFlag("NOT_", name, flag_set);
		}

		// try name with DISABLE_ prefix...
		if (index == -1)
		{
			negate = 1;
			index = FindSpecialFlag("DISABLE_", name, flag_set);
		}

		// try name with USER_ prefix...
		if (index == -1 && allow_user)
		{
			user = 1;
			negate = 0;
			index = FindSpecialFlag("USER_", name, flag_set);
		}
	}

	if (index < 0)
		return CHKF_Unknown;

	(*flag_value) = flag_set[index].flags;

	if (flag_set[index].negative)
		negate = !negate;
  
	if (user)
		return CHKF_User;
  
	if (negate)
		return CHKF_Negative;

	return CHKF_Positive;
}

//
// DDF_DecodeBrackets
//
// Decode a keyword followed by something in () brackets.  Buf_len gives
// the maximum size of the output buffers.  The outer keyword is required
// to be non-empty, though the inside can be empty.  Returns false if
// cannot be parsed (e.g. no brackets).  Handles strings.
//
bool DDF_MainDecodeBrackets(const char *info, char *outer, char *inner,
	int buf_len)
{
	const char *pos = info;
	
	while (*pos && *pos != '(')
		pos++;
	
	if (*pos == 0 || pos == info)
		return false;
	
	if (pos - info >= buf_len)  // overflow
		return false;

	strncpy(outer, info, pos - info);
	outer[pos - info] = 0;

	pos++;  // skip the '('

	info = pos;

	bool in_string = false;

	while (*pos && (in_string || *pos != ')'))
	{
		// handle escaped quotes
		if (pos[0] == '\\' && pos[1] == '"')
		{
			pos += 2;
			continue;
		}

		if (*pos == '"')
			in_string = ! in_string;

		pos++;
	}

	if (*pos == 0)
		return false;

	if (pos - info >= buf_len)  // overflow
		return false;

	strncpy(inner, info, pos - info);
	inner[pos - info] = 0;

	return true;
}

//
// DDF_MainDecodeList
//
// Find the dividing character.  Returns NULL if not found.
// Handles strings and brackets unless simple is true.
//
const char *DDF_MainDecodeList(const char *info, char divider, bool simple)
{
	int  brackets  = 0;
	bool in_string = false;

	const char *pos = info;

	for (;;)
	{
		if (*pos == 0)
			break;

		if (brackets == 0 && !in_string && *pos == divider)
			return pos;

		// handle escaped quotes
		if (! simple)
		{
			if (pos[0] == '\\' && pos[1] == '"')
			{
				pos += 2;
				continue;
			}

			if (*pos == '"')
				in_string = ! in_string;

			if (!in_string && *pos == '(')
				brackets++;

			if (!in_string && *pos == ')')
			{
				brackets--;
				if (brackets < 0)
					DDF_Error("Too many ')' found: %s\n", info);
			}
		}

		pos++;
	}

	if (in_string)
		DDF_Error("Unterminated string found: %s\n", info);

	if (brackets != 0)
		DDF_Error("Unclosed brackets found: %s\n", info);

	return NULL;
}

// DDF OBJECTS

// ---> ddf base class

//
// ddf_base_c Constructor
//
ddf_base_c::ddf_base_c()
{
}

//
// ddf_base_c Deconstructor
//
ddf_base_c::~ddf_base_c()
{
}

//
// ddf_base_c::Copy()
//
void ddf_base_c::Copy(const ddf_base_c &src)
{
	name = src.name;
	number = src.number;
	crc = src.crc;
}

//
// ddf_base_c::Default()
//
void ddf_base_c::Default()
{
	name.Clear();
	number = 0;
	crc.Reset();
}

//
// ddf_base_c::SetUniqueName()
//
void ddf_base_c::SetUniqueName(const char *prefix, int num)
{
	epi::string_c s;
	s.Format("_%s_%d\n", prefix, num);
	name.Set(s.GetString());
}

//
// ddf_base_c assignment operator
//
ddf_base_c& ddf_base_c::operator= (const ddf_base_c &rhs)
{
	if (&rhs != this)
		Copy(rhs);
		
	return *this;
}

// ---> mobj_strref class

const mobjtype_c *mobj_strref_c::GetRef()
{
	if (def)
		return def;

	def = mobjtypes.Lookup(name.GetString());

	return def;
}

// ---> damage class

//
// damage_c Constructor
//
damage_c::damage_c()
{
}

//
// damage_c Copy constructor
//
damage_c::damage_c(damage_c &rhs)
{
	Copy(rhs);
}

//
// damage_c Destructor
//
damage_c::~damage_c()
{
}

//
// damage_c::Copy
//
void damage_c::Copy(damage_c &src)
{
	nominal = src.nominal;
	linear_max = src.linear_max;
	error = src.error;
	delay = src.delay;

	pain = src.pain;
	death = src.death;
	overkill = src.overkill;
	
	no_armour = src.no_armour;
}

//
// damage_c::Default
//
void damage_c::Default(damage_c::default_e def)
{
	switch (def)
	{
		case DEFAULT_MobjChoke:
		{
			nominal	= 6.0f;		
			linear_max = 14.0f;	
			error = -1.0f;			
			delay = 2 * TICRATE;   
			no_armour = true;
			break;
		}

		case DEFAULT_Sector:
		{
			nominal = 0.0f;
			linear_max = -1.0f;
			error = -1.0f;
			delay = 31;
			no_armour = false;
			break;
		}
		
		case DEFAULT_Attack:
		case DEFAULT_Mobj:
		default:
		{
			nominal = 0.0f;
			linear_max = -1.0f;     
			error = -1.0f;     
			delay = 0;      
			no_armour = false;
			break;
		}
	}

	pain.Default();
	death.Default();
	overkill.Default();
}

//
// damage_c assignment operator
//
damage_c& damage_c::operator=(damage_c &rhs)
{
	if (&rhs != this)
		Copy(rhs);
		
	return *this;
}

// ---> label offset class

//
// label_offset_c Constructor
//
label_offset_c::label_offset_c()
{
	offset = 0;
}

//
// label_offset_c Copy constructor
//
label_offset_c::label_offset_c(label_offset_c &rhs)
{
	Copy(rhs);
}

//
// label_offset_c Destructor
//
label_offset_c::~label_offset_c()
{
}

//
// label_offset_c::Copy
//
void label_offset_c::Copy(label_offset_c &src)
{
	label = src.label;
	offset = src.offset;
}

//
// label_offset_c::Default
//
void label_offset_c::Default()
{
	label.Clear();
	offset = 0;
}


//
// label_offset_c assignment operator
//
label_offset_c& label_offset_c::operator=(label_offset_c& rhs)
{
	if (&rhs != this)
		Copy(rhs);
		
	return *this;
}

// ---> dlightinfo class

//
// dlightinfo_c() constructor
//
dlightinfo_c::dlightinfo_c()
{
	Default();
}

//
// dlightinfo_c() Copy constructor
//
dlightinfo_c::dlightinfo_c(dlightinfo_c &rhs)
{
	Copy(rhs);
}

//
// dlightinfo_c::Copy()
//
void dlightinfo_c::Copy(dlightinfo_c &src)
{
	type = src.type;
	intensity = src.intensity;
	colour = src.colour;
	height = src.height;
}

//
// dlightinfo_c::Default()
//
void dlightinfo_c::Default()
{
	type = DLITE_None;
	intensity = 32;
	colour = 0xFFFFFF;    // White (RGB 8:8:8)
	height = PERCENT_MAKE(50);
}

//
// dlightinfo_c assignment operator
//
dlightinfo_c& dlightinfo_c::operator=(dlightinfo_c &rhs)
{
	if (&rhs != this)
		Copy(rhs);

	return *this;
}

// ---> haloinfo class

//
// haloinfo_c() constructor
//
haloinfo_c::haloinfo_c()
{
	Default();
}

//
// haloinfo_c Copy constructor
//
haloinfo_c::haloinfo_c(haloinfo_c &rhs)
{
	Copy(rhs);
}

//
// haloinfo_c::Copy()
//
void haloinfo_c::Copy(haloinfo_c &src)
{
	height = src.height;
	size = src.size;
	minsize = src.minsize;
	maxsize = src.maxsize;
	translucency = src.translucency;
	colour = src.colour;					// (RGB 8:8:8)
	graphic = src.graphic;
}

//
// haloinfo_c::Default()
// 
void haloinfo_c::Default(void)
{
	height = -1.0f;
	size = 32.0f;
	minsize = -1.0f;
	maxsize = -1.0f;
	translucency = PERCENT_MAKE(50);
	colour = 0xFFFFFF;					// (RGB 8:8:8)
	graphic.Clear();
}

//
// haloinfo_c& assignment operator
//
haloinfo_c& haloinfo_c::operator=(haloinfo_c &rhs)
{
	if (&rhs != this)
		Copy(rhs);

	return *this;
}
