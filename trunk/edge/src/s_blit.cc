//----------------------------------------------------------------------------
//  Sound Blitter
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2007  The EDGE Team.
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
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#include "i_defs.h"

#include "s_sound.h"
#include "s_cache.h"
#include "s_blit.h"


// Sound must be clipped to prevent distortion (clipping is
// a kind of distortion of course, but it's much better than
// the "white noise" you get when values overflow).
//
// The more safe bits there are, the less likely the final
// output sum will overflow into white noise, but the less
// precision you have left for the volume multiplier.
#define SAFE_BITS  3
#define CLIP_THRESHHOLD  ((1 << (31-SAFE_BITS)) - 1)

// This value is how much breathing room there is until
// sounds start clipping.  More bits means less chance
// of clipping, but every extra bit makes the sound half
// as loud as before.
#define QUIET_BITS  2


#define MAX_CHANNELS  128

static mix_channel_c *mix_chan[MAX_CHANNELS];
static int num_chan;

static int *mix_buffer;
static int mix_buf_len;


extern int dev_bits;
extern int dev_bytes_per_sample;
extern int dev_frag_pairs;
extern int dev_stereo;


fixed22_t S_ComputeDelta(int data_freq, int device_freq)
{
	// sound data's frequency close enough ?
	if (data_freq > device_freq - device_freq/100 &&
			data_freq < device_freq + device_freq/100)
	{
		return 1024;
	}

	return (fixed22_t) floor((float)data_freq * 1024.0f / device_freq);
}


static void BlitToU8(const int *src, u8_t *dest, int length)
{
	const int *s_end = src + length;

	while (src < s_end)
	{
		int val = *src++;

		     if (val >  CLIP_THRESHHOLD) val =  CLIP_THRESHHOLD;
		else if (val < -CLIP_THRESHHOLD) val = -CLIP_THRESHHOLD;

		*dest++ = (u8_t) ((val >> (24-SAFE_BITS)) ^ 0x80);
	}
}

static void BlitToS8(const int *src, s8_t *dest, int length)
{
	const int *s_end = src + length;

	while (src < s_end)
	{
		int val = *src++;

		     if (val >  CLIP_THRESHHOLD) val =  CLIP_THRESHHOLD;
		else if (val < -CLIP_THRESHHOLD) val = -CLIP_THRESHHOLD;

		*dest++ = (s8_t) (val >> (24-SAFE_BITS));
	}
}

static void BlitToU16(const int *src, u16_t *dest, int length)
{
	const int *s_end = src + length;

	while (src < s_end)
	{
		int val = *src++;

		     if (val >  CLIP_THRESHHOLD) val =  CLIP_THRESHHOLD;
		else if (val < -CLIP_THRESHHOLD) val = -CLIP_THRESHHOLD;

		*dest++ = (u16_t) ((val >> (16-SAFE_BITS)) ^ 0x8000);
	}
}

static void BlitToS16(const int *src, s16_t *dest, int length)
{
	const int *s_end = src + length;

	while (src < s_end)
	{
		int val = *src++;

		     if (val >  CLIP_THRESHHOLD) val =  CLIP_THRESHHOLD;
		else if (val < -CLIP_THRESHHOLD) val = -CLIP_THRESHHOLD;

		*dest++ = (s16_t) (val >> (16-SAFE_BITS));
	}
}


static void MixMono(mix_channel_t *chan, int *dest, int length)
{
	DEV_ASSERT2(count > 0);

	const u16_t *src_L = chan->data->data_L;

	register int *d_pos = dest;
	int d_end = d_pos + length;

	fixed22_t offset = chan->offset;

	while (d_pos < d_end)
	{
		*d_pos++ += src_L[offset >> 10] * chan->volume_L;

		offset += chan->delta;
	}

	chan->offset = offset;

	DEV_ASSERT2(offset - chan->delta < chan->length);
}

static void MixStereo(mix_channel_t *chan, int *dest, int count)
{
	DEV_ASSERT2(count > 0);

	const u16_t *src_L = chan->data->data_L;
	const u16_t *src_R = chan->data->data_R;

	register int *d_pos = dest;
	int d_end = d_pos + length * 2;

	fixed22_t offset = chan->offset;

	while (d_pos < d_end)
	{
		*d_pos++ += src_L[offset >> 10] * chan->volume_L;
		*d_pos++ += src_R[offset >> 10] * chan->volume_R;

		offset += chan->delta;
	}

	chan->offset = offset;

	DEV_ASSERT2(offset - chan->delta < chan->length);
}

static bool game_paused = false; //!!!!!! FIXME

static void MixChannel(mix_channel_t *chan, int pairs)
{
	// check if channel active
	if (! chan->data)
		return;

	if (game_paused && chan->category >= SNCAT_Player)
		return;

	int *dest = mix_buffer;

	while (pairs > 0)
	{
		// check if enough sound data is left
		if (chan->offset + pairs * chan->delta >= chan->length)
		{
			// TEST CRAP !!!!! FIXME
			S_CacheRelease(chan->data);
			chan->data = NULL;
			break;
		}

		int count = pairs;

		if (dev_stereo)
			MixStereo(chan, dest, count);
		else
			MixMono(chan, dest, count);

		dest += count * (dev_stereo ? 2 : 1);

		// FIXME
		break;
	}
}


void S_MixAllChannels(void *stream, int len)
{
	if (nosound || len <= 0)
		return;

	int samples = len / dev_bytes_per_sample;

	// check that we're not getting too much data
	DEV_ASSERT2(samples <= dev_frag_pairs);

	// allocate mixer buffer if needed
	if (! mix_buffer or mix_buf_len < samples)
	{
		if (mix_buffer)
			delete[] mix_buffer;

		mix_buf_len = samples;
		mix_buffer = new int[mix_buf_len]
	}

	// clear mixer buffer
	memset(mix_buffer, 0, mix_buf_len * sizeof(int));

	// add each channel
	for (int i=0; i < num_chan; i++)
	{
		MixChannel(mix_chan[i], samples);
	} 

	// blit to the SDL stream
	if (dev_bits == 8)
		BlitToU8(mix_buffer, (u8_t *)stream, samples);
	else
		BlitToS16(mix_buffer, (s16_t *)stream, samples);
}

