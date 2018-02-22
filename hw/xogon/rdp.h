/**
 * ogon Remote Desktop Services
 * X11 backend
 *
 * Copyright (C) 2005-2013 Jay Sorg
 * Copyright (C) 2013-2018 Thincast Technologies GmbH
 * Copyright (C) 2013-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
 * Jay Sorg
 * Marc-André Moreau <marcandre.moreau@gmail.com>
 * Norbert Federa <norbert.federa@thincast.com>
 */

#ifndef OGON_BACKEND_X_MAIN_H
#define OGON_BACKEND_X_MAIN_H

#include "xorg-config.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netdb.h>

#include <X11/X.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/fonts/fontstruct.h>
#include <X11/extensions/XKBstr.h>

#define ATOM WATOM
#include "mipointer.h"
#include "randrstr.h"
#include "mi.h"
#include "fb.h"
#include "micmap.h"
#include "exevents.h"
#include "xserver-properties.h"
#include "xkbsrv.h"
#undef ATOM

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/freerdp.h>
#include <freerdp/codec/color.h>
#include <ogon/backend.h>

#define XORG_VERSION(_major, _minor, _patch) (((_major) * 10000000) + ((_minor) * 100000) + ((_patch) * 1000) + 0)

#define XOGON_VERSION "1.0.0"

struct _rdpScreenInfoRec
{
	int width;
	int height;
	int depth;
	int scanline;
	int bitsPerPixel;
	int bytesPerPixel;
	int sizeInBytes;
	char* pfbMemory;

	int dpi;

	int rdp_bpp;
	int rdp_Bpp;
	int rdp_Bpp_mask;

	DamagePtr x11Damage;
	RegionRec screenRec;

	BOOL x11DamageRegistered;
	BOOL sendFullDamage;

	CursorPtr pCurs;
};
typedef struct _rdpScreenInfoRec rdpScreenInfoRec;
typedef rdpScreenInfoRec* rdpScreenInfoPtr;


#include "rdpInput.h"
#include "rdpUpdate.h"

#endif /* OGON_BACKEND_X_MAIN_H */
