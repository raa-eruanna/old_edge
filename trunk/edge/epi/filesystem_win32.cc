//----------------------------------------------------------------------------
//  EDGE Filesystem Class for Win32
//----------------------------------------------------------------------------
//
//  Copyright (c) 2003-2007  The EDGE Team.
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
//
#include <windows.h>

#include "epi.h"
#include "strings.h"

#include "filesystem.h"
#include "files_win32.h"  // WTF??

#define MAX_MODE_CHARS 4


namespace epi
{

struct createfile_s
{
	DWORD dwDesiredAccess;                      // access mode
	DWORD dwShareMode;                          // share mode
	DWORD dwCreationDisposition;                // how to create
};


static bool BuildFileStruct(int epiflags, createfile_s* finfo)
{
    // Must have some value in epiflags
    if (epiflags == 0)
        return false;

	// Sanity checking...
	if (!finfo)
		return false;			// Clearly mad

    // Check for any invalid combinations
    if ((epiflags & file_c::ACCESS_WRITE) && (epiflags & file_c::ACCESS_APPEND))
        return false;


    if (epiflags & file_c::ACCESS_READ)
    {
		if (epiflags & file_c::ACCESS_WRITE)
		{
			finfo->dwDesiredAccess = GENERIC_READ|GENERIC_WRITE;
			finfo->dwShareMode = 0;
			finfo->dwCreationDisposition = OPEN_ALWAYS;
		}
		else if (epiflags & file_c::ACCESS_APPEND)
		{
			finfo->dwDesiredAccess = GENERIC_READ|GENERIC_WRITE;
			finfo->dwShareMode = 0;
			finfo->dwCreationDisposition = OPEN_EXISTING;
		}
		else
		{
			finfo->dwDesiredAccess = GENERIC_READ;
			finfo->dwShareMode = FILE_SHARE_READ;
			finfo->dwCreationDisposition = OPEN_EXISTING;
		}
    }
    else
    {
		if (epiflags & file_c::ACCESS_WRITE)
		{
			finfo->dwDesiredAccess = GENERIC_WRITE;
			finfo->dwShareMode = 0;
			finfo->dwCreationDisposition = OPEN_ALWAYS;
		}
		else if (epiflags & file_c::ACCESS_APPEND)
		{
			finfo->dwDesiredAccess = GENERIC_WRITE;
			finfo->dwShareMode = 0;
			finfo->dwCreationDisposition = OPEN_EXISTING;
		}
		else
		{
			return false;
		}
	}
    
    return true;
}


bool FS_GetCurrDir(char *dir, unsigned int bufsize)
{
	SYS_ASSERT(dir);

	return (::GetCurrentDirectory(bufsize, (LPSTR)dir) != FALSE);
}

bool FS_SetCurrDir(const char *dir)
{
	SYS_ASSERT(dir);

	return (::SetCurrentDirectory(dir) != FALSE);
}

bool FS_IsDir(const char *dir)
{
	SYS_ASSERT(dir);

	bool result;
	char curpath[MAX_PATH];

	FS_GetCurrDir(curpath, MAX_PATH);

	result = FS_SetCurrDir(dir);

	FS_SetCurrDir(curpath);
	return result;
}

bool FS_MakeDir(const char *dir)
{
	SYS_ASSERT(dir);

	return (::CreateDirectory(dir, NULL) != FALSE);
}

bool FS_RemoveDir(const char *dir)
{
	SYS_ASSERT(dir);

	return (::RemoveDirectory(dir) != FALSE);
}

bool FS_ReadDir(filesystem_dir_c *fsd, const char *dir, const char *mask)
{
	if (!dir || !fsd || !mask)
		return false;

	filesystem_direntry_s tmp_entry;
	string_c curr_dir;
	HANDLE handle;
	WIN32_FIND_DATA fdata;

	// Bit of scope for the dir buffer to be held on stack 
	// for the minimum amount of time
	{
		char tmp[MAX_PATH+1];
		
		if (! FS_GetCurrDir(tmp, MAX_PATH))
			return false;

		curr_dir = tmp;
	}

	if (! FS_SetCurrDir(dir))
		return false;

	handle = FindFirstFile(mask, &fdata);
	if (handle == INVALID_HANDLE_VALUE)
		return false;

	// Ensure the container is empty
	fsd->Clear();

	do
	{
		tmp_entry.name = new string_c(fdata.cFileName);
		if (!tmp_entry.name)
			return false;

		tmp_entry.size = fdata.nFileSizeLow;
		tmp_entry.dir = (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)?true:false; 

		if (!fsd->AddEntry(&tmp_entry))
		{
			delete tmp_entry.name;
			return false;
		}
	}
	while(FindNextFile(handle, &fdata));

	FindClose(handle);

	FS_SetCurrDir(curr_dir.GetString());
	return true;
}

bool FS_Access(const char *name, unsigned int flags)
{
	createfile_s fs;
	HANDLE handle;

	if (!BuildFileStruct(flags, &fs))
		return false;

	handle = CreateFile(name, fs.dwDesiredAccess, fs.dwShareMode, 
							NULL, fs.dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);

	if (handle == INVALID_HANDLE_VALUE)	
		return false;

	CloseHandle(handle);
	return true;
}

bool FS_Close(file_c *file)
{
    if (!file)
        return false;

    if (file->GetType() == file_c::TYPE_DISK)
    {
		HANDLE handle;

        win32_file_c *diskfile = (win32_file_c*)file;

        handle = diskfile->GetDescriptor();
        if (handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);

        diskfile->Setup(NULL, INVALID_HANDLE_VALUE);
    }

    delete file;
	return true;
}

bool FS_Copy(const char *dest, const char *src)
{
	SYS_ASSERT(src);
	SYS_ASSERT(dest);

	// Copy src to dest overwriting dest if it exists
	return (::CopyFile(src, dest, FALSE) != FALSE);
}

bool FS_Delete(const char *name)
{
	SYS_ASSERT(name);

	return (::DeleteFile(name) != FALSE);
}

file_c* FS_Open(const char *name, unsigned int flags)
{
	SYS_ASSERT(name);

	createfile_s fs;
	win32_file_c *file;
	HANDLE handle;

	if (!BuildFileStruct(flags, &fs))
		return false;

	handle = CreateFile(name, fs.dwDesiredAccess, fs.dwShareMode, 
							NULL, fs.dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);

	if (handle == INVALID_HANDLE_VALUE)	
		return false;

    file = new win32_file_c;
    if (!file)
    {
        CloseHandle(handle);
        return NULL;
    }

    file->Setup(this, handle);
    return file;
}

//
// bool FS_Rename()
//
bool FS_Rename(const char *oldname, const char *newname)
{
	SYS_ASSERT(oldname);
	SYS_ASSERT(newname);

	return (::MoveFile(oldname, newname) != FALSE);
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
