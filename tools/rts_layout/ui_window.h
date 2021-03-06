//------------------------------------------------------------------------
//  Main Window
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

#ifndef __UI_WINDOW_H__
#define __UI_WINDOW_H__

#include "ui_menu.h"


class UI_Grid;
class UI_Panel;

class UI_MainWin : public Fl_Double_Window
{
public:
  UI_MainWin(const char *title);
  virtual ~UI_MainWin();

public:
  // main child widgets

#ifdef MACOSX
  Fl_Sys_Menu_Bar *menu_bar;
#else
  Fl_Menu_Bar *menu_bar;
#endif

  UI_Grid  *grid;
  UI_Panel *panel;

public:
  void SetCursor(Fl_Cursor shape);
  // this is a wrapper around the FLTK cursor() method which
  // prevents the possibly expensive call when the shape hasn't
  // changed.

private:
  Fl_Cursor cursor_shape;
  
  static void quit_callback(Fl_Widget *, void *);
};

extern UI_MainWin * main_win;

extern bool application_quit;


#endif // __UI_WINDOW_H__

//--- editor settings ---
// vi:ts=2:sw=2:expandtab
