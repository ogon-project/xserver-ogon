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
 * Norbert Federa <norbert.federa@thincast.com>
 */

#include "rdp.h"

#include "events.h"
#include "eventstr.h"

#include "input.h"
#include "inpututils.h"

#include <winpr/input.h>

#include "rdpInput.h"
#include <xkbsrv.h>


/**
 * Set XOGON_ENABLE_CUSTOM_AUTOREPEAT to 1 in order to disable the X server's
 * own auto keyboard repeat and to produce a key down event for every key down
 * event received from the RDP client.
 * This prevents unwanted key repeats caused by network latency and implicitly
 * adapts the session's repeat delay and repeat rate to the remote workstation
 * or thin client.
 */
#define XOGON_ENABLE_CUSTOM_AUTOREPEAT 1


extern DeviceIntPtr g_pointer;
extern DeviceIntPtr g_keyboard;
extern DeviceIntPtr g_multitouch;
extern rdpScreenInfoRec g_rdpScreen;
extern int g_nokpcursors;
extern int g_active;

static int g_old_button_mask = 0;

/* Copied from Xvnc/lib/font/util/utilbitmap.c */
static unsigned char g_reverse_byte[0x100] =
{
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

static void KbdDeviceOn(void)
{

}

static void KbdDeviceOff(void)
{

}

static void rdpBell(int volume, DeviceIntPtr pDev, void *ctrl, int cls)
{

}

static void rdpChangeKeyboardControl(DeviceIntPtr pDev, KeybdCtrl *ctrl)
{

}

int rdpKeybdProc(DeviceIntPtr pDevice, int onoff)
{
	DevicePtr pDev;
	XkbRMLVOSet set;

	pDev = (DevicePtr) pDevice;

	switch (onoff)
	{
		case DEVICE_INIT:
			ZeroMemory(&set, sizeof(set));
			set.rules = strdup("evdev");
			set.model = strdup("pc104");
			set.layout = strdup("us");
			set.variant = NULL;
			set.options = NULL;
			InitKeyboardDeviceStruct(pDevice, &set, rdpBell,
					rdpChangeKeyboardControl);
			XkbFreeRMLVOSet(&set, FALSE);
			//XkbDDXChangeControls(pDevice, 0, 0);
			break;

		case DEVICE_ON:
			pDev->on = 1;
			KbdDeviceOn();
			break;

		case DEVICE_OFF:
			pDev->on = 0;
			KbdDeviceOff();
			break;

		case DEVICE_CLOSE:

			if (pDev->on)
			{
				KbdDeviceOff();
			}

			break;
	}

	return Success;
}

static void PtrDeviceInit(void)
{
}

static void PtrDeviceOn(DeviceIntPtr pDev)
{
}

static void PtrDeviceOff(void)
{
}

static void rdpMouseCtrl(DeviceIntPtr pDevice, PtrCtrl *pCtrl)
{
}

int rdpMouseProc(DeviceIntPtr pDevice, int onoff)
{
	int i;
	BYTE map[10];
	DevicePtr pDev;
#define NAXES    4
#define NBUTTONS 10
	Atom btn_labels[NBUTTONS];
	Atom axes_labels[NAXES];

	pDev = (DevicePtr) pDevice;

	switch (onoff)
	{
		case DEVICE_INIT:
			PtrDeviceInit();

			for (i = 0; i < 10; i++)
				map[i] = i;

			/*  .
			 * /!\ button 0 is not used
			 */
			btn_labels[0] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_UNKNOWN);
			btn_labels[1] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_LEFT);
			btn_labels[2] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_MIDDLE);
			btn_labels[3] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_RIGHT);
			btn_labels[4] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_WHEEL_DOWN);
			btn_labels[5] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_WHEEL_UP);
			btn_labels[6] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_HWHEEL_LEFT);
			btn_labels[7] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_HWHEEL_RIGHT);
			btn_labels[8] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_8);
			btn_labels[9] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_9);

			axes_labels[0] = XIGetKnownProperty(AXIS_LABEL_PROP_REL_X);
			axes_labels[1] = XIGetKnownProperty(AXIS_LABEL_PROP_REL_Y);
			axes_labels[2] = XIGetKnownProperty(AXIS_LABEL_PROP_REL_WHEEL);
			axes_labels[3] = XIGetKnownProperty(AXIS_LABEL_PROP_REL_HWHEEL);

			InitPointerDeviceStruct(pDev, map, NBUTTONS, btn_labels, rdpMouseCtrl, GetMotionHistorySize(), NAXES, axes_labels);
			InitValuatorAxisStruct(pDevice, 0, axes_labels[0], NO_AXIS_LIMITS, NO_AXIS_LIMITS, 1, 0, 1, Relative);
			InitValuatorAxisStruct(pDevice, 1, axes_labels[1], NO_AXIS_LIMITS, NO_AXIS_LIMITS, 1, 0, 1, Relative);
			InitValuatorAxisStruct(pDevice, 2, axes_labels[2], NO_AXIS_LIMITS, NO_AXIS_LIMITS, 1, 0, 1, Relative);
			InitValuatorAxisStruct(pDevice, 3, axes_labels[3], NO_AXIS_LIMITS, NO_AXIS_LIMITS, 1, 0, 1, Relative);
			SetScrollValuator(pDevice, 2, SCROLL_TYPE_VERTICAL, -1.0, SCROLL_FLAG_PREFERRED);
			SetScrollValuator(pDevice, 3, SCROLL_TYPE_HORIZONTAL, -1.0, SCROLL_FLAG_NONE);
			break;

		case DEVICE_ON:
			pDev->on = 1;
			PtrDeviceOn(pDevice);
			break;

		case DEVICE_OFF:
			pDev->on = 0;
			PtrDeviceOff();
			break;

		case DEVICE_CLOSE:

			if (pDev->on)
			{
				PtrDeviceOff();
			}

			break;
	}

	return Success;
}


static int multitouchProc(DeviceIntPtr pDevice, int onoff)
{
	BYTE map[10];
	Atom btn_labels[10];
	Atom axes_labels[3];
	DevicePtr pDev;
	int i;

	pDev = (DevicePtr) pDevice;

	switch (onoff)
	{
	case DEVICE_INIT:
		for (i = 0; i < 10; i++)
			map[i] = i;

		btn_labels[0] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_LEFT);
		btn_labels[1] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_MIDDLE);
		btn_labels[2] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_RIGHT);
		btn_labels[3] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_WHEEL_DOWN);
		btn_labels[4] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_WHEEL_UP);
		btn_labels[5] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_HWHEEL_LEFT);
		btn_labels[6] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_HWHEEL_RIGHT);
		btn_labels[7] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_7);
		btn_labels[8] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_8);
		btn_labels[9] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_9);

		axes_labels[0] = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_MT_POSITION_X);
		axes_labels[1] = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_MT_POSITION_Y);

		InitPointerDeviceStruct(pDev, map, 10, btn_labels, rdpMouseCtrl, GetMotionHistorySize(), 3, axes_labels);
		InitTouchClassDeviceStruct(pDevice, 10, XIDirectTouch, 2);

		InitValuatorClassDeviceStruct(pDevice, 2, axes_labels, 1, Absolute);
		InitValuatorAxisStruct(pDevice, 0, axes_labels[0], NO_AXIS_LIMITS, NO_AXIS_LIMITS, 1, 0, 1, Absolute);
		InitValuatorAxisStruct(pDevice, 1, axes_labels[1], NO_AXIS_LIMITS, NO_AXIS_LIMITS, 1, 0, 1, Absolute);
		break;

	case DEVICE_ON:
		pDev->on = 1;
		break;

	case DEVICE_OFF:
		pDev->on = 0;
		break;

	case DEVICE_CLOSE:
		if (pDev->on)
		{
			PtrDeviceOff();
		}
		break;
	}

	return Success;
}


Bool rdpCursorOffScreen(ScreenPtr* ppScreen, int* x, int* y)
{
	return 0;
}

void rdpCrossScreen(ScreenPtr pScreen, Bool entering)
{

}

void rdpPointerWarpCursor(DeviceIntPtr pDev, ScreenPtr pScr, int x, int y)
{
	miPointerWarpCursor(pDev, pScr, x, y);
}

Bool rdpSpriteRealizeCursor(DeviceIntPtr pDev, ScreenPtr pScr, CursorPtr pCurs)
{
	return 1;
}

Bool rdpSpriteUnrealizeCursor(DeviceIntPtr pDev, ScreenPtr pScr, CursorPtr pCurs)
{
	return TRUE;
}

static int get_pixel_safe(char *data, int x, int y, int width, int height, int bpp)
{
	int start;
	int shift;
	int c;
	unsigned int *src32;

	if (x < 0)
	{
		return 0;
	}

	if (y < 0)
	{
		return 0;
	}

	if (x >= width)
	{
		return 0;
	}

	if (y >= height)
	{
		return 0;
	}

	if (bpp == 1)
	{
		width = (width + 7) / 8;
		start = (y * width) + x / 8;
		shift = x % 8;
		c = (unsigned char)(data[start]);
#if (X_BYTE_ORDER == X_LITTLE_ENDIAN)
		return (g_reverse_byte[c] & (0x80 >> shift)) != 0;
#else
		return (c & (0x80 >> shift)) != 0;
#endif
	}
	else if (bpp == 32)
	{
		src32 = (unsigned int*)data;
		return src32[y * width + x];
	}

	return 0;
}

static int GetBit(unsigned char *line, int x)
{
	unsigned char mask;

	if (screenInfo.bitmapBitOrder == LSBFirst)
		mask = (1 << (x & 7));
	else
		mask = (0x80 >> (x & 7));

	line += (x >> 3);

	if (*line & mask)
		return 1;

	return 0;
}

static void set_pixel_safe(char *data, int x, int y, int width, int height, int bpp, int pixel)
{
	int start;
	int shift;
	unsigned int *dst32;

	if (x < 0)
	{
		return;
	}

	if (y < 0)
	{
		return;
	}

	if (x >= width)
	{
		return;
	}

	if (y >= height)
	{
		return;
	}

	if (bpp == 1)
	{
		width = (width + 7) / 8;
		start = (y * width) + x / 8;
		shift = x % 8;

		if (pixel & 1)
		{
			data[start] = data[start] | (0x80 >> shift);
		}
		else
		{
			data[start] = data[start] & ~(0x80 >> shift);
		}
	}
	else if (bpp == 24)
	{
		*(data + (3 * (y * width + x)) + 0) = pixel >> 0;
		*(data + (3 * (y * width + x)) + 1) = pixel >> 8;
		*(data + (3 * (y * width + x)) + 2) = pixel >> 16;
	}
	else if (bpp == 32)
	{
		dst32 = (unsigned int*)data;
		dst32[y * width + x] = pixel;
	}
}

static inline void set_rdp_pointer_andmask_bit(char *data, int x, int y, int width, int height, Bool on)
{
	/**
	 * MS-RDPBCGR 2.2.9.1.1.4.4:
	 * andMaskData (variable): A variable-length array of bytes.
	 * Contains the 1-bpp, bottom-up AND mask scan-line data.
	 * The AND mask is padded to a 2-byte boundary for each encoded scan-line.
	 */
	int stride, offset;
	char mvalue;

	if (width < 0 || x < 0 || x >= width) {
		return;
	}

	if (height < 0 || y < 0 || y >= height) {
		return;
	}

	stride = ((width + 15) >> 4) * 2;
	offset = stride * (height-1-y) + (x >> 3);
	mvalue = 0x80 >> (x & 7);

	if (on) {
		data[offset] |= mvalue;
	} else {
		data[offset] &= ~mvalue;
	}
}

void rdpSpriteSetCursor(DeviceIntPtr pDev, ScreenPtr pScr, CursorPtr pCurs, int x, int y)
{
	/**
	 * The maximum allowed RDP pointer shape width and height is 96 pixels
	 * if the client indicated support for large pointers (LARGE_POINTER_FLAG),
	 * otherwise the maximum width and height is 32.
	 */

	char cur_data[96 * 96 * 4];
	char cur_mask[96 * 96 / 8];
	int i, j, w, h, cw, ch;
	int bpp = 32;
	ogon_msg_set_pointer msg;

	g_rdpScreen.pCurs = pCurs;

	if (!g_active)
		return;

	if (pCurs && pCurs->bits)
	{
		w = pCurs->bits->width;
		h = pCurs->bits->height;
	}
	else
	{
		w = 0;
		h = 0;
	}

	if (w < 1 || h < 1)
	{
		/* cursor must be hidden: send a rdp null system pointer */
		ogon_msg_set_system_pointer spmsg;
		spmsg.ptrType = SYSPTR_NULL;
		spmsg.clientId = 0;
		rdp_send_message(OGON_SERVER_SET_SYSTEM_POINTER, (ogon_message *) &spmsg);
		return;
	}

	cw = MIN(w, 96);
	ch = MIN(h, 96);

	msg.xPos = pCurs->bits->xhot;
	msg.yPos = pCurs->bits->yhot;
	msg.clientId = 0;

	memset(cur_data, 0x00, sizeof(cur_data));
	memset(cur_mask, 0xFF, sizeof(cur_mask));

	if (pCurs->bits->argb)
	{
		int paddedRowBytes;
		unsigned int p;
		char* data;

		data = (char*)(pCurs->bits->argb);
		paddedRowBytes = PixmapBytePad(w, bpp);

		for (j = 0; j < ch; j++)
		{
			for (i = 0; i < cw; i++)
			{
				p = get_pixel_safe(data, i, j, paddedRowBytes / 4, h, bpp);
				if (p>>24) {
					set_rdp_pointer_andmask_bit(cur_mask, i, j, cw, ch, FALSE);
					set_pixel_safe(cur_data, i, ch - 1 - j, cw, ch, bpp, p);
				}
			}
		}
	}
	else
	{
		unsigned char *srcLine = pCurs->bits->source;
		unsigned char *mskLine = pCurs->bits->mask;
		int stride = BitmapBytePad(w);
		CARD32 fg, bg;
		if (!pCurs->bits->source)
		{
			ErrorF(("cursor bits are zero\n"));
			return;
		}

		fg = GetColor(PIXEL_FORMAT_ARGB32, pCurs->foreRed, pCurs->foreGreen, pCurs->foreBlue, 0xff);
		bg = GetColor(PIXEL_FORMAT_ARGB32, pCurs->backRed, pCurs->backGreen, pCurs->backBlue, 0xff);
		for (i = 0; i < h; i++) {
			for (j = 0; j < w; j++) {
				if (GetBit(mskLine, j))
				{
					set_rdp_pointer_andmask_bit(cur_mask, j, i, cw, ch, FALSE);

					if (GetBit(srcLine, j))
						set_pixel_safe(cur_data, j, ch - 1 - i, cw, ch, bpp, fg);
					else
						set_pixel_safe(cur_data, j, ch - 1 - i, cw, ch, bpp, bg);
				}
				else
				{
					set_pixel_safe(cur_data, j, ch - 1 - i, cw, ch, bpp, GetColor(PIXEL_FORMAT_ARGB32, 0,0,0,0));
				}
			}
			srcLine += stride;
			mskLine += stride;
		}
	}

	msg.xorBpp = bpp;
	msg.width = cw;
	msg.height = ch;
	msg.xorMaskData = (BYTE*) cur_data;
	msg.lengthXorMask = ((bpp + 7) >> 3) * cw * ch;
	msg.andMaskData = (BYTE*) cur_mask;
	msg.lengthAndMask = ((cw + 15) >> 4) * 2 * ch;

	rdp_send_message(OGON_SERVER_SET_POINTER, (ogon_message *) &msg);
}

void rdpSpriteMoveCursor(DeviceIntPtr pDev, ScreenPtr pScr, int x, int y)
{

}

Bool rdpSpriteDeviceCursorInitialize(DeviceIntPtr pDev, ScreenPtr pScr)
{
	return TRUE;
}

void rdpSpriteDeviceCursorCleanup(DeviceIntPtr pDev, ScreenPtr pScr)
{

}

static void rdpEnqueueMotion(DeviceIntPtr device, int x, int y, int extraFlags)
{
	int i;
	int nevents;
	double dx, dy;
	int valuators[2];
	ValuatorMask mask;
	InternalEvent* rdp_events;

	dx = (double) x;
	dy = (double) y;
	nevents = GetMaximumEventsNum();
	rdp_events = InitEventList(nevents);

#if (XORG_VERSION_CURRENT > XORG_VERSION(1,14,0))
	miPointerSetPosition(device, Absolute, &dx, &dy, &nevents, rdp_events);
#endif

	valuators[0] = dx;
	valuators[1] = dy;
	valuator_mask_set_range(&mask, 0, 2, valuators);

	nevents = GetPointerEvents(rdp_events, device, MotionNotify, 0,
			POINTER_ABSOLUTE | POINTER_SCREEN | extraFlags, &mask);

	for (i = 0; i < nevents; i++)
		mieqProcessDeviceEvent(device, &rdp_events[i], 0);

	FreeEventList(rdp_events, GetMaximumEventsNum());
}

static void rdpEnqueueTouchEvent(int eventType, int touchId, int x, int y)
{
	int i;
	int nevents;
	double dx, dy;
	int valuators[2];
	ValuatorMask mask;
	InternalEvent* rdp_events;

	dx = (double) x;
	dy = (double) y;
	nevents = GetMaximumEventsNum();
	rdp_events = InitEventList(nevents);

	valuators[0] = dx;
	valuators[1] = dy;
	valuator_mask_set_range(&mask, 0, 2, valuators);

	nevents = GetTouchEvents(rdp_events, g_multitouch, touchId, eventType, 0, &mask);

	for (i = 0; i < nevents; i++)
		mieqProcessDeviceEvent(g_multitouch, &rdp_events[i], 0);

	FreeEventList(rdp_events, GetMaximumEventsNum());
}


static void rdpEnqueueButton(DeviceIntPtr device, int type, int buttons)
{
	int i;
	int nevents;
	ValuatorMask mask;
	InternalEvent* rdp_events;
	int valuators[MAX_VALUATORS] = {0};

	nevents = GetMaximumEventsNum();
	rdp_events = InitEventList(nevents);

	valuator_mask_set_range(&mask, 0, 0, valuators);

	nevents = GetPointerEvents(rdp_events, device, type, buttons, 0, &mask);


	for (i = 0; i < nevents; i++)
		mieqProcessDeviceEvent(device, &rdp_events[i], 0);

	FreeEventList(rdp_events, GetMaximumEventsNum());
}

static void rdpEnqueueKey(int type, int scancode)
{
	int i;
	int nevents;
	InternalEvent* rdp_events;
#if (XOGON_ENABLE_CUSTOM_AUTOREPEAT == 1)
	Bool masterAutoRepeat;
	Bool slaveAutoRepeat;
#endif

	nevents = GetMaximumEventsNum();
	rdp_events = InitEventList(nevents);

	if (g_nokpcursors)
	{
		switch (scancode)
		{
			case 80: // KP_UP
				scancode = 111; // UP
				break;
			case 83: // KP_LEFT
				scancode = 113; // LEFT
				break;
			case 85: // KP_RIGHT
				scancode = 114 ; // RIGHT
				break;
			case 88: // KP_DOWN
				scancode = 116; // DOWN
				break;
			default:
				break;
		}
	}

#if (XOGON_ENABLE_CUSTOM_AUTOREPEAT == 1)
	masterAutoRepeat = g_keyboard->master->kbdfeed->ctrl.autoRepeat;
	slaveAutoRepeat = g_keyboard->kbdfeed->ctrl.autoRepeat;

	if (slaveAutoRepeat) {
		/**
		 * This effectively prevents that any xkbAccessX.c functions initiate
		 * software repeat mode or problematic timers.
		 */
		unsigned int disabled_ctrls = 0;
		disabled_ctrls |= XkbRepeatKeysMask;
		disabled_ctrls |= XkbAccessXKeysMask;
		disabled_ctrls |= XkbSlowKeysMask;
		disabled_ctrls |= XkbBounceKeysMask;
		disabled_ctrls |= XkbAccessXTimeoutMask;

		g_keyboard->key->xkbInfo->desc->ctrls->enabled_ctrls &= ~disabled_ctrls;
		g_keyboard->master->kbdfeed->ctrl.autoRepeat = FALSE;
	}
#endif

	nevents = GetKeyboardEvents(rdp_events, g_keyboard, type, scancode, NULL);

	for (i = 0; i < nevents; i++) {
#if (XOGON_ENABLE_CUSTOM_AUTOREPEAT == 1)
		if (slaveAutoRepeat) {
			/**
			 * If the key is already down (determined by BitIsOn) we need to
			 * tell DDX that generating multiple downs is allowed by setting the
			 * key_repeat device_event member to true
			 */
			if (rdp_events[i].any.type == ET_KeyPress &&
				BitIsOn(g_keyboard->key->down, scancode))
			{
				rdp_events[i].device_event.key_repeat = 1;
			}
		}
#endif
		mieqProcessDeviceEvent(g_keyboard, &rdp_events[i], 0);
	}

#if (XOGON_ENABLE_CUSTOM_AUTOREPEAT == 1)
	g_keyboard->master->kbdfeed->ctrl.autoRepeat = masterAutoRepeat;
	g_keyboard->kbdfeed->ctrl.autoRepeat = slaveAutoRepeat;
#endif

	FreeEventList(rdp_events, GetMaximumEventsNum());
}

void PtrAddMotionEvent(int x, int y)
{
	static int sx = 0;
	static int sy = 0;

	if (sx != x || sy != y)
		rdpEnqueueMotion(g_pointer, x, y, 0);
	sx = x;
	sy = y;
}

void PtrAddButtonEvent(int buttonMask)
{
	int i;
	int type;
	int buttons;

	for (i = 0; i < 10; i++)
	{
		if ((buttonMask ^ g_old_button_mask) & (1 << i))
		{
			if (buttonMask & (1 << i))
			{
				type = ButtonPress;
				buttons = i + 1;
				rdpEnqueueButton(g_pointer, type, buttons);
			}
			else
			{
				type = ButtonRelease;
				buttons = i + 1;
				rdpEnqueueButton(g_pointer, type, buttons);
			}
		}
	}

	g_old_button_mask = buttonMask;
}

void KbdAddScancodeEvent(DWORD flags, DWORD scancode, DWORD keyboardType)
{
	int type = (flags & KBD_FLAGS_DOWN) ? KeyPress : KeyRelease;
	DWORD evdevcode = ogon_rdp_scancode_to_evdev_code(flags, scancode, keyboardType);
#if 0
	fprintf(stderr, "KbdAddScancodeEvent: flags: 0x%04X scancode: 0x%04X evdevcode: 0x%02X (%03d) [%s]\n",
			flags, scancode, evdevcode, evdevcode, ogon_evdev_keyname(evdevcode));
#endif
	rdpEnqueueKey(type, evdevcode + 8);
}

void KbdAddVirtualKeyCodeEvent(DWORD flags, DWORD vkcode)
{
	int type;
	int keycode;
	DWORD vkcodeWithFlags;

	type = (flags & KBD_FLAGS_DOWN) ? KeyPress : KeyRelease;

	vkcodeWithFlags = vkcode | ((flags & KBD_FLAGS_EXTENDED) ? KBDEXT : 0);

	keycode = GetKeycodeFromVirtualKeyCode(vkcodeWithFlags, KEYCODE_TYPE_EVDEV);

	rdpEnqueueKey(type, keycode);
}

void KbdAddUnicodeEvent(DWORD flags, DWORD code)
{
	int type, i, keycode, mapWidth;
	DeviceIntPtr master = g_keyboard->key->xkbInfo->device->master;
	KeySym sym = NoSymbol;
	KeySym newSyms[XkbMaxSymsPerKey];
	KeySymsRec newSymsRec;

	if (code > 0x10ffff)
		return;

	if (code < 0x20 || (code > 0x7e && code < 0xa0))
		return;

	if (code < 0x100)
		sym = (KeySym)code;
	else
		sym = (KeySym)(code | 0x01000000);

	if (sym == NoSymbol)
		return;

#if 0
	KeySym *sym = master->key->xkbInfo->desc->map->syms;
	for (i = 0; i<master->key->xkbInfo->desc->map->num_syms; i++, sym++)
	{
		fprintf(stderr, "DEBUG XXXX %04X\n", *sym);
	}
#endif

#if 0
	{
		KeySymsPtr coremap;
		KeySym* psym;
		int j;

		coremap = XkbGetCoreMap(master);
		if (!coremap)
			return;

		psym = coremap->map;

		for (i = coremap->minKeyCode; i <= coremap->maxKeyCode; i++)
		{
			fprintf(stderr, "keycode %3d:", i);
			for (j=0; j < coremap->mapWidth; j++, psym++)
			{
				fprintf(stderr, " 0x%04x", *psym);
			}
			fprintf(stderr, "\n", *psym);
		}

		free(coremap->map);
		free(coremap);
	}
#endif

	for (i = 0; i < XkbMaxSymsPerKey; i++) {
		newSyms[i] = sym;
	}

	keycode = 255;	/* FIXME: don't assume 255 is unused ! */
	mapWidth = 4;	/* FIXME: use real mapWidth ! */

	newSymsRec.mapWidth = mapWidth;
	newSymsRec.minKeyCode = keycode;
	newSymsRec.maxKeyCode = keycode;
	newSymsRec.map = newSyms;

	XkbApplyMappingChange(master, &newSymsRec, keycode, 1, NULL, serverClient);

	type = (flags & KBD_FLAGS_DOWN) ? KeyPress : KeyRelease;

	rdpEnqueueKey(type, keycode);
}

static void kbdSyncState(int xkb_flags, int rdp_flags, int keycode)
{
	unsigned char rdp_state = rdp_flags ? 1 : 0;
	unsigned char xkb_state = xkb_flags ? 1 : 0;
	if (xkb_state != rdp_state)
	{
		rdpEnqueueKey(KeyPress, keycode);
		rdpEnqueueKey(KeyRelease, keycode);
	}
}

void KbdResetKeyStatesUp(void)
{
	int i, j, k, c;
	CARD8 *down;
	DeviceIntPtr mkbd;

	if (!(mkbd = g_keyboard->key->xkbInfo->device->master))
		return;

	for (i = 0, c = 0, down = mkbd->key->down; i < 32; i++, down++) {
		for (j = 0, k = 1; j < 8; j++, k<<=1, c++) {
			if (*down & k) {
				rdpEnqueueKey(KeyRelease, c);
			}
		}
	}
}

void KbdAddSyncEvent(DWORD flags)
{
	int i;
	int xkb_state;
	KeyClassPtr keyc;
	char numLock = -1;
	char scrollLock = -1;
	DeviceIntPtr pDev = g_keyboard;
	DeviceIntPtr master = pDev->key->xkbInfo->device->master;

	if (!master)
		return; /* no master device */

	KbdResetKeyStatesUp();

	keyc = master->key;
	xkb_state = XkbStateFieldFromRec(&master->key->xkbInfo->state);

	/**
	 * figure out which virtual modifier NumLock and ScrollLock
	 * are and get the the mapped real modifiers
	 */
	for (i = 0; i < XkbNumVirtualMods; ++i)
	{
		Atom mod = keyc->xkbInfo->desc->names->vmods[i];

		if (!mod)
			continue;

		if (!strcmp(NameForAtom(mod), "NumLock"))
			numLock = keyc->xkbInfo->desc->server->vmods[i];
		else if (!strcmp(NameForAtom(mod), "ScrollLock"))
			scrollLock = keyc->xkbInfo->desc->server->vmods[i];
	}

	kbdSyncState(xkb_state & LockMask, flags & KBD_SYNC_CAPS_LOCK, 66);
	kbdSyncState(xkb_state & numLock, flags & KBD_SYNC_NUM_LOCK, 77);
	kbdSyncState(xkb_state & scrollLock, flags & KBD_SYNC_SCROLL_LOCK, 78);
}

void KbdSessionDisconnect(void)
{
	KbdResetKeyStatesUp();
}

UINT multitouchOnClientReady(RdpeiServerContext *context)
{
	int status;

	status = multitouchInit();
	if (status != Success)
	{
		FatalError("Failed to init ogon-backend-x multitouch device.\n");
	}

	return CHANNEL_RC_OK;
}

typedef struct {
	int contactId;
	int x, y;
} TouchContact;

#define MAX_CONTACTS 10
static TouchContact g_contacts[MAX_CONTACTS] = {
		{-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1},
		{-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1},
};

static int movedTouch(int contactId, int x, int y) {
	int i, ret;

	for (i = 0; i < MAX_CONTACTS; i++) {
		if (g_contacts[i].contactId == contactId) {
			ret = (g_contacts[i].x != x) || (g_contacts[i].y != y);
			g_contacts[i].x = x;
			g_contacts[i].y = y;
			return ret;
		}
	}

	for (i = 0; (i < MAX_CONTACTS) && (g_contacts[i].contactId != -1); i++)
		;

	if (i == MAX_CONTACTS) {
		fprintf(stderr, "%s: error can't find an empty slot\n", __FUNCTION__);
		return 1;
	}

	g_contacts[i].contactId = contactId;
	g_contacts[i].x = x;
	g_contacts[i].y = y;
	return 1;

}

static void submitTouchReleased(int contactId) {
	int i;

	for (i = 0; i < MAX_CONTACTS; i++) {
		if (g_contacts[i].contactId == contactId)
			g_contacts[i].contactId = -1;
	}
}

UINT multitouchOnTouchEvent(RdpeiServerContext *context, RDPINPUT_TOUCH_EVENT *touchEvent)
{
	RDPINPUT_TOUCH_FRAME *frame;
	int i, j;

	frame = touchEvent->frames;
	for (i = 0; i < touchEvent->frameCount; i++, frame++)
	{
		RDPINPUT_CONTACT_DATA *contactData = frame->contacts;
		for (j = 0; j < frame->contactCount; j++, contactData++)
		{
			if (contactData->contactFlags & CONTACT_FLAG_UP) {
				rdpEnqueueTouchEvent(XI_TouchEnd, contactData->contactId, contactData->x, contactData->y);
				submitTouchReleased(contactData->contactId);
#if 0
				fprintf(stderr, "end");
#endif
			} else if (contactData->contactFlags & CONTACT_FLAG_DOWN) {
				if (movedTouch(contactData->contactId, contactData->x, contactData->y))
					rdpEnqueueTouchEvent(XI_TouchBegin, contactData->contactId, contactData->x, contactData->y);
#if 0
				fprintf(stderr, "begin");
#endif
			} else {
				if (movedTouch(contactData->contactId, contactData->x, contactData->y))
					rdpEnqueueTouchEvent(XI_TouchUpdate, contactData->contactId, contactData->x, contactData->y);
#if 0
				fprintf(stderr, "update");
#endif
			}

#if 0
			fprintf(stderr, ": flags=0x%x update=%d down=%d up=%d inContact=%d x=%d y=%d\n",
					contactData->contactFlags,
					(contactData->contactFlags & CONTACT_FLAG_UPDATE) != 0,
					(contactData->contactFlags & CONTACT_FLAG_DOWN) != 0,
					(contactData->contactFlags & CONTACT_FLAG_UP) != 0,
					(contactData->contactFlags & CONTACT_FLAG_INCONTACT) != 0,
					contactData->x,
					contactData->y
			);
#endif
		}
	}

	return CHANNEL_RC_OK;
}

UINT multitouchOnTouchReleased(RdpeiServerContext *context, BYTE contactId)
{
	rdpEnqueueTouchEvent(XI_TouchEnd, contactId, 0, 0);
	return CHANNEL_RC_OK;
}


int multitouchInit(void)
{
	if (g_multitouch)
		return Success;

	g_multitouch = AddInputDevice(serverClient, multitouchProc, TRUE);

	if (!g_multitouch)
		return BadAlloc;

	if (asprintf(&g_multitouch->name, "ogon multitouch") == -1)
	{
		g_multitouch->name = NULL;
	    RemoveDevice(g_multitouch, FALSE);
	    return BadAlloc;
	}

	g_multitouch->public.processInputProc = ProcessOtherEvent;
	g_multitouch->public.realInputProc = ProcessOtherEvent;

	g_multitouch->deviceGrab.ActivateGrab = ActivatePointerGrab;
	g_multitouch->deviceGrab.DeactivateGrab = DeactivatePointerGrab;
	g_multitouch->coreEvents = TRUE;
	g_multitouch->spriteInfo->spriteOwner = TRUE;

	g_multitouch->lastSlave = NULL;
	g_multitouch->last.slave = NULL;
	g_multitouch->type = SLAVE;

	if (ActivateDevice(g_multitouch, TRUE) != Success) {
		fprintf(stderr, "unable to activate\n");
	}

	if (!EnableDevice(g_multitouch, TRUE)) {
		fprintf(stderr, "unable to enable\n");
	}

	AttachDevice(NULL, g_multitouch, inputInfo.pointer);

	return Success;
}

