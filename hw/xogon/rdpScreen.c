/**
 * ogon Remote Desktop Services
 * X11 Backend
 *
 * Copyright (C) 2013-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright (C) 2013-2018 Thincast Technologies GmbH
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
 * David FORT <contact@hardening-consulting.com>
 * Marc-André Moreau <marcandre.moreau@gmail.com>
 * Norbert Federa <norbert.federa@thincast.com>
 */

#include "rdp.h"
#include "rdpScreen.h"

#include <stdio.h>
#include <sys/shm.h>
#include <sys/stat.h>

extern rdpScreenInfoRec g_rdpScreen;

int get_min_shared_memory_segment_size(void)
{
#ifdef _GNU_SOURCE
	struct shminfo info;

	if ((shmctl(0, IPC_INFO, (struct shmid_ds*)(void*)&info)) == -1)
		return -1;

	return info.shmmin;
#else
	return -1;
#endif
}

int get_max_shared_memory_segment_size(void)
{
#ifdef _GNU_SOURCE
	struct shminfo info;

	if ((shmctl(0, IPC_INFO, (struct shmid_ds*)(void*)&info)) == -1)
		return -1;

	return info.shmmax;
#else
	return -1;
#endif
}

static int rdpScreenBitsPerPixel(int depth)
{
	if (depth == 1)
		return 1;
	else if (depth <= 8)
		return 8;
	else if (depth <= 16)
		return 16;
	else
		return 32;
}

int rdpScreenPixelToMM(int pixels)
{
	return ((int)(((((double)(pixels)) / g_rdpScreen.dpi) * 25.4)));
}

Bool rdpScreenCreateFrameBuffer(void)
{
	BoxRec screenBox;
	int mod;

	if (g_rdpScreen.pfbMemory != NULL)
	{
		ErrorF("rdpScreenCreateFramebuffer error pfbMemory: %p\n", g_rdpScreen.pfbMemory);
		return FALSE;
	}

	g_rdpScreen.bitsPerPixel = rdpScreenBitsPerPixel(g_rdpScreen.depth);
	g_rdpScreen.bytesPerPixel = g_rdpScreen.bitsPerPixel / 8;

	g_rdpScreen.scanline = g_rdpScreen.width * g_rdpScreen.bytesPerPixel;
	g_rdpScreen.scanline += ((mod = g_rdpScreen.scanline % 16)) ? 16 - mod : 0;

	g_rdpScreen.sizeInBytes = (g_rdpScreen.scanline * g_rdpScreen.height);

	g_rdpScreen.pfbMemory = (char*) malloc(g_rdpScreen.sizeInBytes);

	if (!g_rdpScreen.pfbMemory)
	{
		ErrorF("rdpScreenCreateFrameBuffer: pfbMemory creation failed\n");
		return FALSE;
	}
	memset(g_rdpScreen.pfbMemory, 0, g_rdpScreen.sizeInBytes);

	if (g_rdpScreen.x11Damage)
	{
		RegionUninit(&g_rdpScreen.screenRec);
	}

	screenBox.x1 = 0;
	screenBox.y1 = 0;
	screenBox.x2 = g_rdpScreen.width;
	screenBox.y2 = g_rdpScreen.height;
	RegionInit(&g_rdpScreen.screenRec, &screenBox, 1);

	rdp_send_framebuffer_info();

	return TRUE;
}

Bool rdpScreenDestroyFrameBuffer(void)
{
	if (g_rdpScreen.pfbMemory == NULL)
	{
		ErrorF("rdpScreenDestroyFrameBuffer error pfbMemory: %p\n", g_rdpScreen.pfbMemory);
		return FALSE;
	}

	rdp_detach_rds_framebuffer();

	free(g_rdpScreen.pfbMemory);
	g_rdpScreen.pfbMemory = NULL;

	return TRUE;
}

Bool rdpScreenRecreateFrameBuffer(void)
{
	return ( rdpScreenDestroyFrameBuffer() && rdpScreenCreateFrameBuffer() );
}
