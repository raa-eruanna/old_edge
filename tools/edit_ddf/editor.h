//------------------------------------------------------------------------
//  EDITOR : Editing widget
//------------------------------------------------------------------------
//
//  Editor for DDF   (C) 2005  The EDGE Team
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
//  Based on the "editor.cxx" sample program from FLTK 1.1.6,
//  as described in Chapter 4 of the FLTK Programmer's Guide.
//  
//  Copyright 1998-2004 by Bill Spitzak, Mike Sweet and others.
//
//------------------------------------------------------------------------

#ifndef __EDITDDF_EDITOR_H__
#define __EDITDDF_EDITOR_H__

class W_Editor : public Fl_Text_Editor
{
public:
	W_Editor(int X, int Y, int W, int H, const char *label = 0);
	~W_Editor();

public:
	int handle(int event);
	// FLTK virtual method for handling input events.

public:
	Fl_Text_Buffer *textbuf;
	Fl_Text_Buffer *stylebuf;

private:
	static const int TABLE_SIZE = 26;
	static Fl_Text_Display::Style_Table_Entry table_dark [TABLE_SIZE];
	// static Fl_Text_Display::Style_Table_Entry table_light[TABLE_SIZE];

public:
	enum
	{
		SCHEME_Dark,
		SCHEME_Light
	};
	
	void SetScheme(int kind, int font_h);

	bool Load(const char *filename);
	// returns false if file not found.

	void UpdateStyleRange(int start, int end);
	// called by buffer-update callback.

private:
	int cur_scheme;

	void Parse(const char *text, const char *t_end, char *style, char context);

	int ParseTag(const char *text, const char *t_end, char *style, char *context);
	int ParseDirective(const char *text, const char *t_end, char *style, char *context);
	int ParseItem(const char *text, const char *t_end, char *style, char *context);

	int ParseString(const char *text, const char *t_end, char *style);
	int ParseComment(const char *text, const char *t_end, char *style);
	int ParseWord(const char *text, const char *t_end, char *style);
	int ParseNumber(const char *text, const char *t_end, char *style);

	int ParseBadStuff(const char *text, const char *t_end, char *style, char *context);

	int CheckMalformedString(const char *text, const char *t_end, char *style, char *context);
	int CheckSemicolon(const char *text, const char *t_end, int equal_pos, char *style);
	bool CheckComma(const char *text, const char *t_end);

	void ParseLine(const char *text, const char *t_end, char *style, int equal_pos, char *context);
	void ParseCommand(const char *text, const char *t_end, char *style, int equal_pos, char *context);
	void ParseCommandData(const char *text, const char *t_end, char *style, char *context);
	void ParseStateData(const char *text, const char *t_end, char *style, char *context);
	void ParseFlagData(const char *text, const char *t_end, char *style, char *context);

	void ValidateTag(const char *text, const char *t_end, char *style);
	void ValidateItem(const char *text, const char *t_end, char *style);
	char ValidateCommand(const char *text, const char *t_end, char *style);
	void ValidateBrackets(const char *text, const char *t_end, char *style);
};


#endif /* __EDITDDF_EDITOR_H__ */
