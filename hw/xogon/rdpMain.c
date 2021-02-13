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
 * David FORT <contact@hardening-consulting.com>
 * Jay Sorg
 * Marc-André Moreau <marcandre.moreau@gmail.com>
 * Martin Haimberger <martin.haimberger@thincast.com>
 * Norbert Federa <norbert.federa@thincast.com>
 */

#include "rdp.h"
#include "rdpRandr.h"
#include "rdpScreen.h"
#include "rdpUpdate.h"
#include <version-config.h>

#include "glx_extinit.h"
#include "extension.h"

#include <stdio.h>
#include <sys/shm.h>
#include <sys/stat.h>

#include <winpr/crt.h>
#include <winpr/pipe.h>
#include <ogon/service.h>

#if 0
#define DEBUG_OUT(fmt, ...) ErrorF(fmt, ##__VA_ARGS__)
#else
#define DEBUG_OUT(fmt, ...)
#endif
rdpScreenInfoRec g_rdpScreen;
ScreenPtr g_pScreen = 0;
int g_dpi = 96;

/* main mouse and keyboard devices */
DeviceIntPtr g_pointer = 0;
DeviceIntPtr g_keyboard = 0;
DeviceIntPtr g_multitouch = 0;
int g_multitouchFd = -1;

/* set all these at once, use function set_bpp */
int g_bpp = 16;
int g_Bpp = 2;
int g_Bpp_mask = 0xFFFF;
static int g_firstTime = 1;
static int g_redBits = 5;
static int g_greenBits = 6;
static int g_blueBits = 5;
static int g_initOutputCalled = 0;
int g_width_max = -1;
int g_height_max = -1;
int g_nokpcursors = 0;


/* Common pixmap formats */
static PixmapFormatRec g_formats[MAXFORMATS] =
{
	{ 1, 1, BITMAP_SCANLINE_PAD },
	{ 4, 8, BITMAP_SCANLINE_PAD },
	{ 8, 8, BITMAP_SCANLINE_PAD },
	{ 15, 16, BITMAP_SCANLINE_PAD },
	{ 16, 16, BITMAP_SCANLINE_PAD },
	{ 24, 32, BITMAP_SCANLINE_PAD },
	{ 32, 32, BITMAP_SCANLINE_PAD },
};

static int g_numFormats = 7;

static miPointerSpriteFuncRec g_rdpSpritePointerFuncs =
{
	/* these are in rdpinput.c */
	rdpSpriteRealizeCursor,
	rdpSpriteUnrealizeCursor,
	rdpSpriteSetCursor,
	rdpSpriteMoveCursor,
	rdpSpriteDeviceCursorInitialize,
	rdpSpriteDeviceCursorCleanup
};

static miPointerScreenFuncRec g_rdpPointerCursorFuncs =
{
	/* these are in rdpinput.c */
	rdpCursorOffScreen,
	rdpCrossScreen,
	rdpPointerWarpCursor
};

/* returns error, zero is good */
static int set_bpp(int bpp)
{
	int rv;

	rv = 0;
	g_bpp = bpp;

	if (g_bpp == 8)
	{
		g_Bpp = 1;
		g_Bpp_mask = 0xff;
		g_redBits = 3;
		g_greenBits = 3;
		g_blueBits = 2;
	}
	else if (g_bpp == 15)
	{
		g_Bpp = 2;
		g_Bpp_mask = 0x7FFF;
		g_redBits = 5;
		g_greenBits = 5;
		g_blueBits = 5;
	}
	else if (g_bpp == 16)
	{
		g_Bpp = 2;
		g_Bpp_mask = 0xFFFF;
		g_redBits = 5;
		g_greenBits = 6;
		g_blueBits = 5;
	}
	else if (g_bpp == 24)
	{
		g_Bpp = 4;
		g_Bpp_mask = 0xFFFFFF;
		g_redBits = 8;
		g_greenBits = 8;
		g_blueBits = 8;
	}
	else if (g_bpp == 32)
	{
		g_Bpp = 4;
		g_Bpp_mask = 0xFFFFFF;
		g_redBits = 8;
		g_greenBits = 8;
		g_blueBits = 8;
	}
	else
	{
		rv = 1;
	}

	return rv;
}

static void rdpWakeupHandler(void *blockData, int result)
{
	/*
	 * If select returns -1 the sets are undefined.
	 * don't process the sets in that case.
	 */
	// if (result < 0)
	// {
	// 	return;
	// }
	// rdp_check(blockData, result, pReadmask);
}

static void rdpBlockHandler(void *blockData, OSTimePtr pTimeout, void *pReadmask)
{
	rdp_handle_damage_region(0);
}

static Bool rdpSaveScreen(ScreenPtr pScreen, int on)
{
	return TRUE;
}

static void rdpQueryBestSize(int xclass, unsigned short* pWidth, unsigned short* pHeight, ScreenPtr pScreen)
{
	unsigned short w;

	switch (xclass)
	{
		case CursorShape:

			*pWidth = 96;
			*pHeight = 96;

			if (*pWidth > pScreen->width)
				*pWidth = pScreen->width;
			if (*pHeight > pScreen->height)
				*pHeight = pScreen->height;

			break;

		case TileShape:
		case StippleShape:

			w = *pWidth;

			if ((w & (w - 1)) && w < FB_UNIT)
			{
				for (w = 1; w < *pWidth; w <<= 1);
				; /* Tell the compiler that this is intentional */
					*pWidth = w;
			}
	}
}

static void rdpClientStateChange(CallbackListPtr* cbl, void *myData, void *clt)
{
	dispatchException &= ~DE_RESET; /* hack - force server not to reset */
}

static Bool parseResolution(const char* res, int *width, int *height)
{
	int twidth = 0;
	int theight = 0;

	if (sscanf(res, "%dx%d", &twidth , &theight) != 2)
		return FALSE;
	if (twidth < 0 || theight < 0)
		return FALSE;

	*width = twidth;
	*height = theight;
	return TRUE;
}


static Bool rdpScreenInit(ScreenPtr pScreen, int argc, char** argv)
{
	int dpix;
	int dpiy;
	int ret;
	Bool vis_found;
	VisualPtr vis;
	char *envval;

	g_pScreen = pScreen;

	g_rdpScreen.dpi = g_dpi;
	dpix = g_rdpScreen.dpi;
	dpiy = g_rdpScreen.dpi;
	monitorResolution = g_rdpScreen.dpi;

	ErrorF("ogon-backend-x, an X11 server for ogon\n");
	ErrorF("Version %s (X.Org %s)\n", XOGON_VERSION,VENDOR_MAN_VERSION);
	ErrorF("Copyright (C) 2013-2016 Thincast Technologies GmbH\n");

	if (!rdpScreenCreateFrameBuffer())
	{
		ErrorF("rdpScreenInit: failed to create framebuffer\n");
		return FALSE;
	}

	miClearVisualTypes();

	if (defaultColorVisualClass == -1)
	{
		defaultColorVisualClass = TrueColor;
	}

	if (!miSetVisualTypes(g_rdpScreen.depth, miGetDefaultVisualMask(g_rdpScreen.depth),
			8, defaultColorVisualClass))
	{
		ErrorF("rdpScreenInit miSetVisualTypes failed\n");
		return 0;
	}

	miSetPixmapDepths();

	envval = getenv("OGON_SMAX");
	if (envval)
	{
		if (!parseResolution(envval, &g_width_max, &g_height_max))
		{
			DEBUG_OUT("invalid resolution (\"%s\") passed in environment \n", envval);
		}
	}

	if (g_width_max > 0 && g_rdpScreen.width > g_width_max)
		g_rdpScreen.width = g_width_max;
	if (g_height_max > 0 && g_rdpScreen.height > g_height_max)
		g_rdpScreen.height = g_height_max;
	fprintf(stderr, "ogon-backend-x limited max width to %d and max height to %d\n", g_width_max, g_height_max);

	ret = fbScreenInit(pScreen, g_rdpScreen.pfbMemory,
			g_rdpScreen.width, g_rdpScreen.height,
			dpix, dpiy, g_rdpScreen.scanline / g_rdpScreen.bytesPerPixel, g_rdpScreen.bitsPerPixel);

	if (!ret)
	{
		DEBUG_OUT("rdpScreenInit: error\n");
		return 0;
	}


	/* this is for rgb, not bgr, just doing rgb for now */
	vis = pScreen->visuals + (pScreen->numVisuals - 1);

	while (vis >= pScreen->visuals)
	{
		if ((vis->class | DynamicClass) == DirectColor)
		{
			vis->offsetBlue = 0;
			vis->blueMask = (1 << g_blueBits) - 1;
			vis->offsetGreen = g_blueBits;
			vis->greenMask = ((1 << g_greenBits) - 1) << vis->offsetGreen;
			vis->offsetRed = g_blueBits + g_greenBits;
			vis->redMask = ((1 << g_redBits) - 1) << vis->offsetRed;
		}

		vis--;
	}

	if (g_rdpScreen.bitsPerPixel > 4)
	{
		fbPictureInit(pScreen, 0, 0);
	}

	pScreen->SaveScreen = rdpSaveScreen;
	pScreen->QueryBestSize = rdpQueryBestSize;

	miPointerInitialize(pScreen, &g_rdpSpritePointerFuncs, &g_rdpPointerCursorFuncs, 1);

	vis_found = 0;
	vis = pScreen->visuals + (pScreen->numVisuals - 1);

	while (vis >= pScreen->visuals)
	{
		if (vis->vid == pScreen->rootVisual)
		{
			vis_found = 1;
		}

		vis--;
	}

	if (!vis_found)
	{
		ErrorF("rdpScreenInit: couldn't find root visual\n");
		exit(1);
	}

	ret = 1;

	if (ret)
	{
		ret = fbCreateDefColormap(pScreen);

		if (!ret)
		{
			DEBUG_OUT("rdpScreenInit: fbCreateDefColormap failed\n");
		}
	}
	init_messages_list();

	if (ret)
	{
		ret = rdp_init();

		if (!ret)
		{
			DEBUG_OUT("rdpScreenInit: rdpup_init failed\n");
		}
	}

	if (ret)
	{
		RegisterBlockAndWakeupHandlers(rdpBlockHandler, rdpWakeupHandler, NULL);
	}


	if (!DamageSetup(pScreen))
	{
		FatalError("rdpScreenInit: DamageSetup failed\n");
	}

	g_rdpScreen.x11Damage = DamageCreate((DamageReportFunc) NULL, (DamageDestroyFunc) NULL, DamageReportNone, TRUE, pScreen, pScreen);
	if (!g_rdpScreen.x11Damage)
	{
		FatalError("rdpScreenInit: DamageCreate failed\n");
	}

	rdpRRInit(pScreen);

	return ret;
}

/* this is the first function called, it can be called many times
   returns the number or parameters processed
   if it dosen't apply to the rdp part, return 0 */
int ddxProcessArgument(int argc, char** argv, int i)
{
	if (g_firstTime)
	{
		ZeroMemory(&g_rdpScreen, sizeof(g_rdpScreen));
		g_rdpScreen.width  = 1024;
		g_rdpScreen.height = 768;
		g_rdpScreen.depth = 24;
		set_bpp(24);
		g_firstTime = 0;

		RRExtensionInit(); /* RANDR */
	}

	if (strcmp(argv[i], "-geometry") == 0)
	{
		if (i + 1 >= argc)
		{
			UseMsg();
		}

		if (!parseResolution(argv[i + 1], &g_rdpScreen.width, &g_rdpScreen.height))
		{
			DEBUG_OUT("Invalid geometry %s\n", argv[i + 1]);
			UseMsg();
		}

		rdpWriteMonitorConfig(g_rdpScreen.width, g_rdpScreen.height);

		return 2;
	}

	if (strcmp(argv[i], "-depth") == 0)
	{
		if (i + 1 >= argc)
		{
			UseMsg();
		}

		g_rdpScreen.depth = atoi(argv[i + 1]);

		if (set_bpp(g_rdpScreen.depth) != 0)
		{
			UseMsg();
		}

		return 2;
	}

	if (strcmp(argv[i], "-uds") == 0)
	{
		return 2;
	}
	if (strcmp(argv[i], "-dpi") == 0)
	{
		int dpi;
		dpi = atoi(argv[i+1]);
		if (0 != dpi)
		{
			g_dpi = dpi;
		}
	}
	if (strcmp(argv[i], "-nkc") == 0)
	{
		g_nokpcursors = 1;
		fprintf(stderr, "Disabling keypad cursor keys\n");
		return 2;
	}
	return 0;
}

#if INPUTTHREAD
/** This function is called in Xserver/os/inputthread.c when starting
    the input thread. */
void
ddxInputThreadInit(void)
{
}
#endif

void OsVendorInit(void)
{

}

int XkbDDXSwitchScreen(DeviceIntPtr dev, KeyCode key, XkbAction *act)
{
	DEBUG_OUT("XkbDDXSwitchScreen:\n");
	return 1;
}

int XkbDDXPrivate(DeviceIntPtr dev, KeyCode key, XkbAction *act)
{
	DEBUG_OUT("XkbDDXPrivate:\n");
	return 0;
}

int XkbDDXTerminateServer(DeviceIntPtr dev, KeyCode key, XkbAction *act)
{
	DEBUG_OUT("XkbDDXTerminateServer:\n");
	GiveUp(1);
	return 0;
}

static ExtensionModule rdpExtensions[] =
{
#ifdef GLXEXT
	{ GlxExtensionInit, "GLX", &noGlxExtension },
#endif
};


/* InitOutput is called every time the server resets.  It should call
   AddScreen for each screen (but we only ever have one), and in turn this
   will call rdpScreenInit. */
void InitOutput(ScreenInfo* pScreenInfo, int argc, char** argv)
{
	int i;

	DEBUG_OUT("InitOutput:\n");
	g_initOutputCalled = 1;

	/* initialize pixmap formats */
	pScreenInfo->imageByteOrder = IMAGE_BYTE_ORDER;
	pScreenInfo->bitmapScanlineUnit = BITMAP_SCANLINE_UNIT;
	pScreenInfo->bitmapScanlinePad = BITMAP_SCANLINE_PAD;
	pScreenInfo->bitmapBitOrder = BITMAP_BIT_ORDER;
	pScreenInfo->numPixmapFormats = g_numFormats;

	for (i = 0; i < g_numFormats; i++)
	{
		pScreenInfo->formats[i] = g_formats[i];
	}

	if (!AddCallback(&ClientStateCallback, rdpClientStateChange, NULL))
	{
		ErrorF("InitOutput: AddCallback failed\n");
		return;
	}

	/* initialize screen */
	if (AddScreen(rdpScreenInit, argc, argv) == -1)
	{
		FatalError("Couldn't add screen\n");
	}

	xorgGlxCreateVendor();

	DEBUG_OUT("InitOutput: out\n");
}

void InitInput(int argc, char** argv)
{
	int status;

	DEBUG_OUT("InitInput:\n");

	status = AllocDevicePair(serverClient, "ogon-backend-x", &g_pointer, &g_keyboard,
			rdpMouseProc, rdpKeybdProc, 0);

	if (status != Success)
	{
		FatalError("Failed to init ogon-backend-x default devices.\n");
	}

	mieqInit();
}

void ddxGiveUp(enum ExitCode error)
{
	char unixSocketName[128];

	DEBUG_OUT("ddxGiveUp:\n");

	rdpScreenDestroyFrameBuffer();
	ogon_named_pipe_clean_endpoint(atoi(display), "X11");

	if (g_initOutputCalled)
	{
		sprintf(unixSocketName, "/tmp/.X11-unix/X%s", display);
		unlink(unixSocketName);
	}
}

Bool LegalModifier(unsigned int key, DeviceIntPtr pDev)
{
	return 1; /* true */
}

void ProcessInputEvents(void)
{
	mieqProcessInputEvents();
}

void AbortDDX(enum ExitCode error)
{
	ddxGiveUp(error);
}

void OsVendorFatalError(const char *f, va_list args)
{

}

/* print the command list parameters and exit the program */
void __attribute__((__noreturn__)) ddxUseMsg(void)
{
	ErrorF("\n");
	ErrorF("ogon-backend-x specific options\n");
	ErrorF("-geometry WxH          set framebuffer width & height\n");
	ErrorF("-depth D               set framebuffer depth\n");
	ErrorF("\n");
	exit(1);
}

void CloseInput(void)
{
	DEBUG_OUT("CloseInput\n");
}

void DDXRingBell(int volume, int pitch, int duration)
{
	ogon_msg_beep msg;

	DEBUG_OUT("DDXRingBell volume: %d pitch: %d duration: %d\n", volume, pitch, duration);
	if (volume < 1 || duration < 1 || pitch < 1) {
		return;
	}

	msg.duration = duration;
	msg.frequency = pitch;

	rdp_send_message(OGON_SERVER_BEEP, (ogon_message*) &msg);
}

void DeleteInputDeviceRequest(DeviceIntPtr dev)
{
	DEBUG_OUT("DeleteInputDeviceRequest\n");
}
