//------------------------------------------------------------------------
//  Editing Operations
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

#ifndef __G_EDIT_H__
#define __G_EDIT_H__

#include "g_script.h"


typedef struct
{
  /* this structure is used to reference a certain object or
   * field within a whole script.
   */

  // Map = index into SCRIPT->bits[] vector.
  int M;

  // Rad-trig = index into STARTMAP->pieces[] vector.
  int R; 

  // Thing = index into RADTRIG->things[] vector.
  // will be -1 for radius trigger objects or fields.
  int T;
 
  // Field = specific field of a radius trigger or spawn thing.
  // will be -1 to reference the rad-trig or thing itself.
  int F;
}
reference_t;


typedef enum
{
  FIELD_Integer = 0,
  FIELD_Float,
  FIELD_String,

  FIELD_RadTrigPtr,
  FIELD_ThingPtr,
}
field_type_e;


typedef union
{
  int e_int;
  float e_float;
  const char *e_str;

  rad_trigger_c *e_rad;
  thing_spawn_c *e_thing;
}
edit_value_u;


class edit_op_c
{
public:
  reference_t ref;

  field_type_e type;

  edit_value_u old_val;
  edit_value_u new_val;

  // short description for Undo menu
  std::string desc;

public:
   edit_op_c();
  ~edit_op_c();

public:
  void Perform();
  // perform (or Redo) the operation.

  void Undo();
  // undo the previously performed operation.

///---  const char * Describe() const;
///---  // get a short description of this operation (for the UNDO menu).
///---  // The result must be freed using StringFree() in lib_util.h.
};


#endif // __G_EDIT_H__

//--- editor settings ---
// vi:ts=2:sw=2:expandtab
