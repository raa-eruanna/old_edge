/*
 *  yadex.cc
 *  The main module.
 *  BW & RQ sometime in 1993 or 1994.
 */


/*
This file is part of Yadex.

Yadex incorporates code from DEU 5.21 that was put in the public domain in
1994 by Rapha�l Quinet and Brendon Wyber.

The rest of Yadex is Copyright � 1997-2003 Andr� Majorel and others.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307, USA.
*/


#include "yadex.h"
#include <time.h>

#include "im_color.h"
#include "cfgfile.h"
#include "editlev.h"
#include "game.h"
#include "gfx.h"
#include "help2.h"
#include "levels.h"    /* Because of "viewtex" */
#include "patchdir.h"  /* Because of "p" */
#include "sanity.h"
#include "w_file.h"
#include "w_list.h"
#include "w_name.h"
#include "w_wads.h"



/*
 *  Constants (declared in yadex.h)
 */
const char *const log_file       = "yadex.log";
const char *const msg_unexpected = "unexpected error";
const char *const msg_nomem      = "Not enough memory";


/*
 *  Not real variables -- just unique pointer values
 *  used by functions that return pointers to 
 */
char error_non_unique[1];  // Found more than one
char error_none[1];        // Found none
char error_invalid[1];     // Invalid parameter


/*
 *  Global variables
 */
FILE *      logfile     = NULL;   // Filepointer to the error log
bool        Registered  = false;  // Registered or shareware game?
int         screen_lines = 24;    // Lines that our TTY can display
int         remind_to_build_nodes = 0;  // Remind user to build nodes

// Set from command line and/or config file
bool      autoscroll      = 0;
unsigned long autoscroll_amp    = 10;
unsigned long autoscroll_edge   = 30;
const char *config_file                 = NULL;
int       copy_linedef_reuse_sidedefs = 0;
bool      Debug       = false;
int       default_ceiling_height  = 128;
char      default_ceiling_texture[WAD_FLAT_NAME + 1]  = "CEIL3_5";
int       default_floor_height        = 0;
char      default_floor_texture[WAD_FLAT_NAME + 1]  = "FLOOR4_8";
int       default_light_level       = 144;
char      default_lower_texture[WAD_TEX_NAME + 1] = "STARTAN3";
char      default_middle_texture[WAD_TEX_NAME + 1]  = "STARTAN3";
char      default_upper_texture[WAD_TEX_NAME + 1] = "STARTAN3";
int       default_thing     = 3004;
int       double_click_timeout    = 200;
bool      Expert      = false;
const char *Game      = NULL;
const char *Warp      = NULL;

int       idle_sleep_ms     = 50;
bool      InfoShown     = true;
int       zoom_default      = 0;  // 0 means fit
int       zoom_step     = 0;  // 0 means sqrt(2)
int       digit_zoom_base               = 100;
int       digit_zoom_step               = 0;  // 0 means sqrt(2)
confirm_t insert_vertex_split_linedef = YC_ASK_ONCE;
confirm_t insert_vertex_merge_vertices  = YC_ASK_ONCE;
bool      blindly_swap_sidedefs         = false;
const char *Iwad1     = NULL;
const char *Iwad2     = NULL;
const char *Iwad3     = NULL;
const char *Iwad4     = NULL;
const char *Iwad5     = NULL;
const char *Iwad6     = NULL;
const char *Iwad7     = NULL;
const char *Iwad8     = NULL;
const char *Iwad9     = NULL;
const char *Iwad10      = NULL;
const char *MainWad     = NULL;
#ifdef AYM_MOUSE_HACKS
int       MouseMickeysH     = 5;
int       MouseMickeysV     = 5;
#endif
char **   PatchWads     = NULL;
bool      Quiet       = false;
bool      Quieter     = false;
unsigned long scroll_less   = 10;
unsigned long scroll_more   = 90;
bool      Select0     = false;
int       show_help     = 0;
int       sprite_scale                  = 100;
bool      SwapButtons     = false;
int       verbose     = 0;
int       welcome_message   = 1;
const char *bench     = 0;

// Global variables declared in game.h
yglf_t yg_level_format   = YGLF__;
ygln_t yg_level_name     = YGLN__;
ygpf_t yg_picture_format = YGPF_NORMAL;
ygtf_t yg_texture_format = YGTF_NORMAL;
ygtl_t yg_texture_lumps  = YGTL_NORMAL;

Wad_name sky_flat;


/*
 *  Prototypes of private functions
 */
static int  parse_environment_vars ();
static void print_error_message (const char *fmt, va_list args);


/*
 *  main
 *  Guess what.
 */
int main (int argc, char *argv[])
{
int r;

// Set <screen_lines>
if (getenv ("LINES") != NULL)
   screen_lines = atoi (getenv ("LINES"));
else
   screen_lines = 0;
if (screen_lines == 0)
   screen_lines = 24;


// First detect manually --help and --version
// because parse_command_line_options() cannot.
if (argc == 2 && strcmp (argv[1], "--help") == 0)
   {
   print_usage (stdout);
   if (fflush (stdout) != 0)
     fatal_error ("stdout: %s", strerror (errno));
   exit (0);
   }
if (argc == 2 && strcmp (argv[1], "--version") == 0)
   {
   puts (what ());
   puts ("# Eureka fluff\n");
   if (fflush (stdout) != 0)
     fatal_error ("stdout: %s", strerror (errno));
   exit (0);
   }

// Second a quick pass through the command line
// arguments to detect -?, -f and -help.
r = parse_command_line_options (argc - 1, argv + 1, 1);
if (r)
   goto syntax_error;

if (show_help)
   {
   print_usage (stdout);
   exit (1);
   }

printf ("%s\n", what ());


// The config file provides some values.
if (config_file != NULL)
  r = parse_config_file_user (config_file);
else
  r = parse_config_file_default ();
if (r == 0)
   {
   // Environment variables can override them.
   r = parse_environment_vars ();
   if (r == 0)
      {
      // And the command line argument can override both.
      r = parse_command_line_options (argc - 1, argv + 1, 2);
      }
   }
if (r != 0)
   {
   syntax_error :
   fprintf (stderr, "Try \"yadex --help\" or \"man yadex\".\n");
   exit (1);
   }

if (Game != NULL && strcmp (Game, "doom") == 0)
   {
   if (Iwad1 == NULL)
      {
      err ("You have to tell me where doom.wad is.");
      fprintf (stderr,
         "Use \"-i1 <file>\" or put \"iwad1=<file>\" in yadex.cfg.\n");
      exit (1);
      }
   MainWad = Iwad1;
   }
else if (Game != NULL && strcmp (Game, "doom2") == 0)
   {
   if (Iwad2 == NULL)
      {
      err ("You have to tell me where doom2.wad is.");
      fprintf (stderr,
         "Use \"-i2 <file>\" or put \"iwad2=<file>\" in yadex.cfg.\n");
      exit (1);
      }
   MainWad = Iwad2;
   }
else if (Game != NULL && strcmp (Game, "heretic") == 0)
   {
   if (Iwad3 == NULL)
      {
      err ("You have to tell me where heretic.wad is.");
      fprintf (stderr,
         "Use \"-i3 <file>\" or put \"iwad3=<file>\" in yadex.cfg.\n");
      exit (1);
      }
   MainWad = Iwad3;
   }
else if (Game != NULL && strcmp (Game, "hexen") == 0)
   {
   if (Iwad4 == NULL)
      {
      err ("You have to tell me where hexen.wad is.");
      fprintf (stderr,
         "Use \"-i4 <file>\" or put \"iwad4=<file>\" in yadex.cfg.\n");
      exit (1);
      }
   MainWad = Iwad4;
   }
else if (Game != NULL && strcmp (Game, "strife") == 0)
   {
   if (Iwad5 == NULL)
      {
      err ("You have to tell me where strife1.wad is.");
      fprintf (stderr,
         "Use \"-i5 <file>\" or put \"iwad5=<file>\" in yadex.cfg.\n");
      exit (1);
      }
   MainWad = Iwad5;
   }
else if (Game != NULL && strcmp (Game, "doom02") == 0)
   {
   if (Iwad6 == NULL)
      {
      err ("You have to tell me where the Doom alpha 0.2 iwad is.");
      fprintf (stderr,
         "Use \"-i6 <file>\" or put \"iwad6=<file>\" in yadex.cfg.\n");
      exit (1);
      }
   MainWad = Iwad6;
   }
else if (Game != NULL && strcmp (Game, "doom04") == 0)
   {
   if (Iwad7 == NULL)
      {
      err ("You have to tell me where the Doom alpha 0.4 iwad is.");
      fprintf (stderr,
         "Use \"-i7 <file>\" or put \"iwad7=<file>\" in yadex.cfg.\n");
      exit (1);
      }
   MainWad = Iwad7;
   }
else if (Game != NULL && strcmp (Game, "doom05") == 0)
   {
   if (Iwad8 == NULL)
      {
      err ("You have to tell me where the Doom alpha 0.5 iwad is.");
      fprintf (stderr,
         "Use \"-i8 <file>\" or put \"iwad8=<file>\" in yadex.cfg.\n");
      exit (1);
      }
   MainWad = Iwad8;
   }
else if (Game != NULL && strcmp (Game, "doompr") == 0)
   {
   if (Iwad9 == NULL)
      {
      err ("You have to tell me where the Doom press release iwad is.");
      fprintf (stderr,
         "Use \"-i9 <file>\" or put \"iwad9=<file>\" in yadex.cfg.\n");
      exit (1);
      }
   MainWad = Iwad9;
   }
else if (Game != NULL && strcmp (Game, "strife10") == 0)
   {
   if (Iwad10 == NULL)
      {
      err ("You have to tell me where strife1.wad is.");
      fprintf (stderr,
         "Use \"-i10 <file>\" or put \"iwad10=<file>\" in yadex.cfg.\n");
      exit (1);
      }
   MainWad = Iwad10;
   }
else
   {
   if (Game == NULL)
      err ("You didn't say for which game you want to edit.");
   else
      err ("Unknown game \"%s\"", Game);
   fprintf (stderr,
  "Use \"-g <game>\" on the command line or put \"game=<game>\" in yadex.cfg\n"
  "where <game> is one of \"doom\", \"doom02\", \"doom04\", \"doom05\","
  " \"doom2\",\n\"doompr\", \"heretic\", \"hexen\", \"strife\" and "
  "\"strife10\".\n");
   exit (1);
   }
if (Debug)
   {
   logfile = fopen (log_file, "a");
   if (logfile == NULL)
      warn ("can't open log file \"%s\" (%s)", log_file, strerror (errno));
   LogMessage (": Welcome to Yadex!\n");
   }
if (Quieter)
   Quiet = true;

// Sanity checks (useful when porting).
check_types ();
check_charset ();

// Load game definitions (*.ygd).
InitGameDefs ();
LoadGameDefs (Game);

// Load the iwad and the pwads.
if (OpenMainWad (MainWad))
   fatal_error ("If you don't give me an iwad, I'll quit. I'm serious.");
if (PatchWads)
   {
   const char * const *pwad_name;
   for (pwad_name = PatchWads; *pwad_name; pwad_name++)
      OpenPatchWad (*pwad_name);
   }
/* sanity check */
CloseUnusedWadFiles ();

// BRANCH 1 : benchmarking (-b)
if (false)
   {
   return 0;  // Exit successfully
   }

// BRANCH 2 : normal use ("yadex:" prompt)
else
   {
   if (welcome_message)
      print_welcome (stdout);

   if (strcmp (Game, "hexen") == 0)
      printf (
   "WARNING: Hexen mode is experimental. Don't expect to be able to do any\n"
   "real Hexen editing with it. You can edit levels but you can't save them.\n"
   "And there might be other bugs... BE CAREFUL !\n\n");

   if (strcmp (Game, "strife") == 0)
      printf (
   "WARNING: Strife mode is experimental. Many thing types, linedef types,\n"
   "etc. are missing or wrong. And be careful, there might be bugs.\n\n");

   /* all systems go! */
    if (! Warp || ! Warp[0])
        Warp = "MAP01";

   EditLevel (Warp, 0);
   }

/* that's all, folks! */
CloseWadFiles ();
FreeGameDefs ();
LogMessage (": The end!\n\n\n");
if (logfile != NULL)
   fclose (logfile);
if (remind_to_build_nodes)
   printf ("\n"
      "** You have made changes to one or more wads. Don't forget to pass\n"
      "** them through a nodes builder (E.G. BSP) before running them.\n"
      "** Like this: \"ybsp foo.wad -o tmp.wad; doom -file tmp.wad\"\n\n");
return 0;
}


/*
 *  parse_environment_vars
 *  Check certain environment variables.
 *  Returns 0 on success, <>0 on error.
 */
static int parse_environment_vars ()
{
char *value;

value = getenv ("YADEX_GAME");
if (value != NULL)
   Game = value;
return 0;
}


/*
   play a fascinating tune
*/

void Beep ()
{
  fl_beep();
}


/*
 *  warn
 *  printf a warning message to stdout.
 *  If the format string of the previous invocation did not
 *  end with a '\n', do not prepend the "Warning: " string.
 *
 *  FIXME should handle cases where stdout is not available
 *  (BGI version when in graphics mode).
 */
void warn (const char *fmt, ...)
{
  static bool start_of_line = true;
  va_list args;

  if (start_of_line)
    fputs ("Warning: ", stdout);
  va_start (args, fmt);
  vprintf (fmt, args);
  size_t len = strlen (fmt);
  start_of_line = len > 0 && fmt[len - 1] == '\n';
}


/*
 *  fatal_error
 *  Print an error message and terminate the program with code 2.
 */
void fatal_error (const char *fmt, ...)
{
// With BGI, we have to switch back to text mode
// before printing the error message so do it now...

va_list args;
va_start (args, fmt);
print_error_message (fmt, args);

// ... on the other hand, with X, we don't have to
// call TermGfx() before printing so we do it last so
// that a segfault occuring in TermGfx() does not
// prevent us from seeing the stderr message.

TermGfx ();  // Don't need to sleep (1) either.


// Clean up things and free swap space
ForgetLevelData ();
ForgetWTextureNames ();
ForgetFTextureNames ();
CloseWadFiles ();
exit (2);
}


/*
 *  err
 *  Print an error message but do not terminate the program.
 */
void err (const char *fmt, ...)
{
va_list args;

va_start (args, fmt);
print_error_message (fmt, args);
}


/*
 *  print_error_message
 *  Print an error message to stderr.
 */
static void print_error_message (const char *fmt, va_list args)
{
fflush (stdout);
fputs ("Error: ", stderr);
vfprintf (stderr, fmt, args);
fputc ('\n', stderr);
fflush (stderr);
if (Debug && logfile != NULL)
   {
   fputs ("Error: ", logfile);
   vfprintf (logfile, fmt, args);
   fputc ('\n', logfile);
   fflush (logfile);
   }
}


/*
 *  nf_bug
 *  Report about a non-fatal bug to stderr. The message
 *  should not expand to more than 80 characters.
 */
void nf_bug (const char *fmt, ...)
{
static bool first_time = 1;
static int repeats = 0;
static char msg_prev[81];
char msg[81];

va_list args;
va_start (args, fmt);
y_vsnprintf (msg, sizeof msg, fmt, args);
if (first_time || strncmp (msg, msg_prev, sizeof msg))
   {
   fflush (stdout);
   if (repeats)
      {
      fprintf (stderr, "Bug: Previous message repeated %d times\n",
      repeats);
      repeats = 0;
      }

   fprintf (stderr, "Bug: %s\n", msg);
   fflush (stderr);
   if (first_time)
      {
      fputs ("REPORT ALL \"Bug:\" MESSAGES TO THE MAINTAINER !\n", stderr);
      first_time = 0;
      }
   strncpy (msg_prev, msg, sizeof msg_prev);
   }
else
   {
   repeats++;  // Same message as above
   if (repeats == 10)
      {
      fflush (stdout);
      fprintf (stderr, "Bug: Previous message repeated %d times\n",
      repeats);
      fflush (stderr);
      repeats = 0;
      }
   }
}


/*
   write a message in the log file
*/

void LogMessage (const char *logstr, ...)
{
  va_list  args;
  time_t   tval;
  char    *tstr;

  if (Debug && logfile != NULL)
  {
    va_start (args, logstr);
    /* if the message begins with ":", output the current date & time first */
    if (logstr[0] == ':')
    {
      time (&tval);
      tstr = ctime (&tval);
      tstr[strlen (tstr) - 1] = '\0';
      fprintf (logfile, "%s", tstr);
    }
    vfprintf (logfile, logstr, args);
    fflush (logfile);  /* AYM 19971031 */
  }
}



/*
 *  irgb2rgb
 *  Convert an IRGB colour (16-colour VGA) to an 8-bit-per-component
 *  RGB colour.
 */
void irgb2rgb (int c, rgb_c *rgb)
{
  if (c == 8)  // Special case for DARKGREY
    rgb->r = rgb->g = rgb->b = 0x40;
  else if (c == 6)
  {
    rgb->r = 0xff;  // ORANGE
    rgb->g = 0xaa;
    rgb->b = 0x00;
  }
  else
  {
    rgb->r = (c & 4) ? ((c & 8) ? 0xff : 0x80) : 0;
    rgb->g = (c & 2) ? ((c & 8) ? 0xff : 0x80) : 0;
    rgb->b = (c & 1) ? ((c & 8) ? 0xff : 0x80) : 0;
  }
}


