//------------------------------------------------------------------------
//  SCRIPT Files (high-level)
//------------------------------------------------------------------------
//
//  RTS Layout Tool (C) 2007 Andrew Apted
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

#ifndef __G_SCRIPT_H__
#define __G_SCRIPT_H__


class mobjtype_c;
class rad_trigger_c;
class section_c;


#define INT_UNSPEC    -7755
#define FLOAT_UNSPEC  0.75f


typedef enum
{
  RTS_OK = 0,
  RTS_ERROR = 1,
  RTS_EOF = 2,
}
rts_result_e;


typedef enum
{
  // this is simplified (EDGE supports all 5 skills separately)
  WNAP_Easy   = (1 << 0),
  WNAP_Medium = (1 << 1),
  WNAP_Hard   = (1 << 2),

  WNAP_SP     = (1 << 4),
  WNAP_Coop   = (1 << 5),
  WNAP_DM     = (1 << 6),
}
when_appear_e;

#define WNAP_SkillBits  (WNAP_Easy | WNAP_Medium | WNAP_Hard)
#define WNAP_ModeBits   (WNAP_SP   | WNAP_Coop   | WNAP_DM)


class thing_spawn_c
{
private:
  thing_spawn_c(bool _ambush);

public:
  ~thing_spawn_c();

  int ambush;

  std::string type;

  float x, y, z;
 
  float angle;  // degrees (0 - 360)
  
  int tag;
  int when_appear;

  // FIELD numbers for reference_t
  enum field_ref_e
  {
    F_AMBUSH = 0,
    F_TYPE,
    F_X, F_Y, F_Z,
    F_ANGLE,
    F_TAG,
    F_WHEN_APPEAR,
  };

  rad_trigger_c *parent;

  // the index into the parent's things vector.
  int th_Index;

  // this is not part of script, but looked up later on
  mobjtype_c *ddf_info;

public:
  static bool thing_spawn_c::MatchThing(std::string& line);
  // returns true if this line matches a thing spawner.

  static thing_spawn_c * ReadThing(std::string& line);
  // Create a new thing-spawn definition by parsing the line.
  // The first keyword will already have been checked
  // (SPAWN_THING etc).  Returns NULL if an error occurs.

  void WriteThing(FILE *fp);
  // write this thing-spawn into the given file.

  int&         GetIntRef   (int F);
  float&       GetFloatRef (int F);
  std::string& GetStringRef(int F);

private:
  rts_result_e ParseKeyword(std::string& word);
  // parse keyword parameters like 'TAG=1234'.
};


class rad_trigger_c
{
private:
  rad_trigger_c(bool _rect);

public:
  ~rad_trigger_c();

public:
  int is_rect;

  float mx, my;  // mid point
  float rx, ry;  // radii
  float z1, z2;
 
  std::string name;  // empty = none

  int tag;
  int when_appear;

  // all script lines except stuff covered by the above fields.
  // Does not include the start and end markers.
  std::vector<std::string> lines;

  bool worldspawn;

  // for 'worldspawn' scripts only: all of the entities
  std::vector<thing_spawn_c *> things;

  // FIELD numbers for reference_t
  enum field_ref_e
  {
    F_IS_RECT = 0,
    F_MX, F_MY,
    F_RX, F_RY,
    F_Z1, F_Z2,
    F_NAME,
    F_TAG,
    F_WHEN_APPEAR,
  };

  // the corresponding section (where trig == us)
  section_c *section;
  
  // this index is a pseudo one, counting only radius triggers
  // within the start_map section (ignoring other 'pieces').
  int rad_Index;

  // total number of things: is only used for user information
  // and is NOT required to match the real number (in the vector).
  int th_Total;

public:
  static bool MatchRadTrig(std::string& line);
  // returns true if this line begins a radius trigger
  // (i.e. first keyword in RADIUS_TRIGGER or RECT_TRIGGER).

  static rad_trigger_c * ReadRadTrig(FILE *fp, std::string& first);
  // read a new radius trigger from the file and returns it.
  // Returns NULL if an error occurs.  The parameter 'first'
  // contains the first line, which must have previously matched
  // using MatchRadTrig().

  void WriteRadTrig(FILE *fp);
  // write this script into the given file.

  int&         GetIntRef   (int F);
  float&       GetFloatRef (int F);
  std::string& GetStringRef(int F);

private:
  static bool MatchEndTrig(std::string& line);

  rts_result_e ParseLocation(const char *pos);

  rts_result_e ParseBody(FILE *fp);
  
  rts_result_e ParseCommand(std::string& line);
  // parse the given line of the trigger.  Returns RTS_OK
  // or RTS_ERROR if a problem occurred.

  rts_result_e cmd_Name(const char *args);
  rts_result_e cmd_Tag(const char *args);
  rts_result_e cmd_WhenAppear(const char *args);
};


class section_c
{
public:
   section_c(int _kind);
  ~section_c();

public:
  enum kind_e
  {
    TEXT      = 0,
    START_MAP = 1,
    RAD_TRIG  = 2,
  };

  int kind;

  // index of this section in the parent's vector.
  int sec_Index;

  // --- TEXT ---
  
  std::vector<std::string> lines;

  // --- START_MAP ---
 
  std::string map_name;

  // total number of rad-triggers: only used for user information
  // and is NOT required to match the real number (in the vector).
  int rad_Total;

  // pieces are everything except the START_MAP and END_MAP lines
  std::vector<section_c *> pieces;

  // --- RAD_TRIG ---
  
  rad_trigger_c *trig;

public:
  static bool MatchStartMap(std::string& line);
  // returns true if this line begins with START_MAP.

  static section_c * ReadStartMap(FILE *fp, std::string& first);
  // reads the whole map section (from START_MAP to END_MAP)
  // and returns a new section_c object, or NULL if an error
  // occurred.  The parameter 'first' contains the first line,
  // which must have previously matched with MatchStartMap().

  void WriteSection(FILE *fp);
  // write this section into the given file.

  void AddLine(std::string& line);

private:
  void WriteText(FILE *fp);
  void WriteStartMap(FILE *fp);

  rts_result_e ParseMapName(const char *pos);
  rts_result_e ParsePieces(FILE *fp);

  static bool MatchEndMap(std::string& line);
};


class script_c
{
private:
  script_c();

public:
  ~script_c();
 
public:
  std::vector<section_c *> bits;

public:
  static script_c *Load(FILE *fp);
  // load a whole RTS script file and return the new script_c.
  // If a serious error occurs, shows a dialog message and
  // eventually returns NULL.

  void Save(FILE *fp);
  // save the whole script into the given file.

private:
};


#endif // __G_SCRIPT_H__

//--- editor settings ---
// vi:ts=2:sw=2:expandtab
