/**
 * ogon Remote Desktop Services
 * X11 backend
 *
 * Copyright (C) 2011-2012 Jay Sorg
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
 * Jay Sorg
 * Marc-André Moreau <marcandre.moreau@gmail.com>
 * Norbert Federa <norbert.federa@thincast.com>
 */

#ifndef _RDPRANDR_H
#define _RDPRANDR_H

#include "rdpModes.h"

typedef struct _rdpRandRInfo
{
	int x;
	int y;
	int width;
	int height;
	int mmWidth;
	int mmHeight;

	int numModes;
	int modeCount;
	RRModePtr mode;
	RRModePtr* modes;

	EDID* edid;
	Atom edidAtom;

	Rotation rotations;

	int numCrtcs;
	RRCrtcPtr crtc;
	RRCrtcPtr* crtcs;

	int numOutputs;
	RROutputPtr output;
	RROutputPtr* outputs;

} rdpRandRInfoRec, *rdpRandRInfoPtr;

int rdpRRInit(ScreenPtr pScreen);

rdpRandRInfoPtr rdpGetRandRFromScreen(ScreenPtr pScreen);

#endif
