/**
 * ogon Remote Desktop Services
 * X11 backend
 *
 * Copyright (C) 2005-2012 Jay Sorg
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

#include "rdp.h"

#include <sys/un.h>

#ifdef DPMSExtension
#include "dpmsproc.h"
#endif

#include "XIstubs.h"

Bool noFontCacheExtension = 1;

/* DPMS */

#ifdef DPMSExtension

Bool DPMSSupported(void)
{
	return FALSE;
}

int DPMSSet(ClientPtr client, int level)
{
	return Success;
}

#endif

/* XI */
int SetDeviceMode(ClientPtr client, DeviceIntPtr dev, int mode)
{
	return BadMatch;
}

int SetDeviceValuators(ClientPtr client, DeviceIntPtr dev,
		int *valuators, int first_valuator, int num_valuators)
{
	return BadMatch;
}

int ChangeDeviceControl(ClientPtr client, DeviceIntPtr dev, xDeviceCtl *control)
{
	return BadMatch;
}
