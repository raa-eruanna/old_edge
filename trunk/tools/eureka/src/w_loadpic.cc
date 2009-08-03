//------------------------------------------------------------------------
//  WAD PIC LOADER
//------------------------------------------------------------------------
//
//  Eureka DOOM Editor
//
//  Copyright (C) 2001-2009 Andrew Apted
//  Copyright (C) 1997-2003 Andr� Majorel et al
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

#include "im_color.h"  /* trans_replace */
#include "im_img.h"

#include "w_loadpic.h"
#include "w_file.h"
#include "w_io.h"

#include "w_rawdef.h"
#include "w_wad.h"


// posts are runs of non masked source pixels
typedef struct
{
	// offset down from top.  P_SENTINEL terminates the list.
	byte topdelta;

    // length data bytes follows
	byte length;
	
	/* byte pixels[length+2] */
}
post_t;

#define P_SENTINEL  0xFF


static void DrawColumn(Img& img, const post_t *column, int x, int y)
{
	SYS_ASSERT(column);

	int W = img.width();
	int H = img.height();

	// clip horizontally
	if (x < 0 || x >= W)
		return;

	while (column->topdelta != P_SENTINEL)
	{
		int top = y + (int) column->topdelta;
		int count = column->length;

		byte *src = (byte *) column + 3;
		byte *dest = img.wbuf() + x;

		if (top < 0)
		{
			count += top;
			top = 0;
		}

		if (top + count > H)
			count = H - top;

		// copy the pixels, remapping any TRANS_PIXEL values
		for (; count > 0; count--, top++)
		{
			byte pix = *src++;

			if (pix == TRANS_PIXEL)
				pix = trans_replace;

			dest[top * W] = pix;
		}

		column = (const post_t *) ((const byte *) column + column->length + 4);
	}
}


/*
 *  LoadPicture - read a picture from a wad file into an Img object
 *
 *  If img->is_null() is false, LoadPicture() does not allocate the
 *  buffer itself. The buffer and the picture don't have to have the
 *  same dimensions. Thanks to this, it can also be used to compose
 *  textures : you allocate a single buffer for the whole texture
 *  and then you call LoadPicture() on it once for each patch.
 *  LoadPicture() takes care of all the necessary clipping.
 *
 *  If img->is_null() is true, LoadPicture() sets the size of img
 *  to match that of the picture. This is useful in display_pic().
 *
 *  Return 0 on success, non-zero on failure.
 *
 *  If pic_x_offset == INT_MIN, the picture is centred horizontally.
 *  If pic_y_offset == INT_MIN, the picture is centred vertically.
 */
bool LoadPicture(
   Img& img,      // Game image to load picture into
   const char *lump_name,   // Picture lump name
   int pic_x_offset,    // Coordinates of top left corner of picture
   int pic_y_offset,    // relative to top left corner of buffer
   int *pic_width,    // To return the size of the picture
   int *pic_height)   // (can be NULL)
{
	Lump_c *lump = W_FindLump(lump_name);
	if (! lump)
		FatalError("LoadPicture: no such lump '%s'\n", lump_name);

	byte *raw_data;
	W_LoadLumpData(lump, &raw_data);

	const patch_t *pat = (patch_t *) raw_data;
  
	int width    = LE_S16(pat->width);
	int height   = LE_S16(pat->height);
	int offset_x = LE_S16(pat->leftoffset);
	int offset_y = LE_S16(pat->topoffset);

	// FIXME: validate values (in case we got flat data or so)

	if (pic_width)  *pic_width  = width;
	if (pic_height) *pic_height = height;

	if (img.is_null())
	{
		// our new image will be completely transparent
		img.resize (width, height);
	}

	// Centre the picture?
	if (pic_x_offset == INT_MIN)
		pic_x_offset = (img.width() - width) / 2;
	if (pic_y_offset == INT_MIN)
		pic_y_offset = (img.height() - height) / 2;

	for (int x=0; x < width; x++)
	{
		int offset = LE_S32(pat->columnofs[x]);

		if (offset < 0 || offset >= lump->Length())
			FatalError("Bad image offset 0x%08x in patch [%s]\n", offset, lump_name);

		const post_t *column = (const post_t *) ((const byte *)pat + offset);

		DrawColumn(img, column, pic_x_offset + x, pic_y_offset);
	}

	W_FreeLumpData(&raw_data);
	return true;
}


typedef enum { _MT_BADOFS, _MT_TOOLONG, _MT_TOOMANY } _msg_type_t;


static int add_msg (char type, int arg) { }
static void flush_msg (const char *picname) { }



int LoadPicture0 (
   Img& img,      // Game image to load picture into
   const char *picname,   // Picture lump name
   const Lump_loc& picloc,  // Picture lump location
   int pic_x_offset,    // Coordinates of top left corner of picture
   int pic_y_offset,    // relative to top left corner of buffer
   int *pic_width,    // To return the size of the picture
   int *pic_height)   // (can be NULL)
{
	MDirPtr dir;
	s16_t pic_width_;
	s16_t pic_height_;
	s16_t pic_intrinsic_x_ofs;
	s16_t pic_intrinsic_y_ofs;
	byte  *ColumnData;
	byte  *Column;
	s32_t *NeededOffsets;
	s32_t CurrentOffset;
	int ColumnInMemory;
	long  ActualBufLen;
	int pic_x;
	int pic_x0;
	int pic_x1;
	int pic_y0;
	int pic_y1;
	byte      *buf; /* This variable is set to point to the element of
					   the image buffer where the top of the current column
					   should be pasted. It can be off the image buffer! */

	// Locate the lump where the picture is
	if (picloc.wad != 0)
	{
		MasterDirectory dirbuf;
		dirbuf.wadfile   = picloc.wad;
		dirbuf.dir.start = picloc.ofs;
		dirbuf.dir.size  = picloc.len;
		dir = &dirbuf;
	}
	else
	{
		dir = (MDirPtr) FindMasterDir (MasterDir, picname);
		if (dir == NULL)
		{
			warn ("picture %.*s does not exist.\n", WAD_PIC_NAME, picname);
			return 1;
		}
	}

	// Read the picture header
	dir->wadfile->seek (dir->dir.start);
	if (dir->wadfile->error ())
	{
		warn ("picture %.*s: can't seek to header, giving up\n",
				WAD_PIC_NAME, picname);
		return 1;
	}
	bool dummy_bytes  = true;
	bool long_header  = true;
	bool long_offsets = true;

	if (long_header)
	{
		dir->wadfile->read_i16 (&pic_width_);
		dir->wadfile->read_i16 (&pic_height_);
		dir->wadfile->read_i16 (&pic_intrinsic_x_ofs);  // Read but ignored
		dir->wadfile->read_i16 (&pic_intrinsic_y_ofs);  // Read but ignored
		if (dir->wadfile->error ())
		{
			warn ("picture %.*s: read error in header, giving up\n",
					WAD_PIC_NAME, picname);
			return 1;
		}
	}
	else
	{
		pic_width_          = dir->wadfile->read_u8 ();
		pic_height_         = dir->wadfile->read_u8 ();
		pic_intrinsic_x_ofs = dir->wadfile->read_u8 ();  // Read but ignored
		pic_intrinsic_y_ofs = dir->wadfile->read_u8 ();  // Read but ignored
		if (dir->wadfile->error ())
		{
			warn ("picture %.*s: read error in header, giving up\n",
					WAD_PIC_NAME, picname);
			return 1;
		}
	}

	// If no buffer given by caller, allocate one.
	if (img.is_null ())
	{
		// Sanity checks
		if (pic_width_  < 1 || pic_height_ < 1)
		{
			warn ("picture %.*s: delirious dimensions %dx%d, giving up\n",
					WAD_PIC_NAME, picname, (int) pic_width_, (int) pic_height_);
		}
		const int pic_width_max = 4096;
		if (pic_width_ > pic_width_max)
		{
			warn ("picture %.*s: too wide (%d), clipping to %d\n",
					WAD_PIC_NAME, picname, (int) pic_width_, pic_width_max);
			pic_width_ = pic_width_max;
		}
		const int pic_height_max = 4096;
		if (pic_height_ > pic_height_max)
		{
			warn ("picture %.*s: too high (%d), clipping to %d\n",
					WAD_PIC_NAME, picname, (int) pic_height_, pic_height_max);
			pic_height_ = pic_height_max;
		}
		img.resize (pic_width_, pic_height_);
	}
	int img_width = img.width ();

	// Centre the picture.
	if (pic_x_offset == INT_MIN)
		pic_x_offset = (img_width - pic_width_) / 2;
	if (pic_y_offset == INT_MIN)
		pic_y_offset = (img.height () - pic_height_) / 2;

	/* AYM 19971202: 17 kB is large enough for 128x128 patches. */
#define TEX_COLUMNBUFFERSIZE ((long) 17 * 1024)
	/* Maximum number of bytes per column. The worst case is a
	   509-high column, with every second pixel transparent. That
	   makes 255 posts of 1 pixel, and a final FFh. The total is
	   (255 x 5 + 1) = 1276 bytes per column. */
#define TEX_COLUMNSIZE  1300

	ColumnData    = (byte *) GetMemory (TEX_COLUMNBUFFERSIZE);
	/* FIXME DOS and pic_width_ > 16000 */
	NeededOffsets = (s32_t *) GetMemory ((long) pic_width_ * 4);

	if (long_offsets)
		dir->wadfile->read_i32 (NeededOffsets, pic_width_);
	else
		for (int n = 0; n < pic_width_; n++)
		{
			s16_t ofs;
			dir->wadfile->read_i16 (&ofs);
			NeededOffsets[n] = ofs;
		}
	if (dir->wadfile->error ())
	{
		warn ("picture %.*s: read error in offset table, giving up\n",
				WAD_PIC_NAME, picname);
		FreeMemory (ColumnData);
		FreeMemory (NeededOffsets);
		return 1;
	}

	// Read first column data, and subsequent column data
	if (long_offsets && NeededOffsets[0] != 8 + (long) pic_width_ * 4
			|| !long_offsets && NeededOffsets[0] != 4 + (long) pic_width_ * 2)
	{
		dir->wadfile->seek (dir->dir.start + NeededOffsets[0]);
		if (dir->wadfile->error ())
		{
			warn ("picture %.*s: can't seek to header, giving up\n",
					WAD_PIC_NAME, picname);
			FreeMemory (ColumnData);
			FreeMemory (NeededOffsets);
			return 1;
		}
	}
	ActualBufLen = dir->wadfile->read_vbytes (ColumnData, TEX_COLUMNBUFFERSIZE);
	// FIXME should catch I/O errors

	// Clip the picture horizontally and vertically
	pic_x0 = - pic_x_offset;
	if (pic_x0 < 0)
		pic_x0 = 0;

	pic_x1 = img_width - pic_x_offset - 1;
	if (pic_x1 >= pic_width_)
		pic_x1 = pic_width_ - 1;

	pic_y0 = - pic_y_offset;
	if (pic_y0 < 0)
		pic_y0 = 0;

	pic_y1 = img.height () - pic_y_offset - 1;
	if (pic_y1 >= pic_height_)
		pic_y1 = pic_height_ - 1;

	// For each (non clipped) column of the picture...
	for (pic_x = pic_x0,
			buf = img.wbuf () + MAX(pic_x_offset, 0) + img_width * pic_y_offset;
			pic_x <= pic_x1;
			pic_x++, buf++)
	{
		byte *filedata;

		CurrentOffset  = NeededOffsets[pic_x];
		ColumnInMemory = CurrentOffset >= NeededOffsets[0]
			&& CurrentOffset + TEX_COLUMNSIZE <= NeededOffsets[0] + ActualBufLen;
		if (ColumnInMemory)
			Column = ColumnData + CurrentOffset - NeededOffsets[0];
		else
		{
			Column = (byte *) GetMemory (TEX_COLUMNSIZE);
			dir->wadfile->seek (dir->dir.start + CurrentOffset);
			if (dir->wadfile->error ())
			{
				int too_many = add_msg (_MT_BADOFS, (short) pic_x);
				FreeMemory (Column);
				if (too_many)    // This picture has too many errors. Give up.
					goto pic_end;
				continue;      // Give up on this column
			}
			dir->wadfile->read_vbytes (Column, TEX_COLUMNSIZE);
			// FIXME should catch I/O errors
		}
		filedata = Column;

		// We now have the needed column data, one way or another, so write it

		// For each post of the column...
		{
			byte *post;
			for (post = filedata; *post != 0xff;)
			{
				int post_y_offset;  // Y-offset of top of post to origin of buffer
				int post_height;    // Height of post
				int post_pic_y0;    // Start and end of non-clipped part of post,
				int post_pic_y1;    // relative to top of picture
				int post_y0;    // Start and end of non-clipped part of post,
				int post_y1;    // relative to top of post

				if (post - filedata > TEX_COLUMNSIZE)
				{
					int too_many = add_msg (_MT_TOOLONG, (short) pic_x);
					if (too_many)    // This picture has too many errors. Give up.
					{
						if (! ColumnInMemory)
							FreeMemory (Column);
						goto pic_end;
					}
					break;       // Give up on this column
				}

				post_y_offset = *post++;
				post_height = *post++;
				if (dummy_bytes)
					post++;      // Skip that dummy byte

				post_pic_y0 = post_y_offset;  // Clip the post vertically
				if (post_pic_y0 < pic_y0)
					post_pic_y0 = pic_y0;

				post_pic_y1 = post_y_offset + post_height - 1;
				if (post_pic_y1 > pic_y1)
					post_pic_y1 = pic_y1;

				post_y0 = post_pic_y0 - post_y_offset;
				post_y1 = post_pic_y1 - post_y_offset;


				{         // "Paste" the post onto the buffer
					img_pixel_t *b;
					const byte *p          = post + post_y0;
					const byte *const pmax = post + post_y1;
					int buf_width = img_width;

					for (b = buf + buf_width * (post_y_offset + post_y0);
							p <= pmax;
							b += buf_width, p++)
					{
#ifdef PARANOIA
						if (b < img.buf ())
						{
							BugError("Picture %.*s(%d): b < buffer",
									WAD_PIC_NAME, picname, (int) pic_x);
						}
#endif
						*b = (*p == TRANS_PIXEL) ? trans_replace : *p;
					}
				}

				post += post_height;
				if (dummy_bytes)
					post++;      // Skip the trailing dummy byte
			}  // Post loop
		}

#ifdef PARANOIA
next_column :
#endif
		if (!ColumnInMemory)
			FreeMemory (Column);
	}  // Column loop

pic_end:
	FreeMemory (ColumnData);
	FreeMemory (NeededOffsets);
	flush_msg (picname);
#if 0
	c->flags |= HOOK_DRAWN;
#endif
	if (pic_width)
		*pic_width  = pic_width_;
	if (pic_height)
		*pic_height = pic_height_;
	return 0;
}



//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
