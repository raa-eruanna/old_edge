//------------------------------------------------------------------------
//  SELECTION LISTS
//------------------------------------------------------------------------
//
//  Eureka DOOM Editor (C) 2001-2009 Andrew Apted
//                     (C) 1997-2003 Andr� Majorel et al
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
//  in the public domain in 1994 by Rapha�l Quinet and Brendon Wyber.
//
//------------------------------------------------------------------------

#include "main.h"

#include "m_bitvec.h"
#include "levels.h"
#include "selectn.h"


/*
 *  IsSelected - test if an object is in a selection list
 *
 *  FIXME change the identifier
 *  FIXME slow
 */
bool IsSelected (SelPtr list, int objnum)
{
  SelPtr cur;

  for (cur = list; cur; cur = cur->next)
    if (cur->objnum == objnum)
      return true;
  return false;
}


/*
 *  DumpSelection - list the contents of a selection
 *
 *  FIXME change the identifier
 */
void DumpSelection (SelPtr list)
{
  int n;
  SelPtr cur;

  printf ("Selection:");
  for (n = 0, cur = list; cur; cur = cur->next, n++)
    printf (" %d", cur->objnum);
  printf ("  (%d)\n", n);
}


/*
 *  SelectObject - add an object to a selection list
 *
 *  FIXME change the identifier
 */
void SelectObject (SelPtr *list, int objnum)
{
  SelPtr cur;

  if (! is_obj (objnum))
    FatalError("BUG: SelectObject called with %d", objnum);
  cur = (SelPtr) GetMemory (sizeof (struct SelectionList));
  cur->next = *list;
  cur->objnum = objnum;
  *list = cur;
}


/*
 *  select_unselect_obj - add or remove an object from a selection list
 *
 *  If the object is not in the selection list, add it.  If
 *  it already is, remove it.
 */
void select_unselect_obj (SelPtr *list, int objnum)
{
SelPtr cur;
SelPtr prev;

if (! is_obj (objnum))
  FatalError("s/u_obj called with %d", objnum);
for (prev = NULL, cur = *list; cur != NULL; prev = cur, cur = cur->next)
  // Already selected: unselect it.
  if (cur->objnum == objnum)
  {
    if (prev != NULL)
      prev->next = cur->next;
    else
      *list = cur->next;
    FreeMemory (cur);
    if (prev != NULL)
      cur = prev->next;
    else
      cur = (SelPtr) NULL;
    return;
  }

  // Not selected: select it.
  cur = (SelPtr) GetMemory (sizeof (struct SelectionList));
  cur->next = *list;
  cur->objnum = objnum;
  *list = cur;
}


/*
 *  UnSelectObject - remove an object from a selection list
 *
 *  FIXME change the identifier
 */
void UnSelectObject (SelPtr *list, int objnum)
{
  SelPtr cur, prev;

  if (! is_obj (objnum))
    FatalError("BUG: UnSelectObject called with %d", objnum);
  prev = 0;
  cur = *list;
  while (cur)
  {
    if (cur->objnum == objnum)
    {
      if (prev)
  prev->next = cur->next;
      else
  *list = cur->next;
      FreeMemory (cur);
      if (prev)
  cur = prev->next;
      else
  cur = 0;
    }
    else
    {
      prev = cur;
      cur = cur->next;
    }
  }
}


/*
 *  ForgetSelection - clear a selection list
 *
 *  FIXME change the identifier
 */
void ForgetSelection (SelPtr *list)
{
  SelPtr cur, prev;

  cur = *list;
  while (cur)
  {
    prev = cur;
    cur = cur->next;
    FreeMemory (prev);
  }
  *list = 0;
}


/*
   select all objects inside a given box
*/

void SelectObjectsInBox (SelPtr *list, int objtype, int x0, int y0, int x1, int y1)
{
int n, m;

if (x1 < x0)
   {
   n = x0;
   x0 = x1;
   x1 = n;
   }
if (y1 < y0)
   {
   n = y0;
   y0 = y1;
   y1 = n;
   }

switch (objtype)
   {
   case OBJ_THINGS:
      
      for (n = 0; n < NumThings; n++)
   if (Things[n].x >= x0 && Things[n].x <= x1
    && Things[n].y >= y0 && Things[n].y <= y1)
            select_unselect_obj (list, n);
      break;
   case OBJ_VERTICES:
      
      for (n = 0; n < NumVertices; n++)
   if (Vertices[n].x >= x0 && Vertices[n].x <= x1
    && Vertices[n].y >= y0 && Vertices[n].y <= y1)
            select_unselect_obj (list, n);
      break;
   case OBJ_LINEDEFS:
      
      for (n = 0; n < NumLineDefs; n++)
   {
   /* the two ends of the line must be in the box */
   m = LineDefs[n].start;
   if (Vertices[m].x < x0 || Vertices[m].x > x1
    || Vertices[m].y < y0 || Vertices[m].y > y1)
      continue;
   m = LineDefs[n].end;
   if (Vertices[m].x < x0 || Vertices[m].x > x1
    || Vertices[m].y < y0 || Vertices[m].y > y1)
      continue;
         select_unselect_obj (list, n);
   }
      break;
   case OBJ_SECTORS:
      {
      signed char *sector_status;
      LDPtr ld;

      sector_status = (signed char *) GetMemory (NumSectors);
      memset (sector_status, 0, NumSectors);
      for (n = 0, ld = LineDefs; n < NumLineDefs; n++, ld++)
         {
         VPtr v1, v2;
         int s1, s2;

         v1 = Vertices + ld->start;
         v2 = Vertices + ld->end;

         // Get the numbers of the sectors on both sides of the linedef
   if (is_sidedef (ld->side_R)
    && is_sector (SideDefs[ld->side_R].sector))
      s1 = SideDefs[ld->side_R].sector;
   else
      s1 = OBJ_NO_NONE;
   if (is_sidedef (ld->side_L)
    && is_sector (SideDefs[ld->side_L].sector))
      s2 = SideDefs[ld->side_L].sector;
   else
      s2 = OBJ_NO_NONE;

         // The linedef is entirely within bounds.
         // The sectors it touches _might_ be within bounds too.
         if (v1->x >= x0 && v1->x <= x1
          && v1->y >= y0 && v1->y <= y1
          && v2->x >= x0 && v2->x <= x1
          && v2->y >= y0 && v2->y <= y1)
            {
            if (is_sector (s1) && sector_status[s1] >= 0)
               sector_status[s1] = 1;
            if (is_sector (s2) && sector_status[s2] >= 0)
               sector_status[s2] = 1;
            }

         // It's not. The sectors it touches _can't_ be within bounds.
         else
            {
            if (is_sector (s1))
               sector_status[s1] = -1;
            if (is_sector (s2))
               sector_status[s2] = -1;
            }
         }

      // Now select all the sectors we found to be within bounds
      ForgetSelection (list);
      for (n = 0; n < NumSectors; n++)
         if (sector_status[n] > 0)
            SelectObject (list, n);
      FreeMemory (sector_status);
      }
      break;
   }
}



/*
 *  list_to_bitvec - make a bit vector from a list
 *
 *  The bit vector has <bitvec_size> elements. It's up to
 *  the caller to delete the new bit vector after use.
 */
bitvec_c *list_to_bitvec (SelPtr list, size_t bitvec_size)
{
  SelPtr cur;
  bitvec_c *bitvec;

  bitvec = new bitvec_c (bitvec_size);
  for (cur = list; cur; cur = cur->next)
    bitvec->set (cur->objnum);
  return bitvec;
}


/*
 *  bitvec_to_list - make a list from a bitvec object
 *
 *  The items are inserted in the list from first to last
 *  (i.e. item N in the bitvec is inserted before item N+1).
 *  It's up to the caller to delete the new list after use.
 */
SelPtr bitvec_to_list (const bitvec_c &b)
{
  SelPtr list = 0;
  for (size_t n = 0; n < b.nelements (); n++)
    if (b.get (n))
      SelectObject (&list, n);
  return list;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
