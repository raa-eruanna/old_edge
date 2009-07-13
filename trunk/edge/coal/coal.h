/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/

#ifndef __COAL_API_H__
#define __COAL_API_H__

namespace coal
{

// === COMPILER ==================================

void PR_InitData(void);
void PR_BeginCompilation(void);
bool PR_CompileFile(char *buffer, char *filename);
bool PR_FinishCompilation(void);
void PR_ShowStats(void);


// === VM ========================================

void PR_SetTrace(bool enable);
int PR_FindFunction(const char *func_name);
int PR_ExecuteProgram(int fnum);
double * PR_Parameter(int p);

} // namespace coal

#endif /* __COAL_API_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab