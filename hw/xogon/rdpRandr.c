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

#include "rdp.h"
#include "rdpModes.h"
#include "rdpRandr.h"
#include "rdpScreen.h"

#include <stdio.h>
#include <sys/shm.h>
#include <sys/stat.h>

#include <winpr/crt.h>
#include <winpr/stream.h>

#define LOG_LEVEL 0
#define LLOGLN(_level, _args) \
		do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

extern rdpScreenInfoRec g_rdpScreen;
extern int g_width_max;
extern int g_height_max;

static DevPrivateKeyRec rdpRandRKeyRec;
static DevPrivateKey rdpRandRKey;

rdpRandRInfoPtr rdpGetRandRFromScreen(ScreenPtr pScreen)
{
	return ((rdpRandRInfoPtr) dixLookupPrivate(&(pScreen->devPrivates), rdpRandRKey));
}

static Bool rdpRRSetConfig(ScreenPtr pScreen, Rotation rotateKind, int rate, RRScreenSizePtr pSize)
{
	LLOGLN(0, ("rdpRRSetConfig: rate: %d id: %d width: %d height: %d mmWidth: %d mmHeight: %d",
			rate, pSize->id, pSize->width, pSize->height, pSize->mmWidth, pSize->mmHeight));

	return TRUE;
}

static Bool rdpRRGetInfo(ScreenPtr pScreen, Rotation* pRotations)
{
	rdpRandRInfoPtr randr;

	LLOGLN(0, ("rdpRRGetInfo"));

	randr = rdpGetRandRFromScreen(pScreen);

	if (!randr)
		return FALSE;

	if ((pScreen->width != randr->width) || (pScreen->height != randr->height))
	{
		randr->mmWidth = rdpScreenPixelToMM(randr->width);
		randr->mmHeight = rdpScreenPixelToMM(randr->height);

		RRScreenSizeSet(pScreen, randr->width, randr->height, randr->mmWidth, randr->mmHeight);
		RRTellChanged(pScreen);
	}

	return TRUE;
}

static Bool rdpRRScreenSetSize(ScreenPtr pScreen, CARD16 width, CARD16 height, CARD32 mmWidth, CARD32 mmHeight)
{
	BoxRec box;
	WindowPtr pRoot;
	PixmapPtr screenPixmap;
	rdpRandRInfoPtr randr;
	BOOL recreatefb = FALSE;

	LLOGLN(0, ("rdpRRScreenSetSize: width: %d height: %d mmWidth: %d mmHeight: %d",
			width, height, mmWidth, mmHeight));

	randr = rdpGetRandRFromScreen(pScreen);

	if (!randr)
		return FALSE;

	if ((width < 1) || (height < 1))
		return FALSE;

	randr->width = width;
	randr->height = height;
	randr->mmWidth = mmWidth;
	randr->mmHeight = mmHeight;

	rdpModeSelect(pScreen, randr->width, randr->height);

	SetRootClip(pScreen, FALSE);

	pRoot = pScreen->root;

	if (pScreen->width != width || pScreen->height != height)
		recreatefb = TRUE;

	g_rdpScreen.width = width;
	g_rdpScreen.height = height;

	pScreen->x = 0;
	pScreen->y = 0;
	pScreen->width = width;
	pScreen->height = height;
	pScreen->mmWidth = mmWidth;
	pScreen->mmHeight = mmHeight;

	screenInfo.x = 0;
	screenInfo.y = 0;
	screenInfo.width = width;
	screenInfo.height = height;

	if (recreatefb && !rdpScreenRecreateFrameBuffer())
	{
		return FALSE;
	}

	screenPixmap = pScreen->GetScreenPixmap(pScreen);

	if (screenPixmap)
	{
		pScreen->ModifyPixmapHeader(screenPixmap, width, height,
				g_rdpScreen.depth, g_rdpScreen.bitsPerPixel,
				g_rdpScreen.scanline, g_rdpScreen.pfbMemory);
	}

	box.x1 = 0;
	box.y1 = 0;
	box.x2 = width;
	box.y2 = height;

	RegionInit(&pRoot->winSize, &box, 1);
	RegionInit(&pRoot->borderSize, &box, 1);
	RegionReset(&pRoot->borderClip, &box);
	RegionBreak(&pRoot->clipList);

	pRoot->drawable.width = width;
	pRoot->drawable.height = height;

	ResizeChildrenWinSize(pRoot, 0, 0, 0, 0);

	RRGetInfo(pScreen, TRUE);

	SetRootClip(pScreen, TRUE);

	miPaintWindow(pRoot, &pRoot->borderClip, PW_BACKGROUND);

	RRScreenSizeNotify(pScreen);

	return TRUE;
}

static Bool rdpRRCrtcSet(ScreenPtr pScreen, RRCrtcPtr crtc, RRModePtr mode,
		int x, int y, Rotation rotation, int numOutputs, RROutputPtr* outputs)
{
	rdpRandRInfoPtr randr;
	RRTransformPtr transform = NULL;

	LLOGLN(0, ("rdpRRCrtcSet: x: %d y: %d numOutputs: %d crtc: %p mode: %p",
			x, y, numOutputs, crtc, mode));

	randr = rdpGetRandRFromScreen(pScreen);

	if (!randr)
		return FALSE;

	if (crtc)
	{
		crtc->x = x;
		crtc->y = y;

		transform = RRCrtcGetTransform(crtc);

		if (mode && crtc->mode)
		{
			rdpModeSelect(pScreen, mode->mode.width, mode->mode.height);

			if (!RROutputSetPhysicalSize(randr->output, randr->mmWidth, randr->mmHeight))
				return FALSE;

			RRCrtcNotify(crtc, randr->mode, x, y, rotation, transform, randr->numOutputs, outputs);

			RRScreenSizeSet(pScreen, randr->width, randr->height, randr->mmWidth, randr->mmHeight);
			RRTellChanged(pScreen);
		}
	}

	RRGetInfo(pScreen, TRUE);

	return TRUE;
}

static Bool rdpRRCrtcSetGamma(ScreenPtr pScreen, RRCrtcPtr crtc)
{
	LLOGLN(0, ("rdpRRCrtcSetGamma"));

	return TRUE;
}

static Bool rdpRRCrtcGetGamma(ScreenPtr pScreen, RRCrtcPtr crtc)
{
	LLOGLN(0, ("rdpRRCrtcGetGamma"));

	crtc->gammaSize = 256;

	if (!crtc->gammaRed)
	{
		crtc->gammaRed = (CARD16*) malloc(crtc->gammaSize * sizeof(CARD16));
		ZeroMemory(crtc->gammaRed, crtc->gammaSize * sizeof(CARD16));
	}

	if (!crtc->gammaGreen)
	{
		crtc->gammaGreen = (CARD16*) malloc(crtc->gammaSize * sizeof(CARD16));
		ZeroMemory(crtc->gammaGreen, crtc->gammaSize * sizeof(CARD16));
	}

	if (!crtc->gammaBlue)
	{
		crtc->gammaBlue = (CARD16*) malloc(crtc->gammaSize * sizeof(CARD16));
		ZeroMemory(crtc->gammaBlue, crtc->gammaSize * sizeof(CARD16));
	}

	return TRUE;
}

static Bool rdpRROutputSetProperty(ScreenPtr pScreen, RROutputPtr output, Atom property, RRPropertyValuePtr value)
{
	LLOGLN(0, ("rdpRROutputSetProperty"));

	return TRUE;
}

static Bool rdpRROutputValidateMode(ScreenPtr pScreen, RROutputPtr output, RRModePtr mode)
{
	rrScrPrivPtr pScrPriv;

	LLOGLN(0, ("rdpRROutputValidateMode"));

	pScrPriv = rrGetScrPriv(pScreen);
	if ((pScrPriv->minWidth <= mode->mode.width) && (pScrPriv->maxWidth >= mode->mode.width) &&
			(pScrPriv->minHeight <= mode->mode.height) && (pScrPriv->maxHeight >= mode->mode.height))
	{
		return TRUE;
	}

	return FALSE;
}

static void rdpRRModeDestroy(ScreenPtr pScreen, RRModePtr mode)
{
	LLOGLN(0, ("rdpRRModeDestroy"));
}

static Bool rdpRROutputGetProperty(ScreenPtr pScreen, RROutputPtr output, Atom property)
{
	const char* name;

	name = NameForAtom(property);

	LLOGLN(0, ("rdpRROutputGetProperty: Atom: %s", name));

	if (!name)
		return FALSE;

	if (strcmp(name, "EDID"))
	{

	}

	return TRUE;
}

static Bool rdpRRGetPanning(ScreenPtr pScreen, RRCrtcPtr crtc, BoxPtr totalArea, BoxPtr trackingArea, INT16* border)
{
	rdpRandRInfoPtr randr;

	LLOGLN(100, ("rdpRRGetPanning"));

	randr = rdpGetRandRFromScreen(pScreen);

	if (!randr)
		return FALSE;

	if (crtc)
	{
		LLOGLN(100, ("rdpRRGetPanning: ctrc->id: %d", crtc->id));
	}

	if (totalArea)
	{
		totalArea->x1 = 0;
		totalArea->y1 = 0;
		totalArea->x2 = pScreen->width;
		totalArea->y2 = pScreen->height;

		LLOGLN(100, ("rdpRRGetPanning: totalArea: x1: %d y1: %d x2: %d y2: %d",
				totalArea->x1, totalArea->y1, totalArea->x2, totalArea->y2));
	}

	if (trackingArea)
	{
		trackingArea->x1 = 0;
		trackingArea->y1 = 0;
		trackingArea->x2 = pScreen->width;
		trackingArea->y2 = pScreen->height;

		LLOGLN(100, ("rdpRRGetPanning: trackingArea: x1: %d y1: %d x2: %d y2: %d",
				trackingArea->x1, trackingArea->y1, trackingArea->x2, trackingArea->y2));
	}

	if (border)
	{
		border[0] = 0;
		border[1] = 0;
		border[2] = 0;
		border[3] = 0;
	}

	return TRUE;
}

static Bool rdpRRSetPanning(ScreenPtr pScrn, RRCrtcPtr crtc, BoxPtr totalArea, BoxPtr trackingArea, INT16* border)
{
	LLOGLN(0, ("rdpRRSetPanning"));

	return TRUE;
}

#if (RANDR_INTERFACE_VERSION >= 0x0104)

static Bool rdpRRCrtcSetScanoutPixmap(RRCrtcPtr crtc, PixmapPtr pixmap)
{
	LLOGLN(0, ("rdpRRCrtcSetScanoutPixmap"));

	return TRUE;
}

static Bool rdpRRProviderSetOutputSource(ScreenPtr pScreen, RRProviderPtr provider, RRProviderPtr output_source)
{
	LLOGLN(0, ("rdpRRProviderSetOutputSource"));

	return TRUE;
}

static Bool rdpRRProviderSetOffloadSink(ScreenPtr pScreen, RRProviderPtr provider, RRProviderPtr offload_sink)
{
	LLOGLN(0, ("rdpRRProviderSetOffloadSink"));

	return TRUE;
}

static Bool rdpRRProviderGetProperty(ScreenPtr pScreen, RRProviderPtr provider, Atom property)
{
	LLOGLN(0, ("rdpRRProviderGetProperty"));

	return TRUE;
}

static Bool rdpRRProviderSetProperty(ScreenPtr pScreen, RRProviderPtr provider, Atom property, RRPropertyValuePtr value)
{
	LLOGLN(0, ("rdpRRProviderGetProperty"));

	return TRUE;
}

static void rdpRRProviderDestroy(ScreenPtr pScreen, RRProviderPtr provider)
{
	LLOGLN(0, ("rdpRRProviderDestroy"));
}

#endif

int rdpRRInit(ScreenPtr pScreen)
{
#if RANDR_12_INTERFACE
	rdpRandRInfoPtr randr;
#if (RANDR_INTERFACE_VERSION >= 0x0104)
	RRProviderPtr provider;
	uint32_t capabilities;
#endif
#endif
	rrScrPrivPtr pScrPriv;

	LLOGLN(0, ("rdpRRInit"));

	rdpRandRKey = &rdpRandRKeyRec;

	if (!dixRegisterPrivateKey(&rdpRandRKeyRec, PRIVATE_SCREEN, 0))
		return FALSE;

	randr = (rdpRandRInfoPtr) malloc(sizeof(rdpRandRInfoRec));
	ZeroMemory(randr, sizeof(rdpRandRInfoRec));

	if (!RRScreenInit(pScreen))
		return -1;

	pScrPriv = rrGetScrPriv(pScreen);
	dixSetPrivate(&pScreen->devPrivates, rdpRandRKey, randr);

	pScrPriv->rrSetConfig = rdpRRSetConfig;

	pScrPriv->rrGetInfo = rdpRRGetInfo;

	pScrPriv->rrScreenSetSize = rdpRRScreenSetSize;

#if RANDR_12_INTERFACE
	pScrPriv->rrCrtcSet = rdpRRCrtcSet;
	pScrPriv->rrCrtcSetGamma = rdpRRCrtcSetGamma;
	pScrPriv->rrCrtcGetGamma = rdpRRCrtcGetGamma;
	pScrPriv->rrOutputSetProperty = rdpRROutputSetProperty;
	pScrPriv->rrOutputValidateMode = rdpRROutputValidateMode;
	pScrPriv->rrModeDestroy = rdpRRModeDestroy;

#if RANDR_13_INTERFACE
	pScrPriv->rrOutputGetProperty = rdpRROutputGetProperty;
	pScrPriv->rrGetPanning = rdpRRGetPanning;
	pScrPriv->rrSetPanning = rdpRRSetPanning;
#endif

#if (RANDR_INTERFACE_VERSION >= 0x0104)
	pScrPriv->rrCrtcSetScanoutPixmap = rdpRRCrtcSetScanoutPixmap;
	pScrPriv->rrProviderSetOutputSource = rdpRRProviderSetOutputSource;
	pScrPriv->rrProviderSetOffloadSink = rdpRRProviderSetOffloadSink;
	pScrPriv->rrProviderGetProperty = rdpRRProviderGetProperty;
	pScrPriv->rrProviderSetProperty = rdpRRProviderSetProperty;
	pScrPriv->rrProviderDestroy = rdpRRProviderDestroy;
#endif

	pScreen->x = 0;
	pScreen->y = 0;
	pScreen->mmWidth = rdpScreenPixelToMM(pScreen->width);
	pScreen->mmHeight = rdpScreenPixelToMM(pScreen->height);

	randr->x = pScreen->x;
	randr->y = pScreen->y;
	randr->width = pScreen->width;
	randr->height = pScreen->height;
	randr->mmWidth = pScreen->mmWidth;
	randr->mmHeight = pScreen->mmHeight;

	rdpProbeModes(pScreen);

	RRScreenSetSizeRange(pScreen, 8, 8, g_width_max > 0 ? g_width_max : 16384, g_height_max > 0 ? g_height_max : 16384);

	randr->numCrtcs = 1;
	randr->crtcs = (RRCrtcPtr*) malloc(sizeof(RRCrtcPtr) * randr->numCrtcs);

	randr->crtc = RRCrtcCreate(pScreen, NULL);
	randr->crtcs[0] = randr->crtc;

	if (!randr->crtc)
		return FALSE;

	RRCrtcGammaSetSize(randr->crtc, 256);

	randr->rotations = RR_Rotate_0;
	RRCrtcSetRotations(randr->crtc, randr->rotations);

	RRCrtcSetTransformSupport(randr->crtc, TRUE);

	randr->numOutputs = 1;
	randr->outputs = (RROutputPtr*) malloc(sizeof(RROutputPtr) * randr->numOutputs);

	randr->output = RROutputCreate(pScreen, "RDP-0", strlen("RDP-0"), NULL);
	randr->outputs[0] = randr->output;

	if (!randr->output)
		return -1;

	if (!RROutputSetModes(randr->output, randr->modes, randr->numModes, 0))
		return -1;

	if (!RROutputSetCrtcs(randr->output, randr->crtcs, randr->numCrtcs))
		return -1;

	if (!RROutputSetSubpixelOrder(randr->output, SubPixelUnknown))
		return -1;

	if (!RROutputSetPhysicalSize(randr->output, randr->mmWidth, randr->mmHeight))
		return -1;

	if (!RROutputSetConnection(randr->output, RR_Connected))
		return -1;

	pScrPriv->primaryOutput = randr->output;

	randr->edid = rdpConstructScreenEdid(pScreen);
	rdpSetOutputEdid(randr->output, randr->edid);

#if (RANDR_INTERFACE_VERSION >= 0x0104)
	provider = RRProviderCreate(pScreen, "RDP", strlen("RDP"));

	capabilities = RR_Capability_None;
	capabilities |= RR_Capability_SourceOutput;
	capabilities |= RR_Capability_SinkOutput;
	capabilities |= RR_Capability_SourceOffload;
	capabilities |= RR_Capability_SinkOffload;

	RRProviderSetCapabilities(provider, capabilities);
#endif

	RRCrtcNotify(randr->crtc, randr->mode, randr->x, randr->y, randr->rotations,
			RRCrtcGetTransform(randr->crtc), randr->numOutputs, randr->outputs);
#endif

	return 0;
}
